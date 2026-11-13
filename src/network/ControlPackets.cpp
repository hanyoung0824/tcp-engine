#include "ControlPackets.hpp"

#include "tcpengine/network/packet/PacketWriter.hpp"

#include <chrono>
#include <limits>
#include <stdexcept>

namespace tcpengine::control
{
std::shared_ptr<const Connection::Buffer> make(ControlType type)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(type));
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::shared_ptr<const Connection::Buffer> make(ControlType type, std::string_view text)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(type));
	writer.writeString(text);
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::shared_ptr<const Connection::Buffer> ping()
{
	static const std::shared_ptr<const Connection::Buffer> packet = make(ControlType::Ping);
	return packet;
}

std::shared_ptr<const Connection::Buffer> pong()
{
	static const std::shared_ptr<const Connection::Buffer> packet = make(ControlType::Pong);
	return packet;
}

std::shared_ptr<const Connection::Buffer> serverHello(const ServerHello& hello)
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

std::optional<ServerHello> readServerHello(PacketReader& body)
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

std::shared_ptr<const Connection::Buffer> clientHello(const ClientHello& hello)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(ControlType::ClientHello));
	writer.writeUInt16(hello.engineVersion);
	writer.writeUInt16(hello.appVersion);
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::optional<ClientHello> readClientHello(PacketReader& body)
{
	ClientHello hello;
	if (!body.readUInt16(hello.engineVersion) || !body.readUInt16(hello.appVersion) || !body.isEnd()) {
		return std::nullopt;
	}
	return hello;
}

std::shared_ptr<const Connection::Buffer> disconnect(DisconnectReason reason)
{
	PacketWriter writer(controlPacketId);
	writer.writeUInt16(static_cast<std::uint16_t>(ControlType::Disconnect));
	writer.writeUInt16(static_cast<std::uint16_t>(reason));
	return std::make_shared<const Connection::Buffer>(*writer.finish());
}

std::optional<DisconnectReason> readDisconnect(PacketReader& body)
{
	std::uint16_t reason = 0;
	if (!body.readUInt16(reason) || !body.isEnd()) {
		return std::nullopt;
	}
	return static_cast<DisconnectReason>(reason);
}
}
