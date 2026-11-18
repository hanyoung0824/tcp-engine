#include "tcpengine/client/Client.hpp"

#include "ClientTransportFactory.hpp"
#include "ConnectionEvents.hpp"
#include "ControlPackets.hpp"
#include "EngineLog.hpp"
#include "HeartbeatChecker.hpp"
#include "Logger.hpp"
#include "PacketRouter.hpp"
#include "Reconnector.hpp"

#include <boost/asio/post.hpp>

#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace tcpengine
{
Client::Client(const ClientSettings& settings, std::uint16_t protocolVersion)
	: m_packetRouter(std::make_unique<PacketRouter>()),
	m_settings(settings),
	m_protocolVersion(protocolVersion)
{
}

Client::~Client() = default;

bool Client::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (m_runCalled) {
		engineLogError("registerHandler() must be called before run()");
		return false;
	}
	return m_packetRouter->addDispatcher(id, std::move(dispatcher));
}

void Client::send(const PacketBase& packet)
{
	if (!m_transport) {
		return;
	}
	if (auto buffer = packet.serialize()) {
		m_transport->send(std::move(buffer));
	}
}

int Client::run()
{
	Logger::installTerminateHandler();
	const ScopedLogging logging(m_settings.log);
	if (!logging.isStarted()) {
		return 1;
	}

	if (!validateSettings()) {
		return 1;
	}
	m_runCalled = true;
	if (!callOnStart() || !createComponents()) {
		return 1;
	}

	m_transport->start();
	if (!connectFirstTime()) {
		m_transport->stop();
		return 1;
	}

	onRun();

	quitAndWait();
	m_transport->stop();
	return 0;
}

bool Client::validateSettings() const
{
	try {
		m_settings.validate();
		return true;
	}
	catch (const std::exception& e) {
		engineLogError("Invalid client settings: ", e.what());
		return false;
	}
}

bool Client::callOnStart()
{
	try {
		return onStart();
	}
	catch (const std::exception& e) {
		engineLogError("onStart() failed: ", e.what());
		return false;
	}
}

bool Client::createComponents()
{
	m_transport = makeClientTransport(m_settings);
	if (!m_transport) {
		engineLogError("Unsupported transport");
		return false;
	}

	// 클라이언트는 통계를 보고하지 않으므로 쌓기만 함
	m_heartbeatChecker = std::make_unique<HeartbeatChecker>(m_transport->getExecutor(), std::make_shared<NetworkStats>());
	m_reconnector = std::make_unique<Reconnector>(m_transport->getExecutor(), m_settings.reconnectMaxDelay,
		[this](Reconnector::ConnectResult onResult) { connect(std::move(onResult)); });

	m_packetRouter->setControlHandler(ControlType::ServerHello, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		return handleServerHello(connection, body);
	});
	m_packetRouter->setControlHandler(ControlType::Disconnect, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		return handleDisconnectNotice(body);
	});
	m_packetRouter->setControlHandler(ControlType::SessionStarted, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		return handleSessionStarted(body);
	});
	m_packetRouter->setControlHandler(ControlType::SessionResumed, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		return handleSessionResumed(body);
	});
	m_packetRouter->setControlHandler(ControlType::ResumeFailed, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		return handleResumeFailed(body);
	});

	m_connectionEvents = std::make_shared<const ConnectionEvents>(
		[this](const std::shared_ptr<Connection>& connection) { handleConnected(connection); },
		[this](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
			m_heartbeatChecker->onReceived(*connection);
			return m_packetRouter->processReceived(connection, data);
		},
		[this](const std::shared_ptr<Connection>& connection) { handleDisconnected(connection); });
	return true;
}

bool Client::connectFirstTime()
{
	std::promise<bool> result;
	std::future<bool> connected = result.get_future();
	connect([&result](bool success) { result.set_value(success); });
	return connected.get();
}

void Client::quitAndWait()
{
	// Logout이 연결을 닫기 전에 나가도록 quit()이 끝난 뒤에 transport를 멈춰야 함
	std::promise<void> done;
	boost::asio::post(m_transport->getExecutor(), [this, &done] {
		quit();
		done.set_value();
	});
	done.get_future().wait();
}

void Client::connect(std::function<void(bool connected)> onResult)
{
	m_transport->connect(m_settings.host, m_settings.port, m_settings.connectTimeout, m_connectionEvents,
		[this, onResult = std::move(onResult)](bool connected) {
			// 끝내는 중에 진행하던 재접속이 성공함. 남겨 두면 네트워크 스레드가 끝나지 않으므로 바로 닫음
			if (connected && m_quitting) {
				m_transport->close();
				return;
			}
			onResult(connected);
		});
}

void Client::quit()
{
	m_quitting = true;
	m_reconnector->cancel();
	// 재접속을 기다리지 말고 세션을 바로 끝내 달라고 알림
	if (!m_sessionToken.empty()) {
		m_transport->send(control::make(ControlType::Logout));
	}
	m_transport->close();
}

void Client::handleConnected(const std::shared_ptr<Connection>& connection)
{
	m_connected = true;
	m_reconnector->resetDelay();
	connection->send(control::clientHello(control::ClientHello{ engineProtocolVersion, m_protocolVersion }));
	if (!m_sessionToken.empty()) {
		connection->send(control::make(ControlType::Resume, m_sessionToken));
	}
	onConnected(connection);
}

void Client::handleDisconnected(const std::shared_ptr<Connection>& connection)
{
	m_heartbeatChecker->stop(*connection);
	m_connected = false;

	DisconnectInfo info;
	info.reason = std::exchange(m_disconnectReason, std::nullopt);
	const bool reconnectable = !info.reason || canReconnectAfter(*info.reason);
	if (!reconnectable) {
		m_sessionToken.clear();
	}
	info.willReconnect = reconnectable && !m_quitting && m_settings.reconnectMaxDelay.count() > 0;

	onDisconnected(connection, info);
	if (info.willReconnect) {
		m_reconnector->schedule();
	}
}

bool Client::handleServerHello(const std::shared_ptr<Connection>& connection, PacketReader& body)
{
	const std::optional<control::ServerHello> hello = control::readServerHello(body);
	if (!hello) {
		return false;
	}
	m_heartbeatChecker->start(connection, hello->heartbeat, control::ping());
	return true;
}

bool Client::handleDisconnectNotice(PacketReader& body)
{
	const std::optional<DisconnectReason> reason = control::readDisconnect(body);
	if (!reason) {
		return false;
	}
	engineLogInfo("Server is closing the connection: ", toString(*reason));
	m_disconnectReason = reason;
	return true;
}

bool Client::handleSessionStarted(PacketReader& body)
{
	std::string token;
	if (!body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength) {
		return false;
	}
	m_sessionToken = std::move(token);
	engineLogDebug("Session started");
	return true;
}

bool Client::handleSessionResumed(PacketReader& body)
{
	if (!body.isEnd()) {
		return false;
	}
	engineLogInfo("Session resumed");
	onSessionResumed();
	return true;
}

bool Client::handleResumeFailed(PacketReader& body)
{
	if (!body.isEnd()) {
		return false;
	}
	engineLogInfo("Session resume failed, login again");
	m_sessionToken.clear();
	onSessionLost();
	return true;
}
}
