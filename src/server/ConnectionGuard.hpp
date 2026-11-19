#pragma once

#include "LoginDeadline.hpp"
#include "NetworkStats.hpp"
#include "ReceiveRateLimiter.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace tcpengine
{
    // 연결을 받아 줘도 되는지 판단함. 전송 방식과 무관함
    // 최대 접속 수, 접속 → ClientHello로 버전 확인 → 로그인 순서, 수신량을 봄. 네트워크 스레드에서만 사용
    class ConnectionGuard
    {
    public:
        struct Settings
        {
            std::uint16_t appProtocolVersion = 0;
            double receivePacketsPerSecond = 0;
            std::chrono::seconds loginTimeout{};
            std::size_t maxConnections = 0;
        };

        // 이유를 알리고 정상 종료함
        using Disconnect = std::function<void(const std::shared_ptr<Connection> &connection, DisconnectReason reason)>;

        ConnectionGuard(boost::asio::any_io_executor executor, const Settings &settings,
                        std::shared_ptr<NetworkStats> stats, Disconnect disconnect);

        ConnectionGuard(const ConnectionGuard &) = delete;
        ConnectionGuard &operator=(const ConnectionGuard &) = delete;

        // 새 소켓을 연결로 만들기 전에 부름
        bool canAccept(std::size_t connectionCount) const
        {
            return connectionCount < m_settings.maxConnections;
        }

        void onConnected(const std::shared_ptr<Connection> &connection);

        // 세션을 시작했거나 이어 감. 로그인 시간 제한을 풂
        void onLoggedIn(const Connection &connection);

        void onDisconnected(const Connection &connection);

        // 패킷 하나를 받아도 되는지. 수신 제한을 넘거나 ClientHello 전에 응용 패킷이 오면 RST로 끊고 false
        bool allowPacket(const std::shared_ptr<Connection> &connection, std::uint16_t packetId);

        // ClientHello 본문을 처리. 버전이 다르면 VersionMismatch로 끊음. 형식이 틀리면 false
        bool handleClientHello(const std::shared_ptr<Connection> &connection, PacketReader &body);

        bool isHandshakeDone(const Connection &connection) const;

    private:
        // 로그인 시간 제한은 Accepted와 Handshaken 동안 돌아감
        enum class State
        {
            Accepted,   // 접속함. ClientHello 전이라 제어 패킷만 받음
            Handshaken, // 버전 확인을 마침. 로그인 요청을 받음
            LoggedIn,   // 세션이 붙음
        };

        static const char *toString(State state);

        // 허용하지 않는 전환이면 로그를 남기고 false
        bool changeState(const Connection &connection, State expected, State next);

    private:
        const Settings m_settings;
        const std::shared_ptr<NetworkStats> m_stats;
        const Disconnect m_disconnect;
        ReceiveRateLimiter m_receiveRateLimiter;
        LoginDeadline m_loginDeadline;
        std::unordered_map<Connection::ConnectionId, State> m_states;
    };
} // namespace tcpengine
