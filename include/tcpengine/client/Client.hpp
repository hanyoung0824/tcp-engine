#pragma once

#include "tcpengine/network/PacketDispatcher.hpp"
#include "tcpengine/network/Session.hpp"
#include "tcpengine/client/ClientSettings.hpp"
#include "tcpengine/utility/Log.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace tcpengine
{
class ClientTransport;
class PacketManager;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
class Client
{
public:
	virtual ~Client();

	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	// 설정 파일을 읽어 적용한 뒤 실행. 읽은 값은 onStart()부터 getSettings()로 볼 수 있음
	int run(const std::filesystem::path& configPath);

protected:
	Client();

	// handler(const std::shared_ptr<Session>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addPacketDispatcher(T().getPacketId(), detail::makePacketDispatcher<T>(std::move(handler)));
	}

	// 응용 로거 설정(setLogger) 등 초기화. false를 반환하거나 예외를 던지면 실행하지 않고 종료
	virtual bool onStart() { return true; }

	const ClientSettings& getSettings() const { return m_settings; }

	// 서버가 끊어도 onRun()은 스스로 반환하지 않으므로 isConnected()로 확인 필요
	virtual void onRun() = 0;

	virtual void onConnected(const std::shared_ptr<Session>&) {}

	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

	void send(const IPacket& packet);

	bool isConnected() const;

private:
	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	// m_transport(네트워크 스레드)가 먼저 소멸해야 하므로 m_packetManager를 앞에 둠
	std::unique_ptr<PacketManager> m_packetManager;
	std::unique_ptr<ClientTransport> m_transport;
	ClientSettings m_settings;
	bool m_running = false;
};
}
