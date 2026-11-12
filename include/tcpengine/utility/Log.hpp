#pragma once

#include <sstream>
#include <string>

// 응용 로그를 남길 때 씀. 모두 어느 스레드에서 불러도 되며, 저장만 하고 실제 출력은 로그 스레드가 함
namespace tcpengine
{
enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error,
};

// 인자를 차례로 << 하여 한 문자열로 만듦
template <typename... Args>
std::string formatLog(const Args&... args)
{
	std::ostringstream message;
	(message << ... << args);
	return message.str();
}

// 설정한 출력 레벨로 보아 이 레벨의 로그가 출력되는지
bool isLogEnabled(LogLevel level);

// 이미 만든 문자열을 그대로 남김
void writeLog(LogLevel level, std::string message);

// 걸러질 로그는 문자열을 만들기 전에 버림
template <typename... Args>
void log(LogLevel level, const Args&... args)
{
	if (isLogEnabled(level)) {
		writeLog(level, formatLog(args...));
	}
}

template <typename... Args>
void logDebug(const Args&... args)
{
	log(LogLevel::Debug, args...);
}

template <typename... Args>
void logInfo(const Args&... args)
{
	log(LogLevel::Info, args...);
}

template <typename... Args>
void logWarning(const Args&... args)
{
	log(LogLevel::Warning, args...);
}

template <typename... Args>
void logError(const Args&... args)
{
	log(LogLevel::Error, args...);
}
}
