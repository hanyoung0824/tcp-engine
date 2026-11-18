#include "tcpengine/server/Server.hpp"

#include "ConnectionGuard.hpp"
#include "ConnectionEvents.hpp"
#include "ConnectionManager.hpp"
#include "ControlPackets.hpp"
#include "EngineLog.hpp"
#include "Logger.hpp"
#include "PacketRouter.hpp"
#include "ServerMonitor.hpp"
#include "SessionManager.hpp"
#include "ShutdownSignalHandler.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <exception>
#include <memory>
#include <span>
#include <string>

namespace tcpengine
{
Server::Server(const ServerSettings& settings, std::uint16_t protocolVersion)
	: m_ioContext(std::make_unique<boost::asio::io_context>()),
	m_packetRouter(std::make_unique<PacketRouter>()),
	m_settings(settings),
	m_protocolVersion(protocolVersion)
{
}

Server::~Server() = default;

bool Server::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (m_runCalled) {
		engineLogError("registerHandler() must be called before run()");
		return false;
	}
	return m_packetRouter->addDispatcher(id, std::move(dispatcher));
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
	return m_sessionManager->startSession(connection);
}

std::shared_ptr<Session> Server::findSession(const std::shared_ptr<Connection>& connection) const
{
	return m_sessionManager ? m_sessionManager->find(*connection) : nullptr;
}

int Server::run()
{
	Logger::installTerminateHandler();
	const ScopedLogging logging(m_settings.log);
	if (!logging.isStarted()) {
		return 1;
	}

	// 멈춘 io_context에 남은 핸들러는 이미 사라진 객체를 가리킬 수 있어 다시 돌리지 않음
	if (m_runCalled) {
		engineLogError("run() can only be called once");
		return 1;
	}
	if (!validateSettings()) {
		return 1;
	}
	m_runCalled = true;
	if (!callOnStart()) {
		return 1;
	}

	createComponents();
	const bool started = m_connectionManager->start();
	if (started) {
		runUntilStopped();
	}
	destroyComponents();
	return started ? 0 : 1;
}

bool Server::validateSettings() const
{
	try {
		m_settings.validate();
		return true;
	}
	catch (const std::exception& e) {
		engineLogError("Invalid server settings: ", e.what());
		return false;
	}
}

bool Server::callOnStart()
{
	try {
		return onStart();
	}
	catch (const std::exception& e) {
		engineLogError("onStart() failed: ", e.what());
		return false;
	}
}

void Server::createComponents()
{
	const boost::asio::any_io_executor executor = m_ioContext->get_executor();

	m_monitor = std::make_unique<ServerMonitor>(executor, m_settings.statsInterval, [this] {
		return ServerMonitor::SessionCounts{ m_sessionManager->getSessionCount(), m_sessionManager->getDetachedCount() };
	});

	m_connectionGuard = std::make_unique<ConnectionGuard>(executor,
		ConnectionGuard::Settings{ m_protocolVersion, m_settings.receivePacketsPerSecond, m_settings.loginTimeout },
		m_monitor->getStats(),
		[this](const std::shared_ptr<Connection>& connection, DisconnectReason reason) { disconnect(connection, reason); });
	m_packetRouter->setPacketFilter([this](const std::shared_ptr<Connection>& connection, std::uint16_t packetId) {
		return m_connectionGuard->allowPacket(connection, packetId);
	});
	m_packetRouter->setControlHandler(ControlType::ClientHello, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		return m_connectionGuard->handleClientHello(connection, body);
	});

	m_sessionManager = std::make_unique<SessionManager>(executor, m_settings.session.resumeTimeout,
		SessionManager::Callbacks{
			[this](const std::shared_ptr<Session>& session) { handleSessionStarted(session); },
			[this](const std::shared_ptr<Session>& session) { handleSessionResumed(session); },
			[this](const std::shared_ptr<Session>& session) { onSessionDetached(session); },
			[this](const std::shared_ptr<Session>& session, SessionEndReason reason) { onSessionEnded(session, reason); },
		});
	m_packetRouter->setControlHandler(ControlType::Resume, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		return handleResume(connection, body);
	});
	m_packetRouter->setControlHandler(ControlType::Logout, [this](const std::shared_ptr<Connection>& connection, PacketReader& body) {
		return handleLogout(connection, body);
	});

	m_connectionManager = std::make_unique<ConnectionManager>(executor, m_settings, m_monitor->getStats(),
		control::serverHello(control::ServerHello{ m_settings.heartbeat }),
		std::make_shared<const ConnectionEvents>(
			[this](const std::shared_ptr<Connection>& connection) { handleConnected(connection); },
			[this](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
				return m_packetRouter->processReceived(connection, data);
			},
			[this](const std::shared_ptr<Connection>& connection) { handleDisconnected(connection); }));
}

