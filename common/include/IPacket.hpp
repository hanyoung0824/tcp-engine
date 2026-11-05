#pragma once

#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include "Session.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>

namespace tcpengine
{
// 헤더를 포함한 패킷 최대 크기
constexpr std::size_t maxPacketSize = 4096;

class IPacket
{
public:
	virtual ~IPacket() = default;

	virtual std::uint16_t getPacketId() const = 0;

	virtual bool read(PacketReader& reader) = 0;

	virtual void write(PacketWriter& writer) const = 0;

	// 헤더를 붙인 전송용 바이트. 최대 크기를 넘으면 nullptr
	std::shared_ptr<const Session::Buffer> serialize() const;
};

// 방향 표시용. Server는 ClientToServerPacket, Client는 ServerToClientPacket만 받음
class ClientToServerPacket : public IPacket
{
};

class ServerToClientPacket : public IPacket
{
};

namespace detail
{
	// 본문을 읽어 핸들러를 호출. 본문이 잘못되면 false
	using PacketDispatcher = std::function<bool(const std::shared_ptr<Session>& session, PacketReader& body)>;

	template <typename T, typename Handler>
	PacketDispatcher makePacketDispatcher(Handler handler)
	{
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
}
