#pragma once

#include "Listener.hpp"
#include "Session.hpp"

#include <boost/asio.hpp>

#include <memory>

class TcpServer
{
public:
	TcpServer(unsigned short port, Session::EventHandler handler);

	TcpServer(const TcpServer&) = delete;
	TcpServer& operator=(const TcpServer&) = delete;

	bool start();

	void run();

	void stop();

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_listener.getEndPoint(); }

private:
	boost::asio::io_context m_ioContext;
	Session::EventHandler m_handler;
	Listener m_listener;
	boost::asio::signal_set m_signals;
};
