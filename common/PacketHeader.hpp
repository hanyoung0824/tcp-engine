#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

//   | size (2바이트) | id (2바이트) | body (size - 4 바이트) |
// size는 헤더를 포함한 전체 길이이고, 모든 정수는 little-endian이다.
constexpr std::size_t packetHeaderSize = 4;

constexpr std::size_t maxPacketSize = 4096;

using PacketBuffer = std::vector<std::uint8_t>;
