#include "Connector.hpp"

#include "Log.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

Connector::Connector(const tcp::endpoint& endPoint)
	: m_endPoint(endPoint)
{
}

bool Connector::connect(tcp::socket& socket)
{
	// connect: 소켓을 열고(open) 서버에 접속한다. TCP 연결이 맺어질 때까지 blocking.
	boost::system::error_code ec;
	socket.connect(m_endPoint, ec);
	if (ec) {
		writeLog(LogLevel::Warning, "Failed to connect to ", m_endPoint, ": ", ec.message());
		return false;
	}
	return true;
}
