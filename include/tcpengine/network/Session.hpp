#pragma once

#include "tcpengine/network/packet/IPacket.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace tcpengine
{
// isClosed()를 뺀 모든 함수는 네트워크 스레드(io_context.run() 스레드)에서만 호출
class Session
{
public:
	using SessionId = std::uint64_t;

	using Buffer = PacketBuffer;

public:
	virtual ~Session() = default;

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	virtual void send(std::shared_ptr<const Buffer> data) = 0;

	virtual void close() = 0;

	virtual bool isClosed() const = 0;

	virtual SessionId getId() const = 0;

	virtual const std::string& getAddress() const = 0;

protected:
	Session() = default;
};
}
