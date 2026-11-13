#include "tcpengine/server/ServerSettings.hpp"

#include "tcpengine/utility/JsonConfig.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

namespace tcpengine
{
namespace
{
	HeartbeatSettings readHeartbeat(const nlohmann::json& json)
	{
		HeartbeatSettings heartbeat;
		heartbeat.interval = std::chrono::seconds(json.at("interval").get<std::int64_t>());
		heartbeat.timeout = std::chrono::seconds(json.at("timeout").get<std::int64_t>());
		return heartbeat;
	}
}

void SessionSettings::validate() const
{
	if (resumeTimeout.count() < 0) {
		throw std::invalid_argument("session resumeTimeout must not be negative");
	}
}

void ServerSettings::validate() const
{
	if (port == 0) {
		throw std::invalid_argument("port must be between 1 and 65535");
	}
	if (closeTimeout.count() < 0) {
		throw std::invalid_argument("closeTimeout must not be negative");
	}
	if (sendQueueLimit == 0) {
		throw std::invalid_argument("sendQueueLimitKB must be positive");
	}
	if (receivePacketsPerSecond <= 0) {
		throw std::invalid_argument("receiveLimit packetsPerSecond must be positive");
	}
	if (statsInterval.count() < 0) {
		throw std::invalid_argument("statsIntervalSeconds must not be negative");
	}
	heartbeat.validate();
	session.validate();
	log.validate();
}

ServerSettings ServerSettings::fromJson(const nlohmann::json& file)
{
	ServerSettings settings;
	try {
		const nlohmann::json& engine = file.at("engine");
		const std::optional<TransportType> transport = parseTransportType(engine.at("transport").get<std::string>());
		if (!transport) {
			throw std::invalid_argument("transport must be one of: tcp");
		}
		settings.transport = *transport;

		// unsigned short로 바로 받으면 범위를 넘는 값이 에러 없이 바뀌므로 int로 받아 검사
		const int port = engine.at("port").get<int>();
		if (port < 0 || port > 65535) {
			throw std::invalid_argument("port must be between 1 and 65535");
		}
		settings.port = static_cast<unsigned short>(port);

		settings.heartbeat = readHeartbeat(engine.at("heartbeat"));
		settings.session.resumeTimeout = std::chrono::seconds(engine.at("session").at("resumeTimeout").get<std::int64_t>());
		settings.closeTimeout = std::chrono::seconds(engine.at("closeTimeout").get<std::int64_t>());

		// size_t로 바로 받으면 음수가 에러 없이 큰 값으로 바뀌므로 int64로 받아 검사
		const std::int64_t sendQueueLimitKB = engine.at("sendQueueLimitKB").get<std::int64_t>();
		if (sendQueueLimitKB <= 0) {
			throw std::invalid_argument("sendQueueLimitKB must be positive");
		}
		settings.sendQueueLimit = static_cast<std::size_t>(sendQueueLimitKB) * 1024;
		settings.receivePacketsPerSecond = engine.at("receiveLimit").at("packetsPerSecond").get<double>();
		settings.statsInterval = std::chrono::seconds(engine.at("statsIntervalSeconds").get<std::int64_t>());

		settings.validate();
	}
	catch (const std::exception& e) {
		throw std::runtime_error(std::string("invalid engine settings: ") + e.what());
	}

	settings.log = LogSettings::fromJson(file);
	return settings;
}

ServerSettings ServerSettings::create(const std::filesystem::path& path)
{
	try {
		return fromJson(loadJsonFile(path));
	}
	catch (const std::exception& e) {
		throw std::runtime_error(path.string() + ": " + e.what());
	}
}
}
