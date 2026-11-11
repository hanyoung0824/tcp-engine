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
	ServerHello = 1,     // 서버 → 클라. 접속 직후 한 번. 본문: 하트비트 interval(uint16 초), timeout(uint16 초)
	Ping = 2,            // 클라 → 서버
	Pong = 3,            // 서버 → 클라
	SessionStarted = 4,  // 서버 → 클라. 본문: 재접속 토큰(string)
	Resume = 5,          // 클라 → 서버. 본문: 재접속 토큰(string). 재접속 직후 보냄
	SessionResumed = 6,  // 서버 → 클라
	ResumeFailed = 7,    // 서버 → 클라. 클라이언트는 로그인부터 다시 함
};

// 재접속 토큰 길이 (16바이트를 16진수 문자열로)
constexpr std::size_t sessionTokenLength = 32;
}
