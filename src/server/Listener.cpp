#include "Listener.hpp"

#include "Logger.hpp"

#include <chrono>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
namespace
{
	constexpr std::chrono::milliseconds acceptRetryDelay{ 500 };
}

Listener::Listener(asio::any_io_executor executor, const tcp::endpoint& endPoint)
	: m_acceptor(executor),
	m_retryTimer(executor),
	m_endPoint(endPoint)
{
}

bool Listener::open()
{
	boost::system::error_code ec;

	m_acceptor.open(m_endPoint.protocol(), ec);
	if (ec) {
		engineLogError("Failed to open acceptor: ", ec.message());
		return false;
	}

	// Windows의 SO_REUSEADDR는 사용 중인 포트도 가로챌 수 있어 끄고, Linux에서는 TIME_WAIT 포트 재사용만 허용하므로 켬
#ifdef _WIN32
	m_acceptor.set_option(tcp::acceptor::reuse_address(false), ec);
#else
	m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
#endif
	if (ec) {
		engineLogError("Failed to configure acceptor: ", ec.message());
		return false;
	}

	m_acceptor.bind(m_endPoint, ec);
	if (ec) {
		engineLogError("Failed to bind ", m_endPoint, ": ", ec.message());
		return false;
	}

	m_acceptor.listen(asio::socket_base::max_listen_connections, ec);
	if (ec) {
		engineLogError("Failed to listen: ", ec.message());
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
	m_retryTimer.cancel();
}

void Listener::startAccept()
{
	m_acceptor.async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
		onAccept(ec, std::move(socket));
	});
}

void Listener::onAccept(const boost::system::error_code& ec, tcp::socket socket)
{
	// close() 직전에 수락이 끝나 성공으로 들어온 경우도 있음. 닫힌 acceptor에 다시 걸면 바로 실패해 끝없이 반복함
	if (ec == asio::error::operation_aborted || !m_acceptor.is_open()) {
		return;
	}

	// 상대가 수락 전에 끊은 경우는 서버 문제가 아니므로 바로 다음 접속을 받음
	if (ec && ec != asio::error::connection_aborted) {
		engineLogWarning("Failed to accept: ", ec.message(), ", retrying in ", acceptRetryDelay.count(), "ms");
		retryAcceptLater();
		return;
	}

	if (!ec) {
		m_onAccept(std::move(socket));
	}
	startAccept();
}

void Listener::retryAcceptLater()
{
	m_retryTimer.expires_after(acceptRetryDelay);
	m_retryTimer.async_wait([this](const boost::system::error_code& ec) {
		if (ec || !m_acceptor.is_open()) {
			return;
		}
		startAccept();
	});
}
}
