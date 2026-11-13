#pragma once

#include "PacketHeader.hpp"
#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>

namespace tcpengine
{
// 받은 바이트를 패킷으로 잘라 제어 패킷 핸들러나 응용 핸들러로 보냄. 제어 패킷 형식은 ControlPackets.hpp
class PacketManager
{
public:
	// 본문(ControlType 다음부터)을 읽어 처리. 본문이 잘못되면 false
	using ControlHandler = std::function<bool(const std::shared_ptr<Connection>& connection, PacketReader& body)>;

	// 패킷 하나를 처리하기 전에 부름. false면 그 패킷을 처리하지 않고 연결을 끊음
	using PacketFilter = std::function<bool(const std::shared_ptr<Connection>& connection, std::uint16_t packetId)>;

	PacketManager() = default;

	PacketManager(const PacketManager&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;

	bool addDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

	// 엔진이 직접 처리하지 않는 제어 패킷(세션 등)을 받을 곳. 설정하지 않은 종류는 잘못된 패킷으로 처리함
	void setControlHandler(ControlType type, ControlHandler handler);

	void setPacketFilter(PacketFilter filter);

	std::optional<std::size_t> onReceive(const std::shared_ptr<Connection>& connection,
		std::span<const std::uint8_t> data) const;

private:
	// 패킷 하나를 핸들러로 넘김. 잘못된 패킷이면 로그를 남기고 false
	bool dispatch(const std::shared_ptr<Connection>& connection, std::uint16_t id, PacketReader& body) const;

	bool handleControlPacket(const std::shared_ptr<Connection>& connection, PacketReader& body) const;

private:
	std::unordered_map<std::uint16_t, detail::PacketDispatcher> m_dispatchers;
	std::unordered_map<std::uint16_t, ControlHandler> m_controlHandlers;
	PacketFilter m_packetFilter;
};
}
