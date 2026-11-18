#include "Logger.hpp"

#include "EngineLog.hpp"

#include <spdlog/async_logger.h>
#include <spdlog/details/thread_pool.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace tcpengine
{
namespace
{
	constexpr std::size_t queueSize = 8192;
	// Warning 이상은 바로 쓰고 나머지는 모아서 씀
	constexpr std::chrono::seconds periodicFlushInterval{ 3 };

	constexpr const char* consolePattern = "[%H:%M:%S.%e][%^%l%$][%n][thread %t] %v";
	constexpr const char* filePattern = "[%Y-%m-%d %H:%M:%S.%e][%l][%n][thread %t] %v";

	constexpr std::size_t sourceCount = static_cast<std::size_t>(LogSource::Count);

	// %n으로 줄마다 찍히는 이름
	constexpr std::array<const char*, sourceCount> sourceNames{ "engine", "app" };

	spdlog::level::level_enum toSpdlogLevel(LogLevel level)
	{
		switch (level) {
		case LogLevel::Debug: return spdlog::level::debug;
		case LogLevel::Info: return spdlog::level::info;
		case LogLevel::Warning: return spdlog::level::warn;
		case LogLevel::Error: return spdlog::level::err;
		}
		return spdlog::level::err;
	}
}

// 모든 출처의 로거가 thread_pool 하나(큐 하나, 로그 스레드 하나)를 함께 써서 쓴 순서대로 출력됨
// 출력할 곳은 출처마다 따로 둠. 소멸할 때 thread_pool이 큐에 남은 로그를 모두 출력하고 로그 스레드를 끝냄
class LogSystem
{
public:
	explicit LogSystem(const LogSettings& settings)
		: m_threadPool(std::make_shared<spdlog::details::thread_pool>(queueSize, 1))
	{
		const std::array<const LogOutputSettings*, sourceCount> outputs{ &settings.engine, &settings.app };
		for (std::size_t i = 0; i < sourceCount; ++i) {
			m_loggers[i] = makeLogger(sourceNames[i], *outputs[i]);
		}
		m_periodicFlush = std::jthread([this](std::stop_token stopToken) { flushPeriodically(stopToken); });
	}

	~LogSystem()
	{
		m_periodicFlush.request_stop();
		m_periodicFlush.join();
		// 큐에 남은 로그 뒤에 flush 요청을 넣음. thread_pool이 소멸하며 여기까지 처리함
		flushAll();
	}

	LogSystem(const LogSystem&) = delete;
	LogSystem& operator=(const LogSystem&) = delete;

	spdlog::async_logger& logger(LogSource source) const
	{
		return *m_loggers[static_cast<std::size_t>(source)];
	}

private:
	// 큐가 차면 버리지 않고 기다림
	std::shared_ptr<spdlog::async_logger> makeLogger(const char* name, const LogOutputSettings& output)
	{
		const std::vector<spdlog::sink_ptr> sinks = makeSinks(output);
		auto logger = std::make_shared<spdlog::async_logger>(name, sinks.begin(), sinks.end(), m_threadPool,
			spdlog::async_overflow_policy::block);
		// 어느 싱크도 받지 않을 로그는 로거에서 미리 거름
		logger->set_level(lowestLevel(output));
		logger->flush_on(spdlog::level::warn);
		return logger;
	}

	static std::vector<spdlog::sink_ptr> makeSinks(const LogOutputSettings& output)
	{
		std::vector<spdlog::sink_ptr> sinks;
		if (output.console) {
			auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			console->set_level(toSpdlogLevel(output.console->level));
			console->set_pattern(consolePattern);
			sinks.push_back(std::move(console));
		}
		if (output.file) {
			auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
				output.file->path.string(), output.file->maxSize, output.file->maxFiles);
			file->set_level(toSpdlogLevel(output.file->level));
			file->set_pattern(filePattern);
			sinks.push_back(std::move(file));
		}
		return sinks;
	}

	static spdlog::level::level_enum lowestLevel(const LogOutputSettings& output)
	{
		spdlog::level::level_enum lowest = spdlog::level::off;
		if (output.console) {
			lowest = std::min(lowest, toSpdlogLevel(output.console->level));
		}
		if (output.file) {
			lowest = std::min(lowest, toSpdlogLevel(output.file->level));
		}
		return lowest;
	}

	// 비동기 flush는 큐에 요청만 넣고 바로 돌아옴
	void flushAll()
	{
		for (const std::shared_ptr<spdlog::async_logger>& logger : m_loggers) {
			logger->flush();
		}
	}

	void flushPeriodically(std::stop_token stopToken)
	{
		std::mutex mutex;
		std::condition_variable_any stopRequested;
		std::unique_lock lock(mutex);
		while (!stopRequested.wait_for(lock, stopToken, periodicFlushInterval,
			[&stopToken] { return stopToken.stop_requested(); })) {
			flushAll();
		}
	}

private:
	std::shared_ptr<spdlog::details::thread_pool> m_threadPool;
	std::array<std::shared_ptr<spdlog::async_logger>, sourceCount> m_loggers;
	std::jthread m_periodicFlush;
};

Logger& Logger::instance()
{
	static Logger logger;
	return logger;
}

Logger::Logger() = default;

Logger::~Logger() = default;

bool Logger::start(const LogSettings& settings)
{
	if (m_system) {
		std::cerr << "Failed to start logging: already started\n";
		return false;
	}
	try {
		settings.validate();
		m_system = std::make_unique<LogSystem>(settings);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to start logging: " << e.what() << '\n';
		return false;
	}
}

void Logger::stop()
{
	m_system.reset();
}

bool Logger::isEnabled(LogSource source, LogLevel level) const
{
	return m_system && m_system->logger(source).should_log(toSpdlogLevel(level));
}

void Logger::write(LogSource source, LogLevel level, std::string message)
{
	if (m_system) {
		m_system->logger(source).log(toSpdlogLevel(level), spdlog::string_view_t(message));
	}
}

void Logger::installTerminateHandler()
{
	std::set_terminate([] {
		// 메모리 부족 등으로 여기서 다시 던질 수 있음. 밖으로 나가면 abort()에 닿지 못하므로 모두 삼킴
		try {
			std::string message = "std::terminate() called";
			if (const std::exception_ptr exception = std::current_exception()) {
				try {
					std::rethrow_exception(exception);
				}
				catch (const std::exception& e) {
					message = std::string("Uncaught exception: ") + e.what();
				}
				catch (...) {
					message = "Uncaught unknown exception";
				}
			}
			std::cerr << message << '\n';
		}
		catch (...) {
		}
		std::abort();
	});
}

ScopedLogging::ScopedLogging(const LogSettings& settings)
	: m_started(Logger::instance().start(settings))
{
}

ScopedLogging::~ScopedLogging()
{
	if (m_started) {
		Logger::instance().stop();
	}
}

bool isLogEnabled(LogLevel level)
{
	return Logger::instance().isEnabled(LogSource::App, level);
}

void writeLog(LogLevel level, std::string message)
{
	Logger::instance().write(LogSource::App, level, std::move(message));
}

bool isEngineLogEnabled(LogLevel level)
{
	return Logger::instance().isEnabled(LogSource::Engine, level);
}

void writeEngineLog(LogLevel level, std::string message)
{
	Logger::instance().write(LogSource::Engine, level, std::move(message));
}
}
