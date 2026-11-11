#include "TcpConnection.hpp"

#include "Logger.hpp"

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

TcpConnection::TcpConnection(tcp::socket socket, std::shared_ptr<const ConnectionEventHandler> handler,
	ConnectionId id, CloseCallback onClosed)
	: m_socket(std::move(socket)),
	m_handler(std::move(handler)),
	m_id(id),
	m_onClosed(std::move(onClosed)),
	m_receiveBuffer(receiveBufferSize)
{
	boost::system::error_code ec;
	std::ostringstream address;
	address << m_socket.remote_endpoint(ec);
	m_address = address.str();
}

TcpConnection::~TcpConnection()
{
	engineLogDebug("Connection destroyed: ", m_address);
}

void TcpConnection::start()
{
	engineLogInfo("[thread ", std::this_thread::get_id(), "] Connected: ", m_address);

	if (m_handler->onConnected) {
		m_handler->onConnected(shared_from_this());
	}

	if (m_closed) {
		return;
	}

	startRead();
}

void TcpConnection::send(std::shared_ptr<const Buffer> data)
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

void TcpConnection::close()
{
	if (m_closed.exchange(true)) {
		return;
	}

	boost::system::error_code ec;
	m_socket.shutdown(tcp::socket::shutdown_both, ec);
	m_socket.close(ec);

	if (m_handler->onDisconnected) {
		m_handler->onDisconnected(shared_from_this());
	}

	if (m_onClosed) {
		m_onClosed(m_id);
	}
}

void TcpConnection::startRead()
{
	asio::mutable_buffer freeSpace = asio::buffer(m_receiveBuffer) + m_receivedLength;
	m_socket.async_read_some(freeSpace,
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onRead(ec, length);
		});
}

void TcpConnection::onRead(const boost::system::error_code& ec, std::size_t length)
{
	if (ec) {
		if (ec == asio::error::eof) {
			engineLogInfo("Disconnected by peer: ", m_address);
		}
		else if (ec != asio::error::operation_aborted) {
			engineLogWarning("Read error from ", m_address, ": ", ec.message());
		}
		close();
		return;
	}

	m_receivedLength += length;

	const std::optional<std::size_t> result = m_handler->onReceive(shared_from_this(),
		std::span<const std::uint8_t>(m_receiveBuffer.data(), m_receivedLength));

	if (m_closed) {
		return;
	}

	if (!result) {
		engineLogWarning("Invalid data from ", m_address, ", closing");
		close();
		return;
	}

	const std::size_t processed = *result;
	if (processed > m_receivedLength) {
		engineLogError("Receive handler processed more bytes than received, closing ", m_address);
		close();
		return;
	}

	// 처리한 게 없으면 건너뜀. std::copy는 같은 자리로 복사 불가
	if (processed > 0) {
		std::copy(m_receiveBuffer.begin() + processed, m_receiveBuffer.begin() + m_receivedLength, m_receiveBuffer.begin());
		m_receivedLength -= processed;
	}

	if (m_receivedLength == m_receiveBuffer.size()) {
		engineLogWarning("Receive buffer full from ", m_address, ", closing");
		close();
		return;
	}

	startRead();
}

void TcpConnection::startWrite()
{
	asio::async_write(m_socket, asio::buffer(*m_writeQueue.front()),
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onWrite(ec, length);
		});
}

void TcpConnection::onWrite(const boost::system::error_code& ec, std::size_t)
{
	if (ec) {
		if (ec != asio::error::operation_aborted) {
			engineLogWarning("Write error to ", m_address, ": ", ec.message());
		}
		close();
		return;
	}

	m_writeQueue.pop_front();
	if (!m_writeQueue.empty()) {
		startWrite();
	}
}
}
