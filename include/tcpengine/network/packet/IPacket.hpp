#pragma once

#include "tcpengine/network/packet/PacketReader.hpp"
#include "tcpengine/network/packet/PacketWriter.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tcpengine
{
// 헤더를 포함한 패킷 최대 크기
constexpr std::size_t maxPacketSize = 4096;

// 0은 엔진 제어 패킷(하트비트 등)용. 응용 패킷 id는 이 값부터 사용
constexpr std::uint16_t firstAppPacketId = 1;

using PacketBuffer = std::vector<std::uint8_t>;

class IPacket
{
public:
	virtual ~IPacket() = default;

	virtual std::uint16_t getPacketId() const = 0;

	virtual bool read(PacketReader& reader) = 0;

	virtual void write(PacketWriter& writer) const = 0;

	// 헤더를 붙인 전송용 바이트. 최대 크기를 넘으면 nullptr
	std::shared_ptr<const PacketBuffer> serialize() const;
};
}
