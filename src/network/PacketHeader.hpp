#pragma once

#include <cstddef>
#include <cstdint>

namespace tcpengine
{
//   | size (2바이트) | id (2바이트) | body (size - 4 바이트) |
// size는 헤더를 포함한 전체 길이, 모든 정수는 little-endian
constexpr std::size_t packetHeaderSize = 4;

// 엔진 제어 패킷 id. 본문 첫 uint16이 ControlType이며 PacketManager가 처리하고 응용에는 전달하지 않음
constexpr std::uint16_t controlPacketId = 0;

enum class ControlType : std::uint16_t
{
	HeartbeatConfig = 1, // 서버 → 클라. 본문: interval(uint16 초), timeout(uint16 초)
	Ping = 2,            // 클라 → 서버
	Pong = 3,            // 서버 → 클라
};
}
