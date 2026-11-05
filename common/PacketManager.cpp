#include "PacketManager.hpp"

#include "Log.hpp"
#include "PacketHeader.hpp"
#include "PacketReader.hpp"
#include "Session.hpp"

bool PacketManager::addFactory(std::uint16_t id, PacketFactory factory)
{
	if (!m_factories.emplace(id, std::move(factory)).second) {
		writeLog(LogLevel::Error, "Packet id ", id, " is already registered");
		return false;
	}
	return true;
}

std::optional<std::size_t> PacketManager::onReceive(const std::shared_ptr<Session>& session,
	std::span<const std::uint8_t> data) const
{
	std::size_t processed = 0;
	while (!session->isClosed()) {
		const std::span<const std::uint8_t> rest = data.subspan(processed);

		if (rest.size() < packetHeaderSize) {
			break;
		}

		PacketReader header(rest.first(packetHeaderSize));
		std::uint16_t size = 0;
		std::uint16_t id = 0;
		header.readUInt16(size);
		header.readUInt16(id);

		if (size < packetHeaderSize || size > maxPacketSize) {
			writeLog(LogLevel::Warning, "Invalid packet size ", size, " from ", session->getAddress());
			return std::nullopt;
		}

		if (rest.size() < size) {
			break;
		}

		const auto it = m_factories.find(id);
		if (it == m_factories.end()) {
			writeLog(LogLevel::Warning, "Unknown packet id ", id, " from ", session->getAddress());
			return std::nullopt;
		}

		std::unique_ptr<IPacket> packet = it->second();
		PacketReader body(rest.subspan(packetHeaderSize, size - packetHeaderSize));
		if (!packet->read(body) || !body.isEnd()) {
			writeLog(LogLevel::Warning, "Invalid packet id ", id, " from ", session->getAddress());
			return std::nullopt;
		}

		packet->handle(session);

		processed += size;
	}
	return processed;
}
