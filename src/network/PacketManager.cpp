#include "PacketManager.hpp"

#include "EngineLog.hpp"
#include "tcpengine/network/packet/PacketWriter.hpp"

#include <chrono>
#include <exception>
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

std::shared_ptr<const Connection::Buffer> PacketManager::makeClientHelloPacket(const ClientHello& hello)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(ControlType::ClientHello));
	writer.writeUInt16(hello.engineVersion);
	writer.writeUInt16(hello.appVersion);
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::optional<ClientHello> PacketManager::readClientHello(PacketReader& body)
{
	ClientHello hello;
	if (!body.readUInt16(hello.engineVersion) || !body.readUInt16(hello.appVersion) || !body.isEnd()) {
		return std::nullopt;
	}
	return hello;
}

std::shared_ptr<const Connection::Buffer> PacketManager::makeDisconnectPacket(DisconnectReason reason)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(ControlType::Disconnect));
	writer.writeUInt16(static_cast<std::uint16_t>(reason));
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::optional<DisconnectReason> PacketManager::readDisconnect(PacketReader& body)
{
	std::uint16_t reason = 0;
	if (!body.readUInt16(reason) || !body.isEnd()) {
		return std::nullopt;
	}
	return static_cast<DisconnectReason>(reason);
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
