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

class PacketManager
{
public:
	using PacketFactory = std::function<std::unique_ptr<IPacket>()>;

public:
	PacketManager() = default;

	PacketManager(const PacketManager&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;

	std::optional<std::size_t> onReceive(const std::shared_ptr<Session>& session,
		std::span<const std::uint8_t> data) const;

	template <typename T>
	bool addPacket()
	{
		static_assert(std::is_base_of_v<IPacket, T>, "T must derive from IPacket");

		const std::uint16_t id = T().getPacketId();
		return addFactory(id, [] { return std::make_unique<T>(); });
	}

private:
	bool addFactory(std::uint16_t id, PacketFactory factory);

private:
	std::unordered_map<std::uint16_t, PacketFactory> m_factories;
};
