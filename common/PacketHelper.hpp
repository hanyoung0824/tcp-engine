#pragma once

#include "IPacket.hpp"
#include "Log.hpp"
#include "PacketHeader.hpp"
#include "PacketWriter.hpp"
#include "Session.hpp"

#include <memory>
#include <optional>

// 패킷(IPacket)을 보내기 위한 도우미.

// packet으로 완성된 바이트(헤더 포함)를 만든다. 여러 세션에 같은 패킷을 보낼 때(broadcast) 이 결과를 공유한다.
// 최대 크기를 넘으면 로그를 남기고 nullptr를 반환한다.
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

// packet을 session에 보낸다. 패킷을 만들지 못했으면 false를 반환한다.
// Session::send()와 같이 io_context.run()을 실행하는 스레드에서 호출한다.
inline bool sendPacket(Session& session, const IPacket& packet)
{
	std::shared_ptr<const PacketBuffer> buffer = makePacket(packet);
	if (!buffer) {
		return false;
	}
	session.send(std::move(buffer));
	return true;
}
