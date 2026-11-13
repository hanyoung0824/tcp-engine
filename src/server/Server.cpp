#include "tcpengine/server/Server.hpp"

#include "ConnectionEventHandler.hpp"
#include "EngineLog.hpp"
#include "HeartbeatMonitor.hpp"
#include "LoginTimeoutMonitor.hpp"
#include "Logger.hpp"
#include "PacketManager.hpp"
#include "ReceiveRateLimiter.hpp"
#include "ServerTransportFactory.hpp"
#include "SessionManager.hpp"
#include "StatsReporter.hpp"
#include "ShutdownSignal.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace tcpengine
{
Server::Server(const ServerSettings& settings, std::uint16_t protocolVersion)
	: m_ioContext(std::make_unique<boost::asio::io_context>()),
	m_packetManager(std::make_unique<PacketManager>()),
	m_settings(settings),
	m_protocolVersion(protocolVersion)
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

int Server::run()
{
	Logger::installTerminateHandler();
	const ScopedLogFlush logFlush;
	if (!Logger::start(m_settings.log)) {
		return 1;
	}

	// 멈춘 io_context에 남은 핸들러는 이미 사라진 객체를 가리킬 수 있어 다시 돌리지 않음
	if (m_running) {
		engineLogError("run() can only be called once");
		return 1;
	}
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
	m_networkStats = std::make_shared<NetworkStats>();
	m_transport = makeServerTransport(m_ioContext->get_executor(), m_settings, makeConnectionEvents(std::move(serverHello)), m_networkStats);
	if (!m_transport) {
		engineLogError("Unsupported transport");
		return 1;
	}

	m_heartbeatMonitor = std::make_unique<HeartbeatMonitor>(m_ioContext->get_executor(), m_networkStats);
	m_receiveRateLimiter = std::make_unique<ReceiveRateLimiter>(m_settings.receivePacketsPerSecond);
	m_loginTimeoutMonitor = std::make_unique<LoginTimeoutMonitor>(m_ioContext->get_executor(), m_settings.loginTimeout,
		[this](const std::shared_ptr<Connection>& connection) {
			engineLogWarning("Login timeout from ", connection->getAddress(), ", closing");
			++m_networkStats->loginTimeouts;
			disconnect(connection, DisconnectReason::LoginTimeout);
		});

	// 어기면 RST로 끊음. 연결이 닫히는 중이 되므로 남은 데이터는 잘못된 데이터로 따로 처리되지 않음
	m_packetManager->setPacketFilter([this](const std::shared_ptr<Connection>& connection, std::uint16_t packetId) {
		if (!m_receiveRateLimiter->tryReceive(*connection)) {
			++m_networkStats->receiveLimitResets;
			connection->reset();
			return false;
		}
		if (packetId != controlPacketId && !m_greetedConnections.contains(connection->getId())) {
			engineLogWarning("Packet ", packetId, " before ClientHello from ", connection->getAddress(), ", resetting");
			connection->reset();
			return false;
		}
		++m_networkStats->packetsIn;
		return true;
	});
	createSessionManager();
	m_statsReporter = std::make_unique<StatsReporter>(m_ioContext->get_executor(), m_settings.statsInterval, m_networkStats, *m_sessionManager);

	const bool started = m_transport->start();
	if (started) {
		m_shutdownSignal = std::make_unique<ShutdownSignal>(m_ioContext->get_executor(),
			[this] { stop(); },
			[this] { forceStop(); });
		m_statsReporter->start();
		m_ioContext->run();
		onStop();
	}

	m_shutdownSignal.reset();
	m_statsReporter.reset();
	m_transport.reset();
	m_sessionManager.reset();
	m_heartbeatMonitor.reset();
	m_receiveRateLimiter.reset();
	m_loginTimeoutMonitor.reset();
	m_greetedConnections.clear();
	m_networkStats.reset();
	return started ? 0 : 1;
}

void Server::stop()
{
	if (m_stopRequested.exchange(true)) {
		return;
	}
	boost::asio::post(*m_ioContext, [this] { shutdown(); });
}

void Server::shutdown()
{
	engineLogInfo("Server shutting down");
	m_statsReporter->stop();
	m_transport->stopAccepting();
	// 세션을 먼저 끝내야 재접속 대기 타이머가 남지 않고, 응용이 onSessionEnded에서 보낸 패킷도 연결과 함께 전송됨
	// 세션이 있는 연결은 여기서 ServerShutdown을 받고 닫히기 시작함
	m_sessionManager->stop();
	// 정리는 모두 이 안에서 끝남. 연결이 다 닫힌 뒤 남는 것은 하트비트 타이머 같은 필요 없는 대기뿐이라 바로 멈춤
	m_transport->closeAll(PacketManager::makeDisconnectPacket(DisconnectReason::ServerShutdown),
		[this] { m_ioContext->stop(); });
}

void Server::forceStop()
{
	m_ioContext->stop();
}

void Server::disconnect(const std::shared_ptr<Connection>& connection, DisconnectReason reason)
{
	connection->send(PacketManager::makeDisconnectPacket(reason));
	connection->close();
}

void Server::registerControlHandlers()
{
	m_packetManager->setControlHandler(ControlType::ClientHello, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		const std::optional<ClientHello> hello = PacketManager::readClientHello(body);
		if (!hello) {
			return false;
		}
		if (hello->engineVersion != engineProtocolVersion || hello->appVersion != m_protocolVersion) {
			engineLogWarning("Version mismatch from ", connection->getAddress(), ": engine ", hello->engineVersion,
				" (server ", engineProtocolVersion, "), app ", hello->appVersion, " (server ", m_protocolVersion, ")");
			disconnect(connection, DisconnectReason::VersionMismatch);
			return true;
		}
		m_greetedConnections.insert(connection->getId());
		return true;
	});

	m_packetManager->setControlHandler(ControlType::Resume, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		std::string token;
		if (!m_greetedConnections.contains(connection->getId())
			|| !body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength) {
			return false;
		}
		m_sessionManager->resume(connection, token);
		return true;
	});

	m_packetManager->setControlHandler(ControlType::Logout, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		if (!body.isEnd()) {
			return false;
		}
		m_sessionManager->logout(*connection);
		return true;
	});
}

