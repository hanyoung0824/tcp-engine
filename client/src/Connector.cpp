#include "Connector.hpp"

#include "Log.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
Connector::Connector(const tcp::endpoint& endPoint)
	: m_endPoint(endPoint)
{
}

bool Connector::connect(tcp::socket& socket)
{
	boost::system::error_code ec;
	socket.connect(m_endPoint, ec);
	if (ec) {
		writeLog(LogLevel::Warning, "Failed to connect to ", m_endPoint, ": ", ec.message());
		return false;
	}
	return true;
}
}
