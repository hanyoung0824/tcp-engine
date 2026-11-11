#include "SessionManager.hpp"

#include "Logger.hpp"
#include "PacketManager.hpp"

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

namespace tcpengine
{
namespace
{
	// 재접속 토큰은 다른 사람이 추측하면 세션을 가로챌 수 있으므로 OS 난수로 만듦
	std::string makeToken()
	{
		std::random_device random;
		std::ostringstream token;
		token << std::hex << std::setfill('0');
		for (std::size_t i = 0; i < sessionTokenLength / 8; ++i) {
			token << std::setw(8) << static_cast<std::uint32_t>(random());
		}
		return token.str();
	}
}

class SessionManager::ServerSession final : public Session
{
public:
	ServerSession(SessionId id, std::string token, boost::asio::any_io_executor executor, SessionManager& owner)
		: m_id(id),
		m_token(std::move(token)),
		m_expiryTimer(std::move(executor)),
		m_owner(&owner)
	{
	}

	SessionId getId() const override { return m_id; }

	std::shared_ptr<Connection> getConnection() const override { return m_connection; }

	void close() override
	{
		if (m_owner) {
			m_owner->end(*this);
		}
	}

private:
	friend class SessionManager;

	const SessionId m_id;
	const std::string m_token;
	std::shared_ptr<Connection> m_connection;
	boost::asio::steady_timer m_expiryTimer;
	// 끝난 세션은 nullptr. 응용이 들고 있다가 close()를 불러도 아무 일도 하지 않음
	SessionManager* m_owner;
};

SessionManager::SessionManager(boost::asio::any_io_executor executor, std::chrono::seconds resumeTimeout, Callbacks callbacks)
	: m_executor(std::move(executor)),
	m_resumeTimeout(resumeTimeout),
	m_callbacks(std::move(callbacks))
{
}

SessionManager::~SessionManager()
{
	// 정상 종료 때는 Server가 endAll()을 먼저 부름. 여기서는 콜백 없이 타이머와 연결만 정리
	m_callbacks = {};
	endAll();
}

std::shared_ptr<Session> SessionManager::start(const std::shared_ptr<Connection>& connection)
{
	if (connection->isClosed()) {
		return nullptr;
	}
	if (m_idsByConnection.contains(connection->getId())) {
		engineLogWarning("Connection ", connection->getAddress(), " already has a session");
		return nullptr;
	}

	const Session::SessionId id = m_nextId++;
	auto session = std::make_shared<ServerSession>(id, makeToken(), m_executor, *this);
	m_sessions.emplace(id, session);
	m_idsByToken.emplace(session->m_token, id);
	attach(*session, connection);

	connection->send(PacketManager::makeControlPacket(ControlType::SessionStarted, session->m_token));
	engineLogInfo("Session ", id, " started for ", connection->getAddress());

	if (m_callbacks.onStarted) {
		m_callbacks.onStarted(session);
	}
	return session;
}

std::shared_ptr<Session> SessionManager::find(const Connection& connection) const
{
	const auto idIt = m_idsByConnection.find(connection.getId());
	if (idIt == m_idsByConnection.end()) {
		return nullptr;
	}
	return m_sessions.at(idIt->second);
}

void SessionManager::resume(const std::shared_ptr<Connection>& connection, const std::string& token)
{
	const auto idIt = m_idsByToken.find(token);
	if (idIt == m_idsByToken.end() || m_idsByConnection.contains(connection->getId())) {
		engineLogInfo("Resume failed from ", connection->getAddress());
		connection->send(PacketManager::makeControlPacket(ControlType::ResumeFailed));
		return;
	}

	const std::shared_ptr<ServerSession> session = m_sessions.at(idIt->second);
	if (session->m_connection) {
		// 이전 연결이 끊긴 걸 아직 모르는 경우. 이전 연결을 버리고 새 연결로 넘김
		const std::shared_ptr<Connection> previous = std::move(session->m_connection);
		m_idsByConnection.erase(previous->getId());
		previous->close();
	}
	else {
		session->m_expiryTimer.cancel();
	}

	attach(*session, connection);
	connection->send(PacketManager::makeControlPacket(ControlType::SessionResumed));
	engineLogInfo("Session ", session->m_id, " resumed from ", connection->getAddress());

	if (m_callbacks.onResumed) {
		m_callbacks.onResumed(session);
	}
}

void SessionManager::onConnectionClosed(const Connection& connection)
{
	const auto idIt = m_idsByConnection.find(connection.getId());
	if (idIt == m_idsByConnection.end()) {
		return;
	}

	const std::shared_ptr<ServerSession> session = m_sessions.at(idIt->second);
	m_idsByConnection.erase(idIt);
	session->m_connection.reset();

	if (m_resumeTimeout.count() == 0) {
		end(*session);
		return;
	}

	engineLogInfo("Session ", session->m_id, " detached, waiting ", m_resumeTimeout.count(), "s for resume");
	session->m_expiryTimer.expires_after(m_resumeTimeout);
	session->m_expiryTimer.async_wait([this, weakSession = std::weak_ptr<ServerSession>(session)](const boost::system::error_code& ec) {
		const std::shared_ptr<ServerSession> expired = weakSession.lock();
		if (ec || !expired || expired->m_connection) {
			return;
		}
		engineLogInfo("Session ", expired->m_id, " expired");
		end(*expired);
	});

	if (m_callbacks.onDetached) {
		m_callbacks.onDetached(session);
	}
}

void SessionManager::endAll()
{
	std::vector<std::shared_ptr<ServerSession>> sessions;
	sessions.reserve(m_sessions.size());
	for (const auto& [id, session] : m_sessions) {
		sessions.push_back(session);
	}
	for (const std::shared_ptr<ServerSession>& session : sessions) {
		end(*session);
	}
}

void SessionManager::attach(ServerSession& session, const std::shared_ptr<Connection>& connection)
{
	session.m_connection = connection;
	m_idsByConnection[connection->getId()] = session.m_id;
}

void SessionManager::end(ServerSession& session)
{
	if (!session.m_owner) {
		return;
	}
	session.m_owner = nullptr;
	session.m_expiryTimer.cancel();

	// 콜백이 끝날 때까지 세션이 살아 있도록 잡아 둠
	const std::shared_ptr<ServerSession> keep = m_sessions.at(session.m_id);
	m_sessions.erase(session.m_id);
	m_idsByToken.erase(session.m_token);

	if (std::shared_ptr<Connection> connection = std::move(session.m_connection)) {
		m_idsByConnection.erase(connection->getId());
		connection->close();
	}

	engineLogInfo("Session ", session.m_id, " ended");
	if (m_callbacks.onEnded) {
		m_callbacks.onEnded(keep);
	}
}
}
