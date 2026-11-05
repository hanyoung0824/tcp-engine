#pragma once

#include "IPacket.hpp"
#include "Log.hpp"
#include "PacketHeader.hpp"
#include "PacketWriter.hpp"
#include "Session.hpp"

#include <memory>
#include <optional>

inline std::shared_ptr<const PacketBuffer> makePacket(const IPacket& packet)
{
	PacketWriter writer(packet.getPacketId());
	packet.write(writer);

	std::optional<PacketBuffer> buffer = writer.finish();
	if (!buffer) {
		writeLog(LogLevel::Error, "Packet id ", packet.getPacketId(), " is too large, dropped");
		return nullptr;
	}
	return std::make_shared<const PacketBuffer>(std::move(*buffer));
}

inline bool sendPacket(Session& session, const IPacket& packet)
{
	std::shared_ptr<const PacketBuffer> buffer = makePacket(packet);
	if (!buffer) {
		return false;
	}
	session.send(std::move(buffer));
	return true;
}
