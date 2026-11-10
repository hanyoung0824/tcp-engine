#include "SessionManager.hpp"

#include "Logger.hpp"

namespace tcpengine
{
SessionManager::SessionManager(std::shared_ptr<const SessionConfig> config)
	: m_config(std::move(config))
{
}

void SessionManager::createSession(boost::asio::ip::tcp::socket socket)
{
	if (!m_config->handler.onReceive) {
		engineLogError("No receive handler registered, connection dropped");
		return;
	}

	const Session::SessionId id = m_nextId++;
	auto session = std::make_shared<TcpSession>(std::move(socket), m_config, id,
		[this](Session::SessionId closedId) { removeSession(closedId); });

	m_sessions.emplace(id, session);
	engineLogDebug("Session ", id, " added (", m_sessions.size(), " connected)");

	session->start();
}

void SessionManager::removeSession(Session::SessionId id)
{
	if (m_sessions.erase(id) > 0) {
		engineLogDebug("Session ", id, " removed (", m_sessions.size(), " connected)");
	}
}

void SessionManager::closeAll()
{
	// close()가 removeSession()으로 목록을 바꾸므로 먼저 꺼내 놓고 닫음
	auto sessions = std::move(m_sessions);
	m_sessions.clear();

	engineLogInfo("Closing ", sessions.size(), " sessions");
	for (const auto& [id, session] : sessions) {
		session->close();
	}
}
}
