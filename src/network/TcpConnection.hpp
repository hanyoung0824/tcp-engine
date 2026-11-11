#pragma once

#include "ConnectionEventHandler.hpp"
#include "tcpengine/network/Connection.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
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
	// closeTimeout은 close() 후 남은 데이터를 보내고 상대가 끊기를 기다리는 최대 시간
	TcpConnection(boost::asio::ip::tcp::socket socket, std::shared_ptr<const ConnectionEventHandler> handler,
		ConnectionId id, std::chrono::seconds closeTimeout, CloseCallback onClosed = {});
	~TcpConnection() override;

	void start();

	using Connection::send;

	void send(std::shared_ptr<const Buffer> data) override;

	void close() override;

	// 남은 데이터를 버리고 바로 닫음. 오류나 잘못된 데이터처럼 기다릴 이유가 없을 때 씀
	void abort();

	bool isClosed() const override { return m_closed.load(); }
	ConnectionId getId() const override { return m_id; }
	const std::string& getAddress() const override { return m_address; }

private:
	void startRead();

	void onRead(const boost::system::error_code& ec, std::size_t length);

	void startWrite();

	void onWrite(const boost::system::error_code& ec, std::size_t length);

	// 보낼 것이 끝났음을 알림. 상대가 끊으면 읽기가 eof로 끝나고 abort()로 마무리됨
	void shutdownSend();

private:
	boost::asio::ip::tcp::socket m_socket;
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	const ConnectionId m_id;
	std::string m_address;
	// close()가 불린 뒤로 true. 닫는 중에도 더 보내거나 받은 패킷을 처리하지 않음
	std::atomic<bool> m_closed{ false };
	// 소켓을 실제로 닫고 onDisconnected까지 마침
	bool m_finished = false;
	const std::chrono::seconds m_closeTimeout;
	boost::asio::steady_timer m_closeTimer;

	CloseCallback m_onClosed;

	Buffer m_receiveBuffer;
	std::size_t m_receivedLength = 0;
	std::deque<std::shared_ptr<const Buffer>> m_writeQueue;
};
}
