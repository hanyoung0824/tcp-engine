#include "Client.hpp"

#include <cstdint>
#include <span>

Client::Client()
	: m_client(makeEventHandler())
{
}

Session::EventHandler Client::makeEventHandler()
{
	// 람다는 이벤트가 올 때(접속 후) 호출되므로, 그때는 파생 클래스까지 다 만들어져 가상 함수가 제대로 불린다.
	Session::EventHandler handler;
	handler.onConnected = [this](const std::shared_ptr<Session>& session) {
		onConnected(session);
	};
	handler.onReceive = [this](const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data) {
		return m_packetManager.onReceive(session, data);
	};
	handler.onDisconnected = [this](const std::shared_ptr<Session>& session) {
		onDisconnected(session);
	};
	return handler;
}

int Client::run(const std::string& host, unsigned short port)
{
	// 이제부터 패킷 목록은 읽기만 한다.
	m_running = true;

	// 1. 응용의 준비
	if (!onStart()) {
		return 1;
	}

	// 2. 서버에 접속. 성공하면 네트워크 스레드가 받기를 시작한다. 실패 이유는 엔진 로그로 남는다.
	if (!m_client.connect(host, port)) {
		return 1;
	}

	// 3. 응용 로직
	onRun();

	// 4. 연결을 닫고 네트워크 스레드가 끝나기를 기다린다.
	// 소멸자가 아니라 여기서 닫아야, onDisconnected가 파생 클래스가 살아 있을 때 호출된다.
	m_client.close();
	return 0;
}
