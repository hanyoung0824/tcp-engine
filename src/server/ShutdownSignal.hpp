#pragma once

#include <boost/asio.hpp>

#include <functional>

namespace tcpengine
{
// SIGINT, SIGTERM을 받으면 onSignal을 한 번 부름. 네트워크 스레드에서 사용
class ShutdownSignal
{
public:
	ShutdownSignal(boost::asio::any_io_executor executor, std::function<void()> onSignal);

	ShutdownSignal(const ShutdownSignal&) = delete;
	ShutdownSignal& operator=(const ShutdownSignal&) = delete;

	// 기다리던 시그널을 취소함. 남아 있으면 io_context가 끝나지 않음
	void cancel();

private:
	boost::asio::signal_set m_signals;
	std::function<void()> m_onSignal;
};
}
