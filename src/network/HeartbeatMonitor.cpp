#include "HeartbeatMonitor.hpp"

#include "EngineLog.hpp"

#include <utility>

namespace tcpengine
{
HeartbeatMonitor::Entry::Entry(boost::asio::any_io_executor executor, const std::shared_ptr<Connection>& connection,
	const HeartbeatSettings& settings, std::shared_ptr<const Connection::Buffer> pingData)
	: connection(connection),
	settings(settings),
	pingData(std::move(pingData)),
	timer(std::move(executor)),
	lastReceiveTime(std::chrono::steady_clock::now())
{
}

HeartbeatMonitor::HeartbeatMonitor(boost::asio::any_io_executor executor)
	: m_executor(std::move(executor))
{
}

void HeartbeatMonitor::start(const std::shared_ptr<Connection>& connection, const HeartbeatSettings& settings,
	std::shared_ptr<const Connection::Buffer> pingData)
{
	const Connection::ConnectionId id = connection->getId();
	m_entries.erase(id);
	if (connection->isClosed() || settings.interval.count() == 0) {
		return;
	}

	auto entry = std::make_unique<Entry>(m_executor, connection, settings, std::move(pingData));
	schedule(id, *entry);
	m_entries.emplace(id, std::move(entry));
}

void HeartbeatMonitor::onReceived(const Connection& connection)
{
	const auto it = m_entries.find(connection.getId());
	if (it != m_entries.end()) {
		it->second->lastReceiveTime = std::chrono::steady_clock::now();
	}
}

void HeartbeatMonitor::stop(const Connection& connection)
{
	// 타이머가 함께 소멸하면서 대기 중인 콜백은 취소됨
	m_entries.erase(connection.getId());
}

void HeartbeatMonitor::schedule(Connection::ConnectionId id, Entry& entry)
{
	entry.timer.expires_after(entry.settings.interval);
	entry.timer.async_wait([this, id](const boost::system::error_code& ec) {
		if (!ec) {
			onTimer(id);
		}
	});
}

void HeartbeatMonitor::onTimer(Connection::ConnectionId id)
{
	const auto it = m_entries.find(id);
	if (it == m_entries.end()) {
		return;
	}

	Entry& entry = *it->second;
	const std::shared_ptr<Connection> connection = entry.connection.lock();
	if (!connection || connection->isClosed()) {
		m_entries.erase(it);
		return;
	}

	const auto idleTime = std::chrono::steady_clock::now() - entry.lastReceiveTime;
	if (entry.settings.timeout.count() > 0 && idleTime >= entry.settings.timeout) {
		engineLogWarning("Heartbeat timeout from ", connection->getAddress(), ", resetting");
		m_entries.erase(it);
		// 응답 없는 상대라 정상 종료를 기다려도 소용없음
		connection->reset();
		return;
	}

	if (entry.pingData) {
		connection->send(entry.pingData);
	}
	schedule(id, entry);
}
}
