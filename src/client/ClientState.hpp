#pragma once

#include "tcpengine/client/Client.hpp"

#include <chrono>
#include <cstdint>
#include <memory>

namespace tcpengine
{
    // Client.hpp에는 이름만 선언되어 있어 밑바탕 타입을 맞춰야 함
    enum class ClientStateId : std::uint8_t
    {
        Idle,     // run() 전
        Starting, // 설정 검사, 부품 생성
        Connecting,
        Connected,
        WaitingReconnect,
        Disconnected, // 다시 접속하지 않을 이유로 끊김
        Quitting,     // quit()으로 메인 루프가 끝나 Logout을 보내고 닫는 중
        Stopped,      // 네트워크 스레드까지 끝남
    };

    // 상태마다 받는 이벤트를 처리함. 처리하지 않는 이벤트는 무시
    // 상태가 바뀔 때마다 새로 만들고, 그 상태에서만 쓰는 데이터(재접속 시각 등)를 직접 가짐
    // Client 내부는 이 기반 클래스의 protected 함수로만 만짐. 메인 스레드에서만 사용
    class ClientState
    {
    public:
        explicit ClientState(Client &client) : m_client(client) {}

        virtual ~ClientState() = default;

        ClientState(const ClientState &) = delete;
        ClientState &operator=(const ClientState &) = delete;

        virtual ClientStateId getId() const = 0;

        virtual const char *toString() const = 0;

        // 이 상태에서 next로 바뀌어도 되는지
        virtual bool canChange(ClientStateId next) const = 0;

        virtual bool isConnected() const
        {
            return false;
        }

        // run()은 한 번만 실행할 수 있어 Idle만 true
        virtual bool canRun() const
        {
            return false;
        }

        // 등록하면 true. 기본은 오류 로그를 남기고 false
        virtual bool tryRegisterHandler(std::uint16_t id, detail::PacketDispatcher dispatcher);

        // 이 상태로 바뀐 직후. 이벤트 처리가 모두 끝난 뒤에 불림
        virtual void onEnter() {}

        // 메인 루프의 틱마다 불림
        virtual void onTick(std::chrono::steady_clock::time_point) {}

        // TCP 연결이 붙음
        virtual void onConnected() {}

        // 재접속 시도가 실패함
        virtual void onConnectFailed() {}

        virtual void onDisconnected() {}

        // 메인 루프가 끝남. 기본은 Quitting으로 바꿈
        virtual void onQuit();

    protected:
        // 지금 이벤트 처리가 끝난 뒤, 다음 이벤트보다 먼저 바뀌고 새 상태의 onEnter()가 불림
        void changeState(std::unique_ptr<ClientState> next);

        std::chrono::seconds getReconnectMaxDelay() const;

        bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

        // ClientHello와 재접속 토큰을 보내고 응용에 알림
        void greet();

        void closeConnection();

        // 실패하면 지금 상태의 onConnectFailed()가 불림
        void reconnect();

        // 세션이 있으면 재접속을 기다리지 말고 바로 끝내 달라고 알림
        void sendLogout();

        // 서버가 알려 준 이유를 꺼내고, 다시 붙을 수 없는 이유면 세션 토큰을 지움
        DisconnectInfo takeDisconnectInfo(bool mayReconnect);

        void notifyDisconnected(const DisconnectInfo &info);

    protected:
        Client &m_client;
    };

    // run() 전. 핸들러 등록과 run()을 받음
    std::unique_ptr<ClientState> makeIdleState(Client &client);

    // run()의 단계라 네트워크 이벤트를 받지 않는 상태 (Starting, Stopped)
    std::unique_ptr<ClientState> makePassiveState(Client &client, ClientStateId id);

    // 처음 접속. 실패는 run()이 직접 처리하므로 다시 시도하지 않음
    std::unique_ptr<ClientState> makeFirstConnectingState(Client &client);
} // namespace tcpengine
