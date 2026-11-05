#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// 패킷 형식 (네트워크 계층). PacketWriter가 이 형식으로 만들고, PacketManager가 이 형식으로 자른다.
//
//   | size (2바이트) | id (2바이트) | body (size - 4 바이트) |
//
// - size: 헤더를 포함한 패킷 전체 길이. 이 값으로 패킷 하나의 끝을 안다.
// - id: 무슨 패킷인지. 어떤 id가 있는지는 응용 계층이 정한다 (예: 응용 프로젝트가 정의한 enum class PacketId).
// - 모든 정수는 little-endian. 문자열은 길이(2바이트) + 바이트들.
//
// Session은 이 형식을 모른다(바이트만 다룬다). 다른 형식이 필요하면 PacketManager 대신 다른 수신 콜백을 넘긴다.

// 헤더(size + id) 크기
constexpr std::size_t packetHeaderSize = 4;

// 패킷 하나의 최대 크기 (헤더 포함). 이보다 크다는 size가 오면 연결을 끊는다.
constexpr std::size_t maxPacketSize = 4096;

// 완성된 패킷(헤더 포함)을 담는 바이트 배열
using PacketBuffer = std::vector<std::uint8_t>;
