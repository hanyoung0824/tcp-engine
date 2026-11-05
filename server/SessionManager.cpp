#include "SessionManager.hpp"

#include "Log.hpp"

SessionManager& SessionManager::instance()
{
	// 처음 호출될 때 한 번만 만들어진다 (C++11부터 이 초기화는 스레드 안전하다).
	// main()이 끝난 뒤 소멸하므로, 그때는 목록이 비어 있어야 한다 (소켓이 속한 io_context가 먼저 사라진다).
	static SessionManager instance;
	return instance;
}

void SessionManager::setEventHandler(Session::EventHandler handler)
{
	m_handler = std::move(handler);
}

void SessionManager::create(boost::asio::ip::tcp::socket socket)
{
	if (!m_handler.onReceive) {
		// 등록을 빠뜨린 경우. socket은 여기서 소멸하며 연결이 닫힌다.
		writeLog(LogLevel::Error, "No receive handler registered, connection dropped");
		return;
	}

	// 1. 새 ID로 세션을 만든다. 닫히면 목록에서 빠지도록 remove를 연결한다.
	// 싱글톤은 프로그램이 끝날 때까지 살아 있으므로 this를 캡처해도 안전하다.
	const Session::SessionId id = m_nextId++;
	auto session = std::make_shared<Session>(std::move(socket), m_handler, id,
		[this](Session::SessionId closedId) { remove(closedId); });

	// 2. 목록에 넣는다. onConnected에서 broadcast하면 새 세션도 받는다.
	m_sessions.emplace(id, session);
	writeLog(LogLevel::Debug, "Session ", id, " added (", m_sessions.size(), " connected)");

	// 3. onConnected를 알리고 읽기를 시작한다.
	session->start();
}

void SessionManager::remove(Session::SessionId id)
{
	if (m_sessions.erase(id) > 0) {
		writeLog(LogLevel::Debug, "Session ", id, " removed (", m_sessions.size(), " connected)");
	}
}

void SessionManager::broadcast(const std::shared_ptr<const Session::Buffer>& data)
{
	// Session::send()는 큐에 넣고 바로 반환하며 그 자리에서 close()하지 않으므로,
	// 순회하는 동안 목록이 바뀌지 않는다.
	for (const auto& [id, session] : m_sessions) {
		session->send(data);
	}
}

void SessionManager::closeAll()
{
	// Session::close()가 remove()를 호출해 목록을 바꾸므로,
	// 순회하는 동안 목록이 바뀌지 않도록 먼저 꺼내 놓고 닫는다.
	auto sessions = std::move(m_sessions);
	m_sessions.clear();

	writeLog(LogLevel::Info, "Closing ", sessions.size(), " sessions");
	for (const auto& [id, session] : sessions) {
		session->close();
	}
}
