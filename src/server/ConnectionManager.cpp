#include "ConnectionManager.hpp"

#include "EngineLog.hpp"
#include "PacketManager.hpp"

#include <array>
#include <vector>

namespace tcpengine
{
namespace
{
	// 받지 않을 소켓에 이유를 알리고 닫음. 클라이언트가 보낸 것을 읽지 않은 채 닫으면 RST가 나가
	// 이유까지 버려지므로, 이유를 보내고 FIN을 보낸 뒤 상대가 닫을 때까지 받은 것을 버림. closeTimeout이 지나면 그냥 닫음
	class RefusedSocket : public std::enable_shared_from_this<RefusedSocket>
	{
	public:
		RefusedSocket(boost::asio::ip::tcp::socket socket, std::chrono::seconds closeTimeout)
			: m_socket(std::move(socket)),
			m_timer(m_socket.get_executor()),
			m_closeTimeout(closeTimeout)
		{
		}

		void start(std::shared_ptr<const Connection::Buffer> notice)
		{
			m_timer.expires_after(m_closeTimeout);
			m_timer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
				if (!ec) {
					self->close();
				}
			});

			boost::asio::async_write(m_socket, boost::asio::buffer(*notice),
				[self = shared_from_this(), notice](const boost::system::error_code& ec, std::size_t) {
					if (ec) {
						self->close();
						return;
					}
					boost::system::error_code ignored;
					self->m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
					self->drain();
				});
		}

	private:
		void drain()
		{
			m_socket.async_read_some(boost::asio::buffer(m_discard),
				[self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
					if (ec) {
						self->close();
						return;
					}
					self->drain();
				});
		}

		void close()
		{
			m_timer.cancel();
			boost::system::error_code ignored;
			m_socket.close(ignored);
		}

	private:
		boost::asio::ip::tcp::socket m_socket;
		boost::asio::steady_timer m_timer;
		const std::chrono::seconds m_closeTimeout;
		std::array<std::uint8_t, 256> m_discard{};
	};
}

ConnectionManager::ConnectionManager(std::shared_ptr<const ConnectionEventHandler> handler, ConnectionOptions options,
	std::size_t maxConnections)
	: m_handler(std::move(handler)),
	m_options(options),
	m_maxConnections(maxConnections)
{
}

void ConnectionManager::createConnection(boost::asio::ip::tcp::socket socket)
{
	// 종료 직전에 수락된 소켓. 소멸하면서 닫힘
	if (m_state != State::Open) {
		engineLogDebug("Connection refused while shutting down");
		return;
	}

	if (m_connections.size() >= m_maxConnections) {
		refuse(std::move(socket));
		return;
	}

	const Connection::ConnectionId id = m_nextId++;
	auto connection = std::make_shared<TcpConnection>(std::move(socket), m_handler, id, m_options,
		[this](Connection::ConnectionId closedId) { removeConnection(closedId); });

	m_connections.emplace(id, connection);
	engineLogDebug("Connection ", id, " added (", m_connections.size(), " connected)");

	connection->start();
}

void ConnectionManager::refuse(boost::asio::ip::tcp::socket socket)
{
	++m_options.stats->refusedConnections;
	++m_refusedSinceLog;
	const auto now = std::chrono::steady_clock::now();
	if (now - m_lastRefusalLog >= std::chrono::seconds(1)) {
		engineLogWarning("Server full (", m_maxConnections, " connections), refused ", m_refusedSinceLog, " connection(s)");
		m_refusedSinceLog = 0;
		m_lastRefusalLog = now;
	}

	std::make_shared<RefusedSocket>(std::move(socket), m_options.closeTimeout)
		->start(PacketManager::makeDisconnectPacket(DisconnectReason::ServerFull));
}

void ConnectionManager::removeConnection(Connection::ConnectionId id)
{
	if (m_connections.erase(id) > 0) {
		engineLogDebug("Connection ", id, " removed (", m_connections.size(), " connected)");
	}
	notifyIfAllClosed();
}

void ConnectionManager::closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed)
{
	if (m_state != State::Open) {
		return;
	}
	m_state = State::Closing;
	m_onAllClosed = std::move(onAllClosed);

	// 연결이 닫히면 removeConnection()이 목록을 바꾸므로 복사해 두고 닫음. 연결은 닫힐 때까지 목록에 남음
	std::vector<std::shared_ptr<TcpConnection>> connections;
	connections.reserve(m_connections.size());
	for (const auto& [id, connection] : m_connections) {
		connections.push_back(connection);
	}

	engineLogInfo("Closing ", connections.size(), " connections");
	for (const std::shared_ptr<TcpConnection>& connection : connections) {
		connection->send(farewell);
		connection->close();
	}
	notifyIfAllClosed();
}

void ConnectionManager::notifyIfAllClosed()
{
	if (m_state != State::Closing || !m_connections.empty()) {
		return;
	}
	engineLogInfo("All connections closed");
	m_state = State::Closed;
	const std::function<void()> onAllClosed = std::move(m_onAllClosed);
	if (onAllClosed) {
		onAllClosed();
	}
}
}
