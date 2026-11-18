#pragma once

#include "ConnectionEvents.hpp"
#include "HeartbeatChecker.hpp"
#include "NetworkStats.hpp"
#include "ServerTransport.hpp"
#include "tcpengine/server/ServerSettings.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <functional>
#include <memory>

namespace tcpengine
{
// 접속 받기, 연결 목록, 하트비트로 살아 있는지 확인하기를 맡음
// 접속하면 ServerHello를 보내고 하트비트를 시작한 뒤 events로 알림. 네트워크 스레드에서만 사용
class ConnectionManager
{
public:
	ConnectionManager(boost::asio::any_io_executor executor, const ServerSettings& settings, std::shared_ptr<NetworkStats> stats,
		std::shared_ptr<const Connection::Buffer> serverHello, std::shared_ptr<const ConnectionEvents> events);

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	bool start();

	void stopAccepting();

	// 연결이 모두 닫히면 onAllClosed를 한 번 부름
	void closeAll(std::shared_ptr<const Connection::Buffer> farewell, std::function<void()> onAllClosed);

private:
	std::shared_ptr<const ConnectionEvents> wrapEvents(std::shared_ptr<const Connection::Buffer> serverHello,
		std::shared_ptr<const ConnectionEvents> events);

private:
	const std::shared_ptr<NetworkStats> m_stats;
	// 연결이 닫힐 때 하트비트를 멈추므로 transport보다 나중에 소멸
	HeartbeatChecker m_heartbeatChecker;
	const HeartbeatSettings m_heartbeatSettings;
	std::unique_ptr<ServerTransport> m_transport;
};
}
