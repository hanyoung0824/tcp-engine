#include "PacketReader.hpp"

PacketReader::PacketReader(std::span<const std::uint8_t> data)
	: m_data(data)
{
}

bool PacketReader::readUInt16(std::uint16_t& out)
{
	if (remaining() < 2) {
		return false;
	}

	// little-endian: 낮은 바이트가 먼저 온다.
	out = static_cast<std::uint16_t>(m_data[m_position] | (m_data[m_position + 1] << 8));
	m_position += 2;
	return true;
}

bool PacketReader::readString(std::string& out)
{
	std::uint16_t length = 0;
	if (!readUInt16(length) || remaining() < length) {
		return false;
	}

	const auto begin = m_data.begin() + m_position;
	out.assign(begin, begin + length);
	m_position += length;
	return true;
}
