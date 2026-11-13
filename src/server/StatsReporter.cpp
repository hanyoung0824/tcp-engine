#include "StatsReporter.hpp"

#include "EngineLog.hpp"

#include <cstdint>

namespace tcpengine
{
StatsReporter::StatsReporter(boost::asio::any_io_executor executor, std::chrono::seconds interval,
	std::shared_ptr<NetworkStats> stats, const SessionManager& sessionManager)
	: m_timer(std::move(executor)),
	m_interval(interval),
	m_stats(std::move(stats)),
	m_sessionManager(sessionManager)
{
}

void StatsReporter::start()
{
	if (m_interval.count() == 0) {
		return;
	}
	m_previous = *m_stats;
	m_previousTime = std::chrono::steady_clock::now();
	schedule();
}

void StatsReporter::stop()
{
	m_timer.cancel();
}

void StatsReporter::schedule()
{
	m_timer.expires_after(m_interval);
	m_timer.async_wait([this](const boost::system::error_code& ec) {
		if (ec) {
			return;
		}
		report();
		schedule();
	});
}

void StatsReporter::report()
{
	const NetworkStats& current = *m_stats;
	const auto now = std::chrono::steady_clock::now();
	const double seconds = std::chrono::duration<double>(now - m_previousTime).count();
	const auto perSecond = [seconds](std::uint64_t after, std::uint64_t before) {
		return static_cast<std::uint64_t>(static_cast<double>(after - before) / seconds);
	};

	engineLogInfo("[stats] conn ", current.connectionsOpened - current.connectionsClosed,
		" (+", current.connectionsOpened - m_previous.connectionsOpened,
		" -", current.connectionsClosed - m_previous.connectionsClosed, ")",
		", sessions ", m_sessionManager.getSessionCount(), " (detached ", m_sessionManager.getDetachedCount(), ")",
		" | in ", perSecond(current.packetsIn, m_previous.packetsIn), " pkt/s ",
		perSecond(current.bytesIn, m_previous.bytesIn) / 1024, " KB/s",
		" | out ", perSecond(current.packetsOut, m_previous.packetsOut), " pkt/s ",
		perSecond(current.bytesOut, m_previous.bytesOut) / 1024, " KB/s",
		" | queue peak ", current.peakSendQueueBytes / 1024, " KB",
		" | reset: queue ", current.sendQueueResets - m_previous.sendQueueResets,
		", heartbeat ", current.heartbeatResets - m_previous.heartbeatResets,
		", rate ", current.receiveLimitResets - m_previous.receiveLimitResets);

	m_stats->peakSendQueueBytes = 0;
	m_previous = *m_stats;
	m_previousTime = now;
}
}
