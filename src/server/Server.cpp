#include "tcpengine/server/Server.hpp"

#include "ConnectionEventHandler.hpp"
#include "HeartbeatMonitor.hpp"
#include "Logger.hpp"
#include "PacketManager.hpp"
#include "ServerTransportFactory.hpp"
#include "SessionManager.hpp"

#include <exception>
#include <memory>
#include <span>
#include <string>

namespace tcpengine
{
Server::Server(const ServerSettings& settings)
	: m_packetManager(std::make_unique<PacketManager>()),
	m_settings(settings)
{
}

Server::~Server() = default;

bool Server::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (m_running) {
		engineLogError("registerHandler() must be called before run()");
		return false;
	}
	return m_packetManager->addDispatcher(id, std::move(dispatcher));
}

bool Server::addSessionDispatcher(std::uint16_t id, detail::TargetDispatcher<Session> dispatcher)
{
	return addPacketDispatcher(id, [this, dispatcher = std::move(dispatcher)](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		std::shared_ptr<Session> session = findSession(connection);
		if (!session) {
			// 연결은 유지하고 패킷만 버림
			engineLogWarning("Packet before login from ", connection->getAddress(), ", dropped");
			return true;
		}
		return dispatcher(session, body);
	});
}

std::shared_ptr<Session> Server::startSession(const std::shared_ptr<Connection>& connection)
{
	if (!m_sessionManager) {
		engineLogError("startSession() must be called while the server is running");
		return nullptr;
	}
	return m_sessionManager->start(connection);
}

std::shared_ptr<Session> Server::findSession(const std::shared_ptr<Connection>& connection) const
{
	return m_sessionManager ? m_sessionManager->find(*connection) : nullptr;
}

int Server::run(std::shared_ptr<LogSink> engineLogSink, std::shared_ptr<LogSink> appLogSink)
{
	const ScopedLogFlush logFlush;
	setEngineLogSink(std::move(engineLogSink));
	setAppLogSink(std::move(appLogSink));
	return start();
}

int Server::start()
{
	// 설정 파일을 거치지 않고 만든 값도 같은 규칙으로 검사
	try {
		m_settings.validate();
	}
	catch (const std::exception& e) {
		engineLogError("Invalid server settings: ", e.what());
		return 1;
	}

	std::shared_ptr<const Connection::Buffer> serverHello = PacketManager::makeServerHelloPacket(ServerHello{ m_settings.heartbeat });
	if (!serverHello) {
		engineLogError("Heartbeat interval and timeout must be 65535 seconds or less");
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

	registerControlHandlers();

	// 다른 프로토콜은 settings.transport로 고름
	std::unique_ptr<ServerTransport> transport = makeServerTransport(m_settings, makeConnectionEvents(std::move(serverHello)));
	if (!transport) {
		engineLogError("Unsupported transport");
		return 1;
	}

	// 하트비트와 세션 타이머가 transport의 io_context를 쓰므로 transport보다 먼저 없앰
	m_heartbeatMonitor = std::make_unique<HeartbeatMonitor>(transport->getExecutor());
	createSessionManager(*transport);

	const bool started = transport->start();
	if (started) {
		transport->run();
		m_sessionManager->endAll();
		onStop();
	}

	m_sessionManager.reset();
	m_heartbeatMonitor.reset();
	return started ? 0 : 1;
}

void Server::registerControlHandlers()
{
	m_packetManager->setControlHandler(ControlType::Resume, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		std::string token;
		if (!body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength) {
			return false;
		}
		m_sessionManager->resume(connection, token);
		return true;
	});
}

std::shared_ptr<const ConnectionEventHandler> Server::makeConnectionEvents(std::shared_ptr<const Connection::Buffer> serverHello)
{
	auto connectionEvents = std::make_shared<ConnectionEventHandler>();

	// 접속 직후 ServerHello로 하트비트 값을 알려 줌. 서버는 Ping을 보내지 않고 timeout만 검사함
	connectionEvents->onConnected = [this, serverHello = std::move(serverHello)](const std::shared_ptr<Connection>& connection) {
		connection->send(serverHello);
		m_heartbeatMonitor->start(connection, m_settings.heartbeat, nullptr);
		onConnected(connection);
	};
	connectionEvents->onReceive = [this](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
		m_heartbeatMonitor->onReceived(*connection);
		return m_packetManager->onReceive(connection, data);
	};
	connectionEvents->onDisconnected = [this](const std::shared_ptr<Connection>& connection) {
		m_heartbeatMonitor->stop(*connection);
		onDisconnected(connection);
		if (m_sessionManager) {
			m_sessionManager->onConnectionClosed(*connection);
		}
	};
	return connectionEvents;
}

void Server::createSessionManager(ServerTransport& transport)
{
	m_sessionManager = std::make_unique<SessionManager>(transport.getExecutor(), m_settings.session.resumeTimeout,
		SessionManager::Callbacks{
			[this](const std::shared_ptr<Session>& session) { onSessionStarted(session); },
			[this](const std::shared_ptr<Session>& session) { onSessionResumed(session); },
			[this](const std::shared_ptr<Session>& session) { onSessionDetached(session); },
			[this](const std::shared_ptr<Session>& session) { onSessionEnded(session); },
		});
}
}
