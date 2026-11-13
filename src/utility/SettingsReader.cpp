#include "SettingsReader.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace tcpengine::config
{
unsigned short readPort(const nlohmann::json& json, const char* key)
{
	const int port = json.at(key).get<int>();
	if (port <= 0 || port > 65535) {
		throw std::invalid_argument(std::string(key) + " must be between 1 and 65535");
	}
	return static_cast<unsigned short>(port);
}

std::size_t readPositive(const nlohmann::json& json, const char* key)
{
	const std::int64_t value = json.at(key).get<std::int64_t>();
	if (value <= 0) {
		throw std::invalid_argument(std::string(key) + " must be positive");
	}
	return static_cast<std::size_t>(value);
}

std::chrono::seconds readSeconds(const nlohmann::json& json, const char* key)
{
	return std::chrono::seconds(json.at(key).get<std::int64_t>());
}
}
