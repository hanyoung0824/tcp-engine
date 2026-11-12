#include "tcpengine/client/Client.hpp"

#include "ClientTransportFactory.hpp"
#include "ConnectionEventHandler.hpp"
#include "EngineLog.hpp"
#include "HeartbeatMonitor.hpp"
#include "Logger.hpp"
#include "PacketManager.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace tcpengine
{
Client::Client(const ClientSettings& settings)
	: m_packetManager(std::make_unique<PacketManager>()),
	m_settings(settings)
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

bool Client::isConnected() const
{
	return m_transport && m_transport->isConnected();
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
	m_heartbeatMonitor = std::make_unique<HeartbeatMonitor>(m_transport->getExecutor());

	registerControlHandlers();

	// 하트비트는 서버가 ServerHello로 알려 준 값으로 시작하므로 처음에는 돌리지 않음
	if (!m_transport->connect(m_settings.host, m_settings.port, makeConnectionEvents())) {
		return 1;
	}

	onRun();

	// onDisconnected가 파생 클래스가 살아 있을 때 호출되도록 소멸자가 아니라 여기서 닫음
	m_transport->close();
	return 0;
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

	// 재접속할 때 보낼 토큰을 보관. 자동 재접속은 아직 없음
	m_packetManager->setControlHandler(ControlType::SessionStarted, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		std::string token;
		if (!body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength) {
			return false;
		}
		m_sessionToken = std::move(token);
		engineLogDebug("Session started");
		return true;
	});
	m_packetManager->setControlHandler(ControlType::SessionResumed, [](const std::shared_ptr<Connection>&, PacketReader& body) {
		engineLogInfo("Session resumed");
		return body.isEnd();
	});
	m_packetManager->setControlHandler(ControlType::ResumeFailed, [this](const std::shared_ptr<Connection>&, PacketReader& body) {
		engineLogInfo("Session resume failed");
		m_sessionToken.clear();
		return body.isEnd();
	});
}

std::shared_ptr<const ConnectionEventHandler> Client::makeConnectionEvents()
{
	auto connectionEvents = std::make_shared<ConnectionEventHandler>();
	connectionEvents->onConnected = [this](const std::shared_ptr<Connection>& connection) {
		onConnected(connection);
	};
	connectionEvents->onReceive = [this](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
		m_heartbeatMonitor->onReceived(*connection);
		return m_packetManager->onReceive(connection, data);
	};
	connectionEvents->onDisconnected = [this](const std::shared_ptr<Connection>& connection) {
		m_heartbeatMonitor->stop(*connection);
		onDisconnected(connection);
	};
	return connectionEvents;
}
}
