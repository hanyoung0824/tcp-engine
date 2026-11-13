#include "PacketManager.hpp"

#include "ControlPackets.hpp"
#include "EngineLog.hpp"

#include <exception>

namespace tcpengine
{
bool PacketManager::addDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (id < firstAppPacketId) {
		engineLogError("Packet id ", id, " is reserved for the engine, app ids start at ", firstAppPacketId);
		return false;
	}

	if (!m_dispatchers.emplace(id, std::move(dispatcher)).second) {
		engineLogError("Packet id ", id, " is already registered");
		return false;
	}
	return true;
}

void PacketManager::setControlHandler(ControlType type, ControlHandler handler)
{
	m_controlHandlers[static_cast<std::uint16_t>(type)] = std::move(handler);
}

void PacketManager::setPacketFilter(PacketFilter filter)
{
	m_packetFilter = std::move(filter);
}

std::optional<std::size_t> PacketManager::onReceive(const std::shared_ptr<Connection>& connection,
	std::span<const std::uint8_t> data) const
{
	std::size_t processed = 0;
	while (!connection->isClosed()) {
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
			engineLogWarning("Invalid packet size ", size, " from ", connection->getAddress());
			return std::nullopt;
		}

		if (rest.size() < size) {
			break;
		}

		if (m_packetFilter && !m_packetFilter(connection, id)) {
			return std::nullopt;
		}

		PacketReader body(rest.subspan(packetHeaderSize, size - packetHeaderSize));

		// 핸들러 예외가 io_context 밖으로 나가면 서버 전체가 죽으므로 이 연결만 끊음
		try {
			if (!dispatch(connection, id, body)) {
				return std::nullopt;
			}
		}
		catch (const std::exception& e) {
			engineLogError("Exception while handling packet id ", id, " from ", connection->getAddress(), ": ", e.what());
			return std::nullopt;
		}
		catch (...) {
			engineLogError("Unknown exception while handling packet id ", id, " from ", connection->getAddress());
			return std::nullopt;
		}

		processed += size;
	}
	return processed;
}

bool PacketManager::dispatch(const std::shared_ptr<Connection>& connection, std::uint16_t id, PacketReader& body) const
{
	if (id == controlPacketId) {
		if (!handleControlPacket(connection, body)) {
			engineLogWarning("Invalid control packet from ", connection->getAddress());
			return false;
		}
		return true;
	}

	const auto it = m_dispatchers.find(id);
	if (it == m_dispatchers.end()) {
		engineLogWarning("Unknown packet id ", id, " from ", connection->getAddress());
		return false;
	}

	if (!it->second(connection, body)) {
		engineLogWarning("Invalid packet id ", id, " from ", connection->getAddress());
		return false;
	}
	return true;
}

bool PacketManager::handleControlPacket(const std::shared_ptr<Connection>& connection, PacketReader& body) const
{
	std::uint16_t type = 0;
	if (!body.readUInt16(type)) {
		return false;
	}

	switch (static_cast<ControlType>(type)) {
	case ControlType::Ping:
		if (!body.isEnd()) {
			return false;
		}
		connection->send(control::pong());
		return true;

	case ControlType::Pong:
		return body.isEnd();

	default:
		break;
	}

	const auto it = m_controlHandlers.find(type);
	return it != m_controlHandlers.end() && it->second(connection, body);
}
}
