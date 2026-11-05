#pragma once

#include "Session.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <thread>

class TcpClient
{
public:
	explicit TcpClient(Session::EventHandler handler);

	~TcpClient();

	TcpClient(const TcpClient&) = delete;
	TcpClient& operator=(const TcpClient&) = delete;

	bool connect(const std::string& host, unsigned short port);

	void send(std::shared_ptr<const Session::Buffer> data);

	void close();

	bool isConnected() const { return m_session && !m_session->isClosed(); }

private:
	boost::asio::io_context m_ioContext;
	Session::EventHandler m_handler;
	std::shared_ptr<Session> m_session;
	std::thread m_networkThread;
};
