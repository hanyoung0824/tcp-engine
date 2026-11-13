#pragma once

#include <cstdint>
#include <string_view>

namespace tcpengine
{
// 서버가 연결을 끊기 전에 클라이언트에 알려 주는 이유
// 하트비트, 수신 제한, 쓰기 큐 상한처럼 응답이 없거나 악성인 상대는 알리지 않고 바로 끊음
enum class DisconnectReason : std::uint16_t
{
	ServerShutdown = 1,   // 서버 종료
	Kicked = 2,           // 응용이 세션을 끝냄 (강퇴 등)
	LoginTimeout = 3,     // 접속 후 정해진 시간 안에 로그인하지 않음
	VersionMismatch = 4,  // 프로토콜 버전이 다름
	Replaced = 5,         // 같은 세션으로 다른 연결이 재접속함
	ServerFull = 6,       // 최대 접속 수에 도달
};

// 같은 서버에 다시 접속해 볼 만한 이유인지. 강퇴나 버전 불일치는 다시 접속해도 같은 결과
constexpr bool canReconnectAfter(DisconnectReason reason)
{
	return reason == DisconnectReason::ServerShutdown || reason == DisconnectReason::ServerFull;
}

constexpr std::string_view toString(DisconnectReason reason)
{
	switch (reason) {
	case DisconnectReason::ServerShutdown: return "server shutdown";
	case DisconnectReason::Kicked: return "kicked";
	case DisconnectReason::LoginTimeout: return "login timeout";
	case DisconnectReason::VersionMismatch: return "protocol version mismatch";
	case DisconnectReason::Replaced: return "replaced by another connection";
	case DisconnectReason::ServerFull: return "server full";
	}
	return "unknown";
}
}
