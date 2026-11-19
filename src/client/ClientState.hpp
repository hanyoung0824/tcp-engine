#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

namespace tcpengine
{
class ClientContext;

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
// 메인 스레드에서만 사용
class ClientState
{
  public:
    explicit ClientState(ClientContext &context) : m_context(context)
    {
    }

    virtual ~ClientState() = default;

    ClientState(const ClientState &) = delete;
    ClientState &operator=(const ClientState &) = delete;

    virtual ClientStateId getId() const = 0;

    virtual const char *toString() const = 0;

    // 이 상태에서 next로 바뀌어도 되는지
    virtual bool canChange(ClientStateId next) const = 0;

    // 이 상태로 바뀐 직후. 이벤트 처리가 모두 끝난 뒤에 불림
    virtual void onEnter()
    {
    }

    // 메인 루프의 틱마다 불림
    virtual void onTick(std::chrono::steady_clock::time_point)
    {
    }

    // TCP 연결이 붙음
    virtual void onConnected()
    {
    }

    // 재접속 시도가 실패함
    virtual void onConnectFailed()
    {
    }

    virtual void onDisconnected()
    {
    }

    // 메인 루프가 끝남. 기본은 Quitting으로 바꿈
    virtual void onQuit();

  protected:
    ClientContext &m_context;
};

// run()의 단계라 네트워크 이벤트를 받지 않는 상태 (Idle, Starting, Stopped)
std::unique_ptr<ClientState> makePassiveState(ClientContext &context, ClientStateId id);

// 처음 접속. 실패는 run()이 직접 처리하므로 다시 시도하지 않음
std::unique_ptr<ClientState> makeFirstConnectingState(ClientContext &context);
} // namespace tcpengine
