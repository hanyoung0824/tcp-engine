#include "TcpConnection.hpp"

#include "EngineLog.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace tcpengine
{
namespace
{
	constexpr std::size_t receiveBufferSize = 64 * 1024;
}

TcpConnection::TcpConnection(tcp::socket socket, std::shared_ptr<const ConnectionEventHandler> handler,
	ConnectionId id, ConnectionOptions options, CloseCallback onClosed)
	: m_socket(std::move(socket)),
	m_handler(std::move(handler)),
	m_id(id),
	m_options(options),
	m_closeTimer(m_socket.get_executor()),
	m_onClosed(std::move(onClosed)),
	m_receiveBuffer(receiveBufferSize)
{
	boost::system::error_code ec;
	std::ostringstream address;
	address << m_socket.remote_endpoint(ec);
	m_address = address.str();

	// 작은 패킷을 ACK가 올 때까지 붙잡아 두는 Nagle 알고리즘을 끔. 여러 패킷은 이미 한 번의 쓰기로 묶어 보냄
	m_socket.set_option(tcp::no_delay(true), ec);
}

TcpConnection::~TcpConnection()
{
	engineLogDebug("Connection destroyed: ", m_address);
}

void TcpConnection::start()
{
	engineLogInfo("Connected: ", m_address);

	m_handler->onConnected(shared_from_this());

	// onConnected에서 close()했어도 상대가 끊는 것을 알아야 하므로 읽기는 시작함
	if (m_state != State::Closed) {
		startRead();
	}
}

void TcpConnection::send(std::shared_ptr<const Buffer> data)
{
	if (m_state != State::Open || !data) {
		return;
	}

	if (m_options.sendQueueLimit && m_queuedBytes + data->size() > *m_options.sendQueueLimit) {
		engineLogWarning("Send queue over ", *m_options.sendQueueLimit / 1024, "KB for ", m_address, ", resetting");
		++m_options.stats->sendQueueResets;
		reset();
		return;
	}

	m_queuedBytes += data->size();
	m_options.stats->recordSendQueue(m_queuedBytes);
	m_pendingPackets.push_back(OutgoingPacket{ std::move(data) });

	if (m_writingPackets.empty()) {
		startWrite();
	}
}

void TcpConnection::close()
{
	State expected = State::Open;
	if (!m_state.compare_exchange_strong(expected, State::Closing)) {
		return;
	}

	if (m_options.closeTimeout.count() == 0) {
		abort();
		return;
	}

	m_closeTimer.expires_after(m_options.closeTimeout);
	m_closeTimer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
		if (ec || self->m_state == State::Closed) {
			return;
		}
		engineLogWarning("Close timed out for ", self->m_address, ", resetting");
		self->reset();
	});

	if (m_writingPackets.empty()) {
		shutdownSend();
	}
}

void TcpConnection::abort()
{
	finish(false);
}

void TcpConnection::reset()
{
	if (m_state == State::Closed) {
		return;
	}
	m_state = State::Closing;
	// 브로드캐스트처럼 연결 목록을 도는 중에 불릴 수 있어, 그 목록을 바꾸는 onDisconnected는 나중에 부름
	asio::post(m_socket.get_executor(), [self = shared_from_this()] { self->finish(true); });
}

void TcpConnection::finish(bool sendReset)
{
	if (m_state == State::Closed) {
		return;
	}
	m_state = State::Closed;
	m_closeTimer.cancel();

	boost::system::error_code ec;
	if (sendReset) {
		// linger 0으로 닫으면 커널 송신 버퍼에 남은 데이터를 버리고 RST를 보냄. 받지 않는 상대에게 버퍼를 묶어 두지 않음
		m_socket.set_option(tcp::socket::linger(true, 0), ec);
	}
	else {
		m_socket.shutdown(tcp::socket::shutdown_both, ec);
	}
	m_socket.close(ec);

	m_handler->onDisconnected(shared_from_this());

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
	if (m_state == State::Closed) {
		return;
	}

	if (ec) {
		if (m_state == State::Closing) {
			engineLogDebug("Closed: ", m_address);
		}
		else if (ec == asio::error::eof) {
			engineLogInfo("Disconnected by peer: ", m_address);
		}
		else if (ec != asio::error::operation_aborted) {
			engineLogWarning("Read error from ", m_address, ": ", ec.message());
		}
		abort();
		return;
	}

	m_options.stats->bytesIn += length;

	// 닫는 중에는 상대가 끊을 때까지 받은 데이터를 처리하지 않고 버림
	if (m_state == State::Closing) {
		m_receivedLength = 0;
		startRead();
		return;
	}

	m_receivedLength += length;

	const std::optional<std::size_t> result = m_handler->onReceive(shared_from_this(),
		std::span<const std::uint8_t>(m_receiveBuffer.data(), m_receivedLength));

	if (m_state == State::Closed) {
		return;
	}

	if (m_state == State::Closing) {
		m_receivedLength = 0;
		startRead();
		return;
	}

	if (!result) {
		engineLogWarning("Invalid data from ", m_address, ", closing");
		abort();
		return;
	}

	const std::size_t processed = *result;
	if (processed > m_receivedLength) {
		engineLogError("Receive handler processed more bytes than received, closing ", m_address);
		abort();
		return;
	}

	// 처리한 게 없으면 건너뜀. std::copy는 같은 자리로 복사 불가
	if (processed > 0) {
		std::copy(m_receiveBuffer.begin() + processed, m_receiveBuffer.begin() + m_receivedLength, m_receiveBuffer.begin());
		m_receivedLength -= processed;
	}

	if (m_receivedLength == m_receiveBuffer.size()) {
		engineLogWarning("Receive buffer full from ", m_address, ", closing");
		abort();
		return;
	}

	startRead();
}

void TcpConnection::startWrite()
{
	// 쌓인 패킷을 한 번의 쓰기로 묶어 시스템 호출 수를 줄임. 맞바꾸기만 하므로 패킷을 옮기지 않음
	// 전체 크기는 sendQueueLimit이 막고, 시스템 호출 한 번에 넘기는 개수는 Asio가 OS 한도에 맞춰 나눔
	std::swap(m_pendingPackets, m_writingPackets);

	// async_write는 버퍼 목록을 값으로 복사해 두므로, vector 대신 가리키기만 하는 span으로 넘김
	asio::async_write(m_socket, std::span<const OutgoingPacket>(m_writingPackets),
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onWrite(ec, length);
		});
}

void TcpConnection::onWrite(const boost::system::error_code& ec, std::size_t length)
{
	if (m_state == State::Closed) {
		return;
	}

	if (ec) {
		if (ec != asio::error::operation_aborted) {
			engineLogWarning("Write error to ", m_address, ": ", ec.message());
		}
		abort();
		return;
	}

	m_options.stats->packetsOut += m_writingPackets.size();
	m_options.stats->bytesOut += length;
	m_queuedBytes -= length;
	m_options.stats->recordSendQueue(m_queuedBytes);
	m_writingPackets.clear();

	if (!m_pendingPackets.empty()) {
		startWrite();
	}
	else if (m_state == State::Closing) {
		shutdownSend();
	}
}

void TcpConnection::shutdownSend()
{
	boost::system::error_code ec;
	m_socket.shutdown(tcp::socket::shutdown_send, ec);
}
}
