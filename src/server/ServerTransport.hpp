#pragma once

#include <functional>

namespace tcpengine
{
// 접속 수락과 연결 관리를 맡는 전송 계층. 프로토콜마다 구현을 둠
// 이벤트 루프는 Server가 소유하고 넘겨줌. 모든 함수는 네트워크 스레드에서 호출
class ServerTransport
{
public:
	virtual ~ServerTransport() = default;

	virtual bool start() = 0;

	virtual void stopAccepting() = 0;

	// 모든 연결을 close()하고 이후 들어오는 연결은 받지 않음
	// 연결이 모두 닫히면 onAllClosed를 한 번 부름. 연결이 없으면 바로 부름
	virtual void closeAll(std::function<void()> onAllClosed) = 0;
};
}
