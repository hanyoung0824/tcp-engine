#pragma once

#include "TcpSession.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <thread>

namespace tcpengine
{
class TcpClient
{
public:
	explicit TcpClient(TcpSession::EventHandler handler);

	~TcpClient();

	TcpClient(const TcpClient&) = delete;
	TcpClient& operator=(const TcpClient&) = delete;

	bool connect(const std::string& host, unsigned short port, HeartbeatSettings heartbeat,
		std::shared_ptr<const Session::Buffer> pingData);

	void send(std::shared_ptr<const Session::Buffer> data);

	void close();

	bool isConnected() const { return m_session && !m_session->isClosed(); }

private:
	boost::asio::io_context m_ioContext;
	TcpSession::EventHandler m_handler;
	std::shared_ptr<TcpSession> m_session;
	std::thread m_networkThread;
};
}
