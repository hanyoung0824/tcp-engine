#pragma once

#include <boost/asio/any_io_executor.hpp>

namespace tcpengine
{
// 접속 수락과 연결 관리를 맡는 전송 계층. 프로토콜마다 구현을 둠
class ServerTransport
{
public:
	virtual ~ServerTransport() = default;

	virtual bool start() = 0;

	// stop()이 불리거나 종료 시그널을 받을 때까지 반환하지 않음
	virtual void run() = 0;

	virtual void stop() = 0;

	// 세션 만료 타이머 등 네트워크 스레드에서 돌릴 작업용
	virtual boost::asio::any_io_executor getExecutor() = 0;
};
}
