#pragma once

#include "ConnectionEventHandler.hpp"
#include "ConnectionOptions.hpp"
#include "tcpengine/network/Connection.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
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
		ConnectionId id, ConnectionOptions options, CloseCallback onClosed = {});
	~TcpConnection() override;

	void start();

	using Connection::send;

	// 보내지 못한 데이터가 sendQueueLimit을 넘으면 보내지 않고 연결을 끊음
	void send(std::shared_ptr<const Buffer> data) override;

	void close() override;

	// 남은 데이터를 버리고 바로 닫음. 오류나 잘못된 데이터처럼 기다릴 이유가 없을 때 씀
	void abort();

	// abort()에 더해 커널에 남은 데이터까지 버리고 RST로 끊음. 받지 않는 상대를 끊을 때 씀
	void reset();

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

	// 소켓을 닫고 onDisconnected와 onClosed를 부름. 한 번만 처리함
	void finish(bool sendReset);

private:
	boost::asio::ip::tcp::socket m_socket;
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	const ConnectionId m_id;
	std::string m_address;
	// close()가 불린 뒤로 true. 닫는 중에도 더 보내거나 받은 패킷을 처리하지 않음
	std::atomic<bool> m_closed{ false };
	// 소켓을 실제로 닫고 onDisconnected까지 마침
	bool m_finished = false;
	const ConnectionOptions m_options;
	boost::asio::steady_timer m_closeTimer;

	CloseCallback m_onClosed;

	Buffer m_receiveBuffer;
	std::size_t m_receivedLength = 0;
	std::deque<std::shared_ptr<const Buffer>> m_writeQueue;
	// m_writeQueue에서 아직 보내지 못한 바이트 합
	std::size_t m_queuedBytes = 0;
};
}
