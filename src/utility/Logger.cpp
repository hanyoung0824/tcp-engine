#include "Logger.hpp"

#include "EngineLog.hpp"

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace tcpengine
{
namespace
{
	constexpr std::size_t queueSize = 8192;
	// Warning 이상은 바로 쓰고 나머지는 모아서 씀
	constexpr std::chrono::seconds periodicFlushInterval{ 3 };

	constexpr const char* consolePattern = "[%H:%M:%S.%e][%^%l%$][%n] %v";
	constexpr const char* filePattern = "[%Y-%m-%d %H:%M:%S.%e][%l][%n][thread %t] %v";

	constexpr std::size_t sourceCount = static_cast<std::size_t>(LogSource::MaxCount);

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

	// spdlog의 비동기 flush는 요청만 넣고 바로 돌아옴. 그래서 표시 로그를 큐에 넣고 로그 스레드가 거기 닿을 때까지 기다림
	// 큐가 하나라 n번째 표시에 닿았으면 n번째 번호를 받기 전에 넣은 로그는 모두 출력된 것
	class FlushWaiter
	{
	public:
		std::uint64_t nextTicket()
		{
			std::lock_guard lock(m_mutex);
			return ++m_requested;
		}

		void onMarkerReached()
		{
			{
				std::lock_guard lock(m_mutex);
				++m_reached;
			}
			m_reachedChanged.notify_all();
		}

		bool wait(std::uint64_t ticket, std::optional<std::chrono::milliseconds> timeout)
		{
			std::unique_lock lock(m_mutex);
			const auto reached = [this, ticket] { return m_reached >= ticket; };
			if (!timeout) {
				m_reachedChanged.wait(lock, reached);
				return true;
			}
			return m_reachedChanged.wait_for(lock, *timeout, reached);
		}

	private:
		std::mutex m_mutex;
		std::condition_variable m_reachedChanged;
		std::uint64_t m_requested = 0;
		std::uint64_t m_reached = 0;
	};

	class FlushMarkerSink final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit FlushMarkerSink(FlushWaiter& waiter)
			: m_waiter(waiter)
		{
		}

	protected:
		void sink_it_(const spdlog::details::log_msg&) override
		{
			m_waiter.onMarkerReached();
		}

		void flush_() override {}

	private:
		FlushWaiter& m_waiter;
	};

	// 모든 출처의 로거가 thread_pool 하나(큐 하나, 로그 스레드 하나)를 함께 써서 쓴 순서대로 출력됨
	// 출력할 곳은 출처마다 따로 둠. 소멸할 때 thread_pool이 큐에 남은 로그를 모두 출력하고 로그 스레드를 끝냄
	class LogSystem
	{
	public:
		LogSystem()
			: m_threadPool(std::make_shared<spdlog::details::thread_pool>(queueSize, 1)),
			m_flushMarker(makeLogger("flush", std::make_shared<FlushMarkerSink>(m_flushWaiter)))
		{
			for (std::size_t i = 0; i < sourceCount; ++i) {
				// 실행 중에 출력할 곳을 바꿀 수 있도록 dist_sink로 감쌈
				m_outputs[i] = std::make_shared<spdlog::sinks::dist_sink_mt>();
				m_loggers[i] = makeLogger(sourceNames[i], m_outputs[i]);
				// start() 전의 로그는 버림
				m_loggers[i]->set_level(spdlog::level::off);
				m_loggers[i]->flush_on(spdlog::level::warn);
				// flush_every()는 등록된 로거만 flush함
				spdlog::register_logger(m_loggers[i]);
			}
			spdlog::flush_every(periodicFlushInterval);
		}

		~LogSystem()
		{
			// spdlog 레지스트리가 더 오래 살아남으므로, 주기 flush가 사라진 thread_pool에 닿지 않게 먼저 뺌
			for (const char* name : sourceNames) {
				spdlog::drop(name);
			}
		}

		LogSystem(const LogSystem&) = delete;
		LogSystem& operator=(const LogSystem&) = delete;

		void start(const LogSettings& settings)
		{
			settings.validate();

			// 파일을 모두 연 뒤에 바꿔 끼우므로 중간에 실패해도 이전 출력은 그대로 남음
			const std::array<const LogOutputSettings*, sourceCount> outputs{ &settings.engine, &settings.app };
			std::array<std::vector<spdlog::sink_ptr>, sourceCount> sinks;
			for (std::size_t i = 0; i < sourceCount; ++i) {
				sinks[i] = makeSinks(*outputs[i]);
			}

			for (std::size_t i = 0; i < sourceCount; ++i) {
				m_outputs[i]->set_sinks(std::move(sinks[i]));
				// 어느 싱크도 받지 않을 로그는 로거에서 미리 거름
				m_loggers[i]->set_level(lowestLevel(*outputs[i]));
			}
		}

		spdlog::async_logger& logger(LogSource source)
		{
			return *m_loggers[static_cast<std::size_t>(source)];
		}

		bool flush(std::optional<std::chrono::milliseconds> timeout)
		{
			const std::uint64_t ticket = m_flushWaiter.nextTicket();
			for (const std::shared_ptr<spdlog::async_logger>& logger : m_loggers) {
				logger->flush();
			}
			m_flushMarker->log(spdlog::level::info, "flush");
			return m_flushWaiter.wait(ticket, timeout);
		}

	private:
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

		// 큐가 차면 버리지 않고 기다림
		std::shared_ptr<spdlog::async_logger> makeLogger(const std::string& name, spdlog::sink_ptr sink)
		{
			return std::make_shared<spdlog::async_logger>(name, std::move(sink), m_threadPool,
				spdlog::async_overflow_policy::block);
		}

	private:
		FlushWaiter m_flushWaiter;
		std::shared_ptr<spdlog::details::thread_pool> m_threadPool;
		std::shared_ptr<spdlog::async_logger> m_flushMarker;
		std::array<std::shared_ptr<spdlog::sinks::dist_sink_mt>, sourceCount> m_outputs;
		std::array<std::shared_ptr<spdlog::async_logger>, sourceCount> m_loggers;
	};

	LogSystem& logSystem()
	{
		static LogSystem system;
		return system;
	}
}

