#pragma once

#include "TcpClient.hpp"
#include "IPacket.hpp"
#include "Log.hpp"
#include "PacketHelper.hpp"
#include "PacketManager.hpp"
#include "Session.hpp"

#include <memory>
#include <string>

// 클라이언트 프로그램의 뼈대 (네트워크 계층).
// TcpClient를 만들고, 세션 이벤트를 아래 가상 함수로, 받은 바이트를 패킷 매니저로 연결하고,
// 접속 -> 응용 로직(onRun) -> 종료 순서로 실행한다.
// 응용은 이 클래스를 상속해 생성자에서 받을 패킷 클래스를 등록하고 필요한 함수를 재정의하며,
// main에서는 run()만 호출한다.
//
//   class ChatClient : public Client
//   {
//   public:
//       ChatClient() { registerPacket<ChatNotifyPacket>(); }   // ChatNotifyPacket : S2C_ChatNotify, handle() 오버라이드
//   };
//   int main() { return ChatClient().run("127.0.0.1", 12345); }
//
// 스레드:
// - onStart, onRun: run()을 호출한 스레드 (예: 메인 스레드)
// - onConnected, onDisconnected, 패킷의 handle(): 네트워크 스레드
class Client
{
public:
	virtual ~Client() = default;

	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	// 서버에 접속해 onRun()을 실행하고, onRun()이 반환하면 연결을 닫는다.
	// main()의 반환값으로 쓸 수 있도록, 정상 종료면 0, 시작이나 접속에 실패하면 1을 반환한다.
	int run(const std::string& host, unsigned short port);

protected:
	Client();

	// 이 클라이언트가 받을 패킷 클래스 T(IPacket 상속, handle() 구현)를 등록한다. 파생 클래스의 생성자에서 호출한다.
	// run() 뒤에는 네트워크 스레드가 패킷 목록을 읽으므로 등록하지 않는다.
	template <typename T>
	bool registerPacket()
	{
		if (m_running) {
			writeLog(LogLevel::Error, "registerPacket() must be called before run()");
			return false;
		}
		return m_packetManager.addPacket<T>();
	}

	// 접속 전 준비 (예: 로그 설정). false를 반환하면 접속하지 않고 끝낸다.
	virtual bool onStart() { return true; }

	// 접속된 동안 실행할 응용 로직 (예: 키보드 입력 루프). 반환하면 연결을 닫고 run()이 끝난다.
	virtual void onRun() = 0;

	// 서버에 접속했을 때 (읽기 시작 전, 네트워크 스레드)
	virtual void onConnected(const std::shared_ptr<Session>&) {}

	// 서버와의 연결이 닫혔을 때 (서버가 끊었든 이쪽이 닫았든, 네트워크 스레드)
	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

	// packet을 서버에 보낸다. 어느 스레드에서 불러도 된다.
	void send(const IPacket& packet)
	{
		if (auto buffer = makePacket(packet)) {
			m_client.send(std::move(buffer));
		}
	}

	// 연결되어 있는지 (서버가 끊었거나 닫혔으면 false)
	bool isConnected() const { return m_client.isConnected(); }

private:
	// 세션 이벤트를 이 클래스의 가상 함수로, 받은 바이트를 패킷 매니저로 연결한 처리 함수를 만든다.
	Session::EventHandler makeEventHandler();

private:
	// 멤버는 반대 순서로 소멸한다. m_client(네트워크 스레드)가 먼저 끝나야 패킷 매니저를 안전하게 지울 수 있다.
	PacketManager m_packetManager; // 받은 바이트를 패킷 객체로 바꿔 처리한다. registerPacket()으로만 채운다.
	TcpClient m_client; // 서버와의 연결과 네트워크 스레드
	bool m_running = false; // run()이 시작됐는지
};
