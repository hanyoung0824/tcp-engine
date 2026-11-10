#pragma once

#include "tcpengine/network/HeartbeatSettings.hpp"

namespace tcpengine
{
// Server::run()에 넘긴 설정 파일에서 엔진이 읽어 둔 값. 응용은 getSettings()로 읽기만 함
struct ServerSettings
{
	unsigned short port = 0;
	HeartbeatSettings heartbeat;
};
}
