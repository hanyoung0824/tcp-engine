#pragma once

#include "ConnectionEventHandler.hpp"
#include "tcpengine/network/Connection.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace tcpengine
{
// 반드시 std::make_shared로 생성
class TcpConnection final : public Connection, public std::enable_shared_from_this<TcpConnection>
{
public:
	using CloseCallback = std::function<void(ConnectionId id)>;

public:
	TcpConnection(boost::asio::ip::tcp::socket socket, std::shared_ptr<const ConnectionEventHandler> handler,
		ConnectionId id, CloseCallback onClosed = {});
	~TcpConnection() override;

	void start();

	using Connection::send;

	void send(std::shared_ptr<const Buffer> data) override;

	void close() override;

	bool isClosed() const override { return m_closed.load(); }
	ConnectionId getId() const override { return m_id; }
	const std::string& getAddress() const override { return m_address; }

private:
	void startRead();

	void onRead(const boost::system::error_code& ec, std::size_t length);

	void startWrite();

	void onWrite(const boost::system::error_code& ec, std::size_t length);

private:
	boost::asio::ip::tcp::socket m_socket;
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	const ConnectionId m_id;
	std::string m_address;
	std::atomic<bool> m_closed{ false };

	CloseCallback m_onClosed;

	Buffer m_receiveBuffer;
	std::size_t m_receivedLength = 0;
	std::deque<std::shared_ptr<const Buffer>> m_writeQueue;
};
}