bool Logger::start(const LogSettings& settings)
{
	try {
		logSystem().start(settings);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to start logging: " << e.what() << '\n';
		return false;
	}
}

bool Logger::isEnabled(LogSource source, LogLevel level)
{
	return logSystem().logger(source).should_log(toSpdlogLevel(level));
}

void Logger::write(LogSource source, LogLevel level, std::string message)
{
	logSystem().logger(source).log(toSpdlogLevel(level), spdlog::string_view_t(message));
}

bool Logger::flush(std::optional<std::chrono::milliseconds> timeout)
{
	return logSystem().flush(timeout);
}

void Logger::installTerminateHandler()
{
	std::set_terminate([] {
		// 메모리 부족이나 sink 오류로 여기서 다시 던질 수 있음. 밖으로 나가면 abort()에 닿지 못하므로 모두 삼킴
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
			write(LogSource::Engine, LogLevel::Error, std::move(message));
			// 로그 스레드 자신이 죽는 중이면 flush가 끝나지 않으므로 시간을 제한함
			flush(std::chrono::seconds(2));
		}
		catch (...) {
		}
		std::abort();
	});
}

ScopedLogFlush::~ScopedLogFlush()
{
	Logger::flush();
}

bool isLogEnabled(LogLevel level)
{
	return Logger::isEnabled(LogSource::App, level);
}

void writeLog(LogLevel level, std::string message)
{
	Logger::write(LogSource::App, level, std::move(message));
}

bool isEngineLogEnabled(LogLevel level)
{
	return Logger::isEnabled(LogSource::Engine, level);
}

void writeEngineLog(LogLevel level, std::string message)
{
	Logger::write(LogSource::Engine, level, std::move(message));
}
}
