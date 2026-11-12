#pragma once

#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/packet/Packet.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace tcpengine
{
enum class SessionEndReason
{
	Closed,         // 응용이 Session::close()를 부름 (로그아웃, 강퇴)
	Expired,        // 끊긴 뒤 resumeTimeout 안에 재접속하지 않음 (0이면 끊기는 즉시)
	ServerStopped,  // 서버 종료
};

// 로그인한 사용자 단위. 응용이 Server::startSession()으로 시작함
// 연결이 끊겨도 resumeTimeout 동안 유지되고, 그 안에 재접속하면 새 연결이 붙음
// 모든 함수는 네트워크 스레드에서만 호출
class Session
{
public:
	using SessionId = std::uint64_t;

public:
	virtual ~Session() = default;

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	virtual SessionId getId() const = 0;

	// 지금 붙어 있는 연결. 재접속을 기다리는 동안은 nullptr
	virtual std::shared_ptr<Connection> getConnection() const = 0;

	bool isConnected() const { return getConnection() != nullptr; }

	// 재접속을 기다리는 동안 보낸 것은 버림
	void send(std::shared_ptr<const PacketBuffer> data)
	{
		if (std::shared_ptr<Connection> connection = getConnection()) {
			connection->send(std::move(data));
		}
	}

	// 직렬화해서 보냄. 최대 크기를 넘으면 보내지 않고 false
	bool send(const PacketBase& packet)
	{
		std::shared_ptr<const PacketBuffer> data = packet.serialize();
		if (!data) {
			return false;
		}
		send(std::move(data));
		return true;
	}

	// 재접속을 기다리지 않고 바로 끝냄 (로그아웃, 강퇴). 붙어 있는 연결도 닫음
	virtual void close() = 0;

protected:
	Session() = default;
};
}
