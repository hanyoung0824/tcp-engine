#pragma once

#include "tcpengine/network/Connection.hpp"
#include "tcpengine/server/Session.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace tcpengine
{
// 세션 생성, 연결 분리와 재부착, 만료를 관리함. 네트워크 스레드에서만 사용
class SessionManager
{
public:
	using SessionCallback = std::function<void(const std::shared_ptr<Session>& session)>;

	using SessionEndCallback = std::function<void(const std::shared_ptr<Session>& session, SessionEndReason reason)>;

	struct Callbacks
	{
		SessionCallback onStarted;
		SessionCallback onResumed;
		SessionCallback onDetached;
		SessionEndCallback onEnded;
	};

	SessionManager(boost::asio::any_io_executor executor, std::chrono::seconds resumeTimeout, Callbacks callbacks);
	~SessionManager();

	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	// 연결에 이미 세션이 있거나 stop() 뒤면 nullptr
	std::shared_ptr<Session> start(const std::shared_ptr<Connection>& connection);

	std::shared_ptr<Session> find(const Connection& connection) const;

	std::size_t getSessionCount() const { return m_sessions.size(); }

	// 연결이 끊겨 재접속을 기다리는 세션 수
	std::size_t getDetachedCount() const { return m_sessions.size() - m_idsByConnection.size(); }

	// 클라이언트의 Resume 요청. 결과는 SessionResumed 또는 ResumeFailed로 알림
	void resume(const std::shared_ptr<Connection>& connection, const std::string& token);

	// 연결이 닫힐 때 호출. 세션이 있으면 분리하고 만료 타이머를 시작
	void onConnectionClosed(const Connection& connection);

	// 클라이언트가 Logout을 보냄. 세션이 있으면 재접속 대기 없이 바로 끝냄
	void logout(const Connection& connection);

	// 서버 종료 시 남은 세션을 모두 끝내고 이후 start(), resume()을 거부함
	void stop();

private:
	class ServerSession;

	void endAll();

	void attach(ServerSession& session, const std::shared_ptr<Connection>& connection);

	// 연결을 닫는 방법. Graceful은 이유를 알린 뒤 정상 종료, Reset은 알리지 않고 RST
	enum class ConnectionClose
	{
		Graceful,
		Reset,
	};

	void end(ServerSession& session, SessionEndReason reason, ConnectionClose connectionClose);

private:
	boost::asio::any_io_executor m_executor;
	const std::chrono::seconds m_resumeTimeout;
	Callbacks m_callbacks;

	std::unordered_map<Session::SessionId, std::shared_ptr<ServerSession>> m_sessions;
	std::unordered_map<std::string, Session::SessionId> m_idsByToken;
	std::unordered_map<Connection::ConnectionId, Session::SessionId> m_idsByConnection;
	Session::SessionId m_nextId = 1;
	bool m_stopped = false;
};
}
