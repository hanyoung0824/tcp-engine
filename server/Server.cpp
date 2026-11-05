#include "Server.hpp"

#include "TcpServer.hpp"

#include <cstdint>
#include <span>

int Server::run(unsigned short port)
{
	// 이제부터 패킷 목록은 읽기만 한다.
	m_running = true;

	// 1. 응용의 준비
	if (!onStart()) {
		return 1;
	}

	// 2. 세션 이벤트를 이 클래스의 가상 함수로, 받은 바이트를 패킷 매니저로 연결한다.
	// TcpServer는 이 함수 안에서만 살아 있으므로 this를 잡아도 안전하다.
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

	// 3. 서버를 열고, 종료 신호가 와서 모든 연결이 닫힐 때까지 이 스레드에서 처리한다.
	TcpServer server(port, std::move(handler));
	if (!server.start()) {
		return 1;
	}
	server.run();

	// 4. 응용의 정리
	onStop();
	return 0;
}
