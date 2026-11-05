#pragma once

#include "IPacket.hpp"
#include "Log.hpp"
#include "PacketHelper.hpp"
#include "PacketManager.hpp"
#include "Session.hpp"
#include "TcpClient.hpp"

#include <memory>
#include <string>
#include <type_traits>

//   class ChatClient : public Client
//   {
//   public:
//       ChatClient() { registerPacket<S2C_ChatNotifyHandler>(); }   // S2C_ChatNotifyHandler : S2C_ChatNotify, handle() 오버라이드
//   };
//   int main() { return ChatClient().run("127.0.0.1", 12345); }
// registerPacket()은 파생 클래스 생성자에서 run() 전에 호출한다.
class Client
{
public:
	virtual ~Client() = default;

	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	int run(const std::string& host, unsigned short port);

protected:
	Client();

	template <typename T>
	bool registerPacket()
	{
		static_assert(std::is_base_of_v<ServerToClientPacket, T>, "Client can only register ServerToClientPacket types");

		if (m_running) {
			writeLog(LogLevel::Error, "registerPacket() must be called before run()");
			return false;
		}
		return m_packetManager.addPacket<T>();
	}

	virtual bool onStart() { return true; }

	// 서버가 끊어도 onRun()은 스스로 반환하지 않으므로 isConnected()로 확인한다.
	virtual void onRun() = 0;

	virtual void onConnected(const std::shared_ptr<Session>&) {}

	virtual void onDisconnected(const std::shared_ptr<Session>&) {}

	void send(const IPacket& packet)
	{
		if (auto buffer = makePacket(packet)) {
			m_client.send(std::move(buffer));
		}
	}

	bool isConnected() const { return m_client.isConnected(); }

private:
	Session::EventHandler makeEventHandler();

private:
	// m_client(네트워크 스레드)가 먼저 소멸해야 하므로 m_packetManager를 앞에 둔다.
	PacketManager m_packetManager;
	TcpClient m_client;
	bool m_running = false;
};
