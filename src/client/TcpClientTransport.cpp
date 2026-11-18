#include "TcpClientTransport.hpp"

#include "EngineLog.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
namespace
{
	// 이름 풀이 → 연결을 timeout 안에 한 번 시도함. 진행 중인 비동기 작업들이 잡고 있다가 끝나면 사라짐
	class ConnectAttempt : public std::enable_shared_from_this<ConnectAttempt>
	{
	public:
		using ConnectedHandler = std::function<void(tcp::socket socket)>;
		using FailedHandler = std::function<void()>;

		ConnectAttempt(asio::io_context& ioContext, std::string host, unsigned short port,
			ConnectedHandler onConnected, FailedHandler onFailed)
			: m_resolver(ioContext),
			m_socket(ioContext),
			m_timer(ioContext),
			m_host(std::move(host)),
			m_port(port),
			m_onConnected(std::move(onConnected)),
			m_onFailed(std::move(onFailed))
		{
		}

		void start(std::chrono::seconds timeout)
		{
			m_timer.expires_after(timeout);
			m_timer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
				if (!ec) {
					self->timeOut();
				}
			});

			m_resolver.async_resolve(m_host, std::to_string(m_port),
				[self = shared_from_this()](const boost::system::error_code& ec, const tcp::resolver::results_type& endpoints) {
					self->onResolved(ec, endpoints);
				});
		}

	private:
		void onResolved(const boost::system::error_code& ec, const tcp::resolver::results_type& endpoints)
		{
			if (ec) {
				fail(ec.message());
				return;
			}
			asio::async_connect(m_socket, endpoints,
				[self = shared_from_this()](const boost::system::error_code& connectError, const tcp::endpoint&) {
					self->onConnected(connectError);
				});
		}

		void onConnected(const boost::system::error_code& ec)
		{
			// 시간 제한과 연결 완료가 거의 동시에 일어나면 ec 없이 올 수 있음
			if (ec || m_timedOut) {
				fail(ec.message());
				return;
			}
			m_timer.cancel();
			m_onConnected(std::move(m_socket));
		}

		// 진행 중인 단계를 취소해 그 단계가 오류로 끝나게 함
		void timeOut()
		{
			m_timedOut = true;
			m_resolver.cancel();
			boost::system::error_code ignored;
			m_socket.close(ignored);
		}

		void fail(const std::string& reason)
		{
			m_timer.cancel();
			engineLogWarning("Failed to connect to ", m_host, ':', m_port, ": ", m_timedOut ? "timed out" : reason);
			m_onFailed();
		}

	private:
		tcp::resolver m_resolver;
		tcp::socket m_socket;
		asio::steady_timer m_timer;
		const std::string m_host;
		const unsigned short m_port;
		const ConnectedHandler m_onConnected;
		const FailedHandler m_onFailed;
		bool m_timedOut = false;
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
	std::shared_ptr<const ConnectionEvents> events, ConnectResult onResult)
{
	asio::post(m_ioContext, [this, host, port, timeout, events = std::move(events), onResult = std::move(onResult)] {
		const auto onConnected = [this, events, onResult](tcp::socket socket) {
			m_connection = std::make_shared<TcpConnection>(std::move(socket), events, m_nextConnectionId++, m_connectionOptions);
			m_connection->start();
			onResult(true);
		};
		const auto onFailed = [onResult] { onResult(false); };
		std::make_shared<ConnectAttempt>(m_ioContext, host, port, onConnected, onFailed)->start(timeout);
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
