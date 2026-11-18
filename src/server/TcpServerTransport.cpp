#include "TcpServerTransport.hpp"

#include "EngineLog.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
TcpServerTransport::TcpServerTransport(asio::any_io_executor executor, unsigned short port, ConnectionOptions connectionOptions, CanAccept canAccept,
	std::shared_ptr<const ConnectionEvents> events)
	: m_connectionRegistry(std::move(events), connectionOptions, std::move(canAccept)),
	m_listener(std::move(executor), tcp::endpoint(tcp::v4(), port))
{
}

bool TcpServerTransport::start()
{
	if (!m_listener.open()) {
		return false;
	}

	m_listener.start([this](tcp::socket socket) {
		m_connectionRegistry.addConnection(std::move(socket));
	});

	engineLogInfo("Server listening on ", m_listener.getEndpoint());
	return true;
}

void TcpServerTransport::stopAccepting()
{
	m_listener.close();
}

void TcpServerTransport::closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed)
{
	m_connectionRegistry.closeAll(std::move(farewell), std::move(onAllClosed));
}
}
