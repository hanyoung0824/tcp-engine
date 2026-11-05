#pragma once

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// 연결된 상대 하나와의 연결 상태를 관리하고 바이트를 주고받는 역할 (네트워크 계층).
// 서버(접속한 클라이언트마다 하나)와 클라이언트(서버와의 연결 하나)가 함께 쓴다.
// 패킷의 형식(헤더, 패킷 경계)은 모른다. 받은 바이트를 수신 버퍼에 쌓아 수신 콜백(ReceiveHandler)으로 넘기고,
// 콜백이 "앞에서부터 몇 바이트를 처리했는지" 돌려주면 그만큼만 버퍼에서 지운다.
// 덜 온 패킷의 나머지 바이트는 버퍼에 남아 다음에 받은 바이트와 이어진다.
// 콜백이 잘못된 데이터라고 알리면 연결을 끊는다.
//
// 읽기와 쓰기는 서로 독립적으로 진행된다.
// - 읽기: 항상 하나가 걸려 있다. 받으면 콜백으로 넘기고 바로 다음 읽기를 건다.
// - 쓰기: send()로 들어온 데이터를 큐에 쌓고, async_write는 한 번에 하나씩만 진행한다.
//
// 비동기 작업이 끝나기 전에 Session이 사라지면 안 되므로 반드시 std::make_shared로 만든다.
// 세션 목록과, 진행 중인 비동기 작업의 콜백이 shared_ptr로 Session을 붙잡고 있다.
//
// 소켓은 여러 스레드가 동시에 건드리면 안전하지 않다. 그래서 이 클래스의 함수는
// io_context.run()을 실행하는 스레드에서만 호출한다 (다른 스레드에서는 asio::post로 넘긴다).
// 예외로 isClosed()는 atomic 변수만 읽으므로 어느 스레드에서 불러도 된다.
class Session : public std::enable_shared_from_this<Session>
{
public:
	using SessionId = std::uint64_t;

	// 주고받는 바이트 배열
	using Buffer = std::vector<std::uint8_t>;

	// 연결이 닫혔을 때 세션 목록(SessionManager)에 알리는 콜백 (엔진 내부용).
	using CloseCallback = std::function<void(SessionId id)>;

	// 접속, 종료처럼 세션 하나에 대한 알림을 받는 함수
	using SessionCallback = std::function<void(const std::shared_ptr<Session>& session)>;

	// 바이트를 받았을 때 호출된다. 받은 바이트를 무엇으로 해석할지는 이 콜백을 넘기는 쪽(응용)이 정한다.
	// data는 아직 처리되지 않은 바이트 전체이며 이 함수 안에서만 유효하다.
	// 앞에서부터 처리한 바이트 수를 반환한다 (0이면 아직 패킷 하나가 다 오지 않은 것).
	// 잘못된 데이터면 nullopt를 반환한다. 연결을 끊는 것은 Session이 한다.
	using ReceiveHandler = std::function<std::optional<std::size_t>(
		const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data)>;

	// 응용이 받는 세션 이벤트. 모두 io_context.run()을 실행하는 스레드에서 호출된다.
	struct EventHandler
	{
		SessionCallback onConnected; // 연결되어 읽기를 시작하기 직전 (없어도 된다)
		ReceiveHandler onReceive; // 바이트를 받았을 때 (반드시 있어야 한다)
		SessionCallback onDisconnected; // 연결이 닫혔을 때 한 번. 상대가 끊었든 이쪽이 close()했든 호출된다 (없어도 된다)
	};

public:
	// handler: 응용이 받을 이벤트. 여러 세션이 같은 내용을 복사해 쓴다.
	// id와 onClosed는 세션 목록(SessionManager)이 관리할 때 쓴다. 목록 없이 쓰면 생략한다 (예: 클라이언트).
	Session(boost::asio::ip::tcp::socket socket, EventHandler handler,
		SessionId id = 0, CloseCallback onClosed = {});
	~Session();

	// onConnected를 알리고 읽기를 시작한다. 바로 반환하고, 이후는 콜백으로 진행된다.
	void start();

	// data를 보낸다. 바로 반환하고, 실제 전송은 쓰기 큐를 통해 차례로 진행된다.
	// 여러 세션이 같은 데이터를 공유할 수 있도록 shared_ptr로 받는다 (보내는 동안 살아 있어야 한다).
	// data가 비어 있으면(nullptr) 아무것도 하지 않는다.
	void send(std::shared_ptr<const Buffer> data);

	// 연결을 닫고 onDisconnected, 세션 목록 순서로 알린다. 여러 번 호출해도 한 번만 알린다.
	void close();

	// 연결이 닫혔는지 (상대가 끊었거나 close()를 호출했거나). 어느 스레드에서 불러도 된다.
	bool isClosed() const { return m_closed.load(); }
	SessionId getId() const { return m_id; }
	const std::string& getName() const { return m_name; }

private:
	// 수신 버퍼의 빈 곳으로 바이트가 오기를 비동기로 기다린다.
	void startRead();

	// 받으면 수신 콜백으로 넘기고, 처리한 만큼 버퍼에서 지운 뒤 다음 읽기를 건다.
	void onRead(const boost::system::error_code& ec, std::size_t length);

	// 쓰기 큐의 맨 앞 데이터를 비동기로 보낸다.
	void startWrite();

	// 다 보내면(또는 실패하면) 호출된다. 큐에 남은 게 있으면 이어서 보낸다.
	void onWrite(const boost::system::error_code& ec, std::size_t length);

private:
	boost::asio::ip::tcp::socket m_socket; // 상대와 연결된 소켓
	EventHandler m_handler; // 응용이 받을 이벤트
	const SessionId m_id; // 세션 ID
	std::string m_name; // 상대의 "IP:포트" (로그, 보낸 사람 표시용)
	std::atomic<bool> m_closed{ false }; // close()가 이미 호출됐는지 (다른 스레드도 읽는다)

	CloseCallback m_onClosed; // 연결이 닫혔을 때

	Buffer m_receiveBuffer; // 수신 버퍼 (크기 고정). 앞쪽 m_receivedLength 바이트가 아직 처리되지 않은 데이터.
	std::size_t m_receivedLength = 0; // 수신 버퍼에 쌓인 바이트 수
	std::deque<std::shared_ptr<const Buffer>> m_writeQueue; // 보낼 데이터. 맨 앞이 지금 보내는 중인 것.
};
