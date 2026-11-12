#include "ShutdownSignal.hpp"

#include "EngineLog.hpp"

#include <csignal>

namespace tcpengine
{
ShutdownSignal::ShutdownSignal(boost::asio::any_io_executor executor, std::function<void()> onSignal)
	: m_signals(std::move(executor), SIGINT, SIGTERM),
	m_onSignal(std::move(onSignal))
{
	m_signals.async_wait([this](const boost::system::error_code& ec, int signalNumber) {
		if (ec) {
			return;
		}
		engineLogInfo("Signal ", signalNumber, " received, shutting down");
		m_onSignal();
	});
}

void ShutdownSignal::cancel()
{
	boost::system::error_code ec;
	m_signals.cancel(ec);
}
}
