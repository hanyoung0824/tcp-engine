#include "tcpengine/server/ServerSettings.hpp"

#include "SettingsReader.hpp"
#include "tcpengine/utility/JsonConfig.hpp"

#include <nlohmann/json.hpp>

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
		heartbeat.interval = config::readSeconds(json, "interval");
		heartbeat.timeout = config::readSeconds(json, "timeout");
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
	if (loginTimeout.count() < 0) {
		throw std::invalid_argument("loginTimeoutSeconds must not be negative");
	}
	if (maxConnections == 0) {
		throw std::invalid_argument("maxConnections must be positive");
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

		settings.port = config::readPort(engine, "port");
		settings.heartbeat = readHeartbeat(engine.at("heartbeat"));
		settings.session.resumeTimeout = config::readSeconds(engine.at("session"), "resumeTimeout");
		settings.closeTimeout = config::readSeconds(engine, "closeTimeout");
		settings.sendQueueLimit = config::readPositive(engine, "sendQueueLimitKB") * 1024;
		settings.receivePacketsPerSecond = engine.at("receiveLimit").at("packetsPerSecond").get<double>();
		settings.statsInterval = config::readSeconds(engine, "statsIntervalSeconds");
		settings.loginTimeout = config::readSeconds(engine, "loginTimeoutSeconds");
		settings.maxConnections = config::readPositive(engine, "maxConnections");
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
