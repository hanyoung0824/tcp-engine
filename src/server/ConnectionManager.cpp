#include "ConnectionManager.hpp"

#include "Logger.hpp"

namespace tcpengine
{
ConnectionManager::ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler)
	: m_handler(std::move(handler))
{
}

void ConnectionManager::createConnection(boost::asio::ip::tcp::socket socket)
{
	if (!m_handler->onReceive) {
		engineLogError("No receive handler registered, connection dropped");
		return;
	}

	const Connection::ConnectionId id = m_nextId++;
	auto connection = std::make_shared<TcpConnection>(std::move(socket), m_handler, id,
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
}

void ConnectionManager::closeAll()
{
	// close()가 removeConnection()으로 목록을 바꾸므로 먼저 꺼내 놓고 닫음
	auto connections = std::move(m_connections);
	m_connections.clear();

	engineLogInfo("Closing ", connections.size(), " connections");
	for (const auto& [id, connection] : connections) {
		connection->close();
	}
}
}
