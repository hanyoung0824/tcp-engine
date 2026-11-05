#include "Client.hpp"

#include "ConsoleLogger.hpp"
#include "PacketManager.hpp"
#include "TcpClient.hpp"
#include "TcpSession.hpp"

#include <span>

namespace tcpengine
{
Client::Client()
	: m_packetManager(std::make_unique<PacketManager>())
{
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
	m_client = std::make_unique<TcpClient>(std::move(handler));
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
		m_client->send(std::move(buffer));
	}
}

bool Client::isConnected() const
{
	return m_client->isConnected();
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

	if (!m_client->connect(settings.host, settings.port, settings.heartbeat, PacketManager::makeHeartbeatPacket())) {
		return 1;
	}

	onRun();

	// onDisconnected가 파생 클래스가 살아 있을 때 호출되도록 소멸자가 아니라 여기서 닫음
	m_client->close();
	return 0;
}
}
