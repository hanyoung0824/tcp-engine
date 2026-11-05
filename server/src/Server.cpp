#include "Server.hpp"

#include "ConsoleLogger.hpp"
#include "PacketManager.hpp"
#include "TcpServer.hpp"
#include "TcpSession.hpp"

#include <span>

namespace tcpengine
{
Server::Server()
	: m_packetManager(std::make_unique<PacketManager>())
{
}

Server::~Server() = default;

bool Server::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (m_running) {
		writeLog(LogLevel::Error, "registerHandler() must be called before run()");
		return false;
	}
	return m_packetManager->addDispatcher(id, std::move(dispatcher));
}

int Server::run(const ServerSettings& settings)
{
	ConsoleLogger::instance().setLevel(settings.logLevel);

	if (settings.port == 0) {
		writeLog(LogLevel::Error, "Port is not set");
		return 1;
	}

	if (!isValid(settings.heartbeat)) {
		writeLog(LogLevel::Error, "Invalid heartbeat settings: timeout must be 0 or longer than interval");
		return 1;
	}

	m_running = true;

	if (!onStart()) {
		return 1;
	}

	TcpSession::EventHandler handler;
	handler.onConnected = [this](const std::shared_ptr<Session>& session) {
		onConnected(session);
	};
	handler.onReceive = [this](const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data) {
		return m_packetManager->onReceive(session, data);
	};
	handler.onDisconnected = [this](const std::shared_ptr<Session>& session) {
		onDisconnected(session);
	};

	TcpServer server(settings.port, std::move(handler), settings.heartbeat, PacketManager::makeHeartbeatPacket());
	if (!server.start()) {
		return 1;
	}
	server.run();

	onStop();
	return 0;
}
}
