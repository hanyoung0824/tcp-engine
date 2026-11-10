#include "tcpengine/server/Server.hpp"

#include "Logger.hpp"
#include "PacketManager.hpp"
#include "ServerSettingsParser.hpp"
#include "SessionConfig.hpp"
#include "TcpServerTransport.hpp"
#include "tcpengine/utility/ConsoleLogger.hpp"
#include "tcpengine/utility/JsonConfig.hpp"

#include <exception>
#include <memory>
#include <optional>
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
		engineLogError("registerHandler() must be called before run()");
		return false;
	}
	return m_packetManager->addDispatcher(id, std::move(dispatcher));
}

int Server::run(const std::filesystem::path& configPath)
{
	const ScopedLoggerShutdown loggerShutdown;
	setEngineLogger(std::make_shared<ConsoleLogger>("engine"));

	std::optional<nlohmann::json> json = loadJsonFile(configPath);
	if (!json) {
		return 1;
	}
	std::optional<ServerSettings> parsed = parseServerSettings(*json);
	if (!parsed) {
		engineLogError("Cannot load ", configPath.string());
		return 1;
	}
	m_settings = std::move(*parsed);
	const ServerSettings& settings = m_settings;

	if (settings.port == 0) {
		engineLogError("Port is not set");
		return 1;
	}

	if (!isValid(settings.heartbeat)) {
		engineLogError("Invalid heartbeat settings: timeout must be 0 or longer than interval");
		return 1;
	}

	std::shared_ptr<const Session::Buffer> heartbeatConfig = PacketManager::makeHeartbeatConfigPacket(settings.heartbeat);
	if (!heartbeatConfig) {
		engineLogError("Heartbeat interval and timeout must be 65535 seconds or less");
		return 1;
	}

	m_running = true;

	try {
		if (!onStart()) {
			return 1;
		}
	}
	catch (const std::exception& e) {
		engineLogError("onStart() failed: ", e.what());
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
	// 서버는 Ping을 보내지 않고 timeout만 검사함. 접속 직후 클라이언트에 하트비트 설정을 알려 줌
	config->heartbeat = settings.heartbeat;
	config->greeting = std::move(heartbeatConfig);

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
