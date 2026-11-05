#include "Listener.hpp"

#include "Log.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

Listener::Listener(asio::io_context& ioContext, const tcp::endpoint& endPoint)
	: m_acceptor(ioContext),
	m_endPoint(endPoint)
{
}

bool Listener::open()
{
	// 생성자 한 줄로도 열 수 있지만(open -> reuse_address(true) -> bind -> listen),
	// bind 전에 OS별 옵션을 넣어야 해서 단계별로 연다. 단계마다 error_code로 실패를 확인한다.
	boost::system::error_code ec;

	// 1. open: 소켓을 만든다 (IPv4 / IPv6 중 endpoint의 프로토콜로).
	m_acceptor.open(m_endPoint.protocol(), ec);
	if (ec) {
		writeLog(LogLevel::Error, "Failed to open acceptor: ", ec.message());
		return false;
	}

	// 2. 옵션 (bind 전에 설정해야 한다)
#ifdef _WIN32
	// Windows: SO_REUSEADDR는 사용 중인 포트에도 bind를 허용하므로 끈다.
	// 소켓 기본값도 false지만 의도를 드러내려고 명시한다.
	m_acceptor.set_option(tcp::acceptor::reuse_address(false), ec);
#else
	// Linux: SO_REUSEADDR는 TIME_WAIT가 남은 포트의 재사용만 허용한다.
	// 켜 두면 서버를 바로 재시작할 수 있고, listen 중인 포트를 가로채지는 못한다.
	m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
#endif
	if (ec) {
		writeLog(LogLevel::Error, "Failed to configure acceptor: ", ec.message());
		return false;
	}

	// 3. bind: 소켓을 주소:포트에 묶는다. 포트를 이미 누가 쓰고 있으면 여기서 실패한다.
	m_acceptor.bind(m_endPoint, ec);
	if (ec) {
		writeLog(LogLevel::Error, "Failed to bind ", m_endPoint, ": ", ec.message());
		return false;
	}

	// 4. listen: 이때부터 클라이언트가 접속할 수 있다.
	// 운영체제가 accept 전의 연결을 쌓아 두는 대기열(backlog) 크기도 정한다.
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
	// async_accept: 접속을 기다리지 않고 바로 반환한다.
	// 접속이 오면 io_context가 이 람다를 호출하고, 새로 만들어진 socket을 넘겨준다.
	// Listener는 main에서 io_context.run()보다 오래 살아 있으므로 this를 그대로 잡아도 안전하다.
	m_acceptor.async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
		onAccept(ec, std::move(socket));
	});
}

void Listener::onAccept(const boost::system::error_code& ec, tcp::socket socket)
{
	if (ec == asio::error::operation_aborted) {
		// close()로 acceptor가 닫혔다. 더 이상 접속을 받지 않는다.
		return;
	}

	if (ec) {
		// 접속 하나가 실패해도 서버는 계속 접속을 받는다.
		writeLog(LogLevel::Warning, "Failed to accept: ", ec.message());
	}
	else {
		m_onAccept(std::move(socket));
	}

	// 다음 접속을 기다린다. 이렇게 콜백 안에서 다시 요청해야 접속을 계속 받을 수 있다.
	startAccept();
}
