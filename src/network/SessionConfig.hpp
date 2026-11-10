#pragma once

#include "tcpengine/network/HeartbeatSettings.hpp"
#include "tcpengine/network/Session.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace tcpengine
{
// Transport가 세션 이벤트를 Server/Client에 알리는 통로. 프로토콜과 무관함
struct SessionEventHandler
{
	using SessionCallback = std::function<void(const std::shared_ptr<Session>& session)>;

	// 처리한 바이트 수 반환. nullopt는 잘못된 데이터이며 세션이 닫힘
	using ReceiveHandler = std::function<std::optional<std::size_t>(
		const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data)>;

	SessionCallback onConnected;
	ReceiveHandler onReceive;
	SessionCallback onDisconnected;
};

// 모든 세션이 공유하는 읽기 전용 설정. shared_ptr<const SessionConfig>로 넘김
struct SessionConfig
{
	SessionEventHandler handler;
	// 세션 시작 시 바로 적용할 하트비트. pingData가 없으면 timeout 검사만 함
	HeartbeatSettings heartbeat;
	std::shared_ptr<const Session::Buffer> pingData;
	// 세션 시작 시 가장 먼저 보낼 바이트 (서버의 하트비트 설정 패킷)
	std::shared_ptr<const Session::Buffer> greeting;
};
}
