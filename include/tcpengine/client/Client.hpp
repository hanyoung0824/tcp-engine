#pragma once

#include "tcpengine/client/ClientSettings.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/utility/Log.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>

namespace boost::asio
{
    class io_context;
}

namespace tcpengine
{
    class ClientState;
    enum class ClientStateId : std::uint8_t;
    class ClientTransport;
    struct ConnectionEvents;
    class HeartbeatChecker;
    class PacketRouter;

    struct DisconnectInfo
    {
        // 서버가 알리지 않고 끊었으면 없음
        std::optional<DisconnectReason> reason;
        bool willReconnect = false;
    };

    // run()을 부른 스레드(메인 스레드)가 루프를 돌며 연결 이벤트, 응용 훅, onUpdate()를 처리함
    // 소켓 송수신과 하트비트는 네트워크 스레드가 맡고, 결과는 메인 스레드로 넘김
    // registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
    // 패킷 핸들러에서 예외가 나면 연결을 끊음. 그 밖에서 잡히지 않은 예외는 로그를 남기고 abort()하여 덤프를 남김
    // 연결이 끊기면 reconnectMaxDelay에 따라 다시 접속하고, 세션이 있었으면 이어 감
    class Client
    {
    public:
        virtual ~Client();

        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;

        // 설정이 틀리거나 처음 접속에 실패하면 1 반환
        // quit()이 불리면 세션이 있을 때 Logout을 보내고 닫은 뒤 0 반환
        int run();

        // 어느 스레드에서든 호출 가능. 메인 루프가 지금 틱을 마치고 끝남
        void quit();

    protected:
        // protocolVersion은 응용 프로토콜 버전. 서버와 다르면 서버가 VersionMismatch로 끊음
        Client(const ClientSettings &settings, std::uint16_t protocolVersion);

        // handler(const T&)는 메인 스레드에서 호출
        // 패킷은 네트워크 스레드에서 읽고 검사하며, 잘못된 패킷이면 핸들러를 부르지 않고 연결을 끊음
        template <typename T, typename Handler> bool registerHandler(Handler handler)
        {
            static_assert(std::is_invocable_v<Handler &, const T &>, "Handler must be callable as handler(const T&)");

            auto sharedHandler = std::make_shared<Handler>(std::move(handler));
            return addPacketDispatcher(T::id,
                                       [this, sharedHandler](const std::shared_ptr<Connection> &, PacketReader &body) {
                                           auto packet = std::make_shared<T>();

                                           if (!detail::readPacket(body, *packet))
                                           {
                                               return false;
                                           }

                                           postToMain([this, sharedHandler, packet] {
                                               runPacketHandler(T::id, [&] { (*sharedHandler)(*packet); });
                                           });
                                           return true;
                                       });
        }

        // 네트워크를 열기 직전의 응용 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
        virtual bool onStart()
        {
            return true;
        }

        const ClientSettings &getSettings() const
        {
            return m_settings;
        }

        // 아래 훅은 모두 메인 스레드에서 호출
        // 첫 접속 뒤 tickInterval마다 불림
        virtual void onUpdate() {}

        // 다시 접속할 때마다 onConnected부터 다시 불림
        virtual void onConnected() {}

        virtual void onDisconnected(const DisconnectInfo &) {}

        // 재접속했고 이전 세션이 이어짐
        virtual void onSessionResumed() {}

        // 재접속했지만 세션이 이미 끝나 로그인부터 다시 해야 함
        virtual void onSessionLost() {}

        // 어느 스레드에서든 호출 가능. 연결이 없으면 버림
        void send(const PacketBase &packet);

        // 메인 스레드에서 호출
        bool isConnected() const;

    private:
        // 상태 객체는 ClientState 기반 클래스를 통해서만 Client 내부를 만짐
        friend class ClientState;

        // 아래 셋은 메인 스레드에서만 호출
        // 다음 상태를 기록만 함. 상태 객체의 함수가 끝난 뒤 applyNextState()가 바꿈
        void changeState(std::unique_ptr<ClientState> next);

