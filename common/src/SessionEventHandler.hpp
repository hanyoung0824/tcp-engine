#pragma once

#include "Session.hpp"

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
}
