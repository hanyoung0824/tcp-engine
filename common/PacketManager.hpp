#pragma once

#include "IPacket.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <unordered_map>

class Session;

// 받은 바이트를 패킷 형식(PacketHeader.hpp)으로 잘라 패킷 객체로 바꾸는 역할 (네트워크 계층).
// 세션이 받은 바이트를 size + id 헤더로 잘라, id로 등록된 패킷 클래스의 객체를 새로 만들고 read() -> handle()을 호출한다.
// 형식이 잘못됐거나, 등록되지 않은 id이거나, 읽기에 실패하면 nullopt를 반환한다 (세션이 연결을 끊는다).
//
// Server / Client(프로그램의 뼈대)가 멤버로 하나씩 가진다. 응용은 이 클래스를 직접 만지지 않고
// 앱의 registerPacket<T>()로 등록한다 (앱 생성자에서, run() 전에).
// 등록 후에는 읽기만 하므로 여러 스레드에서 onReceive()를 불러도 잠금이 필요 없다.
//
// Session은 이 클래스를 모른다(바이트만 다룬다). Server/Client가 TcpServer/TcpClient에 넘기는 수신 콜백이 onReceive()를 호출한다.
class PacketManager
{
public:
	// 패킷 객체를 새로 만드는 함수
	using PacketFactory = std::function<std::unique_ptr<IPacket>()>;

public:
	PacketManager() = default;

	PacketManager(const PacketManager&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;

	// 받은 바이트에서 완성된 패킷을 모두 처리하고, 처리한 바이트 수를 반환한다 (Session::ReceiveHandler 형식).
	// 끝에 덜 온 패킷이 있으면 그 앞까지만 처리한다 (나머지는 Session이 보관했다가 다음에 다시 넘긴다).
	std::optional<std::size_t> onReceive(const std::shared_ptr<Session>& session,
		std::span<const std::uint8_t> data) const;

	// 패킷 클래스 T를 T의 getPacketId()로 등록한다. 이후 그 id의 패킷이 올 때마다 T 객체를 새로 만든다.
	// 같은 id가 이미 등록돼 있으면 등록하지 않고 false를 반환한다 (두 클래스에 같은 id를 준 실수).
	// 패킷을 처리하기 시작하기 전(세션이 생기기 전)에만 호출한다.
	template <typename T>
	bool addPacket()
	{
		static_assert(std::is_base_of_v<IPacket, T>, "T must derive from IPacket");

		// id를 알려고 객체를 하나 만들어 본다 (등록할 때 한 번뿐이다).
		const std::uint16_t id = T().getPacketId();
		return addFactory(id, [] { return std::make_unique<T>(); });
	}

private:
	bool addFactory(std::uint16_t id, PacketFactory factory);

private:
	std::unordered_map<std::uint16_t, PacketFactory> m_factories; // id -> 패킷 객체를 만드는 함수
};
