#pragma once

#include "ConnectionEvents.hpp"
#include "ConnectionGuard.hpp"
#include "HeartbeatChecker.hpp"
#include "NetworkStats.hpp"
#include "ServerTransport.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"
#include "tcpengine/server/ServerSettings.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace tcpengine
{
    // TCP 연결 하나가 접속 → 핸드셰이크 → 로그인까지 가는 동안과 그 뒤 살아 있는 동안을 맡음
    // 세션은 모르고 로그인했다는 신호만 받음. 네트워크 스레드에서만 사용
    class ConnectionManager
    {
    public:
        ConnectionManager(boost::asio::any_io_executor executor, const ServerSettings &settings,
                          std::uint16_t appProtocolVersion, std::shared_ptr<NetworkStats> stats,
                          std::shared_ptr<const Connection::Buffer> serverHello,
                          std::shared_ptr<const ConnectionEvents> events);

        ConnectionManager(const ConnectionManager &) = delete;
        ConnectionManager &operator=(const ConnectionManager &) = delete;

        bool start();

        void stopAccepting();

        // 연결이 모두 닫히면 onAllClosed를 한 번 부름
        void closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed);

        // 수신 제한을 넘거나 핸드셰이크 전에 응용 패킷이 오면 RST로 끊고 false
        bool allowPacket(const std::shared_ptr<Connection> &connection, std::uint16_t packetId);

        bool handleClientHello(const std::shared_ptr<Connection> &connection, PacketReader &body);

        bool isHandshakeDone(const Connection &connection) const;

        void onLoggedIn(const Connection &connection);

    private:
        void disconnect(const std::shared_ptr<Connection> &connection, DisconnectReason reason);

        std::shared_ptr<const ConnectionEvents> wrapEvents(std::shared_ptr<const Connection::Buffer> serverHello,
                                                           std::shared_ptr<const ConnectionEvents> events);

    private:
        const std::shared_ptr<NetworkStats> m_stats;
        // 아래 둘은 연결이 닫힐 때 불리므로 transport보다 나중에 소멸
        ConnectionGuard m_guard;
        HeartbeatChecker m_heartbeatChecker;
        const HeartbeatSettings m_heartbeatSettings;
        std::unique_ptr<ServerTransport> m_transport;
    };
} // namespace tcpengine
