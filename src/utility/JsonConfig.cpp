#include "tcpengine/utility/JsonConfig.hpp"

#include "Logger.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace tcpengine
{
std::optional<nlohmann::json> loadJsonFile(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file) {
		engineLogError("Cannot open ", path.string());
		return std::nullopt;
	}

	try {
		nlohmann::json json = nlohmann::json::parse(file);
		if (!json.is_object()) {
			engineLogError(path.string(), ": top level must be an object");
			return std::nullopt;
		}
		return json;
	}
	catch (const nlohmann::json::exception& e) {
		engineLogError(path.string(), ": ", e.what());
		return std::nullopt;
	}
}

unsigned short readPort(const nlohmann::json& json)
{
	const int value = json.at("port").get<int>();
	if (value < 1 || value > 65535) {
		throw std::invalid_argument("port must be between 1 and 65535");
	}
	return static_cast<unsigned short>(value);
}
}
