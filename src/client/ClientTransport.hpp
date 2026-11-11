#pragma once

#include "ConnectionEventHandler.hpp"
#include "tcpengine/network/Connection.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <memory>
#include <string>

namespace tcpengine
{
// 서버 접속과 송수신을 맡는 전송 계층. 프로토콜마다 구현을 둠
class ClientTransport
{
public:
	virtual ~ClientTransport() = default;

	virtual bool connect(const std::string& host, unsigned short port, std::shared_ptr<const ConnectionEventHandler> handler) = 0;

	// 하트비트 타이머 등 네트워크 스레드에서 돌릴 작업용
	virtual boost::asio::any_io_executor getExecutor() = 0;

	// 어느 스레드에서 불러도 됨
	virtual void send(std::shared_ptr<const Connection::Buffer> data) = 0;

	virtual void close() = 0;

	virtual bool isConnected() const = 0;
};
}
