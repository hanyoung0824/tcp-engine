#pragma once

#include "IPacket.hpp"
#include "Log.hpp"
#include "ServerSettings.hpp"
#include "Session.hpp"

#include <cstdint>
#include <memory>
#include <type_traits>

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
		static_assert(std::is_base_of_v<ClientToServerPacket, T>, "Server can only handle ClientToServerPacket types");
		return addPacketDispatcher(T().getPacketId(), detail::makePacketDispatcher<T>(std::move(handler)));
	}

	virtual bool onStart() { return true; }

	virtual void onStop() {}

	virtual void onConnected(const std::shared_ptr<Session>&) {}

	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

private:
	bool addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher);

private:
	std::unique_ptr<PacketManager> m_packetManager;
	bool m_running = false;
};
}
