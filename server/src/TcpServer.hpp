#pragma once

#include "Listener.hpp"
#include "SessionManager.hpp"
#include "TcpSession.hpp"

#include <boost/asio.hpp>

namespace tcpengine
{
class TcpServer
{
public:
	TcpServer(unsigned short port, TcpSession::EventHandler handler, HeartbeatSettings heartbeat,
		std::shared_ptr<const Session::Buffer> pingData);

	TcpServer(const TcpServer&) = delete;
	TcpServer& operator=(const TcpServer&) = delete;

	bool start();

	void run();

	void stop();

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_listener.getEndPoint(); }

private:
	boost::asio::io_context m_ioContext;
	// 세션의 소켓이 io_context보다 먼저 소멸해야 하므로 m_ioContext 뒤에 둠
	SessionManager m_sessionManager;
	Listener m_listener;
	boost::asio::signal_set m_signals;
};
}
