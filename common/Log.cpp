#include "Log.hpp"

namespace
{
	LogHandler& logHandler()
	{
		static LogHandler instance;
		return instance;
	}
}

void setLogHandler(LogHandler handler)
{
	logHandler() = std::move(handler);
}

bool hasLogHandler()
{
	return static_cast<bool>(logHandler());
}

void writeLogMessage(LogLevel level, std::string_view message)
{
	if (const LogHandler& handler = logHandler()) {
		handler(level, message);
	}
}
