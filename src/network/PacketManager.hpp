#pragma once

#include "PacketHeader.hpp"
#include "tcpengine/network/HeartbeatSettings.hpp"
#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>

namespace tcpengine
{
// 서버가 접속 직후 클라이언트에 한 번 보내는 정보. 프로토콜 버전 등이 생기면 여기에 더함
struct ServerHello
{
	HeartbeatSettings heartbeat;
};

class PacketManager
{
public:
	// 본문(ControlType 다음부터)을 읽어 처리. 본문이 잘못되면 false
	using ControlHandler = std::function<bool(const std::shared_ptr<Connection>& connection, PacketReader& body)>;

	// 패킷 하나를 처리하기 전에 부름. false면 그 패킷을 처리하지 않고 연결을 끊음
	using PacketFilter = std::function<bool(const std::shared_ptr<Connection>& connection)>;

	static std::shared_ptr<const Connection::Buffer> makeControlPacket(ControlType type);

	static std::shared_ptr<const Connection::Buffer> makeControlPacket(ControlType type, std::string_view text);

	static std::shared_ptr<const Connection::Buffer> makePingPacket();

	static std::shared_ptr<const Connection::Buffer> makePongPacket();

	// 하트비트 interval과 timeout을 uint16 초로 보내므로 65535초를 넘으면 nullptr
	static std::shared_ptr<const Connection::Buffer> makeServerHelloPacket(const ServerHello& hello);

	// ServerHello 본문(ControlType 다음부터)을 읽음. 형식이나 값이 틀리면 nullopt
	static std::optional<ServerHello> readServerHello(PacketReader& body);

	PacketManager() = default;

	PacketManager(const PacketManager&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;

	bool addDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

	// 엔진이 직접 처리하지 않는 제어 패킷(세션 등)을 받을 곳. 설정하지 않은 종류는 잘못된 패킷으로 처리함
	void setControlHandler(ControlType type, ControlHandler handler);

	void setPacketFilter(PacketFilter filter);

	std::optional<std::size_t> onReceive(const std::shared_ptr<Connection>& connection,
		std::span<const std::uint8_t> data) const;

private:
	// 패킷 하나를 핸들러로 넘김. 잘못된 패킷이면 로그를 남기고 false
	bool dispatch(const std::shared_ptr<Connection>& connection, std::uint16_t id, PacketReader& body) const;

	bool handleControlPacket(const std::shared_ptr<Connection>& connection, PacketReader& body) const;

private:
	std::unordered_map<std::uint16_t, detail::PacketDispatcher> m_dispatchers;
	std::unordered_map<std::uint16_t, ControlHandler> m_controlHandlers;
	PacketFilter m_packetFilter;
};
}
