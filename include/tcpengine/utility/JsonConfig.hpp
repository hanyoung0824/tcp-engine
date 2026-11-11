#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

namespace tcpengine
{
// 파일이 없거나, 열 수 없거나, JSON 형식이 틀리면 std::runtime_error를 던짐. 메시지에 경로는 넣지 않음
// 로거를 쓰기 전에도 부를 수 있도록 로그를 남기지 않음
nlohmann::json loadJsonFile(const std::filesystem::path& path);
}
