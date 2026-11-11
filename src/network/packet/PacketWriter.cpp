#include "tcpengine/network/packet/PacketWriter.hpp"

#include "tcpengine/network/packet/Packet.hpp"

#include <limits>

namespace tcpengine
{
PacketWriter::PacketWriter(std::uint16_t id)
{
	m_buffer.resize(2);
	writeUInt16(id);
}

void PacketWriter::writeUInt16(std::uint16_t value)
{
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

std::optional<std::vector<std::uint8_t>> PacketWriter::finish()
{
	if (m_failed || m_buffer.size() > maxPacketSize) {
		return std::nullopt;
	}

	const auto size = static_cast<std::uint16_t>(m_buffer.size());
	m_buffer[0] = static_cast<std::uint8_t>(size & 0xFF);
	m_buffer[1] = static_cast<std::uint8_t>(size >> 8);
	return std::move(m_buffer);
}
}
