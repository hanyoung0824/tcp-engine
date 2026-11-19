#include "ConnectionManager.hpp"

#include "ControlPackets.hpp"
#include "EngineLog.hpp"
#include "ServerTransportFactory.hpp"

#include <span>
#include <utility>

namespace tcpengine
{
    ConnectionManager::ConnectionManager(boost::asio::any_io_executor executor, const ServerSettings &settings,
                                         std::uint16_t appProtocolVersion, std::shared_ptr<NetworkStats> stats,
                                         std::shared_ptr<const Connection::Buffer> serverHello,
                                         std::shared_ptr<const ConnectionEvents> events)
        : m_stats(std::move(stats)),
          m_guard(executor,
                  ConnectionGuard::Settings{appProtocolVersion, settings.receivePacketsPerSecond, settings.loginTimeout,
                                            settings.maxConnections},
                  m_stats,
                  [this](const std::shared_ptr<Connection> &connection, DisconnectReason reason) {
                      disconnect(connection, reason);
                  }),
          m_heartbeatChecker(executor, m_stats), m_heartbeatSettings(settings.heartbeat),
          m_transport(
              makeServerTransport(executor, settings, wrapEvents(std::move(serverHello), std::move(events)), m_stats,
                                  [this](std::size_t connectionCount) { return m_guard.canAccept(connectionCount); }))
    {
    }

    bool ConnectionManager::start()
    {
        if (!m_transport)
        {
            engineLogError("Unsupported transport");
            return false;
        }

        return m_transport->start();
    }

    void ConnectionManager::stopAccepting()
    {
        m_transport->stopAccepting();
    }

    void ConnectionManager::closeAll(std::shared_ptr<const Connection::Buffer> farewell,
                                     std::function<void()> onAllClosed)
    {
        m_transport->closeAll(std::move(farewell), std::move(onAllClosed));
    }

    bool ConnectionManager::allowPacket(const std::shared_ptr<Connection> &connection, std::uint16_t packetId)
    {
        return m_guard.allowPacket(connection, packetId);
    }

    bool ConnectionManager::handleClientHello(const std::shared_ptr<Connection> &connection, PacketReader &body)
    {
        return m_guard.handleClientHello(connection, body);
    }

    bool ConnectionManager::isHandshakeDone(const Connection &connection) const
    {
        return m_guard.isHandshakeDone(connection);
    }

    void ConnectionManager::onLoggedIn(const Connection &connection)
    {
        m_guard.onLoggedIn(connection);
    }

    void ConnectionManager::disconnect(const std::shared_ptr<Connection> &connection, DisconnectReason reason)
    {
        connection->send(control::disconnect(reason));
        connection->close();
    }

    std::shared_ptr<const ConnectionEvents> ConnectionManager::wrapEvents(
        std::shared_ptr<const Connection::Buffer> serverHello, std::shared_ptr<const ConnectionEvents> events)
    {
        return std::make_shared<const ConnectionEvents>(
            // 서버는 Ping을 보내지 않고 timeout만 검사함. 간격은 ServerHello로 알려 클라이언트가 Ping을 보냄
            [this, serverHello = std::move(serverHello), events](const std::shared_ptr<Connection> &connection) {
                connection->send(serverHello);
                m_heartbeatChecker.start(connection, m_heartbeatSettings, nullptr);
                m_guard.onConnected(connection);
                ++m_stats->connectionsOpened;
                events->onConnected(connection);
            },
            [this, events](const std::shared_ptr<Connection> &connection, std::span<const std::uint8_t> data) {
                m_heartbeatChecker.onReceived(*connection);
                return events->onReceive(connection, data);
            },
            [this, events](const std::shared_ptr<Connection> &connection) {
                m_heartbeatChecker.stop(*connection);
                m_guard.onDisconnected(*connection);
                ++m_stats->connectionsClosed;
                events->onDisconnected(connection);
            });
    }
} // namespace tcpengine
