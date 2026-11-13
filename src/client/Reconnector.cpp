#include "Reconnector.hpp"

#include "EngineLog.hpp"

#include <algorithm>
#include <utility>

namespace tcpengine
{
Reconnector::Reconnector(boost::asio::any_io_executor executor, std::chrono::seconds maxDelay, Attempt attempt)
	: m_timer(std::move(executor)),
	m_maxDelay(maxDelay),
	m_attempt(std::move(attempt))
{
}

void Reconnector::schedule()
{
	if (m_cancelled) {
		return;
	}

	std::chrono::seconds delay(1);
	for (int i = 0; i < m_attempts && delay < m_maxDelay; ++i) {
		delay *= 2;
	}
	delay = std::min(delay, m_maxDelay);
	++m_attempts;

	engineLogInfo("Reconnecting in ", delay.count(), "s (attempt ", m_attempts, ")");
	m_timer.expires_after(delay);
	m_timer.async_wait([this](const boost::system::error_code& ec) {
		if (ec || m_cancelled) {
			return;
		}
		m_attempt([this](bool connected) {
			if (!connected) {
				schedule();
			}
		});
	});
}

void Reconnector::cancel()
{
	m_cancelled = true;
	m_timer.cancel();
}
}
