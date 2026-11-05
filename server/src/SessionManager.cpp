#include "SessionManager.hpp"

#include "Log.hpp"

namespace tcpengine
{
SessionManager::SessionManager(TcpSession::EventHandler handler, HeartbeatSettings heartbeat,
	std::shared_ptr<const Session::Buffer> pingData)
	: m_handler(std::move(handler)),
	m_heartbeat(heartbeat),
	m_pingData(std::move(pingData))
{
}

void SessionManager::create(boost::asio::ip::tcp::socket socket)
{
	if (!m_handler.onReceive) {
		writeLog(LogLevel::Error, "No receive handler registered, connection dropped");
		return;
	}

	const Session::SessionId id = m_nextId++;
	auto session = std::make_shared<TcpSession>(std::move(socket), m_handler, m_heartbeat, m_pingData, id,
		[this](Session::SessionId closedId) { remove(closedId); });

	m_sessions.emplace(id, session);
	writeLog(LogLevel::Debug, "Session ", id, " added (", m_sessions.size(), " connected)");

	session->start();
}

void SessionManager::remove(Session::SessionId id)
{
	if (m_sessions.erase(id) > 0) {
		writeLog(LogLevel::Debug, "Session ", id, " removed (", m_sessions.size(), " connected)");
	}
}

void SessionManager::closeAll()
{
	// close()가 remove()로 목록을 바꾸므로 먼저 꺼내 놓고 닫음
	auto sessions = std::move(m_sessions);
	m_sessions.clear();

	writeLog(LogLevel::Info, "Closing ", sessions.size(), " sessions");
	for (const auto& [id, session] : sessions) {
		session->close();
	}
}
}
