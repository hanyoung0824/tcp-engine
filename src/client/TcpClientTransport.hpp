#pragma once

#include "ClientTransport.hpp"
#include "ConnectionEventHandler.hpp"
#include "ConnectionOptions.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace tcpengine
{
class TcpClientTransport final : public ClientTransport
{
public:
	explicit TcpClientTransport(ConnectionOptions connectionOptions);

	~TcpClientTransport() override;

	TcpClientTransport(const TcpClientTransport&) = delete;
	TcpClientTransport& operator=(const TcpClientTransport&) = delete;

	void start() override;

	void connect(const std::string& host, unsigned short port, std::chrono::seconds timeout,
		std::shared_ptr<const ConnectionEventHandler> handler, ConnectResult onResult) override;

	boost::asio::any_io_executor getExecutor() override { return m_ioContext.get_executor(); }

	void send(std::shared_ptr<const Connection::Buffer> data) override;

	void close() override;

	void stop() override;

private:
	boost::asio::io_context m_ioContext;
	// 연결이 없는 동안에도 네트워크 스레드가 끝나지 않게 붙잡음. stop()에서 놓음
	std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_workGuard;
	const ConnectionOptions m_connectionOptions;
	// 네트워크 스레드에서만 사용
	std::shared_ptr<TcpConnection> m_connection;
	Connection::ConnectionId m_nextConnectionId = 1;
	std::thread m_networkThread;
};
}
