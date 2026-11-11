#include "TcpServerTransport.hpp"

#include "Logger.hpp"

#include <thread>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
TcpServerTransport::TcpServerTransport(asio::any_io_executor executor, unsigned short port, std::chrono::seconds closeTimeout,
	std::shared_ptr<const ConnectionEventHandler> handler)
	: m_connectionManager(std::move(handler), closeTimeout),
	m_listener(std::move(executor), tcp::endpoint(tcp::v4(), port))
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

	engineLogInfo("[thread ", std::this_thread::get_id(), "] Server listening on ", m_listener.getEndPoint());
	return true;
}

void TcpServerTransport::stopAccepting()
{
	m_listener.close();
}

void TcpServerTransport::closeAll(std::function<void()> onAllClosed)
{
	m_connectionManager.closeAll(std::move(onAllClosed));
}
}
