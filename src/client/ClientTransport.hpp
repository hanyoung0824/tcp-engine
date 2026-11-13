#pragma once

#include "ConnectionEventHandler.hpp"
#include "tcpengine/network/Connection.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace tcpengine
{
// 서버 접속과 송수신을 맡는 전송 계층. 프로토콜마다 구현을 둠
// 네트워크 스레드 하나를 두고, 연결이 끊겨도 stop()까지 스레드를 유지해 다시 접속할 수 있음
class ClientTransport
{
public:
	using ConnectResult = std::function<void(bool connected)>;

	virtual ~ClientTransport() = default;

	// 네트워크 스레드를 시작함
	virtual void start() = 0;

	// 비동기로 접속함. host는 IP 주소나 도메인 이름. timeout 안에 접속하지 못하면 실패
	// 결과는 네트워크 스레드에서 onResult로 알림. 성공하면 handler의 onConnected가 먼저 불림
	// 어느 스레드에서 불러도 되지만 이전 연결이 끝난 뒤에만 부름
	virtual void connect(const std::string& host, unsigned short port, std::chrono::seconds timeout,
		std::shared_ptr<const ConnectionEventHandler> handler, ConnectResult onResult) = 0;

	// 하트비트 타이머 등 네트워크 스레드에서 돌릴 작업용
	virtual boost::asio::any_io_executor getExecutor() = 0;

	// 어느 스레드에서 불러도 됨. 연결이 없으면 버림
	virtual void send(std::shared_ptr<const Connection::Buffer> data) = 0;

	// 지금 연결을 정상 종료함. 어느 스레드에서 불러도 됨
	virtual void close() = 0;

	// 남은 연결과 대기 중인 작업이 끝나면 네트워크 스레드를 멈추고 기다림. 네트워크 스레드가 아닌 곳에서 부름
	virtual void stop() = 0;
};
}
