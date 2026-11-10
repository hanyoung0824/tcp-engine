#pragma once

#include "SessionConfig.hpp"
#include "tcpengine/network/Session.hpp"

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
class TcpSession final : public Session, public std::enable_shared_from_this<TcpSession>
{
public:
	using CloseCallback = std::function<void(SessionId id)>;

public:
	TcpSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<const SessionConfig> config,
		SessionId id, CloseCallback onClosed = {});
	~TcpSession() override;

	void start();

	// interval마다 pingData를 보내고(null이면 안 보냄) timeout 동안 받은 게 없으면 닫음. 이미 돌고 있으면 새 값으로 다시 시작
	void startHeartbeat(const HeartbeatSettings& heartbeat, std::shared_ptr<const Buffer> pingData);

	void send(std::shared_ptr<const Buffer> data) override;

	void close() override;

	bool isClosed() const override { return m_closed.load(); }
	SessionId getId() const override { return m_id; }
	const std::string& getAddress() const override { return m_address; }

private:
	void startRead();

	void onRead(const boost::system::error_code& ec, std::size_t length);

	void startWrite();

	void onWrite(const boost::system::error_code& ec, std::size_t length);

	void scheduleHeartbeat();

	void onHeartbeat(const boost::system::error_code& ec);

private:
	boost::asio::ip::tcp::socket m_socket;
	const std::shared_ptr<const SessionConfig> m_config;
	HeartbeatSettings m_heartbeat;
	std::shared_ptr<const Buffer> m_pingData;
	boost::asio::steady_timer m_heartbeatTimer;
	std::chrono::steady_clock::time_point m_lastReceiveTime;
	const SessionId m_id;
	std::string m_address;
	std::atomic<bool> m_closed{ false };

	CloseCallback m_onClosed;

	Buffer m_receiveBuffer;
	std::size_t m_receivedLength = 0;
	std::deque<std::shared_ptr<const Buffer>> m_writeQueue;
};
}
