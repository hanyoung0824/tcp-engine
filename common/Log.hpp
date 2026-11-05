#pragma once

#include <functional>
#include <sstream>
#include <string_view>

enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error,
};

using LogHandler = std::function<void(LogLevel level, std::string_view message)>;

// 네트워크 스레드가 생기기 전에 호출해야 한다.
void setLogHandler(LogHandler handler);

bool hasLogHandler();

void writeLogMessage(LogLevel level, std::string_view message);

template <typename... Args>
void writeLog(LogLevel level, const Args&... args)
{
	if (!hasLogHandler()) {
		return;
	}

	std::ostringstream message;
	(message << ... << args);
	writeLogMessage(level, message.str());
}
