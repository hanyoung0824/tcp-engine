#pragma once

#include "ConnectionEventHandler.hpp"
#include "ConnectionOptions.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace tcpengine
{
class ConnectionManager
{
public:
	// maxConnections를 넘는 새 소켓은 ServerFull을 알리고 바로 닫음
	ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler, ConnectionOptions options, std::size_t maxConnections);

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	void createConnection(boost::asio::ip::tcp::socket socket);

	// 모든 연결에 farewell을 보내고 close()함. 이후 들어오는 소켓은 바로 닫음
	// 이미 닫는 중인 연결은 farewell을 보내지 않음. 연결이 모두 닫히면 onAllClosed를 부름. 한 번만 처리함
	void closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed);

private:
	enum class State
	{
		Open,     // 연결을 받는 중
		Closing,  // closeAll()이 불림. 남은 연결이 닫히기를 기다림
		Closed,   // 모두 닫혔고 onAllClosed를 부름
	};

	void removeConnection(Connection::ConnectionId id);

	void notifyIfAllClosed();

	// 최대 접속 수를 넘은 소켓에 ServerFull을 알리고 닫음. 경고 로그는 1초에 한 번으로 묶음
	void refuse(boost::asio::ip::tcp::socket socket);

private:
	const std::shared_ptr<const ConnectionEventHandler> m_handler;
	const ConnectionOptions m_options;
	const std::size_t m_maxConnections;
	std::uint64_t m_refusedSinceLog = 0;
	std::chrono::steady_clock::time_point m_lastRefusalLog;
	State m_state = State::Open;
	std::function<void()> m_onAllClosed;
	std::unordered_map<Connection::ConnectionId, std::shared_ptr<TcpConnection>> m_connections;
	Connection::ConnectionId m_nextId = 1;
};
}
