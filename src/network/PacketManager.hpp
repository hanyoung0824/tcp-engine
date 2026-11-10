#pragma once

#include "tcpengine/network/HeartbeatSettings.hpp"
#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Session.hpp"
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
class PacketManager
{
public:
	using HeartbeatConfigHandler = std::function<void(const std::shared_ptr<Session>& session, const HeartbeatSettings& heartbeat)>;

	static std::shared_ptr<const Session::Buffer> makePingPacket();

	static std::shared_ptr<const Session::Buffer> makePongPacket();

	// interval과 timeout을 uint16 초로 보내므로 65535초를 넘으면 nullptr
	static std::shared_ptr<const Session::Buffer> makeHeartbeatConfigPacket(const HeartbeatSettings& heartbeat);

	PacketManager() = default;

	PacketManager(const PacketManager&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;

	bool addDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

	// 서버가 보낸 하트비트 설정을 받을 곳. 설정하지 않으면 그 패킷은 잘못된 패킷으로 처리함
	void setHeartbeatConfigHandler(HeartbeatConfigHandler handler);

	std::optional<std::size_t> onReceive(const std::shared_ptr<Session>& session,
		std::span<const std::uint8_t> data) const;

private:
	bool handleControlPacket(const std::shared_ptr<Session>& session, PacketReader& body) const;

private:
	std::unordered_map<std::uint16_t, detail::PacketDispatcher> m_dispatchers;
	HeartbeatConfigHandler m_heartbeatConfigHandler;
};
}
