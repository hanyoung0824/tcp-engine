#pragma once

#include "Log.hpp"
#include "PacketManager.hpp"
#include "Session.hpp"

#include <memory>

// 서버 프로그램의 뼈대 (네트워크 계층).
// TcpServer를 만들고, 세션 이벤트를 아래 가상 함수로, 받은 바이트를 패킷 매니저로 연결하고,
// 종료 신호가 올 때까지 실행하는 과정을 맡는다.
// 응용은 이 클래스를 상속해 생성자에서 받을 패킷 클래스를 등록하고 필요한 함수만 재정의하며,
// main에서는 run()만 호출한다.
//
//   class ChatServer : public Server
//   {
//   public:
//       ChatServer() { registerPacket<C2S_ChatHandler>(); }   // C2S_ChatHandler : C2S_Chat, handle() 오버라이드
//   };
//   int main() { return ChatServer().run(12345); }
//
// 가상 함수와 패킷의 handle()은 모두 run()을 호출한 스레드에서 호출된다
// (onStart/onStop은 run() 처음과 끝, 나머지는 이벤트 루프 안).
class Server
{
public:
	virtual ~Server() = default;

	// 서버를 열고 종료 신호(Ctrl+C, SIGTERM)가 와서 모든 연결이 닫힐 때까지 실행한다.
	// main()의 반환값으로 쓸 수 있도록, 정상 종료면 0, 시작에 실패하면 1을 반환한다.
	int run(unsigned short port);

protected:
	Server() = default;

	// 이 서버가 받을 패킷 클래스 T(IPacket 상속, handle() 구현)를 등록한다. 파생 클래스의 생성자에서 호출한다.
	// run() 뒤에는 패킷 목록을 여러 스레드가 읽을 수 있으므로 등록하지 않는다.
	template <typename T>
	bool registerPacket()
	{
		if (m_running) {
			writeLog(LogLevel::Error, "registerPacket() must be called before run()");
			return false;
		}
		return m_packetManager.addPacket<T>();
	}

	// 시작 전 준비 (예: 로그 설정). false를 반환하면 서버를 열지 않고 끝낸다.
	virtual bool onStart() { return true; }

	// 서버가 멈춘 뒤 정리
	virtual void onStop() {}

	// 클라이언트가 접속했을 때 (읽기 시작 전)
	virtual void onConnected(const std::shared_ptr<Session>&) {}

	// 클라이언트와의 연결이 닫혔을 때 (상대가 끊었든 서버가 닫았든)
	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

private:
	PacketManager m_packetManager; // 받은 바이트를 패킷 객체로 바꿔 처리한다. registerPacket()으로만 채운다.
	bool m_running = false; // run()이 시작됐는지
};
