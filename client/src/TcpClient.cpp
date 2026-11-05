#include "TcpClient.hpp"

#include "Connector.hpp"
#include "Log.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
TcpClient::TcpClient(TcpSession::EventHandler handler)
	: m_handler(std::move(handler))
{
}

TcpClient::~TcpClient()
{
	close();
}

bool TcpClient::connect(const std::string& host, unsigned short port, HeartbeatSettings heartbeat,
	std::shared_ptr<const Session::Buffer> pingData)
{
	if (m_session) {
		writeLog(LogLevel::Error, "Already connected");
		return false;
	}

	boost::system::error_code ec;
	const asio::ip::address address = asio::ip::make_address(host, ec);
	if (ec) {
		writeLog(LogLevel::Error, "Invalid address ", host, ": ", ec.message());
		return false;
	}

	Connector connector(tcp::endpoint(address, port));
	tcp::socket socket(m_ioContext);
	if (!connector.connect(socket)) {
		return false;
	}

	// onConnected도 네트워크 스레드에서 호출되도록 start()를 넘김
	m_session = std::make_shared<TcpSession>(std::move(socket), m_handler, heartbeat, std::move(pingData), 0);
	asio::post(m_ioContext, [session = m_session] { session->start(); });
	m_networkThread = std::thread([this] { m_ioContext.run(); });
	return true;
}

void TcpClient::send(std::shared_ptr<const Session::Buffer> data)
{
	if (!m_session) {
		return;
	}

	asio::post(m_ioContext, [session = m_session, data = std::move(data)] {
		session->send(data);
	});
}

void TcpClient::close()
{
	if (!m_networkThread.joinable()) {
		return;
	}

	asio::post(m_ioContext, [session = m_session] { session->close(); });
	m_networkThread.join();
}
}
