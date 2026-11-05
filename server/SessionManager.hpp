#pragma once

#include "Session.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <unordered_map>

class SessionManager
{
public:
	static SessionManager& instance();

	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	void setEventHandler(Session::EventHandler handler);

	void create(boost::asio::ip::tcp::socket socket);

	void broadcast(const std::shared_ptr<const Session::Buffer>& data);

	void closeAll();

	std::size_t size() const { return m_sessions.size(); }

private:
	SessionManager() = default;

	void remove(Session::SessionId id);

private:
	std::unordered_map<Session::SessionId, std::shared_ptr<Session>> m_sessions;
	Session::SessionId m_nextId = 1;
	Session::EventHandler m_handler;
};
