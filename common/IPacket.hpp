#pragma once

#include "PacketReader.hpp"
#include "PacketWriter.hpp"

#include <cstdint>
#include <memory>

class Session;

// 패킷 하나 (네트워크 계층의 패킷 틀). 실제 패킷 클래스는 응용 계층이 상속해 만든다.
//
// 보통 두 단계로 상속한다.
// 1. 공용 패킷 클래스 (예: 응용 프로젝트의 C2S_Chat): 패킷 id, 필드, read(), write()를 정한다.
//    보내는 쪽은 이 클래스로 패킷을 만들어 보낸다. 필드 순서가 이 한 곳에만 있으므로 보내는 쪽과 받는 쪽이 어긋나지 않는다.
// 2. 받는 쪽 패킷 클래스 (예: 응용 서버의 C2S_ChatHandler : C2S_Chat): handle()만 오버라이드해 받았을 때 할 일을 정하고,
//    앱의 registerPacket<클래스>()로 등록한다.
//
// 패킷이 올 때마다 PacketManager가 등록된 클래스의 객체를 새로 만들어 read() -> handle() 순서로 호출한다.
class IPacket
{
public:
	virtual ~IPacket() = default;

	// 이 패킷의 id. 어떤 id가 있는지는 응용 계층이 정한다 (예: 응용 프로젝트가 정의한 enum class PacketId).
	virtual std::uint16_t getPacketId() const = 0;

	// 본문(id 뒤)을 읽어 필드를 채운다. 받은 데이터는 믿을 수 없으므로 값도 검사한다.
	// 형식이 잘못됐으면 false를 반환한다 (세션이 연결을 끊는다).
	// 본문을 다 읽었는지(남은 바이트가 없는지)는 PacketManager가 검사한다.
	virtual bool read(PacketReader& reader) = 0;

	// 필드를 본문에 쓴다 (보낼 때 makePacket()이 호출한다).
	virtual void write(PacketWriter& writer) const = 0;

	// 받았을 때 할 일. session은 이 패킷을 보낸 상대다. 받는 쪽 패킷 클래스가 오버라이드한다.
	virtual void handle(const std::shared_ptr<Session>& session) = 0;
};
