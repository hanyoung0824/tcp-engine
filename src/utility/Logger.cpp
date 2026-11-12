#include "Logger.hpp"

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/base_sink.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <utility>

namespace tcpengine
{
namespace
{
	constexpr std::size_t sourceCount = 2;
	constexpr std::size_t queueSize = 8192;

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

	LogLevel fromSpdlogLevel(spdlog::level::level_enum level)
	{
		switch (level) {
		case spdlog::level::trace:
		case spdlog::level::debug: return LogLevel::Debug;
		case spdlog::level::info: return LogLevel::Info;
		case spdlog::level::warn: return LogLevel::Warning;
		default: return LogLevel::Error;
		}
	}

	// spdlog의 비동기 flush는 요청만 넣고 바로 돌아오므로, 로그 스레드가 flush에 닿았는지 직접 셈
	class FlushTracker
	{
	public:
		std::uint64_t request()
		{
			std::lock_guard lock(m_mutex);
			return ++m_requested;
		}

		void complete()
		{
			{
				std::lock_guard lock(m_mutex);
				++m_completed;
			}
			m_completedChanged.notify_all();
		}

		bool wait(std::uint64_t ticket, std::optional<std::chrono::milliseconds> timeout)
		{
			std::unique_lock lock(m_mutex);
			const auto done = [this, ticket] { return m_completed >= ticket; };
			if (!timeout) {
				m_completedChanged.wait(lock, done);
				return true;
			}
			return m_completedChanged.wait_for(lock, *timeout, done);
		}

	private:
		std::mutex m_mutex;
		std::condition_variable m_completedChanged;
		std::uint64_t m_requested = 0;
		std::uint64_t m_completed = 0;
	};

	// spdlog 로그를 응용이 넘긴 LogSink로 넘김. 로그 스레드에서만 불림
	class LogSinkAdapter final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit LogSinkAdapter(FlushTracker& flushTracker)
			: m_flushTracker(flushTracker)
		{
		}

		void setTarget(std::shared_ptr<LogSink> target)
		{
			std::lock_guard lock(mutex_);
			m_target = std::move(target);
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& message) override
		{
			if (m_target) {
				m_target->output(LogEntry{ message.time, fromSpdlogLevel(message.level), message.thread_id,
					std::string(message.payload.data(), message.payload.size()) });
			}
		}

		void flush_() override
		{
			m_flushTracker.complete();
		}

	private:
		FlushTracker& m_flushTracker;
		std::shared_ptr<LogSink> m_target;
	};

	// 소멸할 때 thread_pool이 큐에 남은 로그를 모두 출력하고 로그 스레드를 끝냄
	class LogSystem
	{
	public:
		LogSystem()
			: m_threadPool(std::make_shared<spdlog::details::thread_pool>(queueSize, 1))
		{
			constexpr std::array<const char*, sourceCount> names{ "engine", "app" };
			for (std::size_t i = 0; i < sourceCount; ++i) {
				m_adapters[i] = std::make_shared<LogSinkAdapter>(m_flushTracker);
				// 큐가 차면 버리지 않고 기다림. 레벨은 isLogEnabled()와 LogSink가 거르므로 spdlog는 모두 통과
				m_loggers[i] = std::make_shared<spdlog::async_logger>(names[i], m_adapters[i], m_threadPool,
					spdlog::async_overflow_policy::block);
				m_loggers[i]->set_level(spdlog::level::trace);
			}
		}

		void setSink(LogSource source, std::shared_ptr<LogSink> sink)
		{
			const std::size_t index = static_cast<std::size_t>(source);
			m_adapters[index]->setTarget(sink);
			m_sinks[index].store(std::move(sink));
		}

		bool isEnabled(LogSource source, LogLevel level) const
		{
			const std::shared_ptr<LogSink> sink = m_sinks[static_cast<std::size_t>(source)].load();
			return sink && level >= sink->getLevel();
		}

		void write(LogSource source, LogLevel level, const std::string& message)
		{
			m_loggers[static_cast<std::size_t>(source)]->log(toSpdlogLevel(level), spdlog::string_view_t(message));
		}

		// 큐 하나를 함께 쓰므로 엔진 로거의 flush가 처리됐으면 그 전의 응용 로그도 모두 출력된 것
		bool flush(std::optional<std::chrono::milliseconds> timeout)
		{
			const std::uint64_t ticket = m_flushTracker.request();
			m_loggers[static_cast<std::size_t>(LogSource::Engine)]->flush();
			return m_flushTracker.wait(ticket, timeout);
		}

	private:
		FlushTracker m_flushTracker;
		std::shared_ptr<spdlog::details::thread_pool> m_threadPool;
		std::array<std::shared_ptr<LogSinkAdapter>, sourceCount> m_adapters;
		std::array<std::shared_ptr<spdlog::async_logger>, sourceCount> m_loggers;
		std::array<std::atomic<std::shared_ptr<LogSink>>, sourceCount> m_sinks;
	};

	LogSystem& logSystem()
	{
		static LogSystem system;
		return system;
	}
}

void setLogSink(LogSource source, std::shared_ptr<LogSink> sink)
{
	logSystem().setSink(source, std::move(sink));
}

void setEngineLogSink(std::shared_ptr<LogSink> sink)
{
	setLogSink(LogSource::Engine, std::move(sink));
}

void setAppLogSink(std::shared_ptr<LogSink> sink)
{
	setLogSink(LogSource::App, std::move(sink));
}

bool isLogEnabled(LogSource source, LogLevel level)
{
	return logSystem().isEnabled(source, level);
}

void writeLog(LogSource source, LogLevel level, std::string message)
{
	logSystem().write(source, level, message);
}

bool flushLogs(std::optional<std::chrono::milliseconds> timeout)
{
	return logSystem().flush(timeout);
}

void installTerminateHandler()
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
			writeLog(LogSource::Engine, LogLevel::Error, std::move(message));
			// 로그 스레드 자신이 죽는 중이면 flush가 끝나지 않으므로 시간을 제한함
			flushLogs(std::chrono::seconds(2));
		}
		catch (...) {
		}
		std::abort();
	});
}

ScopedLogFlush::~ScopedLogFlush()
{
	flushLogs();
}

namespace detail
{
	bool isLogEnabled(LogLevel level)
	{
		return tcpengine::isLogEnabled(LogSource::App, level);
	}

	void writeLog(LogLevel level, std::string message)
	{
		tcpengine::writeLog(LogSource::App, level, std::move(message));
	}
}
}