std::shared_ptr<const ConnectionEventHandler> Server::makeConnectionEvents(std::shared_ptr<const Connection::Buffer> serverHello)
{
	return std::make_shared<const ConnectionEventHandler>(
		// 접속 직후 ServerHello로 하트비트 값을 알려 줌. 서버는 Ping을 보내지 않고 timeout만 검사함
		[this, serverHello = std::move(serverHello)](const std::shared_ptr<Connection>& connection) {
			connection->send(serverHello);
			m_heartbeatMonitor->start(connection, m_settings.heartbeat, nullptr);
			m_receiveRateLimiter->start(*connection);
			m_loginTimeoutMonitor->start(connection);
			++m_networkStats->connectionsOpened;
			onConnected(connection);
		},
		[this](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
			m_heartbeatMonitor->onReceived(*connection);
			return m_packetManager->onReceive(connection, data);
		},
		[this](const std::shared_ptr<Connection>& connection) {
			m_heartbeatMonitor->stop(*connection);
			m_receiveRateLimiter->stop(*connection);
			m_loginTimeoutMonitor->stop(*connection);
			m_greetedConnections.erase(connection->getId());
			++m_networkStats->connectionsClosed;
			onDisconnected(connection);
			if (m_sessionManager) {
				m_sessionManager->onConnectionClosed(*connection);
			}
		});
}

void Server::createSessionManager()
{
	m_sessionManager = std::make_unique<SessionManager>(m_ioContext->get_executor(), m_settings.session.resumeTimeout,
		SessionManager::Callbacks{
			[this](const std::shared_ptr<Session>& session) {
				m_loginTimeoutMonitor->stop(*session->getConnection());
				onSessionStarted(session);
			},
			[this](const std::shared_ptr<Session>& session) {
				m_loginTimeoutMonitor->stop(*session->getConnection());
				onSessionResumed(session);
			},
			[this](const std::shared_ptr<Session>& session) { onSessionDetached(session); },
			[this](const std::shared_ptr<Session>& session, SessionEndReason reason) { onSessionEnded(session, reason); },
		});
}
}
