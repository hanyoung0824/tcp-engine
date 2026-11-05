#include "PacketWriter.hpp"

#include <limits>

PacketWriter::PacketWriter(std::uint16_t id)
{
	// size는 finish()에서 채운다. 우선 0으로 자리만 잡는다.
	m_buffer.resize(2);
	writeUInt16(id);
}

void PacketWriter::writeUInt16(std::uint16_t value)
{
	// little-endian: 낮은 바이트부터 쓴다. CPU와 상관없이 항상 같은 순서가 된다.
	m_buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
	m_buffer.push_back(static_cast<std::uint8_t>(value >> 8));
}

void PacketWriter::writeString(std::string_view value)
{
	if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
		m_failed = true;
		return;
	}

	writeUInt16(static_cast<std::uint16_t>(value.size()));
	m_buffer.insert(m_buffer.end(), value.begin(), value.end());
}

std::optional<PacketBuffer> PacketWriter::finish()
{
	if (m_failed || m_buffer.size() > maxPacketSize) {
		return std::nullopt;
	}

	const auto size = static_cast<std::uint16_t>(m_buffer.size());
	m_buffer[0] = static_cast<std::uint8_t>(size & 0xFF);
	m_buffer[1] = static_cast<std::uint8_t>(size >> 8);
	return std::move(m_buffer);
}
