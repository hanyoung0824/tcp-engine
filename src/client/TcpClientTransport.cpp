#include "TcpClientTransport.hpp"

#include "EngineLog.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
TcpClientTransport::TcpClientTransport(ConnectionOptions connectionOptions)
	: m_connectionOptions(connectionOptions)
{
}

TcpClientTransport::~TcpClientTransport()
{
	close();
}

bool TcpClientTransport::connect(const std::string& host, unsigned short port, std::shared_ptr<const ConnectionEventHandler> handler)
{
	if (m_connection) {
		engineLogError("Already connected");
		return false;
	}

	boost::system::error_code ec;
	const asio::ip::address address = asio::ip::make_address(host, ec);
	if (ec) {
		engineLogError("Invalid address ", host, ": ", ec.message());
		return false;
	}

	const tcp::endpoint endPoint(address, port);
	tcp::socket socket(m_ioContext);
	socket.connect(endPoint, ec);
	if (ec) {
		engineLogWarning("Failed to connect to ", endPoint, ": ", ec.message());
		return false;
	}

	// onConnected도 네트워크 스레드에서 호출되도록 start()를 넘김
	m_connection = std::make_shared<TcpConnection>(std::move(socket), std::move(handler), 0, m_connectionOptions);
	asio::post(m_ioContext, [connection = m_connection] { connection->start(); });
	m_networkThread = std::thread([this] { m_ioContext.run(); });
	return true;
}

void TcpClientTransport::send(std::shared_ptr<const Connection::Buffer> data)
{
	if (!m_connection) {
		return;
	}

	asio::post(m_ioContext, [connection = m_connection, data = std::move(data)] {
		connection->send(data);
	});
}

void TcpClientTransport::close()
{
	if (!m_networkThread.joinable()) {
		return;
	}

	asio::post(m_ioContext, [connection = m_connection] { connection->close(); });
	m_networkThread.join();
}
}
