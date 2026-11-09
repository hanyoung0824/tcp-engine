#include "Server.hpp"

#include "ConsoleLogger.hpp"
#include "PacketManager.hpp"
#include "SessionConfig.hpp"
#include "TcpServerTransport.hpp"

#include <memory>
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

	auto config = std::make_shared<SessionConfig>();
	config->handler.onConnected = [this](const std::shared_ptr<Session>& session) {
		onSessionConnected(session);
	};
	config->handler.onReceive = [this](const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data) {
		return m_packetManager->onReceive(session, data);
	};
	config->handler.onDisconnected = [this](const std::shared_ptr<Session>& session) {
		onSessionDisconnected(session);
	};
	config->heartbeat = settings.heartbeat;
	config->pingData = PacketManager::makeHeartbeatPacket();

	// 다른 프로토콜을 쓰려면 여기서 해당 ServerTransport 구현을 만듦
	std::unique_ptr<ServerTransport> transport = std::make_unique<TcpServerTransport>(settings.port, std::move(config));
	if (!transport->start()) {
		return 1;
	}
	transport->run();

	onStop();
	return 0;
}
}
