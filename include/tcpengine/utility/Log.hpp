#pragma once

#include <chrono>
#include <cstddef>
#include <sstream>
#include <string>

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
	// OS가 매긴 스레드 번호
	std::size_t threadId;
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

	bool isLogEnabled(LogLevel level);

	void writeLog(LogLevel level, std::string message);

	// 걸러질 로그는 문자열을 만들기 전에 버림
	template <typename... Args>
	void log(LogLevel level, const Args&... args)
	{
		if (isLogEnabled(level)) {
			writeLog(level, formatLog(args...));
		}
	}
}

// 어느 스레드에서 불러도 됨. 저장만 하고 실제 출력은 로그 스레드가 함
template <typename... Args>
void logDebug(const Args&... args)
{
	detail::log(LogLevel::Debug, args...);
}

template <typename... Args>
void logInfo(const Args&... args)
{
	detail::log(LogLevel::Info, args...);
}

template <typename... Args>
void logWarning(const Args&... args)
{
	detail::log(LogLevel::Warning, args...);
}

template <typename... Args>
void logError(const Args&... args)
{
	detail::log(LogLevel::Error, args...);
}
}
