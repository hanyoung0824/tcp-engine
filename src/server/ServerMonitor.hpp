#pragma once

#include "NetworkStats.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

namespace tcpengine
{
    // 서버 상태를 모으고 interval마다 engine 로그 한 줄로 남김. 횟수는 지난 보고 이후의 값, 송수신은 초당 값
    // 송수신 통계는 getStats()를 받은 쪽들이 쌓음. 네트워크 스레드에서만 사용
    class ServerMonitor
    {
    public:
        struct SessionCounts
        {
            std::size_t total = 0;
            // 연결이 끊겨 재접속을 기다리는 세션
            std::size_t detached = 0;
        };
        using SessionCounter = std::function<SessionCounts()>;

        ServerMonitor(boost::asio::any_io_executor executor, std::chrono::seconds interval,
                      SessionCounter countSessions);

        ServerMonitor(const ServerMonitor &) = delete;
        ServerMonitor &operator=(const ServerMonitor &) = delete;

        const std::shared_ptr<NetworkStats> &getStats() const
        {
            return m_stats;
        }

        // interval이 0이면 아무것도 하지 않음
        void start();

        void stop();

    private:
        void schedule();

        void report();

    private:
        boost::asio::steady_timer m_timer;
        const std::chrono::seconds m_interval;
        const SessionCounter m_countSessions;
        const std::shared_ptr<NetworkStats> m_stats = std::make_shared<NetworkStats>();

        NetworkStats m_previous;
        std::chrono::steady_clock::time_point m_previousTime;
    };
} // namespace tcpengine
