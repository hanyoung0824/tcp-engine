#pragma once

#include "ConnectionEventHandler.hpp"
#include "ConnectionOptions.hpp"
#include "tcpengine/network/Connection.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

	// abort()에 더해 커널에 남은 데이터까지 버리고 RST로 끊음
	void reset() override;

	bool isClosed() const override { return m_state != State::Open; }
	ConnectionId getId() const override { return m_id; }
	const std::string& getAddress() const override { return m_address; }

private:
	// 보낼 패킷 하나. 바이트를 소유하고, async_write가 버퍼 목록으로 읽을 때는 (주소, 길이)로 바뀜
	// 바이트는 복사하지 않으므로 브로드캐스트 버퍼를 여러 연결이 함께 씀
	struct OutgoingPacket
	{
		std::shared_ptr<const Buffer> data;

		operator boost::asio::const_buffer() const { return boost::asio::buffer(*data); }
	};

	enum class State
	{
		Open,     // 주고받는 중
		Closing,  // close()나 reset()이 불림. 더 보내지 않고 받은 것은 버림
		Closed,   // 소켓을 닫고 onDisconnected까지 마침
	};

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
	// 바꾸는 것은 네트워크 스레드뿐이지만 isClosed()는 어느 스레드에서나 읽으므로 atomic
	std::atomic<State> m_state{ State::Open };
	const ConnectionOptions m_options;
	boost::asio::steady_timer m_closeTimer;

	CloseCallback m_onClosed;

	Buffer m_receiveBuffer;
	std::size_t m_receivedLength = 0;
	// 보내려고 기다리는 패킷. 쓰는 중인 묶음이 있을 때만 쌓이고, 쓰기를 시작할 때 m_writingPackets와 맞바꿈
	std::vector<OutgoingPacket> m_pendingPackets;
	// 지금 한 번의 쓰기로 보내는 중인 묶음. async_write가 버퍼 목록으로 그대로 읽으므로 끝날 때까지 건드리지 않음
	std::vector<OutgoingPacket> m_writingPackets;
	// 대기 중인 것과 쓰는 중인 것을 합친, 아직 보내지 못한 바이트
	std::size_t m_queuedBytes = 0;
};
}
