#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/client/ClientSettings.hpp"
#include "tcpengine/utility/Log.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace tcpengine
{
class ClientTransport;
struct ConnectionEventHandler;
class HeartbeatMonitor;
class PacketManager;
class Reconnector;

// 연결이 끊긴 뒤 응용에 알리는 내용
struct DisconnectInfo
{
	// 서버가 끊기 전에 알려 준 이유. 알리지 않고 끊겼으면 없음
	std::optional<DisconnectReason> reason;
	// 엔진이 다시 접속할 예정인지
	bool reconnecting = false;
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

	// 설정의 log대로 로그 출력을 시작하고 실행함. 설정 검사도 여기서 하며 실패하면 이유를 남기고 1 반환
	// 처음 접속에 실패해도 1. 접속하면 onRun()이 끝날 때 서버에 Logout을 보내고 닫은 뒤 0 반환
	int run();

protected:
	// 설정 파일에서 읽으려면 ClientSettings::create() 또는 ClientSettings::fromJson()을 씀
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

	// 다시 접속해 이전 세션을 이어 감
	virtual void onSessionResumed() {}

	// 다시 접속했지만 세션이 이미 끝나 로그인부터 다시 해야 함
	virtual void onSessionLost() {}

	// 연결이 없으면 버림. 어느 스레드에서 불러도 됨
	void send(const PacketBase& packet);

	bool isConnected() const { return m_connected; }

private:
	// m_settings를 검증하고 실행
	int start();

	// 엔진 제어 패킷(ServerHello, Disconnect, 세션) 처리 등록
	void registerControlHandlers();

	// transport가 연결 이벤트를 알릴 콜백. 하트비트, 재접속과 응용 훅으로 이어 줌
	std::shared_ptr<const ConnectionEventHandler> makeConnectionEvents();

	// 네트워크 스레드에서 접속을 시도함
	void connect(std::function<void(bool connected)> onResult);

	// onRun()이 끝남. 세션이 있으면 Logout을 보내고 닫음. 네트워크 스레드에서 호출
	void quit();

	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	// 뒤에 둔 것이 먼저 소멸함. 재접속 타이머 → 하트비트 → transport(네트워크 스레드) → PacketManager 순
	std::unique_ptr<PacketManager> m_packetManager;
	// run() 때 settings.transport에 맞게 만듦
	std::unique_ptr<ClientTransport> m_transport;
	std::shared_ptr<const ConnectionEventHandler> m_connectionEvents;
	std::unique_ptr<HeartbeatMonitor> m_heartbeatMonitor;
	std::unique_ptr<Reconnector> m_reconnector;
	const ClientSettings m_settings;
	const std::uint16_t m_protocolVersion;
	bool m_running = false;
	// 메인 스레드가 읽으므로 atomic
	std::atomic<bool> m_connected{ false };

	// 아래는 네트워크 스레드에서만 사용
	// 서버가 준 재접속 토큰. 비어 있으면 세션 없음
	std::string m_sessionToken;
	// 서버가 Disconnect로 알려 준 이유. 연결이 끊길 때 응용에 넘기고 지움
	std::optional<DisconnectReason> m_disconnectReason;
	bool m_quitting = false;
};
}
