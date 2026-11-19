#pragma once

#include "ClientState.hpp"
#include "tcpengine/client/Client.hpp"

#include <chrono>
#include <memory>

namespace tcpengine
{
    // 상태 객체가 Client에 시킬 수 있는 일. 메인 스레드에서만 사용
    class ClientContext
    {
        public:
            explicit ClientContext(Client &client) : m_client(client)
            {
            }

            // 바로 바뀌며 이전 상태 객체는 지금 이벤트 처리가 끝난 뒤에 사라짐. 새 상태의 onEnter()도 그때 불림
            void changeState(std::unique_ptr<ClientState> next);

            std::chrono::seconds getReconnectMaxDelay() const;

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

        private:
            Client &m_client;
    };
} // namespace tcpengine
