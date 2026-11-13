#pragma once

#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/HeartbeatSettings.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <memory>
#include <unordered_map>

namespace tcpengine
{
// 연결마다 하트비트를 돌림. Connection 인터페이스만 쓰므로 프로토콜과 무관함
// 네트워크 스레드에서만 사용
class HeartbeatMonitor
{
public:
	explicit HeartbeatMonitor(boost::asio::any_io_executor executor);

	HeartbeatMonitor(const HeartbeatMonitor&) = delete;
	HeartbeatMonitor& operator=(const HeartbeatMonitor&) = delete;

	// interval마다 pingData를 보내고(null이면 안 보냄) timeout 동안 받은 게 없으면 연결을 RST로 끊음
	// interval이 0이면 아무것도 하지 않음. 이미 돌고 있으면 새 값으로 다시 시작
	void start(const std::shared_ptr<Connection>& connection, const HeartbeatSettings& settings,
		std::shared_ptr<const Connection::Buffer> pingData);

	// 연결에서 무엇이든 받았을 때 호출
	void onReceived(const Connection& connection);

	void stop(const Connection& connection);

private:
	struct Entry
	{
		Entry(boost::asio::any_io_executor executor, const std::shared_ptr<Connection>& connection,
			const HeartbeatSettings& settings, std::shared_ptr<const Connection::Buffer> pingData);

		std::weak_ptr<Connection> connection;
		HeartbeatSettings settings;
		std::shared_ptr<const Connection::Buffer> pingData;
		boost::asio::steady_timer timer;
		std::chrono::steady_clock::time_point lastReceiveTime;
	};

	void schedule(Connection::ConnectionId id, Entry& entry);

	void onTimer(Connection::ConnectionId id);

private:
	boost::asio::any_io_executor m_executor;
	std::unordered_map<Connection::ConnectionId, std::unique_ptr<Entry>> m_entries;
};
}
