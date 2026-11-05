#include "TcpClient.hpp"

#include "Connector.hpp"
#include "Log.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;

TcpClient::TcpClient(Session::EventHandler handler)
	: m_handler(std::move(handler))
{
}

TcpClient::~TcpClient()
{
	close();
}

bool TcpClient::connect(const std::string& host, unsigned short port)
{
	if (m_session) {
		writeLog(LogLevel::Error, "Already connected");
		return false;
	}

	// 1. 접속할 서버 주소 만들기
	boost::system::error_code ec;
	const asio::ip::address address = asio::ip::make_address(host, ec);
	if (ec) {
		writeLog(LogLevel::Error, "Invalid address ", host, ": ", ec.message());
		return false;
	}

	// 2. 서버에 접속 (아직 네트워크 스레드가 없으므로 동기로 접속한다)
	Connector connector(tcp::endpoint(address, port));
	tcp::socket socket(m_ioContext);
	if (!connector.connect(socket)) {
		return false;
	}

	// 3. 세션을 만들고 네트워크 스레드에서 이벤트 루프 실행.
	// start()도 네트워크 스레드로 넘겨, onConnected를 포함한 모든 세션 이벤트가 같은 스레드에서 호출되게 한다.
	m_session = std::make_shared<Session>(std::move(socket), m_handler);
	asio::post(m_ioContext, [session = m_session] { session->start(); });
	m_networkThread = std::thread([this] { m_ioContext.run(); });
	return true;
}

void TcpClient::send(std::shared_ptr<const Session::Buffer> data)
{
	if (!m_session) {
		return;
	}

	// asio::post: 함수를 io_context의 할 일 목록에 넣는다. 실제 실행은 네트워크 스레드에서 한다.
	asio::post(m_ioContext, [session = m_session, data = std::move(data)] {
		session->send(data);
	});
}

void TcpClient::close()
{
	if (!m_networkThread.joinable()) {
		return;
	}

	// 닫기도 네트워크 스레드에서 하도록 넘긴다.
	// 소켓이 닫히면 걸려 있던 작업이 끝나고, 할 일이 없어진 run()이 반환해 네트워크 스레드가 끝난다.
	// (서버가 먼저 끊어 run()이 이미 반환했다면 넘긴 일은 실행되지 않고, join은 바로 끝난다.)
	asio::post(m_ioContext, [session = m_session] { session->close(); });
	m_networkThread.join();
}
