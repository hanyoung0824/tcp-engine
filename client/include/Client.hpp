#pragma once

#include "ClientSettings.hpp"
#include "IPacket.hpp"
#include "Log.hpp"
#include "Session.hpp"

#include <cstdint>
#include <memory>

namespace tcpengine
{
class PacketManager;
class TcpClient;

// registerHandler()는 파생 클래스 생성자에서 run() 전에 호출
class Client
{
public:
	virtual ~Client();

	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	int run(const ClientSettings& settings);

protected:
	Client();

	// handler(const std::shared_ptr<Session>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addPacketDispatcher(T().getPacketId(), detail::makePacketDispatcher<T>(std::move(handler)));
	}

	virtual bool onStart() { return true; }

	// 서버가 끊어도 onRun()은 스스로 반환하지 않으므로 isConnected()로 확인 필요
	virtual void onRun() = 0;

	virtual void onConnected(const std::shared_ptr<Session>&) {}

	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

	void send(const IPacket& packet);

	bool isConnected() const;

private:
	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	// m_client(네트워크 스레드)가 먼저 소멸해야 하므로 m_packetManager를 앞에 둠
	std::unique_ptr<PacketManager> m_packetManager;
	std::unique_ptr<TcpClient> m_client;
	bool m_running = false;
};
}
