#pragma once

#include "Log.hpp"

#include <atomic>
#include <string_view>

namespace tcpengine
{
class ConsoleLogger final : public Logger
{
public:
	static ConsoleLogger& instance();

	void setLevel(LogLevel level);

	bool isEnabled(LogLevel level) const override;

	void write(LogLevel level, std::string_view message) override;

private:
	ConsoleLogger() = default;

private:
	std::atomic<LogLevel> m_minLevel{ LogLevel::Info };
};
}
