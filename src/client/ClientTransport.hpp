#pragma once

#include "ConnectionEvents.hpp"
#include "tcpengine/network/Connection.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace tcpengine
{
    // 서버 접속과 송수신을 맡는 전송 계층. 프로토콜마다 구현을 둠
    // 이벤트 루프와 네트워크 스레드는 Client가 소유하고 executor로 넘겨줌
    class ClientTransport
    {
    public:
        using ConnectResult = std::function<void(bool connected)>;

        virtual ~ClientTransport() = default;

        // host는 IP 주소나 도메인 이름. timeout 안에 접속하지 못하면 실패
        // 결과는 네트워크 스레드에서 onResult로 알림. 성공하면 events의 onConnected가 먼저 불림
        // 어느 스레드에서든 호출 가능. 이전 연결이 끝난 뒤에만 부름
        virtual void connect(const std::string &host, unsigned short port, std::chrono::seconds timeout,
                             std::shared_ptr<const ConnectionEvents> events, ConnectResult onResult) = 0;

        // 어느 스레드에서든 호출 가능. 연결이 없으면 버림
        virtual void send(std::shared_ptr<const Connection::Buffer> data) = 0;

        // 지금 연결을 정상 종료함. 어느 스레드에서든 호출 가능
        virtual void close() = 0;
    };
} // namespace tcpengine
