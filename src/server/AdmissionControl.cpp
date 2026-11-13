#include "AdmissionControl.hpp"

#include "ControlPackets.hpp"
#include "EngineLog.hpp"

#include <optional>
#include <utility>

namespace tcpengine
{
AdmissionControl::AdmissionControl(boost::asio::any_io_executor executor, const Settings& settings,
	std::shared_ptr<NetworkStats> stats, Disconnect disconnect)
	: m_settings(settings),
	m_stats(std::move(stats)),
	m_disconnect(std::move(disconnect)),
	m_receiveRateLimiter(settings.receivePacketsPerSecond),
	m_loginTimeoutMonitor(std::move(executor), settings.loginTimeout, [this](const std::shared_ptr<Connection>& connection) {
		engineLogWarning("Login timeout from ", connection->getAddress(), ", closing");
		++m_stats->loginTimeouts;
		m_disconnect(connection, DisconnectReason::LoginTimeout);
	})
{
}

void AdmissionControl::onConnected(const std::shared_ptr<Connection>& connection)
{
	m_receiveRateLimiter.start(*connection);
	m_loginTimeoutMonitor.start(connection);
}

void AdmissionControl::onLoggedIn(const Connection& connection)
{
	m_loginTimeoutMonitor.stop(connection);
}

void AdmissionControl::onDisconnected(const Connection& connection)
{
	m_receiveRateLimiter.stop(connection);
	m_loginTimeoutMonitor.stop(connection);
	m_greetedConnections.erase(connection.getId());
}

bool AdmissionControl::allowPacket(const std::shared_ptr<Connection>& connection, std::uint16_t packetId)
{
	// 어기면 RST로 끊음. 연결이 닫히는 중이 되므로 남은 데이터는 잘못된 데이터로 따로 처리되지 않음
	if (!m_receiveRateLimiter.tryReceive(*connection)) {
		engineLogWarning("Receive rate over ", m_settings.receivePacketsPerSecond, " packets/s from ", connection->getAddress(), ", resetting");
		++m_stats->receiveLimitResets;
		connection->reset();
		return false;
	}
	if (packetId != controlPacketId && !isGreeted(*connection)) {
		engineLogWarning("Packet ", packetId, " before ClientHello from ", connection->getAddress(), ", resetting");
		connection->reset();
		return false;
	}
	++m_stats->packetsIn;
	return true;
}

bool AdmissionControl::handleClientHello(const std::shared_ptr<Connection>& connection, PacketReader& body)
{
	const std::optional<control::ClientHello> hello = control::readClientHello(body);
	if (!hello) {
		return false;
	}
	if (hello->engineVersion != engineProtocolVersion || hello->appVersion != m_settings.appProtocolVersion) {
		engineLogWarning("Version mismatch from ", connection->getAddress(), ": engine ", hello->engineVersion,
			" (server ", engineProtocolVersion, "), app ", hello->appVersion, " (server ", m_settings.appProtocolVersion, "), closing");
		m_disconnect(connection, DisconnectReason::VersionMismatch);
		return true;
	}
	m_greetedConnections.insert(connection->getId());
	return true;
}

bool AdmissionControl::isGreeted(const Connection& connection) const
{
	return m_greetedConnections.contains(connection.getId());
}
}
