#include "PacketManager.hpp"

#include "Log.hpp"
#include "PacketHeader.hpp"
#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include "Session.hpp"

namespace tcpengine
{
std::shared_ptr<const Session::Buffer> PacketManager::makeHeartbeatPacket()
{
	PacketWriter writer(heartbeatPacketId);
	return std::make_shared<const Session::Buffer>(*writer.finish());
}

bool PacketManager::addDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (id == heartbeatPacketId) {
		writeLog(LogLevel::Error, "Packet id ", id, " is reserved for heartbeat");
		return false;
	}

	if (!m_dispatchers.emplace(id, std::move(dispatcher)).second) {
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

		if (id == heartbeatPacketId) {
			if (size != packetHeaderSize) {
				writeLog(LogLevel::Warning, "Invalid heartbeat size ", size, " from ", session->getAddress());
				return std::nullopt;
			}
			processed += size;
			continue;
		}

		const auto it = m_dispatchers.find(id);
		if (it == m_dispatchers.end()) {
			writeLog(LogLevel::Warning, "Unknown packet id ", id, " from ", session->getAddress());
			return std::nullopt;
		}

		PacketReader body(rest.subspan(packetHeaderSize, size - packetHeaderSize));
		if (!it->second(session, body)) {
			writeLog(LogLevel::Warning, "Invalid packet id ", id, " from ", session->getAddress());
			return std::nullopt;
		}

		processed += size;
	}
	return processed;
}
}
