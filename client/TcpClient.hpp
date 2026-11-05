#pragma once

#include "Session.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <thread>

// TCP 클라이언트 하나를 묶은 모듈 (네트워크 계층). 클라이언트 프로그램의 뼈대인 Client가 내부에서 쓴다.
// io_context, 네트워크 스레드, 서버와의 세션을 한곳에 모았다.
// 세션 이벤트(접속, 수신, 종료)의 처리는 생성할 때 넘긴 응용 쪽 함수에 맡긴다 (패킷 형식은 모른다).
//
// 스레드 구성:
// - 네트워크 스레드: connect()가 만든다. io_context.run()으로 받기/보내기를 처리한다. 소켓은 이 스레드만 만진다.
//   세션 이벤트도 모두 이 스레드에서 호출된다.
// - 호출하는 스레드(예: 메인): send()/close()는 할 일을 asio::post로 네트워크 스레드에 넘긴다.
//
// connect(), close(), 소멸자는 같은 스레드(TcpClient를 만든 스레드)에서 호출한다.
class TcpClient
{
public:
	explicit TcpClient(Session::EventHandler handler);

	// 연결이 남아 있으면 닫고 네트워크 스레드가 끝나기를 기다린다.
	~TcpClient();

	TcpClient(const TcpClient&) = delete;
	TcpClient& operator=(const TcpClient&) = delete;

	// 서버에 접속할 때까지 멈춰(blocking) 기다린 뒤, 받기를 시작하고 네트워크 스레드를 만든다.
	// 실패하면 이유를 출력한 뒤 false를 반환한다. 한 TcpClient로 한 번만 접속할 수 있다.
	bool connect(const std::string& host, unsigned short port);

	// data를 보낸다. 어느 스레드에서 불러도 된다. 연결이 없거나 닫혔으면 버린다.
	void send(std::shared_ptr<const Session::Buffer> data);

	// 연결을 닫고 네트워크 스레드가 끝나기를 기다린다. 여러 번 호출해도 된다.
	void close();

	// 연결되어 있는지 (서버가 끊었거나 close()를 호출했으면 false)
	bool isConnected() const { return m_session && !m_session->isClosed(); }

private:
	// io_context가 먼저 만들어지고 마지막에 소멸해야, 여기에 속한 소켓(세션)이 안전하다.
	boost::asio::io_context m_ioContext; // 비동기 작업의 완료 콜백을 실행한다
	Session::EventHandler m_handler; // 세션 이벤트 처리 함수
	std::shared_ptr<Session> m_session; // 서버와의 연결. connect()가 만든다.
	std::thread m_networkThread; // io_context.run()을 실행하는 스레드
};
