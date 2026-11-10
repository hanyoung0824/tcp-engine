#pragma once

#include "tcpengine/client/ClientSettings.hpp"

#include <nlohmann/json_fwd.hpp>

#include <optional>

namespace tcpengine
{
// 실패하면 엔진 로그에 이유를 남기고 nullopt
std::optional<ClientSettings> parseClientSettings(const nlohmann::json& json);
}
