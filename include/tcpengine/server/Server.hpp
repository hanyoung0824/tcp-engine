#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Session.hpp"
#include "tcpengine/server/ServerSettings.hpp"
#include "tcpengine/utility/Log.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace tcpengine
{
class PacketManager;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
class Server
{
public:
	virtual ~Server();

	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;

	// 설정 파일을 읽어 적용한 뒤 실행. 읽은 값은 onStart()부터 getSettings()로 볼 수 있음
	int run(const std::filesystem::path& configPath);

protected:
	Server();

	// handler(const std::shared_ptr<Session>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addPacketDispatcher(T().getPacketId(), detail::makePacketDispatcher<T>(std::move(handler)));
	}

	// 응용 로거 설정(setLogger) 등 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
	virtual bool onStart() { return true; }

	const ServerSettings& getSettings() const { return m_settings; }

	virtual void onStop() {}

	virtual void onSessionConnected(const std::shared_ptr<Session>&) {}

	virtual void onSessionDisconnected(const std::shared_ptr<Session>&) {}

private:
	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	std::unique_ptr<PacketManager> m_packetManager;
	ServerSettings m_settings;
	bool m_running = false;
};
}
