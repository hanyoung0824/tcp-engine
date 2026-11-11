#include "tcpengine/utility/JsonConfig.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace tcpengine
{
nlohmann::json loadJsonFile(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file) {
		throw std::runtime_error("cannot open file");
	}

	nlohmann::json json;
	try {
		json = nlohmann::json::parse(file);
	}
	catch (const nlohmann::json::exception& e) {
		throw std::runtime_error(e.what());
	}

	if (!json.is_object()) {
		throw std::runtime_error("top level must be an object");
	}
	return json;
}
}
