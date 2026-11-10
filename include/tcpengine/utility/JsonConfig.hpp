#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>

namespace tcpengine
{
// 파일이 없거나, 열 수 없거나, JSON 형식이 틀리면 nullopt
std::optional<nlohmann::json> loadJsonFile(const std::filesystem::path& path);

// read* 함수는 항목이 없거나 값이 틀리면 std::exception 계열을 던짐
unsigned short readPort(const nlohmann::json& json);
}
