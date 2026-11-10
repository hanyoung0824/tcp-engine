#pragma once

#include <boost/asio.hpp>

namespace tcpengine
{
class Connector
{
public:
	explicit Connector(const boost::asio::ip::tcp::endpoint& endPoint);

	bool connect(boost::asio::ip::tcp::socket& socket);

	const boost::asio::ip::tcp::endpoint& getEndPoint() const { return m_endPoint; }

private:
	const boost::asio::ip::tcp::endpoint m_endPoint;
};
}
