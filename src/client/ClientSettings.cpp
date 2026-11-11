#include "tcpengine/client/ClientSettings.hpp"

#include "tcpengine/utility/JsonConfig.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
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
}

ClientSettings ClientSettings::fromJson(const nlohmann::json& file)
{
	try {
		const nlohmann::json& engine = file.at("engine");
		ClientSettings settings;
		const std::optional<TransportType> transport = parseTransportType(engine.at("transport").get<std::string>());
		if (!transport) {
			throw std::invalid_argument("transport must be one of: tcp");
		}
		settings.transport = *transport;

		settings.host = engine.at("host").get<std::string>();

		// unsigned short로 바로 받으면 범위를 넘는 값이 에러 없이 바뀌므로 int로 받아 검사
		const int port = engine.at("port").get<int>();
		if (port < 0 || port > 65535) {
			throw std::invalid_argument("port must be between 1 and 65535");
		}
		settings.port = static_cast<unsigned short>(port);
		settings.closeTimeout = std::chrono::seconds(engine.at("closeTimeout").get<std::int64_t>());

		settings.validate();
		return settings;
	}
	catch (const std::exception& e) {
		throw std::runtime_error(std::string("invalid engine settings: ") + e.what());
	}
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
