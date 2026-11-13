#include "LoginTimeoutMonitor.hpp"

#include <utility>

namespace tcpengine
{
LoginTimeoutMonitor::Entry::Entry(boost::asio::any_io_executor executor, const std::shared_ptr<Connection>& connection)
	: connection(connection),
	timer(std::move(executor))
{
}

LoginTimeoutMonitor::LoginTimeoutMonitor(boost::asio::any_io_executor executor, std::chrono::seconds timeout,
	TimeoutHandler onTimeout)
	: m_executor(std::move(executor)),
	m_timeout(timeout),
	m_onTimeout(std::move(onTimeout))
{
}

void LoginTimeoutMonitor::start(const std::shared_ptr<Connection>& connection)
{
	if (m_timeout.count() == 0) {
		return;
	}

	const Connection::ConnectionId id = connection->getId();
	auto entry = std::make_unique<Entry>(m_executor, connection);
	entry->timer.expires_after(m_timeout);
	entry->timer.async_wait([this, id](const boost::system::error_code& ec) {
		if (!ec) {
			onTimer(id);
		}
	});
	m_entries.insert_or_assign(id, std::move(entry));
}

void LoginTimeoutMonitor::stop(const Connection& connection)
{
	// 타이머가 함께 소멸하면서 대기 중인 콜백은 취소됨
	m_entries.erase(connection.getId());
}

void LoginTimeoutMonitor::onTimer(Connection::ConnectionId id)
{
	const auto it = m_entries.find(id);
	if (it == m_entries.end()) {
		return;
	}
	const std::shared_ptr<Connection> connection = it->second->connection.lock();
	m_entries.erase(it);
	if (connection && !connection->isClosed()) {
		m_onTimeout(connection);
	}
}
}
