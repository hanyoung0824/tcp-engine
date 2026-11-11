#pragma once

#include "ClientTransport.hpp"
#include "ConnectionEventHandler.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <thread>

namespace tcpengine
{
class TcpClientTransport final : public ClientTransport
{
public:
	TcpClientTransport() = default;

	~TcpClientTransport() override;

	TcpClientTransport(const TcpClientTransport&) = delete;
	TcpClientTransport& operator=(const TcpClientTransport&) = delete;

	bool connect(const std::string& host, unsigned short port, std::shared_ptr<const ConnectionEventHandler> handler) override;

	boost::asio::any_io_executor getExecutor() override { return m_ioContext.get_executor(); }

	void send(std::shared_ptr<const Connection::Buffer> data) override;

	void close() override;

	bool isConnected() const override { return m_connection && !m_connection->isClosed(); }

private:
	boost::asio::io_context m_ioContext;
	std::shared_ptr<TcpConnection> m_connection;
	std::thread m_networkThread;
};
}
