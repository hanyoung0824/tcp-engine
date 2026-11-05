#pragma once

#include "HeartbeatSettings.hpp"
#include "Session.hpp"

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

	using SessionCallback = std::function<void(const std::shared_ptr<Session>& session)>;

	// 처리한 바이트 수 반환. nullopt는 잘못된 데이터이며 세션이 닫힘
	using ReceiveHandler = std::function<std::optional<std::size_t>(
		const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data)>;

	struct EventHandler
	{
		SessionCallback onConnected;
		ReceiveHandler onReceive;
		SessionCallback onDisconnected;
	};

public:
	// pingData는 heartbeat.interval마다 보낼 바이트
	TcpSession(boost::asio::ip::tcp::socket socket, EventHandler handler, HeartbeatSettings heartbeat,
		std::shared_ptr<const Buffer> pingData, SessionId id, CloseCallback onClosed = {});
	~TcpSession() override;

	void start();

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

	void startHeartbeat();

	void onHeartbeat(const boost::system::error_code& ec);

private:
	boost::asio::ip::tcp::socket m_socket;
	EventHandler m_handler;
	const HeartbeatSettings m_heartbeat;
	const std::shared_ptr<const Buffer> m_pingData;
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
