#pragma once

#include <boost/asio.hpp>

#include <functional>

namespace tcpengine
{
class Listener
{
public:
	using AcceptHandler = std::function<void(boost::asio::ip::tcp::socket)>;

public:
	Listener(boost::asio::any_io_executor executor, const boost::asio::ip::tcp::endpoint& endPoint);

	bool open();

	void start(AcceptHandler onAccept);

	void close();

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_endPoint; }

private:
	void startAccept();

	void onAccept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket);

	// 파일 디스크립터 부족처럼 바로 다시 해도 실패할 오류 뒤에 잠시 쉬었다 다시 받음
	void retryAcceptLater();

private:
	boost::asio::ip::tcp::acceptor m_acceptor;
	boost::asio::steady_timer m_retryTimer;
	const boost::asio::ip::tcp::endpoint m_endPoint;
	AcceptHandler m_onAccept;
};
}
