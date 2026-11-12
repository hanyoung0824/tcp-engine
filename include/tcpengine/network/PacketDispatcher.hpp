#pragma once

#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/packet/Packet.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <functional>
#include <memory>
#include <type_traits>

namespace tcpengine::detail
{
// 본문을 읽고 검사를 통과하면 핸들러를 호출. 형식이나 값이 잘못되면 false
// Target은 핸들러가 받는 대상 (Connection 또는 서버의 Session)
template <typename Target>
using TargetDispatcher = std::function<bool(const std::shared_ptr<Target>& target, PacketReader& body)>;

using PacketDispatcher = TargetDispatcher<Connection>;

template <typename T, typename Target, typename Handler>
TargetDispatcher<Target> makePacketDispatcher(Handler handler)
{
	static_assert(std::is_base_of_v<PacketBase, T>, "T must derive from PacketBase");
	static_assert(std::is_invocable_v<Handler&, const std::shared_ptr<Target>&, const T&>,
		"Handler must be callable as handler(const std::shared_ptr<Target>&, const T&)");

	return [handler = std::move(handler)](const std::shared_ptr<Target>& target, PacketReader& body) mutable {
		T packet;
		if (!packet.read(body) || !body.isEnd() || !packet.validate()) {
			return false;
		}
		handler(target, packet);
		return true;
	};
}
}
