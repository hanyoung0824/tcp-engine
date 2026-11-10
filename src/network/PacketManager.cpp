#include "PacketManager.hpp"

#include "Logger.hpp"
#include "PacketHeader.hpp"
#include "tcpengine/network/packet/PacketWriter.hpp"

#include <chrono>
#include <limits>

namespace tcpengine
{
namespace
{
	std::shared_ptr<const Session::Buffer> makeControlPacket(ControlType type)
	{
		PacketWriter writer(controlPacketId);
		writer.writeUInt16(static_cast<std::uint16_t>(type));
		return std::make_shared<const Session::Buffer>(*writer.finish());
	}
}

std::shared_ptr<const Session::Buffer> PacketManager::makePingPacket()
{
	static const std::shared_ptr<const Session::Buffer> packet = makeControlPacket(ControlType::Ping);
	return packet;
}

std::shared_ptr<const Session::Buffer> PacketManager::makePongPacket()
{
	static const std::shared_ptr<const Session::Buffer> packet = makeControlPacket(ControlType::Pong);
	return packet;
}

std::shared_ptr<const Session::Buffer> PacketManager::makeHeartbeatConfigPacket(const HeartbeatSettings& heartbeat)
{
	constexpr auto maxSeconds = std::numeric_limits<std::uint16_t>::max();
	if (heartbeat.interval.count() > maxSeconds || heartbeat.timeout.count() > maxSeconds) {
		return nullptr;
	}

	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(ControlType::HeartbeatConfig));
	writer.writeUInt16(static_cast<std::uint16_t>(heartbeat.interval.count()));
	writer.writeUInt16(static_cast<std::uint16_t>(heartbeat.timeout.count()));
	return std::make_shared<const Session::Buffer>(*writer.finish());
}

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

void PacketManager::setHeartbeatConfigHandler(HeartbeatConfigHandler handler)
{
	m_heartbeatConfigHandler = std::move(handler);
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
			engineLogWarning("Invalid packet size ", size, " from ", session->getAddress());
			return std::nullopt;
		}

		if (rest.size() < size) {
			break;
		}

		PacketReader body(rest.subspan(packetHeaderSize, size - packetHeaderSize));

		if (id == controlPacketId) {
			if (!handleControlPacket(session, body)) {
				engineLogWarning("Invalid control packet from ", session->getAddress());
				return std::nullopt;
			}
			processed += size;
			continue;
		}

		const auto it = m_dispatchers.find(id);
		if (it == m_dispatchers.end()) {
			engineLogWarning("Unknown packet id ", id, " from ", session->getAddress());
			return std::nullopt;
		}

		if (!it->second(session, body)) {
			engineLogWarning("Invalid packet id ", id, " from ", session->getAddress());
			return std::nullopt;
		}

		processed += size;
	}
	return processed;
}

bool PacketManager::handleControlPacket(const std::shared_ptr<Session>& session, PacketReader& body) const
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
		session->send(makePongPacket());
		return true;

	case ControlType::Pong:
		return body.isEnd();

	case ControlType::HeartbeatConfig: {
		std::uint16_t interval = 0;
		std::uint16_t timeout = 0;
		if (!m_heartbeatConfigHandler || !body.readUInt16(interval) || !body.readUInt16(timeout) || !body.isEnd()) {
			return false;
		}

		const HeartbeatSettings heartbeat{ std::chrono::seconds(interval), std::chrono::seconds(timeout) };
		if (!isValid(heartbeat)) {
			return false;
		}
		m_heartbeatConfigHandler(session, heartbeat);
		return true;
	}
	}
	return false;
}
}
