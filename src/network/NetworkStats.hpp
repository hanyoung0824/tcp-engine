#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace tcpengine
{
// 연결들이 함께 쌓는 송수신 통계. 횟수는 시작부터 누적이며 보고하는 쪽이 이전 값과의 차이로 구간 값을 구함
// 네트워크 스레드에서만 사용. 멀티스레드가 되면 atomic이나 스레드별 카운터로 바꿔야 함
struct NetworkStats
{
	std::uint64_t connectionsOpened = 0;
	std::uint64_t connectionsClosed = 0;

	std::uint64_t packetsIn = 0;
	std::uint64_t bytesIn = 0;
	// 보내기를 마친 것만 셈. 큐에 쌓인 것은 peakSendQueueBytes로 봄
	std::uint64_t packetsOut = 0;
	std::uint64_t bytesOut = 0;

	// 받는 쪽이 따라오지 못해 쓰기 큐 상한을 넘음
	std::uint64_t sendQueueResets = 0;
	// 하트비트 timeout 동안 아무것도 받지 못함
	std::uint64_t heartbeatResets = 0;
	// 초당 받을 수 있는 패킷 수를 넘음
	std::uint64_t receiveLimitResets = 0;

	// 보고 구간 동안 본 연결별 쓰기 큐 크기 중 가장 큰 값. 넣을 때와 보낼 때마다 확인하며 보고하는 쪽이 매번 0으로 되돌림
	std::size_t peakSendQueueBytes = 0;

	void recordSendQueue(std::size_t queuedBytes) { peakSendQueueBytes = std::max(peakSendQueueBytes, queuedBytes); }
};
}
