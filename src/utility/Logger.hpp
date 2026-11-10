#pragma once

#include "tcpengine/utility/Log.hpp"
#include "tcpengine/utility/LogInterface.hpp"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tcpengine
{
// write()는 로그를 벡터에 저장만 하고, 로그 스레드가 벡터를 통째로 꺼내 출력할 곳(LogInterface)에 넘김
// 출력할 곳이 없으면 저장만 함. 끼우면 저장해 둔 것부터 출력
// shutdown() 뒤에는 저장하지 않고 호출한 스레드에서 바로 출력
class Logger
{
public:
	Logger() = default;
	~Logger();

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	void setOutput(std::shared_ptr<LogInterface> output);

	void write(LogLevel level, std::string message);

	// 저장된 로그를 모두 출력하고 로그 스레드를 끝냄
	void shutdown();

private:
	void runLoop();

private:
	// 출력할 곳이 끝내 안 정해질 때 메모리가 계속 늘지 않도록 함
	static constexpr std::size_t maxPendingEntries = 10000;

	std::mutex m_mutex;
	std::condition_variable m_wakeUp;
	std::vector<LogEntry> m_pending;
	std::shared_ptr<LogInterface> m_output;
	bool m_stopping = false;
	bool m_shutDown = false;
	std::thread m_thread;
};

// 응용의 log*() 함수가 쓰는 로거
Logger& getAppLogger();

// 엔진 내부용 로거. 응용에는 공개하지 않음
Logger& getEngineLogger();

void setEngineLogger(std::shared_ptr<LogInterface> logger);

template <typename... Args>
void engineLogDebug(const Args&... args)
{
	getEngineLogger().write(LogLevel::Debug, detail::formatLog(args...));
}

template <typename... Args>
void engineLogInfo(const Args&... args)
{
	getEngineLogger().write(LogLevel::Info, detail::formatLog(args...));
}

template <typename... Args>
void engineLogWarning(const Args&... args)
{
	getEngineLogger().write(LogLevel::Warning, detail::formatLog(args...));
}

template <typename... Args>
void engineLogError(const Args&... args)
{
	getEngineLogger().write(LogLevel::Error, detail::formatLog(args...));
}

// 소멸 시 두 로거를 shutdown. Server::run()과 Client::run()이 끝날 때 남은 로그를 출력하기 위함
class ScopedLoggerShutdown
{
public:
	ScopedLoggerShutdown() = default;
	~ScopedLoggerShutdown();

	ScopedLoggerShutdown(const ScopedLoggerShutdown&) = delete;
	ScopedLoggerShutdown& operator=(const ScopedLoggerShutdown&) = delete;
};
}
