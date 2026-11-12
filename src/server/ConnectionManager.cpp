#include "ConnectionManager.hpp"

#include "EngineLog.hpp"

#include <vector>

namespace tcpengine
{
ConnectionManager::ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler, ConnectionOptions options)
	: m_handler(std::move(handler)),
	m_options(options)
{
}

void ConnectionManager::createConnection(boost::asio::ip::tcp::socket socket)
{
	if (!m_handler->onReceive) {
		engineLogError("No receive handler registered, connection dropped");
		return;
	}

	// 종료 직전에 수락된 소켓. 소멸하면서 닫힘
	if (m_closingAll) {
		engineLogDebug("Connection refused while shutting down");
		return;
	}

	const Connection::ConnectionId id = m_nextId++;
	auto connection = std::make_shared<TcpConnection>(std::move(socket), m_handler, id, m_options,
		[this](Connection::ConnectionId closedId) { removeConnection(closedId); });

	m_connections.emplace(id, connection);
	engineLogDebug("Connection ", id, " added (", m_connections.size(), " connected)");

	connection->start();
}

void ConnectionManager::removeConnection(Connection::ConnectionId id)
{
	if (m_connections.erase(id) > 0) {
		engineLogDebug("Connection ", id, " removed (", m_connections.size(), " connected)");
	}
	notifyIfAllClosed();
}

void ConnectionManager::closeAll(std::function<void()> onAllClosed)
{
	m_closingAll = true;
	m_onAllClosed = std::move(onAllClosed);

	// 연결이 닫히면 removeConnection()이 목록을 바꾸므로 복사해 두고 닫음. 연결은 닫힐 때까지 목록에 남음
	std::vector<std::shared_ptr<TcpConnection>> connections;
	connections.reserve(m_connections.size());
	for (const auto& [id, connection] : m_connections) {
		connections.push_back(connection);
	}

	engineLogInfo("Closing ", connections.size(), " connections");
	for (const std::shared_ptr<TcpConnection>& connection : connections) {
		connection->close();
	}
	notifyIfAllClosed();
}

void ConnectionManager::notifyIfAllClosed()
{
	if (!m_closingAll || !m_connections.empty() || !m_onAllClosed) {
		return;
	}
	engineLogInfo("All connections closed");
	const std::function<void()> onAllClosed = std::move(m_onAllClosed);
	m_onAllClosed = nullptr;
	onAllClosed();
}
}
