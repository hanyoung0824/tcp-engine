#pragma once

#include "HeartbeatSettings.hpp"
#include "Log.hpp"

#include <string>

namespace tcpengine
{
struct ClientSettings
{
	std::string host;
	unsigned short port;
	HeartbeatSettings heartbeat;
	LogLevel logLevel;
};
}
