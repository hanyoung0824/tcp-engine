#pragma once

#include "tcpengine/utility/Log.hpp"
#include "tcpengine/utility/LogSink.hpp"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tcpengine
{
enum class LogSource
{
	Engine,
	App,
};

// 엔진 로그와 응용 로그를 한 큐에 쌓아 쓴 순서대로 처리하고, 출력은 출처별 LogSink로 함
// write()는 저장만 하고 로그 스레드가 큐를 통째로 꺼내 출력함
// 출력할 곳이 없는 출처의 로그는 따로 보관했다가 출력할 곳을 끼우면 그것부터 출력
// shutdown() 뒤에는 저장하지 않고 호출한 스레드에서 바로 출력
class Logger
{
public:
	Logger() = default;
	~Logger();

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	void setOutput(LogSource source, std::shared_ptr<LogSink> output);

	void write(LogSource source, LogLevel level, std::string message);

	// 지금까지 쌓인 로그가 모두 출력될 때까지 기다림. 출력할 곳이 없는 출처의 로그는 기다리지 않음
	void flush();

	// 쌓인 로그를 모두 출력하고 로그 스레드를 끝냄
	void shutdown();

private:
	struct Item
	{
		LogSource source;
		LogEntry entry;
	};

	void runLoop();

	std::shared_ptr<LogSink>& outputOf(LogSource source) { return m_outputs[static_cast<std::size_t>(source)]; }

	std::vector<Item>& heldOf(LogSource source) { return m_held[static_cast<std::size_t>(source)]; }

	// 출력할 곳이 없으면 보관함. 보관 중인 로그가 너무 많으면 오래된 것부터 버림
	void deliverOrHold(Item item);

private:
	static constexpr std::size_t sourceCount = 2;
	// 출력할 곳이 끝내 안 정해질 때 메모리가 계속 늘지 않도록 함
	static constexpr std::size_t maxHeldEntries = 10000;

	std::mutex m_mutex;
	std::condition_variable m_wakeUp;
	std::condition_variable m_drained;
	std::vector<Item> m_pending;
	std::array<std::shared_ptr<LogSink>, sourceCount> m_outputs;
	std::array<std::vector<Item>, sourceCount> m_held;
	bool m_stopping = false;
	// 로그 스레드가 꺼낸 로그를 출력하는 중
	bool m_delivering = false;
	bool m_shutDown = false;
	std::thread m_thread;
};

Logger& getLogger();

void setEngineLogSink(std::shared_ptr<LogSink> sink);

void setAppLogSink(std::shared_ptr<LogSink> sink);

template <typename... Args>
void engineLogDebug(const Args&... args)
{
	getLogger().write(LogSource::Engine, LogLevel::Debug, detail::formatLog(args...));
}

template <typename... Args>
void engineLogInfo(const Args&... args)
{
	getLogger().write(LogSource::Engine, LogLevel::Info, detail::formatLog(args...));
}

template <typename... Args>
void engineLogWarning(const Args&... args)
{
	getLogger().write(LogSource::Engine, LogLevel::Warning, detail::formatLog(args...));
}

template <typename... Args>
void engineLogError(const Args&... args)
{
	getLogger().write(LogSource::Engine, LogLevel::Error, detail::formatLog(args...));
}

// 소멸 시 로거를 flush. Server::run()과 Client::run()이 어떤 경로로 끝나도 남은 로그를 출력하기 위함
// 로그 스레드는 프로그램 종료 때 Logger 소멸자가 끝냄
class ScopedLogFlush
{
public:
	ScopedLogFlush() = default;
	~ScopedLogFlush();

	ScopedLogFlush(const ScopedLogFlush&) = delete;
	ScopedLogFlush& operator=(const ScopedLogFlush&) = delete;
};
}
