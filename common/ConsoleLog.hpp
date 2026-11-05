#pragma once

#include "Log.hpp"

#include <iostream>
#include <string_view>

inline void setConsoleLogHandler(LogLevel minLevel)
{
	setLogHandler([minLevel](LogLevel level, std::string_view message) {
		if (level < minLevel) {
			return;
		}

		std::ostream& out = level >= LogLevel::Warning ? std::cerr : std::cout;
		switch (level) {
		case LogLevel::Debug: out << "[DEBUG] "; break;
		case LogLevel::Info: out << "[INFO] "; break;
		case LogLevel::Warning: out << "[WARN] "; break;
		case LogLevel::Error: out << "[ERROR] "; break;
		}
		out << message << '\n';
	});
}
