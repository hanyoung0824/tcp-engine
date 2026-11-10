#include "tcpengine/network/packet/IPacket.hpp"

#include <optional>

namespace tcpengine
{
std::shared_ptr<const PacketBuffer> IPacket::serialize() const
{
	PacketWriter writer(getPacketId());
	write(writer);

	std::optional<PacketBuffer> buffer = writer.finish();
	if (!buffer) {
		return nullptr;
	}
	return std::make_shared<const PacketBuffer>(std::move(*buffer));
}
}
