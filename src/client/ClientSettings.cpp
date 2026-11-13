#include "tcpengine/client/ClientSettings.hpp"

#include "SettingsReader.hpp"
#include "tcpengine/utility/JsonConfig.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

namespace tcpengine
{
void ClientSettings::validate() const
{
	if (host.empty()) {
		throw std::invalid_argument("host must not be empty");
	}
	if (port == 0) {
		throw std::invalid_argument("port must be between 1 and 65535");
	}
	if (closeTimeout.count() < 0) {
		throw std::invalid_argument("closeTimeout must not be negative");
	}
	if (connectTimeout.count() <= 0) {
		throw std::invalid_argument("connectTimeoutSeconds must be positive");
	}
	if (reconnectMaxDelay.count() < 0) {
		throw std::invalid_argument("reconnectMaxDelaySeconds must not be negative");
	}
	log.validate();
}

ClientSettings ClientSettings::fromJson(const nlohmann::json& file)
{
	ClientSettings settings;
	try {
		const nlohmann::json& engine = file.at("engine");
		const std::optional<TransportType> transport = parseTransportType(engine.at("transport").get<std::string>());
		if (!transport) {
			throw std::invalid_argument("transport must be one of: tcp");
		}
		settings.transport = *transport;

		settings.host = engine.at("host").get<std::string>();

		settings.port = config::readPort(engine, "port");
		settings.closeTimeout = config::readSeconds(engine, "closeTimeout");
		settings.connectTimeout = config::readSeconds(engine, "connectTimeoutSeconds");
		settings.reconnectMaxDelay = config::readSeconds(engine, "reconnectMaxDelaySeconds");
		settings.validate();
	}
	catch (const std::exception& e) {
		throw std::runtime_error(std::string("invalid engine settings: ") + e.what());
	}

	settings.log = LogSettings::fromJson(file);
	return settings;
}

ClientSettings ClientSettings::create(const std::filesystem::path& path)
{
	try {
		return fromJson(loadJsonFile(path));
	}
	catch (const std::exception& e) {
		throw std::runtime_error(path.string() + ": " + e.what());
	}
}
}
