#include "ServerMonitor.hpp"

#include "EngineLog.hpp"

#include <cstdint>

namespace tcpengine
{
ServerMonitor::ServerMonitor(boost::asio::any_io_executor executor, std::chrono::seconds interval, SessionCounter countSessions)
	: m_timer(std::move(executor)),
	m_interval(interval),
	m_countSessions(std::move(countSessions))
{
}

void ServerMonitor::start()
{
	if (m_interval.count() == 0) {
		return;
	}
	m_previous = *m_stats;
	m_previousTime = std::chrono::steady_clock::now();
	schedule();
}

void ServerMonitor::stop()
{
	m_timer.cancel();
}

void ServerMonitor::schedule()
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

void ServerMonitor::report()
{
	const NetworkStats& current = *m_stats;
	const auto now = std::chrono::steady_clock::now();
	const double seconds = std::chrono::duration<double>(now - m_previousTime).count();
	const SessionCounts sessions = m_countSessions();
	const auto perSecond = [seconds](std::uint64_t after, std::uint64_t before) {
		return static_cast<std::uint64_t>(static_cast<double>(after - before) / seconds);
	};

	engineLogInfo("[stats] conn ", current.connectionsOpened - current.connectionsClosed,
		" (+", current.connectionsOpened - m_previous.connectionsOpened,
		" -", current.connectionsClosed - m_previous.connectionsClosed, ")",
		", sessions ", sessions.total, " (detached ", sessions.detached, ")",
		" | in ", perSecond(current.packetsIn, m_previous.packetsIn), " pkt/s ",
		perSecond(current.bytesIn, m_previous.bytesIn) / 1024, " KB/s",
		" | out ", perSecond(current.packetsOut, m_previous.packetsOut), " pkt/s ",
		perSecond(current.bytesOut, m_previous.bytesOut) / 1024, " KB/s",
		" | queue peak ", current.peakSendQueueBytes / 1024, " KB",
		" | dropped: queue ", current.sendQueueResets - m_previous.sendQueueResets,
		", heartbeat ", current.heartbeatResets - m_previous.heartbeatResets,
		", rate ", current.receiveLimitResets - m_previous.receiveLimitResets,
		", login ", current.loginTimeouts - m_previous.loginTimeouts,
		" | refused ", current.refusedConnections - m_previous.refusedConnections);

	m_stats->peakSendQueueBytes = 0;
	m_previous = *m_stats;
	m_previousTime = now;
}
}
