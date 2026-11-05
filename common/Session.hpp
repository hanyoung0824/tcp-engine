#pragma once

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// 반드시 std::make_shared로 만든다.
// isClosed()를 뺀 모든 함수는 네트워크 스레드(io_context.run() 스레드)에서만 호출한다.
class Session : public std::enable_shared_from_this<Session>
{
public:
	using SessionId = std::uint64_t;

	using Buffer = std::vector<std::uint8_t>;

	using CloseCallback = std::function<void(SessionId id)>;

	using SessionCallback = std::function<void(const std::shared_ptr<Session>& session)>;

	// 처리한 바이트 수를 반환한다. nullopt는 잘못된 데이터이며 세션이 닫힌다.
	using ReceiveHandler = std::function<std::optional<std::size_t>(
		const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data)>;

	struct EventHandler
	{
		SessionCallback onConnected;
		ReceiveHandler onReceive;
		SessionCallback onDisconnected;
	};

public:
	Session(boost::asio::ip::tcp::socket socket, EventHandler handler,
		SessionId id = 0, CloseCallback onClosed = {});
	~Session();

	void start();

	void send(std::shared_ptr<const Buffer> data);

	void close();

	bool isClosed() const { return m_closed.load(); }
	SessionId getId() const { return m_id; }
	const std::string& getAddress() const { return m_address; }

private:
	void startRead();

	void onRead(const boost::system::error_code& ec, std::size_t length);

	void startWrite();

	void onWrite(const boost::system::error_code& ec, std::size_t length);

private:
	boost::asio::ip::tcp::socket m_socket;
	EventHandler m_handler;
	const SessionId m_id;
	std::string m_address;
	std::atomic<bool> m_closed{ false };

	CloseCallback m_onClosed;

	Buffer m_receiveBuffer;
	std::size_t m_receivedLength = 0;
	std::deque<std::shared_ptr<const Buffer>> m_writeQueue;
};
