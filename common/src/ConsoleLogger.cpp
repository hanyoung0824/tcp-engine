#include "ConsoleLogger.hpp"

#include <iostream>

namespace tcpengine
{
namespace
{
	std::string_view levelTag(LogLevel level)
	{
		switch (level) {
		case LogLevel::Debug: return "[DEBUG] ";
		case LogLevel::Info: return "[INFO] ";
		case LogLevel::Warning: return "[WARN] ";
		case LogLevel::Error: return "[ERROR] ";
		}
		return "";
	}
}

Logger& getLogger()
{
	return ConsoleLogger::instance();
}

ConsoleLogger& ConsoleLogger::instance()
{
	static ConsoleLogger instance;
	return instance;
}

void ConsoleLogger::setLevel(LogLevel level)
{
	m_minLevel.store(level, std::memory_order_relaxed);
}

bool ConsoleLogger::isEnabled(LogLevel level) const
{
	return level >= m_minLevel.load(std::memory_order_relaxed);
}

void ConsoleLogger::write(LogLevel level, std::string_view message)
{
	if (!isEnabled(level)) {
		return;
	}

	std::ostream& out = level >= LogLevel::Warning ? std::cerr : std::cout;
	out << levelTag(level) << message << '\n';
}
}
