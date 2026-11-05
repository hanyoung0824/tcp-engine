#include "IPacket.hpp"

#include "Log.hpp"

#include <optional>

namespace tcpengine
{
std::shared_ptr<const Session::Buffer> IPacket::serialize() const
{
	PacketWriter writer(getPacketId());
	write(writer);

	std::optional<Session::Buffer> buffer = writer.finish();
	if (!buffer) {
		writeLog(LogLevel::Error, "Packet id ", getPacketId(), " is too large, dropped");
		return nullptr;
	}
	return std::make_shared<const Session::Buffer>(std::move(*buffer));
}
}
