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
	SessionManager::instance().setEventHandler(m_handler);

	if (!m_listener.open()) {
		return false;
	}

	m_listener.start([](tcp::socket socket) {
		SessionManager::instance().create(std::move(socket));
	});

	m_signals.async_wait([this](const boost::system::error_code& ec, int signalNumber) {
		if (ec) {
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
	m_ioContext.run();
}

void TcpServer::stop()
{
	asio::post(m_ioContext, [this] {
		boost::system::error_code ec;
		m_signals.cancel(ec);
		m_listener.close();
		SessionManager::instance().closeAll();
	});
}
