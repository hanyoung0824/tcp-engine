#pragma once

#include "ConnectionEventHandler.hpp"
#include "ConnectionOptions.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <functional>
#include <memory>
#include <unordered_map>

namespace tcpengine
{
class ConnectionManager
{
public:
	ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler, ConnectionOptions options);

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	void createConnection(boost::asio::ip::tcp::socket socket);

	// 모든 연결을 close()하고 이후 들어오는 소켓은 바로 닫음. 연결이 모두 닫히면 onAllClosed를 부름. 한 번만 처리함
	void closeAll(std::function<void()> onAllClosed);

private:
	enum class State
	{
		Open,     // 연결을 받는 중
		Closing,  // closeAll()이 불림. 남은 연결이 닫히기를 기다림
		Closed,   // 모두 닫혔고 onAllClosed를 부름
	};

	void removeConnection(Connection::ConnectionId id);

	void notifyIfAllClosed();

private:
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	const ConnectionOptions m_options;
	State m_state = State::Open;
	std::function<void()> m_onAllClosed;
	std::unordered_map<Connection::ConnectionId, std::shared_ptr<TcpConnection>> m_connections;
	Connection::ConnectionId m_nextId = 1;
};
}
