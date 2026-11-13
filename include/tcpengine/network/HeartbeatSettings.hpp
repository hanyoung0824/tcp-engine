#pragma once

#include <chrono>
#include <stdexcept>

namespace tcpengine
{
// 서버가 접속 직후 클라이언트에 알려 줌. 클라이언트는 interval마다 Ping을 보내고 서버는 Pong으로 답함
// 양쪽 모두 timeout 동안 아무것도 받지 못하면 닫음. interval이 0이면 사용 안 함, timeout이 0이면 끊지 않음
struct HeartbeatSettings
{
	std::chrono::seconds interval{};
	std::chrono::seconds timeout{};

	// 틀리면 이유를 담은 std::invalid_argument를 던짐
	void validate() const
	{
		if (interval.count() < 0 || timeout.count() < 0) {
			throw std::invalid_argument("heartbeat values must not be negative");
		}
		// interval이 0이면 하트비트가 돌지 않아 timeout 검사도 하지 않으므로 설정 실수로 봄
		if (interval.count() == 0 && timeout.count() != 0) {
			throw std::invalid_argument("heartbeat timeout needs a positive interval");
		}
		if (timeout.count() != 0 && timeout <= interval) {
			throw std::invalid_argument("heartbeat timeout must be 0 or longer than interval");
		}
	}
};
}
