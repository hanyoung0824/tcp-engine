#pragma once

#include "SessionConfig.hpp"
#include "tcpengine/network/HeartbeatSettings.hpp"
#include "tcpengine/network/Session.hpp"

#include <memory>
#include <string>

namespace tcpengine
{
// 서버 접속과 송수신을 맡는 전송 계층. 프로토콜마다 구현을 둠
class ClientTransport
{
public:
	virtual ~ClientTransport() = default;

	virtual bool connect(const std::string& host, unsigned short port, std::shared_ptr<const SessionConfig> config) = 0;

	// 서버가 알려 준 하트비트를 세션에 적용. 네트워크 스레드에서 호출
	virtual void startHeartbeat(const HeartbeatSettings& heartbeat, std::shared_ptr<const Session::Buffer> pingData) = 0;

	// 어느 스레드에서 불러도 됨
	virtual void send(std::shared_ptr<const Session::Buffer> data) = 0;

	virtual void close() = 0;

	virtual bool isConnected() const = 0;
};
}
