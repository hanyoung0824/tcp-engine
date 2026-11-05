#pragma once

#include "IPacket.hpp"
#include "Log.hpp"
#include "PacketManager.hpp"
#include "Session.hpp"

#include <memory>
#include <type_traits>

//   class ChatServer : public Server
//   {
//   public:
//       ChatServer() { registerPacket<C2S_ChatHandler>(); }   // C2S_ChatHandler : C2S_Chat, handle() 오버라이드
//   };
//   int main() { return ChatServer().run(12345); }
// registerPacket()은 파생 클래스 생성자에서 run() 전에 호출한다.
class Server
{
public:
	virtual ~Server() = default;

	int run(unsigned short port);

protected:
	Server() = default;

	template <typename T>
	bool registerPacket()
	{
		static_assert(std::is_base_of_v<ClientToServerPacket, T>, "Server can only register ClientToServerPacket types");

		if (m_running) {
			writeLog(LogLevel::Error, "registerPacket() must be called before run()");
			return false;
		}
		return m_packetManager.addPacket<T>();
	}

	virtual bool onStart() { return true; }

	virtual void onStop() {}

	virtual void onConnected(const std::shared_ptr<Session>&) {}

	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

private:
	PacketManager m_packetManager;
	bool m_running = false;
};
