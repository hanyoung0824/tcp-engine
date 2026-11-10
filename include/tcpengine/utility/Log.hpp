#pragma once

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace tcpengine
{
enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error,
};

struct LogEntry
{
	std::chrono::system_clock::time_point time;
	LogLevel level;
	std::thread::id threadId;
	std::string message;
};

class LogInterface;

// 출력할 곳을 끼움. 끼우기 전의 로그는 저장만 해 두었다가 끼우면 그때 출력함
// 표시할 레벨은 LogInterface::setLevel()로 정함
void setLogger(std::shared_ptr<LogInterface> logger);

namespace detail
{
	template <typename... Args>
	std::string formatLog(const Args&... args)
	{
		std::ostringstream message;
		(message << ... << args);
		return message.str();
	}

	void writeLog(LogLevel level, std::string message);
}

// 어느 스레드에서 불러도 됨. 저장만 하고 실제 출력은 로그 스레드가 함
template <typename... Args>
void logDebug(const Args&... args)
{
	detail::writeLog(LogLevel::Debug, detail::formatLog(args...));
}

template <typename... Args>
void logInfo(const Args&... args)
{
	detail::writeLog(LogLevel::Info, detail::formatLog(args...));
}

template <typename... Args>
void logWarning(const Args&... args)
{
	detail::writeLog(LogLevel::Warning, detail::formatLog(args...));
}

template <typename... Args>
void logError(const Args&... args)
{
	detail::writeLog(LogLevel::Error, detail::formatLog(args...));
}
}
