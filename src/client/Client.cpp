#include "tcpengine/client/Client.hpp"

#include "ClientSettingsParser.hpp"
#include "Logger.hpp"
#include "PacketManager.hpp"
#include "SessionConfig.hpp"
#include "TcpClientTransport.hpp"
#include "tcpengine/utility/ConsoleLogger.hpp"
#include "tcpengine/utility/JsonConfig.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <span>

namespace tcpengine
{
Client::Client()
	: m_packetManager(std::make_unique<PacketManager>()),
	// 다른 프로토콜을 쓰려면 여기서 해당 ClientTransport 구현을 만듦
	m_transport(std::make_unique<TcpClientTransport>())
{
}

Client::~Client() = default;

bool Client::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
{
	if (m_running) {
		engineLogError("registerHandler() must be called before run()");
		return false;
	}
	return m_packetManager->addDispatcher(id, std::move(dispatcher));
}

void Client::send(const IPacket& packet)
{
	if (auto buffer = packet.serialize()) {
		m_transport->send(std::move(buffer));
	}
}

bool Client::isConnected() const
{
	return m_transport->isConnected();
}

int Client::run(const std::filesystem::path& configPath)
{
	const ScopedLoggerShutdown loggerShutdown;
	setEngineLogger(std::make_shared<ConsoleLogger>("engine"));

	std::optional<nlohmann::json> json = loadJsonFile(configPath);
	if (!json) {
		return 1;
	}
	std::optional<ClientSettings> parsed = parseClientSettings(*json);
	if (!parsed) {
		engineLogError("Cannot load ", configPath.string());
		return 1;
	}
	m_settings = std::move(*parsed);
	const ClientSettings& settings = m_settings;

	if (settings.host.empty() || settings.port == 0) {
		engineLogError("Host or port is not set");
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

	m_packetManager->setHeartbeatConfigHandler([this](const std::shared_ptr<Session>&, const HeartbeatSettings& heartbeat) {
		m_transport->startHeartbeat(heartbeat, PacketManager::makePingPacket());
	});

	auto config = std::make_shared<SessionConfig>();
	config->handler.onConnected = [this](const std::shared_ptr<Session>& session) {
		onConnected(session);
	};
	config->handler.onReceive = [this](const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data) {
		return m_packetManager->onReceive(session, data);
	};
	config->handler.onDisconnected = [this](const std::shared_ptr<Session>& session) {
		onDisconnected(session);
	};

	// 하트비트는 서버가 알려 준 값으로 시작하므로 처음에는 비워 둠
	if (!m_transport->connect(settings.host, settings.port, std::move(config))) {
		return 1;
	}

	onRun();

	// onDisconnected가 파생 클래스가 살아 있을 때 호출되도록 소멸자가 아니라 여기서 닫음
	m_transport->close();
	return 0;
}
}
