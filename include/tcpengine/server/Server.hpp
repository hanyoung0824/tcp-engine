#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/server/ServerSettings.hpp"
#include "tcpengine/server/Session.hpp"
#include "tcpengine/utility/Log.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace boost::asio
{
class io_context;
}

namespace tcpengine
{
struct ConnectionEventHandler;
class HeartbeatMonitor;
class PacketManager;
class ServerTransport;
class SessionManager;
class ShutdownSignal;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
// 패킷 핸들러에서 예외가 나면 그 연결만 끊음. 그 밖에서 잡히지 않은 예외는 로그를 남기고 abort()하여 덤프를 남김
class Server
{
public:
	virtual ~Server();

	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;

	// 설정의 log대로 로그 출력을 시작하고 실행함. 설정 검사도 여기서 하며 실패하면 이유를 남기고 1 반환
	// 한 번만 호출할 수 있음
	int run();

	// 어느 스레드에서 불러도 되고 여러 번 불러도 한 번만 처리함. SIGINT, SIGTERM도 이 함수를 거침
	// 접속 받기 중단 → 세션 모두 끝냄(onSessionEnded) → 연결마다 남은 데이터를 보내고 닫음 → run() 반환
	// 종료 중에는 새 연결과 새 세션을 받지 않음. run() 전에 부르면 run()이 시작하자마자 종료함
	void stop();

protected:
	// 설정 파일에서 읽으려면 ServerSettings::create() 또는 ServerSettings::fromJson()을 씀
	explicit Server(const ServerSettings& settings);

	// 세션이 있는 연결(로그인 후)에서만 받는 패킷. 세션 없이 오면 엔진이 버리고 로그를 남김
	// handler(const std::shared_ptr<Session>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addSessionDispatcher(T::id, detail::makePacketDispatcher<T, Session>(std::move(handler)));
	}

	// 로그인 전에도 받는 패킷 (로그인 요청 등)
	// handler(const std::shared_ptr<Connection>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerConnectionHandler(Handler handler)
	{
		return addPacketDispatcher(T::id, detail::makePacketDispatcher<T, Connection>(std::move(handler)));
	}

	// 네트워크를 열기 직전의 응용 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
	virtual bool onStart() { return true; }

	const ServerSettings& getSettings() const { return m_settings; }

	virtual void onStop() {}

	virtual void onConnected(const std::shared_ptr<Connection>&) {}

	virtual void onDisconnected(const std::shared_ptr<Connection>&) {}

	// 로그인을 마친 연결에 세션을 시작함. 클라이언트에는 재접속 토큰이 전달됨
	// 네트워크 스레드에서 호출. 이미 세션이 있거나 닫힌 연결이면 nullptr
	std::shared_ptr<Session> startSession(const std::shared_ptr<Connection>& connection);

	// 연결에 붙어 있는 세션. 없으면 nullptr
	std::shared_ptr<Session> findSession(const std::shared_ptr<Connection>& connection) const;

	virtual void onSessionStarted(const std::shared_ptr<Session>&) {}

	// 재접속으로 새 연결이 붙음
	virtual void onSessionResumed(const std::shared_ptr<Session>&) {}

	// 연결이 끊겨 재접속을 기다리기 시작함
	virtual void onSessionDetached(const std::shared_ptr<Session>&) {}

	virtual void onSessionEnded(const std::shared_ptr<Session>&, SessionEndReason) {}

private:
	// m_settings를 검증하고 실행
	int start();

	// 네트워크 스레드에서 종료 과정을 진행
	void shutdown();

	// 엔진 제어 패킷(Resume) 처리 등록
	void registerControlHandlers();

	// transport가 연결 이벤트를 알릴 콜백. 하트비트, 세션, 응용 훅으로 이어 줌
	std::shared_ptr<const ConnectionEventHandler> makeConnectionEvents(std::shared_ptr<const Connection::Buffer> serverHello);

	// 세션 콜백을 응용 훅(onSession*)으로 이어 줌
	void createSessionManager();

	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

	bool addSessionDispatcher(std::uint16_t id, detail::TargetDispatcher<Session> dispatcher);

private:
	// 네트워크 이벤트 루프. Server와 수명이 같아 stop()이 언제든 작업을 넘길 수 있음
	// 멈춘 뒤 남은 핸들러가 다른 멤버를 잡고 있을 수 있으므로 가장 앞에 두어 가장 나중에 소멸
	std::unique_ptr<boost::asio::io_context> m_ioContext;
	std::unique_ptr<PacketManager> m_packetManager;
	// 아래 넷은 start() 동안만 있음
	std::unique_ptr<HeartbeatMonitor> m_heartbeatMonitor;
	std::unique_ptr<SessionManager> m_sessionManager;
	std::unique_ptr<ServerTransport> m_transport;
	std::unique_ptr<ShutdownSignal> m_shutdownSignal;
	const ServerSettings m_settings;
	bool m_running = false;
	std::atomic<bool> m_stopRequested{ false };
};
}
