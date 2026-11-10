#include "ServerSettingsParser.hpp"

#include "Logger.hpp"
#include "tcpengine/utility/JsonConfig.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>

namespace tcpengine
{
namespace
{
	HeartbeatSettings readHeartbeat(const nlohmann::json& json)
	{
		const nlohmann::json& heartbeat = json.at("heartbeat");
		const auto interval = heartbeat.at("interval").get<std::int64_t>();
		const auto timeout = heartbeat.at("timeout").get<std::int64_t>();
		if (interval < 0 || timeout < 0) {
			throw std::invalid_argument("heartbeat values must not be negative");
		}

		HeartbeatSettings settings;
		settings.interval = std::chrono::seconds(interval);
		settings.timeout = std::chrono::seconds(timeout);
		return settings;
	}
}

std::optional<ServerSettings> parseServerSettings(const nlohmann::json& json)
{
	try {
		ServerSettings settings;
		settings.port = readPort(json);
		settings.heartbeat = readHeartbeat(json);
		return settings;
	}
	catch (const std::exception& e) {
		engineLogError("Invalid server settings: ", e.what());
		return std::nullopt;
	}
}
}
