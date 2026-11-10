#include "Logger.hpp"

#include <exception>
#include <utility>

namespace tcpengine
{
namespace
{
	void deliver(LogInterface& output, const LogEntry& entry)
	{
		// 출력 오류로 로그 스레드가 끝나지 않도록 함
		try {
			output.output(entry);
		}
		catch (const std::exception&) {
		}
	}
}

Logger::~Logger()
{
	shutdown();
}

void Logger::setOutput(std::shared_ptr<LogInterface> output)
{
	{
		std::lock_guard lock(m_mutex);
		m_output = std::move(output);
		if (m_output && !m_shutDown && !m_thread.joinable()) {
			m_thread = std::thread([this] { runLoop(); });
		}
	}
	m_wakeUp.notify_one();
}

void Logger::write(LogLevel level, std::string message)
{
	LogEntry entry{ std::chrono::system_clock::now(), level, std::this_thread::get_id(), std::move(message) };

	std::lock_guard lock(m_mutex);
	if (m_shutDown) {
		if (m_output) {
			deliver(*m_output, entry);
		}
		return;
	}

	if (!m_output && m_pending.size() >= maxPendingEntries) {
		m_pending.erase(m_pending.begin());
	}
	m_pending.push_back(std::move(entry));
	if (m_output) {
		m_wakeUp.notify_one();
	}
}

void Logger::shutdown()
{
	{
		std::lock_guard lock(m_mutex);
		if (m_shutDown) {
			return;
		}
		m_shutDown = true;
		m_stopping = true;
	}
	m_wakeUp.notify_one();
	if (m_thread.joinable()) {
		m_thread.join();
	}

	// 출력할 곳이 없어 남은 로그는 버림
	std::lock_guard lock(m_mutex);
	m_pending.clear();
}

void Logger::runLoop()
{
	std::vector<LogEntry> entries;
	while (true) {
		std::shared_ptr<LogInterface> output;
		{
			std::unique_lock lock(m_mutex);
			m_wakeUp.wait(lock, [this] { return (m_output && !m_pending.empty()) || m_stopping; });
			if (!m_output) {
				return;
			}
			entries.swap(m_pending);
			output = m_output;
			if (entries.empty() && m_stopping) {
				return;
			}
		}

		for (const LogEntry& entry : entries) {
			deliver(*output, entry);
		}
		entries.clear();
	}
}

Logger& getAppLogger()
{
	static Logger logger;
	return logger;
}

Logger& getEngineLogger()
{
	static Logger logger;
	return logger;
}

void setEngineLogger(std::shared_ptr<LogInterface> logger)
{
	getEngineLogger().setOutput(std::move(logger));
}

ScopedLoggerShutdown::~ScopedLoggerShutdown()
{
	getAppLogger().shutdown();
	getEngineLogger().shutdown();
}

void setLogger(std::shared_ptr<LogInterface> logger)
{
	getAppLogger().setOutput(std::move(logger));
}

namespace detail
{
	void writeLog(LogLevel level, std::string message)
	{
		getAppLogger().write(level, std::move(message));
	}
}
}
