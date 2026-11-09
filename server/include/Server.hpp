#pragma once

#include "IPacket.hpp"
#include "Log.hpp"
#include "ServerSettings.hpp"
#include "Session.hpp"

#include <cstdint>
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

	int run(const ServerSettings& settings);

protected:
	Server();

	// handler(const std::shared_ptr<Session>&, const T&)는 네트워크 스레드에서 호출
	template <typename T, typename Handler>
	bool registerHandler(Handler handler)
	{
		return addPacketDispatcher(T().getPacketId(), detail::makePacketDispatcher<T>(std::move(handler)));
	}

	virtual bool onStart() { return true; }

	virtual void onStop() {}

	virtual void onSessionConnected(const std::shared_ptr<Session>&) {}

	virtual void onSessionDisconnected(const std::shared_ptr<Session>&) {}

private:
	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	std::unique_ptr<PacketManager> m_packetManager;
	bool m_running = false;
};
}
