#pragma once

#include "NetworkStats.hpp"
#include "SessionManager.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <memory>

namespace tcpengine
{
// interval마다 서버 상태를 engine 로그 한 줄로 남김. 횟수는 지난 보고 이후의 값, 송수신은 초당 값
// 네트워크 스레드에서만 사용
class StatsReporter
{
public:
	StatsReporter(boost::asio::any_io_executor executor, std::chrono::seconds interval,
		std::shared_ptr<NetworkStats> stats, const SessionManager& sessionManager);

	StatsReporter(const StatsReporter&) = delete;
	StatsReporter& operator=(const StatsReporter&) = delete;

	// interval이 0이면 아무것도 하지 않음
	void start();

	void stop();

private:
	void schedule();

	void report();

private:
	boost::asio::steady_timer m_timer;
	const std::chrono::seconds m_interval;
	const std::shared_ptr<NetworkStats> m_stats;
	const SessionManager& m_sessionManager;

	NetworkStats m_previous;
	std::chrono::steady_clock::time_point m_previousTime;
};
}
