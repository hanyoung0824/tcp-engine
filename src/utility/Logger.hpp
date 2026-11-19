#pragma once

#include "tcpengine/utility/Log.hpp"
#include "tcpengine/utility/LogSettings.hpp"

#include <memory>
#include <string>

namespace tcpengine
{
    enum class LogSource
    {
        Engine,
        App,
        Count, // 출처 개수. 출처가 아님
    };

    class LogSystem;

    // 엔진 안에서 로그 시스템을 시작하고 끝내는 곳. 프로세스에 하나뿐인 싱글톤
    // 엔진 코드가 로그를 남길 때는 EngineLog.hpp를 씀
    // spdlog 비동기 로거 위에서 동작하며 spdlog는 Logger.cpp 안에만 두어 엔진의 다른 층과 응용은 모름
    // 엔진 로그와 응용 로그는 로그 스레드 하나와 큐 하나를 함께 써서 쓴 순서대로 출력됨
    // 시작 전과 끝난 뒤의 로그는 버림
    class Logger
    {
    public:
        static Logger &instance();

        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        // 설정대로 콘솔과 파일 싱크를 만들고 출력을 시작함. 로그를 남기는 다른 스레드가 없을 때만 호출
        // 설정이 틀리거나 파일을 열 수 없거나 이미 시작했으면 로그로 남길 수 없으므로 stderr에 출력하고 false
        bool start(const LogSettings &settings);

        // 큐에 남은 로그를 모두 출력하고 로그 스레드를 끝냄. 로그를 남기는 다른 스레드가 모두 끝난 뒤 호출
        void stop();

        // 잠금 없이 레벨만 봄
        bool isEnabled(LogSource source, LogLevel level) const;

        void write(LogSource source, LogLevel level, std::string message);

        // 잡히지 않은 예외 등으로 std::terminate()가 불리면 원인을 stderr에 쓰고 abort()하여 덤프를 남김
        // 예외를 던진 지점에서 스택을 되감지 않고 불리므로 덤프에 던진 위치가 남음
        static void installTerminateHandler();

    private:
        Logger();
        ~Logger();

        // start()와 stop() 사이에만 있음. 그 밖에서 로그를 남기는 스레드가 없으므로 잠금 없음
        std::unique_ptr<LogSystem> m_system;
    };

    // 생성 시 Logger::start(), 시작에 성공했으면 소멸 시 Logger::stop()
    // Server::run()과 Client::run()이 어떤 경로로 끝나도 남은 로그를 출력하기 위함. run()이 끝난 뒤의 로그는 버림
    class ScopedLogging
    {
    public:
        explicit ScopedLogging(const LogSettings &settings);
        ~ScopedLogging();

        ScopedLogging(const ScopedLogging &) = delete;
        ScopedLogging &operator=(const ScopedLogging &) = delete;

        bool isStarted() const
        {
            return m_started;
        }

    private:
        bool m_started;
    };
} // namespace tcpengine
