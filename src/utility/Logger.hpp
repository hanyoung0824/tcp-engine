#pragma once

#include "tcpengine/utility/Log.hpp"
#include "tcpengine/utility/LogSink.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

// spdlog 비동기 로거 위에서 동작함. spdlog는 Logger.cpp 안에만 두어 엔진의 다른 층과 응용은 모름
// 엔진 로그와 응용 로그는 로그 스레드 하나와 큐 하나를 함께 써서 쓴 순서대로 출력됨
namespace tcpengine
{
enum class LogSource
{
	Engine,
	App,
};

// 출력할 곳이 정해지기 전의 로그는 버림
void setLogSink(LogSource source, std::shared_ptr<LogSink> sink);

void setEngineLogSink(std::shared_ptr<LogSink> sink);

void setAppLogSink(std::shared_ptr<LogSink> sink);

// 잠금 없이 출력할 곳의 레벨만 봄
bool isLogEnabled(LogSource source, LogLevel level);

void writeLog(LogSource source, LogLevel level, std::string message);

// 지금까지 쓴 로그가 모두 출력될 때까지 기다림. timeout이 지나면 포기하고 false
bool flushLogs(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

// 걸러질 로그는 문자열을 만들기 전에 버림
template <typename... Args>
void engineLog(LogLevel level, const Args&... args)
{
	if (isLogEnabled(LogSource::Engine, level)) {
		writeLog(LogSource::Engine, level, detail::formatLog(args...));
	}
}

template <typename... Args>
void engineLogDebug(const Args&... args)
{
	engineLog(LogLevel::Debug, args...);
}

template <typename... Args>
void engineLogInfo(const Args&... args)
{
	engineLog(LogLevel::Info, args...);
}

template <typename... Args>
void engineLogWarning(const Args&... args)
{
	engineLog(LogLevel::Warning, args...);
}

template <typename... Args>
void engineLogError(const Args&... args)
{
	engineLog(LogLevel::Error, args...);
}

// 잡히지 않은 예외 등으로 std::terminate()가 불리면 로그를 남기고 flush한 뒤 abort()하여 덤프를 남김
// 예외를 던진 지점에서 스택을 되감지 않고 불리므로 덤프에 던진 위치가 남음
void installTerminateHandler();

// 소멸 시 로그를 flush. Server::run()과 Client::run()이 어떤 경로로 끝나도 남은 로그를 출력하기 위함
class ScopedLogFlush
{
public:
	ScopedLogFlush() = default;
	~ScopedLogFlush();

	ScopedLogFlush(const ScopedLogFlush&) = delete;
	ScopedLogFlush& operator=(const ScopedLogFlush&) = delete;
};
}
