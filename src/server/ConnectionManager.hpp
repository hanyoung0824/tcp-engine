#pragma once

#include "ConnectionEventHandler.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>

namespace tcpengine
{
class ConnectionManager
{
public:
	ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler, std::chrono::seconds closeTimeout);

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	void createConnection(boost::asio::ip::tcp::socket socket);

	// 모든 연결을 close()하고 이후 들어오는 소켓은 바로 닫음. 연결이 모두 닫히면 onAllClosed를 한 번 부름
	void closeAll(std::function<void()> onAllClosed);

private:
	void removeConnection(Connection::ConnectionId id);

	void notifyIfAllClosed();

private:
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	const std::chrono::seconds m_closeTimeout;
	bool m_closingAll = false;
	std::function<void()> m_onAllClosed;
	std::unordered_map<Connection::ConnectionId, std::shared_ptr<TcpConnection>> m_connections;
	Connection::ConnectionId m_nextId = 1;
};
}
