#pragma once

#include <chrono>
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
