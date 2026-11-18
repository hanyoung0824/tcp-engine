#include "ConnectionManager.hpp"

#include "EngineLog.hpp"
#include "ServerTransportFactory.hpp"

#include <span>
#include <utility>

namespace tcpengine
{
ConnectionManager::ConnectionManager(boost::asio::any_io_executor executor, const ServerSettings& settings, std::shared_ptr<NetworkStats> stats,
	std::shared_ptr<const Connection::Buffer> serverHello, std::shared_ptr<const ConnectionEvents> events)
	: m_stats(std::move(stats)),
	m_heartbeatChecker(executor, m_stats),
	m_heartbeatSettings(settings.heartbeat),
	m_transport(makeServerTransport(executor, settings, wrapEvents(std::move(serverHello), std::move(events)), m_stats))
{
}

bool ConnectionManager::start()
{
	if (!m_transport) {
		engineLogError("Unsupported transport");
		return false;
	}
	return m_transport->start();
}

void ConnectionManager::stopAccepting()
{
	m_transport->stopAccepting();
}

void ConnectionManager::closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed)
{
	m_transport->closeAll(std::move(farewell), std::move(onAllClosed));
}

std::shared_ptr<const ConnectionEvents> ConnectionManager::wrapEvents(std::shared_ptr<const Connection::Buffer> serverHello,
	std::shared_ptr<const ConnectionEvents> events)
{
	return std::make_shared<const ConnectionEvents>(
		// 서버는 Ping을 보내지 않고 timeout만 검사함. 간격은 ServerHello로 알려 클라이언트가 Ping을 보냄
		[this, serverHello = std::move(serverHello), events](const std::shared_ptr<Connection>& connection) {
			connection->send(serverHello);
			m_heartbeatChecker.start(connection, m_heartbeatSettings, nullptr);
			++m_stats->connectionsOpened;
			events->onConnected(connection);
		},
		[this, events](const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data) {
			m_heartbeatChecker.onReceived(*connection);
			return events->onReceive(connection, data);
		},
		[this, events](const std::shared_ptr<Connection>& connection) {
			m_heartbeatChecker.stop(*connection);
			++m_stats->connectionsClosed;
			events->onDisconnected(connection);
		});
}
}
