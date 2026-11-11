#pragma once

#include "tcpengine/network/packet/Packet.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace tcpengine
{
// isClosed()를 뺀 모든 함수는 네트워크 스레드(io_context.run() 스레드)에서만 호출
class Connection
{
public:
	using ConnectionId = std::uint64_t;

	using Buffer = PacketBuffer;

public:
	virtual ~Connection() = default;

	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

	virtual void send(std::shared_ptr<const Buffer> data) = 0;

	// 직렬화해서 보냄. 최대 크기를 넘으면 보내지 않고 false
	// 같은 패킷을 여러 연결에 보낼 때는 serialize()를 한 번 하고 버퍼를 보내는 쪽이 효율적
	bool send(const PacketBase& packet)
	{
		std::shared_ptr<const Buffer> data = packet.serialize();
		if (!data) {
			return false;
		}
		send(std::move(data));
		return true;
	}

	virtual void close() = 0;

	virtual bool isClosed() const = 0;

	virtual ConnectionId getId() const = 0;

	virtual const std::string& getAddress() const = 0;

protected:
	Connection() = default;
};
}
