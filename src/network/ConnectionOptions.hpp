#pragma once

#include "NetworkStats.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>

namespace tcpengine
{
struct ConnectionOptions
{
	// close() 후 남은 데이터를 보내고 상대가 끊기를 기다리는 최대 시간. 0이면 바로 닫음
	std::chrono::seconds closeTimeout{ 0 };

	// 아직 보내지 못한 바이트가 이 값을 넘으면 받는 쪽이 따라오지 못하는 것으로 보고 바로 끊음
	// 없으면 제한하지 않음
	std::optional<std::size_t> sendQueueLimit;

	// 송수신 통계를 쌓을 곳. 같은 서버나 클라이언트의 연결이 함께 씀
	std::shared_ptr<NetworkStats> stats = std::make_shared<NetworkStats>();
};
}
