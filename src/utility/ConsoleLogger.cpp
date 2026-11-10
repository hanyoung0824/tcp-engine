#include "tcpengine/utility/ConsoleLogger.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <utility>

namespace tcpengine
{
namespace
{
	std::string_view levelTag(LogLevel level)
	{
		switch (level) {
		case LogLevel::Debug: return "[DEBUG]";
		case LogLevel::Info: return "[INFO]";
		case LogLevel::Warning: return "[WARN]";
		case LogLevel::Error: return "[ERROR]";
		}
		return "";
	}

	std::tm toLocalTime(std::time_t time)
	{
		std::tm local{};
#ifdef _WIN32
		localtime_s(&local, &time);
#else
		localtime_r(&time, &local);
#endif
		return local;
	}

	std::mutex& consoleMutex()
	{
		static std::mutex mutex;
		return mutex;
	}
}

ConsoleLogger::ConsoleLogger(std::string name)
	: m_name(std::move(name))
{
}

void ConsoleLogger::write(const LogEntry& entry)
{
	using namespace std::chrono;

	const std::tm local = toLocalTime(system_clock::to_time_t(entry.time));
	const auto milliseconds = duration_cast<std::chrono::milliseconds>(entry.time.time_since_epoch()).count() % 1000;

	std::ostringstream line;
	line << '[' << std::put_time(&local, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << milliseconds << ']'
		<< levelTag(entry.level) << '[' << m_name << "] " << entry.message << '\n';

	std::ostream& out = entry.level >= LogLevel::Warning ? std::cerr : std::cout;
	std::lock_guard lock(consoleMutex());
	out << line.str() << std::flush;
}
}
