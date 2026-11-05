#pragma once

#include "HeartbeatSettings.hpp"
#include "Log.hpp"

namespace tcpengine
{
struct ServerSettings
{
	unsigned short port;
	HeartbeatSettings heartbeat;
	LogLevel logLevel;
};
}
