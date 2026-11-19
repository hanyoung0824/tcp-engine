#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/client/ClientSettings.hpp"
#include "tcpengine/utility/Log.hpp"

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace boost::asio
{
class io_context;
}

namespace tcpengine
{
class ClientContext;
class ClientState;
enum class ClientStateId : std::uint8_t;
class ClientTransport;
struct ConnectionEvents;
class HeartbeatChecker;
class PacketRouter;

struct DisconnectInfo
{
	// 서버가 알리지 않고 끊었으면 없음
	std::optional<DisconnectReason> reason;
	bool willReconnect = false;
};

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
// 패킷 핸들러에서 예외가 나면 연결을 끊음. 그 밖에서 잡히지 않은 예외는 로그를 남기고 abort()하여 덤프를 남김
// 연결이 끊기면 reconnectMaxDelay에 따라 다시 접속하고, 세션이 있었으면 이어 감
class Client
{
public:
	virtual ~Client();

	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	// 설정이 틀리거나 처음 접속에 실패하면 1 반환
	// onRun()이 끝나면 세션이 있을 때 Logout을 보내고 닫은 뒤 0 반환
	int run();

protected:
	// protocolVersion은 응용 프로토콜 버전. 서버와 다르면 서버가 VersionMismatch로 끊음
	Client(const ClientSettings& settings, std::uint16_t protocolVersion);

	// handler(const std::shared_ptr<Connection>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addPacketDispatcher(T::id, detail::makePacketDispatcher<T, Connection>(std::move(handler)));
	}

	// 네트워크를 열기 직전의 응용 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
	virtual bool onStart() { return true; }

	const ClientSettings& getSettings() const { return m_settings; }

	// 메인 스레드에서 호출. 연결이 끊겨도 스스로 반환하지 않으므로 isConnected()로 확인 필요
	virtual void onRun() = 0;

	// 아래 훅은 네트워크 스레드에서 호출. 다시 접속할 때마다 onConnected부터 다시 불림
	virtual void onConnected(const std::shared_ptr<Connection>&) {}

	virtual void onDisconnected(const std::shared_ptr<Connection>&, const DisconnectInfo&) {}

	// 재접속했고 이전 세션이 이어짐
	virtual void onSessionResumed() {}

	// 재접속했지만 세션이 이미 끝나 로그인부터 다시 해야 함
	virtual void onSessionLost() {}

	// 어느 스레드에서든 호출 가능. 연결이 없으면 버림
	void send(const PacketBase& packet);

	bool isConnected() const;

private:
	// 상태 객체가 Client에 일을 시키는 통로
	friend class ClientContext;

	// 아래 셋은 네트워크 스레드가 돌지 않을 때는 메인 스레드, 도는 동안은 네트워크 스레드에서만 호출
	// 상태 객체의 함수 안에서도 부를 수 있도록 이전 상태 객체는 finishStateChange()까지 살려 둠
	void changeState(std::unique_ptr<ClientState> next);

	// 지금 상태에 이벤트를 넘기고 그 사이 바뀐 상태를 마무리함
	void dispatch(const std::function<void(ClientState&)>& event);

	// 이전 상태 객체를 없애고 새 상태의 onEnter()를 부름
	void finishStateChange();

	// 첫 접속 실패나 시작 실패 뒤 정리
	int stopWithError();

	// 어느 스레드에서든 호출 가능
	ClientStateId getStateId() const;

	bool validateSettings() const;

	bool callOnStart();

	// 지원하지 않는 transport면 false
	bool createComponents();

	// 처음 접속은 실패해도 다시 시도하지 않음
	std::future<bool> postFirstConnect();

	void startNetworkThread();

	// 연결을 닫고 남은 작업이 끝날 때까지 기다림. 네트워크 스레드가 아닌 곳에서 호출
	void stopNetworkThread();

	// 메인 스레드에서 네트워크 스레드의 quit()이 끝날 때까지 기다림
	void quitAndWait();

	// 결과는 네트워크 스레드에서 onResult로 알림
	void connect(std::function<void(bool connected)> onResult);

	// 아래는 네트워크 스레드에서만 호출. 연결 이벤트는 지금 상태 객체에 넘김
	void reconnect();

	void quit();

	void handleConnected(const std::shared_ptr<Connection>& connection);

	void handleDisconnected(const std::shared_ptr<Connection>& connection);

	bool handleServerHello(const std::shared_ptr<Connection>& connection, PacketReader& body);

	bool handleDisconnectNotice(PacketReader& body);

	bool handleSessionStarted(PacketReader& body);

	bool handleSessionResumed(PacketReader& body);

	bool handleResumeFailed(PacketReader& body);

	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	struct WorkGuard;

	// 멈춘 뒤 남은 핸들러가 다른 멤버를 잡고 있을 수 있어 가장 나중에 소멸
	std::unique_ptr<boost::asio::io_context> m_ioContext;
	std::unique_ptr<PacketRouter> m_packetRouter;
	std::unique_ptr<ClientTransport> m_transport;
	std::shared_ptr<const ConnectionEvents> m_connectionEvents;
	std::unique_ptr<HeartbeatChecker> m_heartbeatChecker;
	// 연결도 타이머도 없는 동안에도 run()이 반환하지 않게 붙잡음. stopNetworkThread()에서 놓음
	std::unique_ptr<WorkGuard> m_workGuard;
	std::thread m_networkThread;
	const ClientSettings m_settings;
	const std::uint16_t m_protocolVersion;
	// 상태 객체들이 참조하므로 상태보다 먼저 만들고 나중에 없앰
	std::unique_ptr<ClientContext> m_context;
	// m_state를 바꿀 때와 다른 스레드가 읽을 때만 잠금. 바꾸는 스레드 안에서 읽을 때는 잠그지 않음
	mutable std::mutex m_stateMutex;
	std::unique_ptr<ClientState> m_state;
	std::vector<std::unique_ptr<ClientState>> m_retiredStates;
	bool m_enterPending = false;
	// 응용 훅에서 연결을 닫아 이벤트 안에서 이벤트가 다시 올 수 있음. 가장 바깥 이벤트가 끝날 때만 마무리
	int m_dispatchDepth = 0;

	// 아래는 네트워크 스레드에서만 사용
	// 비어 있으면 세션 없음
	std::string m_sessionToken;
	// 서버가 Disconnect로 알려 준 이유. 연결이 끊길 때 응용에 넘기고 지움
	std::optional<DisconnectReason> m_disconnectReason;
};
}
