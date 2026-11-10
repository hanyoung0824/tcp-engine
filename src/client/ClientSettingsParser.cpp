#include "ClientSettingsParser.hpp"

#include "Logger.hpp"
#include "tcpengine/utility/JsonConfig.hpp"

#include <exception>

namespace tcpengine
{
std::optional<ClientSettings> parseClientSettings(const nlohmann::json& json)
{
	try {
		ClientSettings settings;
		settings.host = json.at("host").get<std::string>();
		settings.port = readPort(json);
		return settings;
	}
	catch (const std::exception& e) {
		engineLogError("Invalid client settings: ", e.what());
		return std::nullopt;
	}
}
}
