#pragma once

#include <boost/asio.hpp>

#include <functional>

// 서버 소켓(acceptor)을 열고 클라이언트 접속을 비동기로 받는 역할.
// 접속을 받으면 그 클라이언트 전용 socket을 콜백으로 넘겨줄 뿐, 데이터는 주고받지 않는다.
class Listener
{
public:
	// 접속을 받았을 때 호출할 함수. 접속한 클라이언트의 socket을 넘겨받는다.
	using AcceptHandler = std::function<void(boost::asio::ip::tcp::socket)>;

public:
	Listener(boost::asio::io_context& ioContext, const boost::asio::ip::tcp::endpoint& endPoint);

	// acceptor를 열고 접속을 받을 수 있는 상태(listen)로 만든다.
	// 실패하면 이유를 출력한 뒤 false를 반환한다.
	bool open();

	// 접속 받기를 시작한다. 바로 반환하고, 접속이 올 때마다 onAccept가 호출된다.
	// 콜백은 io_context.run()을 실행 중인 스레드에서 호출된다.
	void start(AcceptHandler onAccept);

	// 접속 받기를 멈추고 acceptor를 닫는다. 대기 중이던 async_accept는 operation_aborted로 끝난다.
	void close();

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_endPoint; }

private:
	// 다음 접속 하나를 비동기로 기다린다.
	void startAccept();

	// 접속이 오면(또는 실패하면) 호출된다.
	void onAccept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket);

private:
	boost::asio::ip::tcp::acceptor m_acceptor; // 접속을 받는 전용 소켓
	const boost::asio::ip::tcp::endpoint m_endPoint; // 대기할 주소:포트
	AcceptHandler m_onAccept; // 접속을 받았을 때 호출할 함수
};
