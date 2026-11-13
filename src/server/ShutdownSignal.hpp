#pragma once

#include <boost/asio.hpp>

#include <functional>

namespace tcpengine
{
// SIGINT, SIGTERM을 받을 때마다 알림. 첫 번째는 onShutdown(정상 종료), 그다음은 onForce(기다리지 않고 종료)
// 네트워크 스레드에서 사용
class ShutdownSignal
{
public:
	ShutdownSignal(boost::asio::any_io_executor executor, std::function<void()> onShutdown, std::function<void()> onForce);

	ShutdownSignal(const ShutdownSignal&) = delete;
	ShutdownSignal& operator=(const ShutdownSignal&) = delete;

private:
	void wait();

private:
	boost::asio::signal_set m_signals;
	const std::function<void()> m_onShutdown;
	const std::function<void()> m_onForce;
	bool m_received = false;
};
}
