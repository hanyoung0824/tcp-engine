#pragma once

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <cstddef>

// 설정 JSON에서 값 하나를 읽음. 키가 없거나 형식이 틀리면 nlohmann::json 예외, 범위가 틀리면 std::invalid_argument
// 0 이상 같은 의미상의 규칙은 각 설정의 validate()가 검사함
namespace tcpengine::config
{
// 1~65535. unsigned short로 바로 받으면 범위를 넘는 값이 에러 없이 바뀌므로 int로 받아 검사
unsigned short readPort(const nlohmann::json& json, const char* key);

// 1 이상. size_t로 바로 받으면 음수가 에러 없이 큰 값으로 바뀌므로 int64로 받아 검사
std::size_t readPositive(const nlohmann::json& json, const char* key);

std::chrono::seconds readSeconds(const nlohmann::json& json, const char* key);
}
