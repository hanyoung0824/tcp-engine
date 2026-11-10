#pragma once

#include "tcpengine/server/ServerSettings.hpp"

#include <nlohmann/json_fwd.hpp>

#include <optional>

namespace tcpengine
{
// 실패하면 엔진 로그에 이유를 남기고 nullopt
std::optional<ServerSettings> parseServerSettings(const nlohmann::json& json);
}
