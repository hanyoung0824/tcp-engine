#include "tcpengine/utility/LogSettings.hpp"

#include "SettingsReader.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace tcpengine
{
namespace
{
	constexpr std::size_t bytesPerMegabyte = 1024 * 1024;

	LogLevel readLevel(const nlohmann::json& json)
	{
		const std::string name = json.at("level").get<std::string>();
		if (name == "debug") {
			return LogLevel::Debug;
		}
		if (name == "info") {
			return LogLevel::Info;
		}
		if (name == "warning") {
			return LogLevel::Warning;
		}
		if (name == "error") {
			return LogLevel::Error;
		}
		throw std::invalid_argument("log level must be one of: debug, info, warning, error");
	}

	ConsoleLogSettings readConsole(const nlohmann::json& json)
	{
		ConsoleLogSettings settings;
		settings.level = readLevel(json);
		return settings;
	}

	FileLogSettings readFile(const nlohmann::json& json)
	{
		FileLogSettings settings;
		settings.level = readLevel(json);
		settings.path = json.at("path").get<std::string>();
		settings.maxSize = config::readPositive(json, "maxSizeMB") * bytesPerMegabyte;
		settings.maxFiles = config::readPositive(json, "maxFiles");
		return settings;
	}

	LogOutputSettings readOutput(const nlohmann::json& json)
	{
		LogOutputSettings settings;
		if (const auto console = json.find("console"); console != json.end()) {
			settings.console = readConsole(*console);
		}
		if (const auto file = json.find("file"); file != json.end()) {
			settings.file = readFile(*file);
		}
		return settings;
	}
}

void LogOutputSettings::validate() const
{
	if (!file) {
		return;
	}
	if (file->path.empty()) {
		throw std::invalid_argument("log file path must not be empty");
	}
	if (file->maxSize == 0 || file->maxFiles == 0) {
		throw std::invalid_argument("log file maxSize and maxFiles must be positive");
	}
}

void LogSettings::validate() const
{
	engine.validate();
	app.validate();

	if (engine.file && app.file
		&& std::filesystem::absolute(engine.file->path).lexically_normal() == std::filesystem::absolute(app.file->path).lexically_normal()) {
		throw std::invalid_argument("engine and app log files must be different");
	}
}

LogSettings LogSettings::fromJson(const nlohmann::json& file)
{
	try {
		const nlohmann::json& log = file.at("log");
		LogSettings settings;
		settings.engine = readOutput(log.at("engine"));
		settings.app = readOutput(log.at("app"));
		settings.validate();
		return settings;
	}
	catch (const std::exception& e) {
		throw std::runtime_error(std::string("invalid log settings: ") + e.what());
	}
}
}