        // 지금 상태에 이벤트를 넘기고, 다음 이벤트를 받기 전에 바뀐 상태를 적용함
        void dispatch(const std::function<void(ClientState &)> &event);

        // 기록된 다음 상태로 바꾸고 onEnter()를 부름. onEnter()에서 또 바뀌면 이어서 처리
        void applyNextState();

        // 첫 접속 실패나 시작 실패 뒤 정리
        int stopWithError();

        bool validateSettings() const;

        bool callOnStart();

        // 지원하지 않는 transport면 false
        bool createComponents();

        // 처음 접속은 실패해도 다시 시도하지 않음
        std::future<bool> postFirstConnect();

        void startNetworkThread();

        // 연결을 닫고 남은 작업이 끝날 때까지 기다림. 메인 스레드에서 호출
        void stopNetworkThread();

        // quit()이 불릴 때까지 이벤트를 처리하고 tickInterval마다 onUpdate()를 부름
        void runMainLoop();

        // 네트워크 스레드가 넘긴 일을 모두 실행
        void processMainEvents();

        // 어느 스레드에서든 호출 가능. 메인 루프가 다음에 깨어날 때 실행
        void postToMain(std::function<void()> event);

        // 메인 스레드에서 호출. 핸들러가 예외를 던지면 로그를 남기고 연결을 끊음
        void runPacketHandler(std::uint16_t packetId, const std::function<void()> &handle);

        // 결과는 네트워크 스레드에서 onResult로 알림
        void connect(std::function<void(bool connected)> onResult);

        // 메인 스레드에서 호출. 실패하면 메인 스레드에서 지금 상태의 onConnectFailed()를 부름
        void reconnect();

        // 아래는 네트워크 스레드에서 호출. 상태와 세션 데이터는 메인 스레드로 넘겨 바꿈
        void handleConnected(const std::shared_ptr<Connection> &connection);

        void handleDisconnected(const std::shared_ptr<Connection> &connection);

        bool handleServerHello(const std::shared_ptr<Connection> &connection, PacketReader &body);

        bool handleDisconnectNotice(PacketReader &body);

        bool handleSessionStarted(PacketReader &body);

        bool handleSessionResumed(PacketReader &body);

        bool handleResumeFailed(PacketReader &body);

        bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

    private:
        struct WorkGuard;

        // 멈춘 뒤 남은 핸들러가 다른 멤버를 잡고 있을 수 있어 가장 나중에 소멸
        std::unique_ptr<boost::asio::io_context> m_ioContext;
        std::unique_ptr<PacketRouter> m_packetRouter;
        std::unique_ptr<ClientTransport> m_transport;
        std::shared_ptr<const ConnectionEvents> m_connectionEvents;
        // 네트워크 스레드에서만 사용
        std::unique_ptr<HeartbeatChecker> m_heartbeatChecker;
        // 연결이 없는 동안에도 네트워크 스레드의 run()이 반환하지 않게 붙잡음. stopNetworkThread()에서 놓음
        std::unique_ptr<WorkGuard> m_workGuard;
        std::thread m_networkThread;
        const ClientSettings m_settings;
        const std::uint16_t m_protocolVersion;

        // 네트워크 스레드가 메인 스레드에 넘긴 일
        std::mutex m_mainEventMutex;
        std::condition_variable m_mainEventReady;
        std::deque<std::function<void()>> m_mainEvents;
        std::atomic<bool> m_quitRequested{false};

        // 아래는 메인 스레드에서만 사용
        std::unique_ptr<ClientState> m_state;
        std::unique_ptr<ClientState> m_nextState;
        // onConnected()를 부른 연결이 아직 있음. onDisconnected()와 짝을 맞춤
        bool m_connectionAnnounced = false;
        // 비어 있으면 세션 없음
        std::string m_sessionToken;
        // 서버가 Disconnect로 알려 준 이유. 연결이 끊길 때 응용에 넘기고 지움
        std::optional<DisconnectReason> m_disconnectReason;
    };
} // namespace tcpengine
