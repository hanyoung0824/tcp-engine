#pragma once

#include "PacketHeader.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

class PacketWriter
{
public:
	explicit PacketWriter(std::uint16_t id);

	template <typename Id>
		requires std::is_enum_v<Id>
	explicit PacketWriter(Id id)
		: PacketWriter(static_cast<std::uint16_t>(id))
	{
	}

	void writeUInt16(std::uint16_t value);

	void writeString(std::string_view value);

	std::optional<PacketBuffer> finish();

private:
	PacketBuffer m_buffer;
	bool m_failed = false;
};
