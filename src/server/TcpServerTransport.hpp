#pragma once

#include "Listener.hpp"
#include "ServerTransport.hpp"
#include "SessionConfig.hpp"
#include "SessionManager.hpp"

#include <boost/asio.hpp>

#include <memory>

namespace tcpengine
{
class TcpServerTransport final : public ServerTransport
{
public:
	TcpServerTransport(unsigned short port, std::shared_ptr<const SessionConfig> config);

	TcpServerTransport(const TcpServerTransport&) = delete;
	TcpServerTransport& operator=(const TcpServerTransport&) = delete;

	bool start() override;

	void run() override;

	void stop() override;

private:
	boost::asio::io_context m_ioContext;
	// 세션의 소켓이 io_context보다 먼저 소멸해야 하므로 m_ioContext 뒤에 둠
	SessionManager m_sessionManager;
	Listener m_listener;
	boost::asio::signal_set m_signals;
};
}
