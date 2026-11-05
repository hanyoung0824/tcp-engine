#pragma once

#include <boost/asio.hpp>

// 서버에 접속하는 역할.
// 접속에 성공하면 연결된 socket을 넘겨줄 뿐, 데이터는 주고받지 않는다.
class Connector
{
public:
	explicit Connector(const boost::asio::ip::tcp::endpoint& endPoint);

	// 서버에 접속할 때까지 멈춰(blocking) 기다린다.
	// 성공하면 연결된 socket을 socket에 채우고 true를 반환한다.
	// 실패하면 이유를 출력한 뒤 false를 반환한다. 예: 서버가 없으면 "connection refused".
	bool connect(boost::asio::ip::tcp::socket& socket);

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_endPoint; }

private:
	const boost::asio::ip::tcp::endpoint m_endPoint; // 접속할 서버 주소:포트
};
