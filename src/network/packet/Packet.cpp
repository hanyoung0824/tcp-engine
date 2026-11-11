#include "tcpengine/network/packet/Packet.hpp"

#include <optional>

namespace tcpengine
{
std::shared_ptr<const PacketBuffer> PacketBase::serialize() const
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
