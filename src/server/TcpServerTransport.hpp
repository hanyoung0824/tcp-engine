#pragma once

#include "Listener.hpp"
#include "ServerTransport.hpp"
#include "ConnectionEventHandler.hpp"
#include "ConnectionManager.hpp"
#include "ConnectionOptions.hpp"

#include <boost/asio.hpp>

#include <functional>
#include <memory>

namespace tcpengine
{
class TcpServerTransport final : public ServerTransport
{
public:
	TcpServerTransport(boost::asio::any_io_executor executor, unsigned short port, ConnectionOptions connectionOptions, std::size_t maxConnections,
		std::shared_ptr<const ConnectionEventHandler> handler);

	TcpServerTransport(const TcpServerTransport&) = delete;
	TcpServerTransport& operator=(const TcpServerTransport&) = delete;

	bool start() override;

	void stopAccepting() override;

	void closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed) override;

private:
	ConnectionManager m_connectionManager;
	Listener m_listener;
};
}
