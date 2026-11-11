#include "TcpServerTransport.hpp"

#include "Logger.hpp"

#include <csignal>
#include <thread>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
TcpServerTransport::TcpServerTransport(unsigned short port, std::shared_ptr<const ConnectionEventHandler> handler)
	: m_connectionManager(std::move(handler)),
	m_listener(m_ioContext, tcp::endpoint(tcp::v4(), port)),
	m_signals(m_ioContext, SIGINT, SIGTERM)
{
}

bool TcpServerTransport::start()
{
	if (!m_listener.open()) {
		return false;
	}

	m_listener.start([this](tcp::socket socket) {
		m_connectionManager.createConnection(std::move(socket));
	});

	m_signals.async_wait([this](const boost::system::error_code& ec, int signalNumber) {
		if (ec) {
			return;
		}
		engineLogInfo("Signal ", signalNumber, " received, shutting down");
		stop();
	});

	engineLogInfo("[thread ", std::this_thread::get_id(), "] Server listening on ", m_listener.getEndPoint());
	return true;
}

void TcpServerTransport::run()
{
	m_ioContext.run();
}

void TcpServerTransport::stop()
{
	asio::post(m_ioContext, [this] {
		boost::system::error_code ec;
		m_signals.cancel(ec);
		m_listener.close();
		m_connectionManager.closeAll();
	});
}
}
