#pragma once

#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/server/ServerSettings.hpp"
#include "tcpengine/server/Session.hpp"
#include "tcpengine/utility/Log.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace boost::asio
{
class io_context;
}

namespace tcpengine
{
class ConnectionManager;
class PacketRouter;
class ServerMonitor;
class SessionManager;
class ShutdownSignalHandler;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
// 패킷 핸들러에서 예외가 나면 그 연결만 끊음. 그 밖에서 잡히지 않은 예외는 로그를 남기고 abort()하여 덤프를 남김
class Server
{
  public:
    virtual ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    // 한 번만 호출 가능. 설정이 틀리거나 시작하지 못하면 1 반환
    int run();

    // 어느 스레드에서든 호출 가능, 여러 번 불러도 한 번만 처리. SIGINT, SIGTERM도 이 함수를 거침
    // 세션을 모두 끝내고 연결마다 ServerShutdown을 알리고 닫은 뒤 run()이 반환됨
    // 신호가 한 번 더 오면 연결이 닫히기를 기다리지 않고 바로 반환됨
    void stop();

  protected:
    // protocolVersion은 응용 프로토콜 버전. 클라이언트가 알려 준 값과 다르면 VersionMismatch로 끊음
    Server(const ServerSettings &settings, std::uint16_t protocolVersion);

    // 세션이 있는 연결(로그인 후)에서만 받는 패킷. 세션 없이 오면 엔진이 버리고 로그를 남김
    // handler(const std::shared_ptr<Session>&, const T&)는 네트워크 스레드에서 호출
    template <typename T, typename Handler> bool registerHandler(Handler handler)
    {
        return addSessionDispatcher(T::id, detail::makePacketDispatcher<T, Session>(std::move(handler)));
    }

    // 로그인 전에도 받는 패킷 (로그인 요청 등)
    // handler(const std::shared_ptr<Connection>&, const T&)는 네트워크 스레드에서 호출
    template <typename T, typename Handler> bool registerConnectionHandler(Handler handler)
    {
        return addPacketDispatcher(T::id, detail::makePacketDispatcher<T, Connection>(std::move(handler)));
    }

    // 네트워크를 열기 직전의 응용 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
    virtual bool onStart()
    {
        return true;
    }

    const ServerSettings &getSettings() const
    {
        return m_settings;
    }

    virtual void onStop()
    {
    }

    virtual void onConnected(const std::shared_ptr<Connection> &)
    {
    }

    virtual void onDisconnected(const std::shared_ptr<Connection> &)
    {
    }

    // 로그인을 마친 연결에 세션을 시작함. 클라이언트에는 재접속 토큰이 전달됨
    // 네트워크 스레드에서 호출. 이미 세션이 있거나 닫힌 연결이면 nullptr
    std::shared_ptr<Session> startSession(const std::shared_ptr<Connection> &connection);

    // 연결에 붙어 있는 세션. 없으면 nullptr
    std::shared_ptr<Session> findSession(const std::shared_ptr<Connection> &connection) const;

    virtual void onSessionStarted(const std::shared_ptr<Session> &)
    {
    }

    // 재접속으로 새 연결이 붙음
    virtual void onSessionResumed(const std::shared_ptr<Session> &)
    {
    }

    // 연결이 끊겨 재접속을 기다리기 시작함
    virtual void onSessionDetached(const std::shared_ptr<Session> &)
    {
    }

    virtual void onSessionEnded(const std::shared_ptr<Session> &, SessionEndReason)
    {
    }

  private:
    enum class State
    {
        Idle,     // run() 전
        Starting, // 설정 검사, onStart(), 부품 생성
        Running,
        Stopping, // 세션을 끝내고 연결이 닫히기를 기다림
        Stopped,
    };

    static const char *toString(State state);

    static bool canChange(State from, State to);

    // run()을 부른 스레드(네트워크 스레드)에서만 호출
    void changeState(State next);

    bool validateSettings() const;

    bool callOnStart();

    void createComponents();

    void runUntilStopped();

    void destroyComponents();

    // 네트워크 스레드에서만 호출
    void gracefulStop();

    // 두 번째 종료 신호. 연결이 닫히기를 기다리지 않음
    void forceStop();

    void handleDisconnected(const std::shared_ptr<Connection> &connection);

    void handleSessionStarted(const std::shared_ptr<Session> &session);

    void handleSessionResumed(const std::shared_ptr<Session> &session);

    bool handleResume(const std::shared_ptr<Connection> &connection, PacketReader &body);

    bool handleLogout(const std::shared_ptr<Connection> &connection, PacketReader &body);

    bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

    bool addSessionDispatcher(std::uint16_t id, detail::TargetDispatcher<Session> dispatcher);

  private:
    // Server와 수명이 같아 stop()이 언제든 작업을 넘길 수 있음
    // 멈춘 뒤 남은 핸들러가 다른 멤버를 잡고 있을 수 있어 가장 나중에 소멸
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<PacketRouter> m_packetRouter;
    // 아래는 run() 동안만 있음. 연결 이벤트가 앞의 것들을 부르므로 이 순서로 만들고 반대로 없앰
    std::unique_ptr<ServerMonitor> m_monitor;
    std::unique_ptr<SessionManager> m_sessionManager;
    std::unique_ptr<ConnectionManager> m_connectionManager;
    std::unique_ptr<ShutdownSignalHandler> m_shutdownSignalHandler;
    const ServerSettings m_settings;
    const std::uint16_t m_protocolVersion;
    std::atomic<State> m_state{State::Idle};
    // stop()은 run() 전이나 다른 스레드에서도 불리므로 상태와 따로 둠
    std::atomic<bool> m_stopRequested{false};
};
} // namespace tcpengine
