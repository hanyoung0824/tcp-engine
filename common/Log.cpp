#include "Log.hpp"

namespace
{
	// 등록된 로그 함수. 함수 안의 static으로 두어, 처음 쓸 때 만들어지도록 한다 (전역 변수 초기화 순서 문제를 피한다).
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