void Server::runUntilStopped()
{
	m_shutdownSignalHandler = std::make_unique<ShutdownSignalHandler>(m_ioContext->get_executor(),
		[this] { stop(); },
		[this] { forceStop(); });
	m_monitor->start();
	m_ioContext->run();
	onStop();
}

void Server::destroyComponents()
{
	m_shutdownSignalHandler.reset();
	m_connectionManager.reset();
	m_sessionManager.reset();
	m_connectionGuard.reset();
	m_monitor.reset();
}

void Server::stop()
{
	if (m_stopRequested.exchange(true)) {
		return;
	}
	boost::asio::post(*m_ioContext, [this] { gracefulStop(); });
}

void Server::gracefulStop()
{
	engineLogInfo("Server shutting down");
	m_monitor->stop();
	m_connectionManager->stopAccepting();
	// 세션을 먼저 끝내야 재접속 대기 타이머가 남지 않고, 응용이 onSessionEnded에서 보낸 패킷도 연결과 함께 전송됨
	// 세션이 있는 연결은 여기서 ServerShutdown을 받고 닫히기 시작함
	m_sessionManager->stop();
	// 정리는 모두 이 안에서 끝남. 연결이 다 닫힌 뒤 남는 것은 하트비트 타이머 같은 필요 없는 대기뿐이라 바로 멈춤
	m_connectionManager->closeAll(control::disconnect(DisconnectReason::ServerShutdown),
		[this] { m_ioContext->stop(); });
}

void Server::forceStop()
{
	m_ioContext->stop();
}

void Server::disconnect(const std::shared_ptr<Connection>& connection, DisconnectReason reason)
{
	connection->send(control::disconnect(reason));
	connection->close();
}

void Server::handleConnected(const std::shared_ptr<Connection>& connection)
{
	m_connectionGuard->onConnected(connection);
	onConnected(connection);
}

void Server::handleDisconnected(const std::shared_ptr<Connection>& connection)
{
	m_connectionGuard->onDisconnected(*connection);
	onDisconnected(connection);
	m_sessionManager->onConnectionClosed(*connection);
}

void Server::handleSessionStarted(const std::shared_ptr<Session>& session)
{
	m_connectionGuard->onLoggedIn(*session->getConnection());
	onSessionStarted(session);
}

void Server::handleSessionResumed(const std::shared_ptr<Session>& session)
{
	m_connectionGuard->onLoggedIn(*session->getConnection());
	onSessionResumed(session);
}

bool Server::handleResume(const std::shared_ptr<Connection>& connection, PacketReader& body)
{
	std::string token;
	if (!m_connectionGuard->isHandshakeDone(*connection)
		|| !body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength) {
		return false;
	}
	m_sessionManager->resume(connection, token);
	return true;
}

bool Server::handleLogout(const std::shared_ptr<Connection>& connection, PacketReader& body)
{
	if (!body.isEnd()) {
		return false;
	}
	m_sessionManager->logout(*connection);
	return true;
}
}
