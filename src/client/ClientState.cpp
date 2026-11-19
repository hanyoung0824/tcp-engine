#include "ClientState.hpp"

#include "ClientTransport.hpp"
#include "ControlPackets.hpp"
#include "EngineLog.hpp"
#include "PacketRouter.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

namespace tcpengine
{
    namespace
    {
        class IdleState final : public ClientState
        {
        public:
            using ClientState::ClientState;

            ClientStateId getId() const override
            {
                return ClientStateId::Idle;
            }

            const char *toString() const override
            {
                return "Idle";
            }

            bool canChange(ClientStateId next) const override
            {
                return next == ClientStateId::Starting;
            }

            bool onRunRequested() override
            {
                return true;
            }

            bool onRegisterHandler(std::uint16_t id, detail::PacketDispatcher dispatcher) override
            {
                return addPacketDispatcher(id, std::move(dispatcher));
            }

            void onQuit() override {}
        };

        // Starting은 Connecting, 시작에 실패하면 Stopped로만 감. Stopped는 마지막 상태
        class PassiveState final : public ClientState
        {
        public:
            PassiveState(Client &client, ClientStateId id) : ClientState(client), m_id(id) {}

            ClientStateId getId() const override
            {
                return m_id;
            }

            const char *toString() const override
            {
                return m_id == ClientStateId::Starting ? "Starting" : "Stopped";
            }

            bool canChange(ClientStateId next) const override
            {
                return m_id == ClientStateId::Starting &&
                       (next == ClientStateId::Connecting || next == ClientStateId::Stopped);
            }

            void onQuit() override {}

        private:
            const ClientStateId m_id;
        };

        class QuittingState final : public ClientState
        {
        public:
            using ClientState::ClientState;

            ClientStateId getId() const override
            {
                return ClientStateId::Quitting;
            }

            const char *toString() const override
            {
                return "Quitting";
            }

            bool canChange(ClientStateId next) const override
            {
                return next == ClientStateId::Stopped;
            }

            void onEnter() override
            {
                sendLogout();
                closeConnection();
            }

            // 끝내는 중에 진행하던 재접속이 성공함. 응용에 알리지 않고 바로 닫음
            void onConnected() override
            {
                closeConnection();
            }

            void onDisconnected() override
            {
                notifyDisconnected(takeDisconnectInfo(false));
            }

            void onQuit() override {}
        };

        class DisconnectedState final : public ClientState
        {
        public:
            DisconnectedState(Client &client, const DisconnectInfo &info) : ClientState(client), m_info(info) {}

            ClientStateId getId() const override
            {
                return ClientStateId::Disconnected;
            }

            const char *toString() const override
            {
                return "Disconnected";
            }

            bool canChange(ClientStateId next) const override
            {
                return next == ClientStateId::Quitting;
            }

            void onEnter() override
            {
                notifyDisconnected(m_info);
            }

        private:
            const DisconnectInfo m_info;
        };

        class ConnectedState final : public ClientState
        {
        public:
            using ClientState::ClientState;

            ClientStateId getId() const override
            {
                return ClientStateId::Connected;
            }

            const char *toString() const override
            {
                return "Connected";
            }

            bool canChange(ClientStateId next) const override
            {
                return next == ClientStateId::WaitingReconnect || next == ClientStateId::Disconnected ||
                       next == ClientStateId::Quitting;
            }

            bool isConnected() const override
            {
                return true;
            }

            void onEnter() override
            {
                greet();
            }

            void onDisconnected() override;
        };

        // attempt번째 재접속을 1, 2, 4초…로 늘려 가며 maxDelay까지 기다렸다가 시도함
        // 시각이 됐는지는 메인 루프의 틱마다 확인
        // 연결이 끊겨 들어왔으면 disconnectInfo로 응용에 먼저 알림. 재접속 실패로 들어오면 없음
        class WaitingReconnectState final : public ClientState
        {
        public:
            WaitingReconnectState(Client &client, int attempt,
                                  std::optional<DisconnectInfo> disconnectInfo = std::nullopt)
                : ClientState(client), m_attempt(attempt), m_disconnectInfo(std::move(disconnectInfo))
            {
            }

            ClientStateId getId() const override
            {
                return ClientStateId::WaitingReconnect;
            }

            const char *toString() const override
            {
                return "WaitingReconnect";
            }

            bool canChange(ClientStateId next) const override
            {
                return next == ClientStateId::Connecting || next == ClientStateId::Quitting;
            }

            void onEnter() override
            {
                if (m_disconnectInfo)
                {
                    notifyDisconnected(*m_disconnectInfo);
                }

                const std::chrono::seconds delay = delayFor(m_attempt, getReconnectMaxDelay());
                engineLogInfo("Reconnecting in ", delay.count(), "s (attempt ", m_attempt, ")");
                m_reconnectAt = std::chrono::steady_clock::now() + delay;
            }

            void onTick(std::chrono::steady_clock::time_point now) override;

