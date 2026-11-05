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
	Listener(boost::asio::io_context& ioContext, const boost::asio::ip::tcp::endpoint& endPoint);

	bool open();

	void start(AcceptHandler onAccept);

	void close();

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_endPoint; }

private:
	void startAccept();

	void onAccept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket);

private:
	boost::asio::ip::tcp::acceptor m_acceptor;
	const boost::asio::ip::tcp::endpoint m_endPoint;
	AcceptHandler m_onAccept;
};
}
