#include "Logger.hpp"

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
		outputOf(source) = std::move(output);
		if (!outputOf(source)) {
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
			outputs = m_outputs;
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

ScopedLogFlush::~ScopedLogFlush()
{
	getLogger().flush();
}

namespace detail
{
	void writeLog(LogLevel level, std::string message)
	{
		getLogger().write(LogSource::App, level, std::move(message));
	}
}
}
