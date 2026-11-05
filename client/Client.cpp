#include "Client.hpp"

#include <cstdint>
#include <span>

Client::Client()
	: m_client(makeEventHandler())
{
}

Session::EventHandler Client::makeEventHandler()
{
	Session::EventHandler handler;
	handler.onConnected = [this](const std::shared_ptr<Session>& session) {
		onConnected(session);
	};
	handler.onReceive = [this](const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data) {
		return m_packetManager.onReceive(session, data);
	};
	handler.onDisconnected = [this](const std::shared_ptr<Session>& session) {
		onDisconnected(session);
	};
	return handler;
}

int Client::run(const std::string& host, unsigned short port)
{
	m_running = true;

	if (!onStart()) {
		return 1;
	}

	if (!m_client.connect(host, port)) {
		return 1;
	}

	onRun();

	// onDisconnected가 파생 클래스가 살아 있을 때 호출되도록 소멸자가 아니라 여기서 닫는다.
	m_client.close();
	return 0;
}
