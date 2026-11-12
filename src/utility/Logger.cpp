#include "Logger.hpp"

#include <cstdlib>
#include <exception>
#include <iterator>
#include <utility>

namespace tcpengine
{
namespace
{
	void deliver(LogSink& output, const LogEntry& entry)
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

void Logger::setOutput(LogSource source, std::shared_ptr<LogSink> output)
{
	{
		std::lock_guard lock(m_mutex);
		const bool hasOutput = output != nullptr;
		m_outputs[static_cast<std::size_t>(source)].store(std::move(output));
		if (!hasOutput) {
			return;
		}

		// 보관해 둔 로그를 큐 맨 앞에 넣어 먼저 출력되게 함
		std::vector<Item>& held = heldOf(source);
		m_pending.insert(m_pending.begin(), std::make_move_iterator(held.begin()), std::make_move_iterator(held.end()));
		held.clear();

		if (!m_shutDown && !m_thread.joinable()) {
			m_thread = std::thread([this] { runLoop(); });
		}
	}
	m_wakeUp.notify_one();
}

bool Logger::isEnabled(LogSource source, LogLevel level) const
{
	const std::shared_ptr<LogSink> output = outputOf(source);
	return !output || level >= output->getLevel();
}

void Logger::write(LogSource source, LogLevel level, std::string message)
{
	Item item{ source, LogEntry{ std::chrono::system_clock::now(), level, std::this_thread::get_id(), std::move(message) } };

	std::lock_guard lock(m_mutex);
	if (m_shutDown) {
		if (const std::shared_ptr<LogSink>& output = outputOf(source)) {
			deliver(*output, item.entry);
		}
		return;
	}

	if (!m_thread.joinable()) {
		deliverOrHold(std::move(item));
		return;
	}
	m_pending.push_back(std::move(item));
	m_wakeUp.notify_one();
}

void Logger::flush()
{
	std::unique_lock lock(m_mutex);
	m_drained.wait(lock, [this] { return !m_thread.joinable() || m_shutDown || (m_pending.empty() && !m_delivering); });
}

void Logger::writeAndFlushBeforeAbort(LogSource source, std::string message, std::chrono::milliseconds timeout)
{
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	Item item{ source, LogEntry{ std::chrono::system_clock::now(), LogLevel::Error, std::this_thread::get_id(), std::move(message) } };

	std::unique_lock lock(m_mutex, std::defer_lock);
	while (!lock.try_lock()) {
		if (std::chrono::steady_clock::now() >= deadline) {
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 로그 스레드가 없거나 로그 스레드 자신이 죽는 중이면 기다려도 출력되지 않으므로 직접 출력
	if (m_shutDown || !m_thread.joinable() || m_thread.get_id() == std::this_thread::get_id()) {
		if (const std::shared_ptr<LogSink>& output = outputOf(source)) {
			deliver(*output, item.entry);
		}
		return;
	}

	m_pending.push_back(std::move(item));
	m_wakeUp.notify_one();
	m_drained.wait_until(lock, deadline, [this] { return m_pending.empty() && !m_delivering; });
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
	m_drained.notify_all();
	if (m_thread.joinable()) {
		m_thread.join();
	}

	// 출력할 곳이 없어 남은 로그는 버림
	std::lock_guard lock(m_mutex);
	m_pending.clear();
	for (std::vector<Item>& held : m_held) {
		held.clear();
	}
}

void Logger::runLoop()
{
	std::vector<Item> items;
	while (true) {
		std::array<std::shared_ptr<LogSink>, sourceCount> outputs;
		{
			std::unique_lock lock(m_mutex);
			m_wakeUp.wait(lock, [this] { return !m_pending.empty() || m_stopping; });
			items.swap(m_pending);
			if (items.empty() && m_stopping) {
				return;
			}
			for (std::size_t i = 0; i < sourceCount; ++i) {
				outputs[i] = m_outputs[i].load();
			}
			m_delivering = true;
		}

		// 출력할 곳이 없는 출처의 로그는 다시 보관함
		std::vector<Item> unassigned;
		for (Item& item : items) {
			if (const std::shared_ptr<LogSink>& output = outputs[static_cast<std::size_t>(item.source)]) {
				deliver(*output, item.entry);
			}
			else {
				unassigned.push_back(std::move(item));
			}
		}
		items.clear();

		{
			std::lock_guard lock(m_mutex);
			for (Item& item : unassigned) {
				deliverOrHold(std::move(item));
			}
			m_delivering = false;
		}
		m_drained.notify_all();
	}
}

void Logger::deliverOrHold(Item item)
{
	if (const std::shared_ptr<LogSink>& output = outputOf(item.source); output && m_thread.joinable()) {
		// 스레드가 꺼낸 뒤에 출력할 곳이 생긴 경우. 순서를 지키도록 큐로 돌려보냄
		m_pending.push_back(std::move(item));
		m_wakeUp.notify_one();
		return;
	}

	std::vector<Item>& held = heldOf(item.source);
	if (held.size() >= maxHeldEntries) {
		held.erase(held.begin());
	}
	held.push_back(std::move(item));
}

Logger& getLogger()
{
	static Logger logger;
	return logger;
}

void setEngineLogSink(std::shared_ptr<LogSink> sink)
{
	getLogger().setOutput(LogSource::Engine, std::move(sink));
}

void setAppLogSink(std::shared_ptr<LogSink> sink)
{
	getLogger().setOutput(LogSource::App, std::move(sink));
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
			getLogger().writeAndFlushBeforeAbort(LogSource::Engine, std::move(message), std::chrono::seconds(2));
		}
		catch (...) {
		}
		std::abort();
	});
}

ScopedLogFlush::~ScopedLogFlush()
{
	getLogger().flush();
}

namespace detail
{
	bool isLogEnabled(LogLevel level)
	{
		return getLogger().isEnabled(LogSource::App, level);
	}

	void writeLog(LogLevel level, std::string message)
	{
		getLogger().write(LogSource::App, level, std::move(message));
	}
}
}
