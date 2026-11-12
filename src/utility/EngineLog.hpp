#pragma once

#include "tcpengine/utility/Log.hpp"

#include <string>

// 엔진 코드가 로그를 남길 때 씀. 응용의 Log.hpp와 같은 모양이며 engine 로거로 감
// 로그 시스템을 시작하거나 flush하는 것은 Logger.hpp
namespace tcpengine
{
bool isEngineLogEnabled(LogLevel level);

void writeEngineLog(LogLevel level, std::string message);

// 걸러질 로그는 문자열을 만들기 전에 버림
template <typename... Args>
void engineLog(LogLevel level, const Args&... args)
{
	if (isEngineLogEnabled(level)) {
		writeEngineLog(level, formatLog(args...));
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
}
