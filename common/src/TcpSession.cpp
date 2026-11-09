#include "TcpSession.hpp"

#include "Log.hpp"

#include <algorithm>
#include <sstream>
#include <thread>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
namespace
{
	constexpr std::size_t receiveBufferSize = 64 * 1024;
}

TcpSession::TcpSession(tcp::socket socket, std::shared_ptr<const SessionConfig> config,
	SessionId id, CloseCallback onClosed)
	: m_socket(std::move(socket)),
	m_config(std::move(config)),
	m_heartbeatTimer(m_socket.get_executor()),
	m_id(id),
	m_onClosed(std::move(onClosed)),
	m_receiveBuffer(receiveBufferSize)
{
	boost::system::error_code ec;
	std::ostringstream address;
	address << m_socket.remote_endpoint(ec);
	m_address = address.str();
}

TcpSession::~TcpSession()
{
	writeLog(LogLevel::Debug, "Session destroyed: ", m_address);
}

void TcpSession::start()
{
	writeLog(LogLevel::Info, "[thread ", std::this_thread::get_id(), "] Connected: ", m_address);

	if (m_config->handler.onConnected) {
		m_config->handler.onConnected(shared_from_this());
	}

	if (m_closed) {
		return;
	}

	m_lastReceiveTime = std::chrono::steady_clock::now();
	startRead();

	if (m_config->heartbeat.interval.count() > 0) {
		startHeartbeat();
	}
}

void TcpSession::send(std::shared_ptr<const Buffer> data)
{
	if (m_closed || !data) {
		return;
	}

	const bool writing = !m_writeQueue.empty();
	m_writeQueue.push_back(std::move(data));

	if (!writing) {
		startWrite();
	}
}

void TcpSession::close()
{
	if (m_closed.exchange(true)) {
		return;
	}

	m_heartbeatTimer.cancel();

	boost::system::error_code ec;
	m_socket.shutdown(tcp::socket::shutdown_both, ec);
	m_socket.close(ec);

	if (m_config->handler.onDisconnected) {
		m_config->handler.onDisconnected(shared_from_this());
	}

	if (m_onClosed) {
		m_onClosed(m_id);
	}
}

void TcpSession::startRead()
{
	asio::mutable_buffer freeSpace = asio::buffer(m_receiveBuffer) + m_receivedLength;
	m_socket.async_read_some(freeSpace,
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onRead(ec, length);
		});
}

void TcpSession::onRead(const boost::system::error_code& ec, std::size_t length)
{
	if (ec) {
		if (ec == asio::error::eof) {
			writeLog(LogLevel::Info, "Disconnected by peer: ", m_address);
		}
		else if (ec != asio::error::operation_aborted) {
			writeLog(LogLevel::Warning, "Read error from ", m_address, ": ", ec.message());
		}
		close();
		return;
	}

	m_lastReceiveTime = std::chrono::steady_clock::now();
	m_receivedLength += length;

	const std::optional<std::size_t> result = m_config->handler.onReceive(shared_from_this(),
		std::span<const std::uint8_t>(m_receiveBuffer.data(), m_receivedLength));

	if (m_closed) {
		return;
	}

	if (!result) {
		writeLog(LogLevel::Warning, "Invalid data from ", m_address, ", closing");
		close();
		return;
	}

	const std::size_t processed = *result;
	if (processed > m_receivedLength) {
		writeLog(LogLevel::Error, "Receive handler processed more bytes than received, closing ", m_address);
		close();
		return;
	}

	// 처리한 게 없으면 건너뜀. std::copy는 같은 자리로 복사 불가
	if (processed > 0) {
		std::copy(m_receiveBuffer.begin() + processed, m_receiveBuffer.begin() + m_receivedLength, m_receiveBuffer.begin());
		m_receivedLength -= processed;
	}

	if (m_receivedLength == m_receiveBuffer.size()) {
		writeLog(LogLevel::Warning, "Receive buffer full from ", m_address, ", closing");
		close();
		return;
	}

	startRead();
}

void TcpSession::startWrite()
{
	asio::async_write(m_socket, asio::buffer(*m_writeQueue.front()),
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onWrite(ec, length);
		});
}

void TcpSession::onWrite(const boost::system::error_code& ec, std::size_t)
{
	if (ec) {
		if (ec != asio::error::operation_aborted) {
			writeLog(LogLevel::Warning, "Write error to ", m_address, ": ", ec.message());
		}
		close();
		return;
	}

	m_writeQueue.pop_front();
	if (!m_writeQueue.empty()) {
		startWrite();
	}
}

void TcpSession::startHeartbeat()
{
	m_heartbeatTimer.expires_after(m_config->heartbeat.interval);
	m_heartbeatTimer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
		self->onHeartbeat(ec);
	});
}

void TcpSession::onHeartbeat(const boost::system::error_code& ec)
{
	if (ec || m_closed) {
		return;
	}

	const auto idleTime = std::chrono::steady_clock::now() - m_lastReceiveTime;
	if (m_config->heartbeat.timeout.count() > 0 && idleTime >= m_config->heartbeat.timeout) {
		writeLog(LogLevel::Warning, "Heartbeat timeout from ", m_address, ", closing");
		close();
		return;
	}

	send(m_config->pingData);
	startHeartbeat();
}
}
