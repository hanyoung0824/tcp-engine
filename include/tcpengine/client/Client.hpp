#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/client/ClientSettings.hpp"
#include "tcpengine/utility/Log.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace tcpengine
{
class ClientTransport;
struct ConnectionEventHandler;
class HeartbeatMonitor;
class PacketManager;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
// 패킷 핸들러에서 예외가 나면 연결을 끊음. 그 밖에서 잡히지 않은 예외는 로그를 남기고 abort()하여 덤프를 남김
class Client
{
public:
	virtual ~Client();

	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	// 설정의 log대로 로그 출력을 시작하고 실행함. 설정 검사도 여기서 하며 실패하면 이유를 남기고 1 반환
	int run();

protected:
	// 설정 파일에서 읽으려면 ClientSettings::create() 또는 ClientSettings::fromJson()을 씀
	explicit Client(const ClientSettings& settings);

	// handler(const std::shared_ptr<Connection>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addPacketDispatcher(T::id, detail::makePacketDispatcher<T, Connection>(std::move(handler)));
	}

	// 네트워크를 열기 직전의 응용 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
	virtual bool onStart() { return true; }

	const ClientSettings& getSettings() const { return m_settings; }

	// 서버가 끊어도 onRun()은 스스로 반환하지 않으므로 isConnected()로 확인 필요
	virtual void onRun() = 0;

	virtual void onConnected(const std::shared_ptr<Connection>&) {}

	virtual void onDisconnected(const std::shared_ptr<Connection>&) {}

	void send(const PacketBase& packet);

	bool isConnected() const;

private:
	// m_settings를 검증하고 실행
	int start();

	// 엔진 제어 패킷(ServerHello, 세션) 처리 등록
	void registerControlHandlers();

	// transport가 연결 이벤트를 알릴 콜백. 하트비트와 응용 훅으로 이어 줌
	std::shared_ptr<const ConnectionEventHandler> makeConnectionEvents();

	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	// 뒤에 둔 것이 먼저 소멸함. 하트비트 타이머 → transport(네트워크 스레드) → PacketManager 순
	std::unique_ptr<PacketManager> m_packetManager;
	// run() 때 settings.transport에 맞게 만듦
	std::unique_ptr<ClientTransport> m_transport;
	std::unique_ptr<HeartbeatMonitor> m_heartbeatMonitor;
	const ClientSettings m_settings;
	// 서버가 준 재접속 토큰. 네트워크 스레드에서만 사용
	std::string m_sessionToken;
	bool m_running = false;
};
}
