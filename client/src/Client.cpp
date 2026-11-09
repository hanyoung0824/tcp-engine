#include "Client.hpp"

#include "ConsoleLogger.hpp"
#include "PacketManager.hpp"
#include "SessionConfig.hpp"
#include "TcpClientTransport.hpp"

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
		writeLog(LogLevel::Error, "registerHandler() must be called before run()");
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

int Client::run(const ClientSettings& settings)
{
	ConsoleLogger::instance().setLevel(settings.logLevel);

	if (settings.host.empty() || settings.port == 0) {
		writeLog(LogLevel::Error, "Host or port is not set");
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
		onConnected(session);
	};
	config->handler.onReceive = [this](const std::shared_ptr<Session>& session, std::span<const std::uint8_t> data) {
		return m_packetManager->onReceive(session, data);
	};
	config->handler.onDisconnected = [this](const std::shared_ptr<Session>& session) {
		onDisconnected(session);
	};
	config->heartbeat = settings.heartbeat;
	config->pingData = PacketManager::makeHeartbeatPacket();

	if (!m_transport->connect(settings.host, settings.port, std::move(config))) {
		return 1;
	}

	onRun();

	// onDisconnected가 파생 클래스가 살아 있을 때 호출되도록 소멸자가 아니라 여기서 닫음
	m_transport->close();
	return 0;
}
}
