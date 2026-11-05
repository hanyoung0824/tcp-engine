#include "TcpServer.hpp"

#include "Log.hpp"
#include "SessionManager.hpp"

#include <csignal>
#include <thread>

namespace asio = boost::asio;
using asio::ip::tcp;

TcpServer::TcpServer(unsigned short port, Session::EventHandler handler)
	: m_handler(std::move(handler)),
	m_listener(m_ioContext, tcp::endpoint(tcp::v4(), port)),
	m_signals(m_ioContext, SIGINT, SIGTERM)
{
}

bool TcpServer::start()
{
	// 1. 새 세션들이 쓸 이벤트 처리 함수를 세션 목록에 등록한다.
	SessionManager::instance().setEventHandler(m_handler);

	// 2. 서버 소켓 열기: open -> 옵션 -> bind -> listen
	if (!m_listener.open()) {
		return false;
	}

	// 3. 접속 받기 시작: 접속이 올 때마다 SessionManager가 세션을 만들어 목록에 넣고 시작시킨다.
	m_listener.start([](tcp::socket socket) {
		SessionManager::instance().create(std::move(socket));
	});

	// 4. 종료 신호를 비동기로 기다린다. 오면 stop()으로 정리한다.
	m_signals.async_wait([this](const boost::system::error_code& ec, int signalNumber) {
		if (ec) {
			// stop()에서 cancel()로 기다리기를 취소했다.
			return;
		}
		writeLog(LogLevel::Info, "Signal ", signalNumber, " received, shutting down");
		stop();
	});

	writeLog(LogLevel::Info, "[thread ", std::this_thread::get_id(), "] Server listening on ", m_listener.getEndPoint());
	return true;
}

void TcpServer::run()
{
	// 완료된 비동기 작업의 콜백을 이 스레드에서 차례로 실행한다.
	// 살아 있는 세션은 항상 읽기를 걸어 두므로, run()이 반환했다면 세션 목록은 이미 비어 있다.
	// (세션 목록은 싱글톤이라 TcpServer, 즉 io_context보다 늦게 소멸한다.)
	m_ioContext.run();
}

void TcpServer::stop()
{
	// Listener, signal_set, 세션 목록은 run()을 실행하는 스레드에서만 건드리므로 그 스레드로 넘긴다.
	// 걸려 있던 작업이 모두 취소되어 끝나면, 할 일이 없어진 run()이 스스로 반환한다 (io_context.stop()으로 강제로 멈추지 않는다).
	asio::post(m_ioContext, [this] {
		boost::system::error_code ec;
		m_signals.cancel(ec);
		m_listener.close();
		SessionManager::instance().closeAll();
	});
}
