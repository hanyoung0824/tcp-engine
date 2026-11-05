#pragma once

#include "Listener.hpp"
#include "Session.hpp"

#include <boost/asio.hpp>

#include <memory>

// TCP 서버 하나를 묶은 모듈 (네트워크 계층). 서버 프로그램의 뼈대인 Server가 내부에서 쓴다.
// io_context, 접속 받기(Listener), 세션 목록(SessionManager), 종료 신호(Ctrl+C, SIGTERM) 처리를 한곳에 모았다.
// 세션 이벤트(접속, 수신, 종료)의 처리는 생성할 때 넘긴 응용 쪽 함수에 맡긴다 (패킷 형식은 모른다).
//
// 사용 순서: TcpServer server(port, handler); if (!server.start()) { 실패 } server.run();
//
// 지금은 run()을 호출한 스레드 하나가 모든 접속을 처리한다.
class TcpServer
{
public:
	// handler: 모든 세션이 쓸 이벤트 처리 함수
	TcpServer(unsigned short port, Session::EventHandler handler);

	TcpServer(const TcpServer&) = delete;
	TcpServer& operator=(const TcpServer&) = delete;

	// 서버 소켓을 열고(listen) 접속 받기를 시작한다. 실패하면 이유를 출력한 뒤 false를 반환한다.
	bool start();

	// 이벤트 루프를 실행한다. stop()이 호출되거나 종료 신호가 와서 모든 작업이 끝날 때까지 반환하지 않는다.
	void run();

	// 새 접속을 막고 모든 세션을 닫는다. 그러면 run()이 반환한다.
	// 실제 처리는 run()을 실행하는 스레드로 넘기므로 어느 스레드에서 불러도 된다.
	void stop();

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_listener.getEndPoint(); }

private:
	// 멤버는 선언 순서대로 만들어지고 반대 순서로 소멸한다.
	// io_context가 먼저 만들어지고 마지막에 소멸해야, 여기에 속한 Listener와 signal_set이 안전하다.
	boost::asio::io_context m_ioContext; // 비동기 작업의 완료 콜백을 실행한다
	Session::EventHandler m_handler; // 모든 세션이 쓸 이벤트 처리 함수
	Listener m_listener; // 접속을 받는다
	boost::asio::signal_set m_signals; // 종료 신호(Ctrl+C, SIGTERM)를 기다린다
};
