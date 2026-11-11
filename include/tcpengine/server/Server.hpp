#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/server/ServerSettings.hpp"
#include "tcpengine/server/Session.hpp"
#include "tcpengine/utility/Log.hpp"
#include "tcpengine/utility/LogSink.hpp"

#include <cstdint>
#include <memory>

namespace tcpengine
{
struct ConnectionEventHandler;
class HeartbeatMonitor;
class PacketManager;
class ServerTransport;
class SessionManager;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
class Server
{
public:
	virtual ~Server();

	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;

	// 로거는 엔진 로그와 응용 로그를 각각 출력할 곳. 설정 검사도 여기서 하며 실패하면 로그를 남기고 1 반환
	int run(std::shared_ptr<LogSink> engineLogSink, std::shared_ptr<LogSink> appLogSink);

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

	// 재접속 시간이 지났거나 close()됐거나 서버가 끝남
	virtual void onSessionEnded(const std::shared_ptr<Session>&) {}

private:
	// m_settings를 검증하고 실행
	int start();

	// 엔진 제어 패킷(Resume) 처리 등록
	void registerControlHandlers();

	// transport가 연결 이벤트를 알릴 콜백. 하트비트, 세션, 응용 훅으로 이어 줌
	std::shared_ptr<const ConnectionEventHandler> makeConnectionEvents(std::shared_ptr<const Connection::Buffer> serverHello);

	// 세션 콜백을 응용 훅(onSession*)으로 이어 줌
	void createSessionManager(ServerTransport& transport);

	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

	bool addSessionDispatcher(std::uint16_t id, detail::TargetDispatcher<Session> dispatcher);

private:
	std::unique_ptr<PacketManager> m_packetManager;
	// start() 동안만 있음
	std::unique_ptr<HeartbeatMonitor> m_heartbeatMonitor;
	std::unique_ptr<SessionManager> m_sessionManager;
	const ServerSettings m_settings;
	bool m_running = false;
};
}
