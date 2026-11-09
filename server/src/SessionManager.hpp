#pragma once

#include "SessionConfig.hpp"
#include "TcpSession.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <unordered_map>

namespace tcpengine
{
class SessionManager
{
public:
	explicit SessionManager(std::shared_ptr<const SessionConfig> config);

	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	void createSession(boost::asio::ip::tcp::socket socket);

	void closeAll();

	std::size_t size() const { return m_sessions.size(); }

private:
	void removeSession(Session::SessionId id);

private:
	const std::shared_ptr<const SessionConfig> m_config;
	std::unordered_map<Session::SessionId, std::shared_ptr<TcpSession>> m_sessions;
	Session::SessionId m_nextId = 1;
};
}
