#include "Server.hpp"

#include "TcpServer.hpp"

#include <cstdint>
#include <span>

int Server::run(unsigned short port)
{
	m_running = true;

	if (!onStart()) {
		return 1;
	}

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

	TcpServer server(port, std::move(handler));
	if (!server.start()) {
		return 1;
	}
	server.run();

	onStop();
	return 0;
}
