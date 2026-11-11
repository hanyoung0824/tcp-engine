#pragma once

#include "ConnectionEventHandler.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <unordered_map>

namespace tcpengine
{
class ConnectionManager
{
public:
	explicit ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler);

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	void createConnection(boost::asio::ip::tcp::socket socket);

	void closeAll();

	std::size_t size() const { return m_connections.size(); }

private:
	void removeConnection(Connection::ConnectionId id);

private:
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	std::unordered_map<Connection::ConnectionId, std::shared_ptr<TcpConnection>> m_connections;
	Connection::ConnectionId m_nextId = 1;
};
}
