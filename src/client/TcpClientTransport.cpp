#include "TcpClientTransport.hpp"

#include "EngineLog.hpp"

#include <string>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
namespace
{
	// 접속 시도 하나에 필요한 것. 이름 풀이, 연결, 시간 제한 콜백이 함께 잡고 있다가 끝나면 사라짐
	struct ConnectAttempt
	{
		explicit ConnectAttempt(asio::io_context& ioContext)
			: resolver(ioContext),
			socket(ioContext),
			timer(ioContext)
		{
		}

		tcp::resolver resolver;
		tcp::socket socket;
		asio::steady_timer timer;
		bool timedOut = false;
	};
}

TcpClientTransport::TcpClientTransport(ConnectionOptions connectionOptions)
	: m_connectionOptions(connectionOptions)
{
}

TcpClientTransport::~TcpClientTransport()
{
	stop();
}

void TcpClientTransport::start()
{
	if (m_networkThread.joinable()) {
		return;
	}
	m_workGuard.emplace(m_ioContext.get_executor());
	m_networkThread = std::thread([this] { m_ioContext.run(); });
}

void TcpClientTransport::connect(const std::string& host, unsigned short port, std::chrono::seconds timeout,
	std::shared_ptr<const ConnectionEventHandler> handler, ConnectResult onResult)
{
	asio::post(m_ioContext, [this, host, port, timeout, handler = std::move(handler), onResult = std::move(onResult)]() mutable {
		auto attempt = std::make_shared<ConnectAttempt>(m_ioContext);

		attempt->timer.expires_after(timeout);
		attempt->timer.async_wait([attempt](const boost::system::error_code& ec) {
			if (ec) {
				return;
			}
			attempt->timedOut = true;
			boost::system::error_code ignored;
			attempt->resolver.cancel();
			attempt->socket.close(ignored);
		});

		const auto fail = [attempt, host, port, onResult](const std::string& reason) {
			attempt->timer.cancel();
			engineLogWarning("Failed to connect to ", host, ':', port, ": ", attempt->timedOut ? "timed out" : reason);
			onResult(false);
		};

		attempt->resolver.async_resolve(host, std::to_string(port),
			[this, attempt, handler = std::move(handler), onResult, fail](const boost::system::error_code& ec,
				const tcp::resolver::results_type& endPoints) mutable {
				if (ec) {
					fail(ec.message());
					return;
				}
				asio::async_connect(attempt->socket, endPoints,
					[this, attempt, handler = std::move(handler), onResult, fail](const boost::system::error_code& connectError,
						const tcp::endpoint&) mutable {
						if (connectError || attempt->timedOut) {
							fail(connectError.message());
							return;
						}
						attempt->timer.cancel();
						m_connection = std::make_shared<TcpConnection>(std::move(attempt->socket), std::move(handler),
							m_nextConnectionId++, m_connectionOptions);
						m_connection->start();
						onResult(true);
					});
			});
	});
}

void TcpClientTransport::send(std::shared_ptr<const Connection::Buffer> data)
{
	asio::post(m_ioContext, [this, data = std::move(data)] {
		if (m_connection) {
			m_connection->send(data);
		}
	});
}

void TcpClientTransport::close()
{
	asio::post(m_ioContext, [this] {
		if (m_connection) {
			m_connection->close();
		}
	});
}

void TcpClientTransport::stop()
{
	if (!m_networkThread.joinable()) {
		return;
	}
	close();
	// 붙잡던 것을 놓으면 연결이 닫히고 타이머가 끝난 뒤 io_context가 스스로 끝남
	m_workGuard.reset();
	m_networkThread.join();
}
}
