#include "TcpClientTransport.hpp"

#include "Connector.hpp"
#include "Logger.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
TcpClientTransport::~TcpClientTransport()
{
	close();
}

bool TcpClientTransport::connect(const std::string& host, unsigned short port, std::shared_ptr<const SessionConfig> config)
{
	if (m_session) {
		engineLogError("Already connected");
		return false;
	}

	boost::system::error_code ec;
	const asio::ip::address address = asio::ip::make_address(host, ec);
	if (ec) {
		engineLogError("Invalid address ", host, ": ", ec.message());
		return false;
	}

	Connector connector(tcp::endpoint(address, port));
	tcp::socket socket(m_ioContext);
	if (!connector.connect(socket)) {
		return false;
	}

	// onConnected도 네트워크 스레드에서 호출되도록 start()를 넘김
	m_session = std::make_shared<TcpSession>(std::move(socket), std::move(config), 0);
	asio::post(m_ioContext, [session = m_session] { session->start(); });
	m_networkThread = std::thread([this] { m_ioContext.run(); });
	return true;
}

void TcpClientTransport::startHeartbeat(const HeartbeatSettings& heartbeat, std::shared_ptr<const Session::Buffer> pingData)
{
	if (m_session) {
		m_session->startHeartbeat(heartbeat, std::move(pingData));
	}
}

void TcpClientTransport::send(std::shared_ptr<const Session::Buffer> data)
{
	if (!m_session) {
		return;
	}

	asio::post(m_ioContext, [session = m_session, data = std::move(data)] {
		session->send(data);
	});
}

void TcpClientTransport::close()
{
	if (!m_networkThread.joinable()) {
		return;
	}

	asio::post(m_ioContext, [session = m_session] { session->close(); });
	m_networkThread.join();
}
}
