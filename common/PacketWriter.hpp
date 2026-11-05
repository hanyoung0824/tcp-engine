#pragma once

#include "PacketHeader.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

// 보낼 패킷을 만드는 역할. size(2) + id(2) + 본문 순서로 쓴다.
//
//   PacketWriter writer(PacketId::C2S_Chat);   // id는 정수 또는 enum class
//   writer.writeString("hello");
//   std::optional<PacketBuffer> packet = writer.finish(); // size가 채워진 완성 패킷
class PacketWriter
{
public:
	// size 자리(2바이트)를 비워 두고 id를 쓴다.
	explicit PacketWriter(std::uint16_t id);

	// enum class로 된 id도 받는다 (예: PacketId::C2S_Chat).
	template <typename Id>
		requires std::is_enum_v<Id>
	explicit PacketWriter(Id id)
		: PacketWriter(static_cast<std::uint16_t>(id))
	{
	}

	void writeUInt16(std::uint16_t value);

	// 길이(2바이트) + 바이트들. 65535바이트보다 길면 실패로 표시된다.
	void writeString(std::string_view value);

	// size를 채워 완성된 패킷을 돌려준다.
	// maxPacketSize를 넘었거나 쓰는 중에 실패했으면 std::nullopt.
	std::optional<PacketBuffer> finish();

private:
	PacketBuffer m_buffer; // size + 지금까지 쓴 데이터
	bool m_failed = false; // 쓰는 중에 실패했는지
};
