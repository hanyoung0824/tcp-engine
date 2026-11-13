#include "ShutdownSignal.hpp"

#include "EngineLog.hpp"

#include <csignal>

namespace tcpengine
{
ShutdownSignal::ShutdownSignal(boost::asio::any_io_executor executor, std::function<void()> onShutdown,
	std::function<void()> onForce)
	: m_signals(std::move(executor), SIGINT, SIGTERM),
	m_onShutdown(std::move(onShutdown)),
	m_onForce(std::move(onForce))
{
	wait();
}

void ShutdownSignal::wait()
{
	m_signals.async_wait([this](const boost::system::error_code& ec, int signalNumber) {
		if (ec) {
			return;
		}
		if (!m_received) {
			m_received = true;
			engineLogInfo("Signal ", signalNumber, " received, shutting down (again to stop without waiting)");
			m_onShutdown();
		}
		else {
			engineLogWarning("Signal ", signalNumber, " received again, stopping without waiting for connections");
			m_onForce();
		}
		wait();
	});
}
}
