#include "tcpengine/client/Client.hpp"

#include "ClientTransportFactory.hpp"
#include "ConnectionEventHandler.hpp"
#include "EngineLog.hpp"
#include "HeartbeatMonitor.hpp"
#include "Logger.hpp"
#include "PacketManager.hpp"
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
	: m_packetManager(std::make_unique<PacketManager>()),
	m_settings(settings),
	m_protocolVersion(protocolVersion)
{
}

Client::~Client() = default;

bool Client::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (m_running) {
		engineLogError("registerHandler() must be called before run()");
		return false;
	}
	return m_packetManager->addDispatcher(id, std::move(dispatcher));
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
	const ScopedLogFlush logFlush;
	if (!Logger::start(m_settings.log)) {
		return 1;
	}
	return start();
}

int Client::start()
{
	// 설정 파일을 거치지 않고 만든 값도 같은 규칙으로 검사
	try {
		m_settings.validate();
	}
	catch (const std::exception& e) {
		engineLogError("Invalid client settings: ", e.what());
		return 1;
	}

	m_running = true;

	try {
		if (!onStart()) {
			return 1;
		}
	}
	catch (const std::exception& e) {
		engineLogError("onStart() failed: ", e.what());
		return 1;
	}

	// 다른 프로토콜은 settings.transport로 고름
	m_transport = makeClientTransport(m_settings);
	if (!m_transport) {
		engineLogError("Unsupported transport");
		return 1;
	}
	// 클라이언트는 통계를 보고하지 않으므로 쌓기만 함
	m_heartbeatMonitor = std::make_unique<HeartbeatMonitor>(m_transport->getExecutor(), std::make_shared<NetworkStats>());
	m_reconnector = std::make_unique<Reconnector>(m_transport->getExecutor(), m_settings.reconnectMaxDelay,
		[this](Reconnector::ConnectResult onResult) { connect(std::move(onResult)); });
	registerControlHandlers();
	m_connectionEvents = makeConnectionEvents();
	m_transport->start();

	// 처음 접속은 실패하면 다시 시도하지 않고 끝냄
	std::promise<bool> firstConnect;
	std::future<bool> connected = firstConnect.get_future();
	connect([&firstConnect](bool result) { firstConnect.set_value(result); });
	if (!connected.get()) {
		m_transport->stop();
		return 1;
	}

	onRun();

	// Logout을 보내고 닫는 일이 끝난 뒤에 transport를 멈춰야 Logout이 닫기보다 먼저 나감
	std::promise<void> quitDone;
	boost::asio::post(m_transport->getExecutor(), [this, &quitDone] {
		quit();
		quitDone.set_value();
	});
	quitDone.get_future().wait();
	m_transport->stop();
	return 0;
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
	// 세션이 있으면 재접속을 기다리지 말고 바로 끝내 달라고 알림
	if (!m_sessionToken.empty()) {
		m_transport->send(PacketManager::makeControlPacket(ControlType::Logout));
	}
	m_transport->close();
}

void Client::registerControlHandlers()
{
	// 서버가 알려 준 간격으로 하트비트를 시작. 클라이언트가 Ping을 보내고 서버가 Pong으로 답함
	m_packetManager->setControlHandler(ControlType::ServerHello, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		const std::optional<ServerHello> hello = PacketManager::readServerHello(body);
		if (!hello) {
			return false;
		}
		m_heartbeatMonitor->start(connection, hello->heartbeat, PacketManager::makePingPacket());
		return true;
	});

	m_packetManager->setControlHandler(ControlType::Disconnect, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		const std::optional<DisconnectReason> reason = PacketManager::readDisconnect(body);
		if (!reason) {
			return false;
		}
		engineLogInfo("Server is closing the connection: ", toString(*reason));
		m_disconnectReason = reason;
		return true;
	});

	// 다시 접속할 때 보낼 토큰을 보관
	m_packetManager->setControlHandler(ControlType::SessionStarted, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		std::string token;
		if (!body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength) {
			return false;
		}
		m_sessionToken = std::move(token);
		engineLogDebug("Session started");
		return true;
	});
	m_packetManager->setControlHandler(ControlType::SessionResumed, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		if (!body.isEnd()) {
			return false;
		}
		engineLogInfo("Session resumed");
		onSessionResumed();
		return true;
	});
	m_packetManager->setControlHandler(ControlType::ResumeFailed, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		if (!body.isEnd()) {
			return false;
		}
		engineLogInfo("Session resume failed, login again");
		m_sessionToken.clear();
		onSessionLost();
		return true;
	});
}

std::shared_ptr<const ConnectionEventHandler> Client::makeConnectionEvents()
{
	return std::make_shared<const ConnectionEventHandler>(
		// 가장 먼저 버전을 알리고, 세션이 있었으면 이어 달라고 요청함
		[this](const std::shared_ptr<Connection>& connection) {
			m_connected = true;
			m_reconnector->reset();
			connection->send(PacketManager::makeClientHelloPacket(ClientHello{ engineProtocolVersion, m_protocolVersion }));
			if (!m_sessionToken.empty()) {
				connection->send(PacketManager::makeControlPacket(ControlType::Resume, m_sessionToken));
			}
			onConnected(connection);
		},
		[this](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
			m_heartbeatMonitor->onReceived(*connection);
			return m_packetManager->onReceive(connection, data);
		},
		[this](const std::shared_ptr<Connection>& connection) {
			m_heartbeatMonitor->stop(*connection);
			m_connected = false;

			DisconnectInfo info;
			info.reason = std::exchange(m_disconnectReason, std::nullopt);
			const bool reconnectable = !info.reason || canReconnectAfter(*info.reason);
			if (!reconnectable) {
				m_sessionToken.clear();
			}
			info.reconnecting = reconnectable && !m_quitting && m_settings.reconnectMaxDelay.count() > 0;

			onDisconnected(connection, info);
			if (info.reconnecting) {
				m_reconnector->schedule();
			}
		});
}
}
