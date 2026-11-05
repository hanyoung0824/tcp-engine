#include "Listener.hpp"

#include "Log.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
Listener::Listener(asio::io_context& ioContext, const tcp::endpoint& endPoint)
	: m_acceptor(ioContext),
	m_endPoint(endPoint)
{
}

bool Listener::open()
{
	boost::system::error_code ec;

	m_acceptor.open(m_endPoint.protocol(), ec);
	if (ec) {
		writeLog(LogLevel::Error, "Failed to open acceptor: ", ec.message());
		return false;
	}

	// Windows의 SO_REUSEADDR는 사용 중인 포트도 가로챌 수 있어 끄고, Linux에서는 TIME_WAIT 포트 재사용만 허용하므로 켬
#ifdef _WIN32
	m_acceptor.set_option(tcp::acceptor::reuse_address(false), ec);
#else
	m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
#endif
	if (ec) {
		writeLog(LogLevel::Error, "Failed to configure acceptor: ", ec.message());
		return false;
	}

	m_acceptor.bind(m_endPoint, ec);
	if (ec) {
		writeLog(LogLevel::Error, "Failed to bind ", m_endPoint, ": ", ec.message());
		return false;
	}

	m_acceptor.listen(asio::socket_base::max_listen_connections, ec);
	if (ec) {
		writeLog(LogLevel::Error, "Failed to listen: ", ec.message());
		return false;
	}

	return true;
}

void Listener::start(AcceptHandler onAccept)
{
	m_onAccept = std::move(onAccept);
	startAccept();
}

void Listener::close()
{
	boost::system::error_code ec;
	m_acceptor.close(ec);
}

void Listener::startAccept()
{
	m_acceptor.async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
		onAccept(ec, std::move(socket));
	});
}

void Listener::onAccept(const boost::system::error_code& ec, tcp::socket socket)
{
	if (ec == asio::error::operation_aborted) {
		return;
	}

	if (ec) {
		writeLog(LogLevel::Warning, "Failed to accept: ", ec.message());
	}
	else {
		m_onAccept(std::move(socket));
	}

	startAccept();
}
}
