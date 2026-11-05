#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace tcpengine
{
class PacketWriter
{
public:
	explicit PacketWriter(std::uint16_t id);

	void writeUInt16(std::uint16_t value);

	void writeString(std::string_view value);

	// 헤더의 size를 채워 반환. 실패했거나 최대 크기를 넘으면 nullopt
	std::optional<std::vector<std::uint8_t>> finish();

private:
	std::vector<std::uint8_t> m_buffer;
	bool m_failed = false;
};
}
