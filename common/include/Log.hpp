#pragma once

#include <sstream>
#include <string_view>

namespace tcpengine
{
enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error,
};

// 출력 레벨은 ServerSettings/ClientSettings의 logLevel로 정함
class Logger
{
public:
	virtual ~Logger() = default;

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	virtual bool isEnabled(LogLevel level) const = 0;

	virtual void write(LogLevel level, std::string_view message) = 0;

protected:
	Logger() = default;
};

Logger& getLogger();

template <typename... Args>
void writeLog(LogLevel level, const Args&... args)
{
	Logger& logger = getLogger();
	if (!logger.isEnabled(level)) {
		return;
	}

	std::ostringstream message;
	(message << ... << args);
	logger.write(level, message.str());
}
}
