#pragma once

#include <cstddef>
#include <cstdint>

namespace tcpengine
{
//   | size (2바이트) | id (2바이트) | body (size - 4 바이트) |
// size는 헤더를 포함한 전체 길이, 모든 정수는 little-endian
constexpr std::size_t packetHeaderSize = 4;

// 엔진 예약 id. 헤더만 있는 패킷이며 PacketManager가 받아서 버림
constexpr std::uint16_t heartbeatPacketId = 0;
}
