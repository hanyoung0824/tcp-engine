#pragma once

#include <chrono>

namespace tcpengine
{
// 서버가 접속 직후 클라이언트에 알려 줌. 클라이언트는 interval마다 Ping을 보내고 서버는 Pong으로 답함
// 양쪽 모두 timeout 동안 아무것도 받지 못하면 닫음. interval이 0이면 사용 안 함, timeout이 0이면 끊지 않음
struct HeartbeatSettings
{
	std::chrono::seconds interval{ 0 };
	std::chrono::seconds timeout{ 0 };
};

inline bool isValid(const HeartbeatSettings& heartbeat)
{
	return heartbeat.interval.count() >= 0 && heartbeat.timeout.count() >= 0
		&& (heartbeat.timeout.count() == 0 || heartbeat.timeout > heartbeat.interval);
}
}
