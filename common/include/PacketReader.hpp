#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace tcpengine
{
class PacketReader
{
public:
	explicit PacketReader(std::span<const std::uint8_t> data);

	bool readUInt16(std::uint16_t& out);
	bool readString(std::string& out);

	bool isEnd() const { return m_position == m_data.size(); }

private:
	std::size_t remaining() const { return m_data.size() - m_position; }

private:
	std::span<const std::uint8_t> m_data;
	std::size_t m_position = 0;
};
}
