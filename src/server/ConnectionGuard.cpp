#include "ConnectionGuard.hpp"

#include "ControlPackets.hpp"
#include "EngineLog.hpp"

#include <optional>
#include <utility>

namespace tcpengine
{
ConnectionGuard::ConnectionGuard(boost::asio::any_io_executor executor, const Settings& settings,
	std::shared_ptr<NetworkStats> stats, Disconnect disconnect)
	: m_settings(settings),
	m_stats(std::move(stats)),
	m_disconnect(std::move(disconnect)),
	m_receiveRateLimiter(settings.receivePacketsPerSecond),
	m_loginDeadline(std::move(executor), settings.loginTimeout, [this](const std::shared_ptr<Connection>& connection) {
		engineLogWarning("Login timeout from ", connection->getAddress(), ", closing");
		++m_stats->loginTimeouts;
		m_disconnect(connection, DisconnectReason::LoginTimeout);
	})
{
}

void ConnectionGuard::onConnected(const std::shared_ptr<Connection>& connection)
{
	m_states[connection->getId()] = State::Accepted;
	m_receiveRateLimiter.start(*connection);
	m_loginDeadline.start(connection);
}

void ConnectionGuard::onLoggedIn(const Connection& connection)
{
	if (changeState(connection, State::Handshaken, State::LoggedIn)) {
		m_loginDeadline.stop(connection);
	}
}

void ConnectionGuard::onDisconnected(const Connection& connection)
{
	m_receiveRateLimiter.stop(connection);
	m_loginDeadline.stop(connection);
	m_states.erase(connection.getId());
}

bool ConnectionGuard::allowPacket(const std::shared_ptr<Connection>& connection, std::uint16_t packetId)
{
	// 어기면 RST로 끊음. 연결이 닫히는 중이 되므로 남은 데이터는 잘못된 데이터로 따로 처리되지 않음
	if (!m_receiveRateLimiter.tryReceive(*connection)) {
		engineLogWarning("Receive rate over ", m_settings.receivePacketsPerSecond, " packets/s from ", connection->getAddress(), ", resetting");
		++m_stats->receiveLimitResets;
		connection->reset();
		return false;
	}
	if (packetId != controlPacketId && !isHandshakeDone(*connection)) {
		engineLogWarning("Packet ", packetId, " before ClientHello from ", connection->getAddress(), ", resetting");
		connection->reset();
		return false;
	}
	++m_stats->packetsIn;
	return true;
}

bool ConnectionGuard::handleClientHello(const std::shared_ptr<Connection>& connection, PacketReader& body)
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
	// 두 번째 ClientHello는 잘못된 패킷으로 보고 끊음
	return changeState(*connection, State::Accepted, State::Handshaken);
}

bool ConnectionGuard::isHandshakeDone(const Connection& connection) const
{
	const auto it = m_states.find(connection.getId());
	return it != m_states.end() && it->second != State::Accepted;
}

const char* ConnectionGuard::toString(State state)
{
	switch (state) {
	case State::Accepted: return "Accepted";
	case State::Handshaken: return "Handshaken";
	case State::LoggedIn: return "LoggedIn";
	}
	return "Unknown";
}

bool ConnectionGuard::changeState(const Connection& connection, State expected, State next)
{
	const auto it = m_states.find(connection.getId());
	if (it == m_states.end() || it->second != expected) {
		engineLogWarning("Unexpected connection state change from ", connection.getAddress(), ": ",
			it == m_states.end() ? "none" : toString(it->second), " -> ", toString(next));
		return false;
	}
	engineLogDebug("Connection ", connection.getId(), " state: ", toString(expected), " -> ", toString(next));
	it->second = next;
	return true;
}
}
