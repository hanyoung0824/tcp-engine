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

class PacketBase
{
public:
	virtual ~PacketBase() = default;

	virtual std::uint16_t getPacketId() const = 0;

	// 바이트를 필드로 읽음. 형식만 보며 값 규칙은 validate()에서 검사
	virtual bool read(PacketReader& reader) = 0;

	// 필드 값 규칙(길이, 범위 등). 받은 패킷은 이걸 통과해야 핸들러로 넘어감
	virtual bool validate() const { return true; }

	virtual void write(PacketWriter& writer) const = 0;

	// 헤더를 붙인 전송용 바이트. 최대 크기를 넘으면 nullptr
	std::shared_ptr<const PacketBuffer> serialize() const;
};

// 응용 패킷은 이걸 상속. Id는 정수 또는 정수 기반 enum
// 예: class Chat : public tcpengine::Packet<PacketId::Chat>
template <auto Id>
class Packet : public PacketBase
{
public:
	static constexpr std::uint16_t id = static_cast<std::uint16_t>(Id);
	static_assert(id >= firstAppPacketId, "packet id must be firstAppPacketId or greater");

	std::uint16_t getPacketId() const final { return id; }
};
}
