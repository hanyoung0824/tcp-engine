#pragma once

#include "tcpengine/network/Connection.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

namespace tcpengine
{
// Transport가 연결 이벤트를 Server/Client에 알리는 통로. 프로토콜과 무관함
// 모든 연결이 공유하므로 shared_ptr<const ConnectionEventHandler>로 넘김
// 콜백은 모두 필수라 쓰는 쪽은 비었는지 확인하지 않음
struct ConnectionEventHandler
{
	using ConnectionCallback = std::function<void(const std::shared_ptr<Connection>& connection)>;

	// 처리한 바이트 수 반환. nullopt는 잘못된 데이터이며 연결이 닫힘
	using ReceiveHandler = std::function<std::optional<std::size_t>(
		const std::shared_ptr<Connection>& connection, std::span<const std::uint8_t> data)>;

	// 하나라도 비어 있으면 std::invalid_argument
	ConnectionEventHandler(ConnectionCallback onConnected, ReceiveHandler onReceive, ConnectionCallback onDisconnected)
		: onConnected(std::move(onConnected)),
		onReceive(std::move(onReceive)),
		onDisconnected(std::move(onDisconnected))
	{
		if (!this->onConnected || !this->onReceive || !this->onDisconnected) {
			throw std::invalid_argument("ConnectionEventHandler needs every callback");
		}
	}

	const ConnectionCallback onConnected;
	const ReceiveHandler onReceive;
	const ConnectionCallback onDisconnected;
};
}
