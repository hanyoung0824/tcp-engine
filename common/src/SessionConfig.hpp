#pragma once

#include "HeartbeatSettings.hpp"
#include "Session.hpp"
#include "SessionEventHandler.hpp"

#include <memory>

namespace tcpengine
{
// 모든 세션이 공유하는 읽기 전용 설정. shared_ptr<const SessionConfig>로 넘김
struct SessionConfig
{
	SessionEventHandler handler;
	HeartbeatSettings heartbeat;
	// heartbeat.interval마다 보낼 바이트
	std::shared_ptr<const Session::Buffer> pingData;
};
}
