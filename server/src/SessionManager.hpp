#pragma once

#include "TcpSession.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <unordered_map>

namespace tcpengine
{
class SessionManager
{
public:
	SessionManager(TcpSession::EventHandler handler, HeartbeatSettings heartbeat,
		std::shared_ptr<const Session::Buffer> pingData);

	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	void create(boost::asio::ip::tcp::socket socket);

	void closeAll();

	std::size_t size() const { return m_sessions.size(); }

private:
	void remove(Session::SessionId id);

private:
	const TcpSession::EventHandler m_handler;
	const HeartbeatSettings m_heartbeat;
	const std::shared_ptr<const Session::Buffer> m_pingData;
	std::unordered_map<Session::SessionId, std::shared_ptr<TcpSession>> m_sessions;
	Session::SessionId m_nextId = 1;
};
}
