#include "PacketManager.hpp"

#include "Logger.hpp"
#include "tcpengine/network/packet/PacketWriter.hpp"

#include <chrono>
#include <limits>
#include <stdexcept>

namespace tcpengine
{
std::shared_ptr<const Connection::Buffer> PacketManager::makeControlPacket(ControlType type)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(type));
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::shared_ptr<const Connection::Buffer> PacketManager::makeControlPacket(ControlType type, std::string_view text)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(type));
	writer.writeString(text);
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::shared_ptr<const Connection::Buffer> PacketManager::makePingPacket()
{
	static const std::shared_ptr<const Connection::Buffer> packet = makeControlPacket(ControlType::Ping);
	return packet;
}

std::shared_ptr<const Connection::Buffer> PacketManager::makePongPacket()
{
	static const std::shared_ptr<const Connection::Buffer> packet = makeControlPacket(ControlType::Pong);
	return packet;
}

std::shared_ptr<const Connection::Buffer> PacketManager::makeServerHelloPacket(const ServerHello& hello)
{
	const HeartbeatSettings& heartbeat = hello.heartbeat;
	constexpr auto maxSeconds = std::numeric_limits<std::uint16_t>::max();
	if (heartbeat.interval.count() > maxSeconds || heartbeat.timeout.count() > maxSeconds) {
		return nullptr;
	}

	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(ControlType::ServerHello));
	writer.writeUInt16(static_cast<std::uint16_t>(heartbeat.interval.count()));
	writer.writeUInt16(static_cast<std::uint16_t>(heartbeat.timeout.count()));
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::optional<ServerHello> PacketManager::readServerHello(PacketReader& body)
{
	std::uint16_t interval = 0;
	std::uint16_t timeout = 0;
	if (!body.readUInt16(interval) || !body.readUInt16(timeout) || !body.isEnd()) {
		return std::nullopt;
	}

	ServerHello hello;
	hello.heartbeat = HeartbeatSettings{ std::chrono::seconds(interval), std::chrono::seconds(timeout) };
	try {
		hello.heartbeat.validate();
	}
	catch (const std::invalid_argument&) {
		return std::nullopt;
	}
	return hello;
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

void PacketManager::setControlHandler(ControlType type, ControlHandler handler)
{
	m_controlHandlers[static_cast<std::uint16_t>(type)] = std::move(handler);
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

		PacketReader body(rest.subspan(packetHeaderSize, size - packetHeaderSize));

		if (id == controlPacketId) {
			if (!handleControlPacket(connection, body)) {
				engineLogWarning("Invalid control packet from ", connection->getAddress());
				return std::nullopt;
			}
			processed += size;
			continue;
		}

		const auto it = m_dispatchers.find(id);
		if (it == m_dispatchers.end()) {
			engineLogWarning("Unknown packet id ", id, " from ", connection->getAddress());
			return std::nullopt;
		}

		if (!it->second(connection, body)) {
			engineLogWarning("Invalid packet id ", id, " from ", connection->getAddress());
			return std::nullopt;
		}

		processed += size;
	}
	return processed;
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
		connection->send(makePongPacket());
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
