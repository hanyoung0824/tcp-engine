#pragma once

#include "tcpengine/network/Session.hpp"
#include "tcpengine/network/packet/IPacket.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <functional>
#include <memory>
#include <type_traits>

namespace tcpengine::detail
{
// 본문을 읽어 핸들러를 호출. 본문이 잘못되면 false
using PacketDispatcher = std::function<bool(const std::shared_ptr<Session>& session, PacketReader& body)>;

template <typename T, typename Handler>
PacketDispatcher makePacketDispatcher(Handler handler)
{
	static_assert(std::is_base_of_v<IPacket, T>, "T must derive from IPacket");
	static_assert(std::is_invocable_v<Handler&, const std::shared_ptr<Session>&, const T&>,
		"Handler must be callable as handler(const std::shared_ptr<Session>&, const T&)");

	return [handler = std::move(handler)](const std::shared_ptr<Session>& session, PacketReader& body) mutable {
		T packet;
		if (!packet.read(body) || !body.isEnd()) {
			return false;
		}
		handler(session, packet);
		return true;
	};
}
}
