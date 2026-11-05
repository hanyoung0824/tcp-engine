#include "Session.hpp"

#include "Log.hpp"

#include <algorithm>
#include <sstream>
#include <thread>

namespace asio = boost::asio;
using asio::ip::tcp;

namespace
{
	// 수신 버퍼 크기. 처리되지 않은 바이트가 이만큼 쌓이면(끝나지 않는 데이터) 연결을 끊는다.
	// 패킷 형식과 상관없이, 상대가 메모리를 무한정 쓰게 만들지 못하도록 막는 장치다.
	constexpr std::size_t receiveBufferSize = 64 * 1024;
}

Session::Session(tcp::socket socket, EventHandler handler, SessionId id, CloseCallback onClosed)
	: m_socket(std::move(socket)),
	m_handler(std::move(handler)),
	m_id(id),
	m_onClosed(std::move(onClosed)),
	m_receiveBuffer(receiveBufferSize)
{
	boost::system::error_code ec;
	std::ostringstream name;
	name << m_socket.remote_endpoint(ec);
	m_name = name.str();
}

Session::~Session()
{
	// 목록에서 빠지고 마지막 콜백도 끝나 아무도 이 Session을 붙잡지 않을 때 호출된다.
	writeLog(LogLevel::Debug, "Session destroyed: ", m_name);
}

void Session::start()
{
	writeLog(LogLevel::Info, "[thread ", std::this_thread::get_id(), "] Connected: ", m_name);

	// 1. 접속을 알린다. 응용은 여기서 이 세션의 데이터를 준비하거나 다른 세션에 알릴 수 있다.
	if (m_handler.onConnected) {
		m_handler.onConnected(shared_from_this());
	}

	// 2. 알리는 중에 닫혔으면(예: 응용이 접속을 거부) 읽기를 시작하지 않는다.
	if (m_closed) {
		return;
	}

	startRead();
}

void Session::send(std::shared_ptr<const Buffer> data)
{
	if (m_closed || !data) {
		return;
	}

	// 1. 큐에 쌓는다.
	const bool writing = !m_writeQueue.empty();
	m_writeQueue.push_back(std::move(data));

	// 2. 이미 보내는 중이면 onWrite가 이어서 보내므로 여기서는 시작하지 않는다.
	// 같은 소켓에 async_write를 동시에 두 개 걸면 두 데이터가 섞여 전송될 수 있다.
	if (!writing) {
		startWrite();
	}
}

void Session::close()
{
	// exchange: true로 바꾸면서 이전 값을 돌려준다. 이미 true였으면 닫는 중이거나 닫힌 것이다.
	if (m_closed.exchange(true)) {
		return;
	}

	// 1. 소켓을 닫으면 걸려 있던 async_read_some / async_write는 operation_aborted로 끝난다.
	boost::system::error_code ec;
	m_socket.shutdown(tcp::socket::shutdown_both, ec);
	m_socket.close(ec);

	// 2. 응용에 종료를 알린다. 응용은 여기서 이 세션의 데이터를 정리한다.
	if (m_handler.onDisconnected) {
		m_handler.onDisconnected(shared_from_this());
	}

	// 3. 세션 목록에 알린다 (목록에서 빠진다). 남은 콜백들이 끝나면 Session이 소멸한다.
	if (m_onClosed) {
		m_onClosed(m_id);
	}
}

void Session::startRead()
{
	// async_read_some: 조금이라도 오면 바로 콜백을 호출한다 (몇 바이트가 올지는 정해져 있지 않다).
	// 이미 쌓여 있는 바이트 뒤, 버퍼의 빈 곳에 받는다.
	asio::mutable_buffer freeSpace = asio::buffer(m_receiveBuffer) + m_receivedLength;
	m_socket.async_read_some(freeSpace,
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onRead(ec, length);
		});
}

void Session::onRead(const boost::system::error_code& ec, std::size_t length)
{
	if (ec) {
		if (ec == asio::error::eof) {
			writeLog(LogLevel::Info, "Disconnected by peer: ", m_name);
		}
		else if (ec != asio::error::operation_aborted) {
			writeLog(LogLevel::Warning, "Read error from ", m_name, ": ", ec.message());
		}
		close();
		return;
	}

	m_receivedLength += length;

	// 1. 쌓인 바이트 전체를 수신 콜백으로 넘긴다. 무엇인지, 어디까지가 패킷인지는 콜백이 정한다.
	const std::optional<std::size_t> result = m_handler.onReceive(shared_from_this(),
		std::span<const std::uint8_t>(m_receiveBuffer.data(), m_receivedLength));

	// 2. 처리 중에 닫혔으면(예: 패킷 처리 중 close() 호출) 더 받지 않는다.
	if (m_closed) {
		return;
	}

	// 3. 잘못된 데이터면 끊는다. 자세한 이유는 콜백 쪽에서 남긴다.
	if (!result) {
		writeLog(LogLevel::Warning, "Invalid data from ", m_name, ", closing");
		close();
		return;
	}

	const std::size_t processed = *result;
	if (processed > m_receivedLength) {
		writeLog(LogLevel::Error, "Receive handler processed more bytes than received, closing ", m_name);
		close();
		return;
	}

	// 4. 처리한 만큼 지우고, 남은 바이트(덜 온 패킷)를 버퍼 앞으로 옮긴다.
	// (처리한 게 없으면 옮길 필요가 없다. std::copy는 같은 자리로 복사하는 것을 허용하지 않는다.)
	if (processed > 0) {
		std::copy(m_receiveBuffer.begin() + processed, m_receiveBuffer.begin() + m_receivedLength, m_receiveBuffer.begin());
		m_receivedLength -= processed;
	}

	// 5. 버퍼가 꽉 찼는데 처리된 게 없으면 더 받을 곳이 없다 (끝나지 않는 데이터).
	if (m_receivedLength == m_receiveBuffer.size()) {
		writeLog(LogLevel::Warning, "Receive buffer full from ", m_name, ", closing");
		close();
		return;
	}

	startRead();
}

void Session::startWrite()
{
	// asio::async_write: 맨 앞 데이터를 전부 보내면 콜백이 호출된다.
	// 데이터는 큐 안의 shared_ptr가 붙잡고 있으므로 보내는 동안 사라지지 않는다.
	asio::async_write(m_socket, asio::buffer(*m_writeQueue.front()),
		[self = shared_from_this()](const boost::system::error_code& ec, std::size_t length) {
			self->onWrite(ec, length);
		});
}

void Session::onWrite(const boost::system::error_code& ec, std::size_t)
{
	if (ec) {
		if (ec != asio::error::operation_aborted) {
			writeLog(LogLevel::Warning, "Write error to ", m_name, ": ", ec.message());
		}
		close();
		return;
	}

	// 다 보낸 데이터를 빼고, 남은 게 있으면 이어서 보낸다.
	m_writeQueue.pop_front();
	if (!m_writeQueue.empty()) {
		startWrite();
	}
}
