#pragma once

#include "IPacket.hpp"
#include "Session.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>

namespace tcpengine
{
class PacketManager
{
public:
	static std::shared_ptr<const Session::Buffer> makeHeartbeatPacket();

	PacketManager() = default;

	PacketManager(const PacketManager&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;

	bool addDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

	std::optional<std::size_t> onReceive(const std::shared_ptr<Session>& session,
		std::span<const std::uint8_t> data) const;

private:
	std::unordered_map<std::uint16_t, detail::PacketDispatcher> m_dispatchers;
};
}