        private:
            static std::chrono::seconds delayFor(int attempt, std::chrono::seconds maxDelay)
            {
                std::chrono::seconds delay(1);

                for (int i = 1; i < attempt && delay < maxDelay; ++i)
                {
                    delay *= 2;
                }

                return std::min(delay, maxDelay);
            }

        private:
            const int m_attempt;
            const std::optional<DisconnectInfo> m_disconnectInfo;
            std::chrono::steady_clock::time_point m_reconnectAt;
        };

        // attempt가 0이면 처음 접속. 처음 접속은 run()이 시작하고 실패도 run()이 처리함
        class ConnectingState final : public ClientState
        {
        public:
            ConnectingState(Client &client, int attempt) : ClientState(client), m_attempt(attempt) {}

            ClientStateId getId() const override
            {
                return ClientStateId::Connecting;
            }

            const char *toString() const override
            {
                return "Connecting";
            }

            // 처음 접속에 실패하면 Stopped
            bool canChange(ClientStateId next) const override
            {
                return next == ClientStateId::Connected || next == ClientStateId::WaitingReconnect ||
                       next == ClientStateId::Quitting || next == ClientStateId::Stopped;
            }

            void onEnter() override
            {
                if (m_attempt > 0)
                {
                    reconnect();
                }
            }

            void onConnected() override
            {
                changeState(std::make_unique<ConnectedState>(m_client));
            }

            void onConnectFailed() override
            {
                changeState(std::make_unique<WaitingReconnectState>(m_client, m_attempt + 1));
            }

        private:
            const int m_attempt;
        };

        void ConnectedState::onDisconnected()
        {
            const DisconnectInfo info = takeDisconnectInfo(true);

            if (info.willReconnect)
            {
                changeState(std::make_unique<WaitingReconnectState>(m_client, 1, info));
            }
            else
            {
                changeState(std::make_unique<DisconnectedState>(m_client, info));
            }
        }

        void WaitingReconnectState::onTick(std::chrono::steady_clock::time_point now)
        {
            if (now >= m_reconnectAt)
            {
                changeState(std::make_unique<ConnectingState>(m_client, m_attempt));
            }
        }
    } // namespace

    void ClientState::onQuit()
    {
        changeState(std::make_unique<QuittingState>(m_client));
    }

    bool ClientState::onRunRequested()
    {
        engineLogError("run() can only be called once");
        return false;
    }

    bool ClientState::onRegisterHandler(std::uint16_t, detail::PacketDispatcher)
    {
        engineLogError("registerHandler() must be called before run()");
        return false;
    }

    std::unique_ptr<ClientState> makeIdleState(Client &client)
    {
        return std::make_unique<IdleState>(client);
    }

    std::unique_ptr<ClientState> makePassiveState(Client &client, ClientStateId id)
    {
        return std::make_unique<PassiveState>(client, id);
    }

    std::unique_ptr<ClientState> makeFirstConnectingState(Client &client)
    {
        return std::make_unique<ConnectingState>(client, 0);
    }

    void ClientState::changeState(std::unique_ptr<ClientState> next)
    {
        m_client.changeState(std::move(next));
    }

    bool ClientState::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
    {
        return m_client.m_packetRouter->addDispatcher(id, std::move(dispatcher));
    }

    std::chrono::seconds ClientState::getReconnectMaxDelay() const
    {
        return m_client.m_settings.reconnectMaxDelay;
    }

    void ClientState::greet()
    {
        m_client.m_transport->send(
            control::clientHello(control::ClientHello{engineProtocolVersion, m_client.m_protocolVersion}));

        if (!m_client.m_sessionToken.empty())
        {
            m_client.m_transport->send(control::make(ControlType::Resume, m_client.m_sessionToken));
        }

        m_client.m_connectionAnnounced = true;
        m_client.onConnected();
    }

    void ClientState::closeConnection()
    {
        m_client.m_transport->close();
    }

    void ClientState::reconnect()
    {
        m_client.reconnect();
    }

    void ClientState::sendLogout()
    {
        if (!m_client.m_sessionToken.empty())
        {
            m_client.m_transport->send(control::make(ControlType::Logout));
        }
    }

    DisconnectInfo ClientState::takeDisconnectInfo(bool mayReconnect)
    {
        DisconnectInfo info;
        info.reason = std::exchange(m_client.m_disconnectReason, std::nullopt);
        const bool reconnectable = !info.reason || canReconnectAfter(*info.reason);

        if (!reconnectable)
        {
            m_client.m_sessionToken.clear();
        }

        info.willReconnect = reconnectable && mayReconnect && m_client.m_settings.reconnectMaxDelay.count() > 0;
        return info;
    }

    void ClientState::notifyDisconnected(const DisconnectInfo &info)
    {
        if (!std::exchange(m_client.m_connectionAnnounced, false))
        {
            return;
        }

        m_client.onDisconnected(info);
    }
} // namespace tcpengine
