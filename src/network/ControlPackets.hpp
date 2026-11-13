#pragma once

#include "PacketHeader.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/network/HeartbeatSettings.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

// 엔진 제어 패킷(id 0)을 만들고 본문을 읽음. 상태가 없어 어디서나 부름
// read*는 ControlType 다음부터 읽으며, 형식이나 값이 틀리면 nullopt
namespace tcpengine::control
{
// 서버가 접속 직후 클라이언트에 한 번 보내는 정보
struct ServerHello
{
	HeartbeatSettings heartbeat;
};

// 클라이언트가 접속 직후 가장 먼저 보내는 정보. 서버는 버전이 다르면 VersionMismatch로 끊음
struct ClientHello
{
	std::uint16_t engineVersion = 0;
	std::uint16_t appVersion = 0;
};

std::shared_ptr<const Connection::Buffer> make(ControlType type);

std::shared_ptr<const Connection::Buffer> make(ControlType type, std::string_view text);

std::shared_ptr<const Connection::Buffer> ping();

std::shared_ptr<const Connection::Buffer> pong();

// 하트비트 interval과 timeout을 uint16 초로 보내므로 65535초를 넘으면 nullptr
std::shared_ptr<const Connection::Buffer> serverHello(const ServerHello& hello);

std::optional<ServerHello> readServerHello(PacketReader& body);

std::shared_ptr<const Connection::Buffer> clientHello(const ClientHello& hello);

std::optional<ClientHello> readClientHello(PacketReader& body);

std::shared_ptr<const Connection::Buffer> disconnect(DisconnectReason reason);

std::optional<DisconnectReason> readDisconnect(PacketReader& body);
}
