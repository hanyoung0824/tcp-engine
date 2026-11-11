#pragma once

#include "Listener.hpp"
#include "ServerTransport.hpp"
#include "ConnectionEventHandler.hpp"
#include "ConnectionManager.hpp"

#include <boost/asio.hpp>

#include <memory>

namespace tcpengine
{
class TcpServerTransport final : public ServerTransport
{
public:
	TcpServerTransport(unsigned short port, std::shared_ptr<const ConnectionEventHandler> handler);

	TcpServerTransport(const TcpServerTransport&) = delete;
	TcpServerTransport& operator=(const TcpServerTransport&) = delete;

	bool start() override;

	void run() override;

	void stop() override;

	boost::asio::any_io_executor getExecutor() override { return m_ioContext.get_executor(); }

private:
	boost::asio::io_context m_ioContext;
	// 연결의 소켓이 io_context보다 먼저 소멸해야 하므로 m_ioContext 뒤에 둠
	ConnectionManager m_connectionManager;
	Listener m_listener;
	boost::asio::signal_set m_signals;
};
}
