#pragma once

#include <chrono>

namespace tcpengine
{
// interval마다 ping을 보내고, timeout 동안 아무것도 받지 못하면 닫음
// interval이 0이면 사용 안 함, timeout이 0이면 ping만 보냄
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
