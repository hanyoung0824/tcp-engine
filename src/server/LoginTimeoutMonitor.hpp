#pragma once

#include "tcpengine/network/Connection.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>

namespace tcpengine
{
// 접속한 뒤 timeout 안에 로그인(세션 시작이나 재접속)하지 않은 연결을 알림
// 로그인하지 않은 연결이 하트비트만 보내며 자리를 차지하지 못하게 함. 네트워크 스레드에서만 사용
class LoginTimeoutMonitor
{
public:
	using TimeoutHandler = std::function<void(const std::shared_ptr<Connection>& connection)>;

	// timeout이 0이면 아무것도 하지 않음
	LoginTimeoutMonitor(boost::asio::any_io_executor executor, std::chrono::seconds timeout, TimeoutHandler onTimeout);

	LoginTimeoutMonitor(const LoginTimeoutMonitor&) = delete;
	LoginTimeoutMonitor& operator=(const LoginTimeoutMonitor&) = delete;

	void start(const std::shared_ptr<Connection>& connection);

	// 로그인했거나 연결이 끊김
	void stop(const Connection& connection);

private:
	struct Entry
	{
		Entry(boost::asio::any_io_executor executor, const std::shared_ptr<Connection>& connection);

		std::weak_ptr<Connection> connection;
		boost::asio::steady_timer timer;
	};

	void onTimer(Connection::ConnectionId id);

private:
	boost::asio::any_io_executor m_executor;
	const std::chrono::seconds m_timeout;
	const TimeoutHandler m_onTimeout;
	std::unordered_map<Connection::ConnectionId, std::unique_ptr<Entry>> m_entries;
};
}
