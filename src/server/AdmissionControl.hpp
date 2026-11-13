#pragma once

#include "LoginTimeoutMonitor.hpp"
#include "NetworkStats.hpp"
#include "ReceiveRateLimiter.hpp"
#include "tcpengine/network/Connection.hpp"
#include "tcpengine/network/DisconnectReason.hpp"
#include "tcpengine/network/packet/PacketReader.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_set>

namespace tcpengine
{
// 새 연결이 정상 클라이언트로 인정받는 과정을 맡음. 전송 방식과 무관하며, 연결 객체의 목록은 transport가 관리함
// 접속 → ClientHello로 버전 확인 → 로그인(세션 시작) 순서를 지키는지, 너무 많이 보내지 않는지 봄
// 네트워크 스레드에서만 사용
class AdmissionControl
{
public:
	struct Settings
	{
		std::uint16_t appProtocolVersion = 0;
		double receivePacketsPerSecond = 0;
		std::chrono::seconds loginTimeout{};
	};

	// 이유를 알리고 정상 종료함
	using Disconnect = std::function<void(const std::shared_ptr<Connection>& connection, DisconnectReason reason)>;

	AdmissionControl(boost::asio::any_io_executor executor, const Settings& settings, std::shared_ptr<NetworkStats> stats,
		Disconnect disconnect);

	AdmissionControl(const AdmissionControl&) = delete;
	AdmissionControl& operator=(const AdmissionControl&) = delete;

	void onConnected(const std::shared_ptr<Connection>& connection);

	// 세션을 시작했거나 이어 감. 로그인 시간 제한을 풂
	void onLoggedIn(const Connection& connection);

	void onDisconnected(const Connection& connection);

	// 패킷 하나를 받아도 되는지. 수신 제한을 넘거나 ClientHello 전에 응용 패킷이 오면 RST로 끊고 false
	bool allowPacket(const std::shared_ptr<Connection>& connection, std::uint16_t packetId);

	// ClientHello 본문을 처리. 버전이 다르면 VersionMismatch로 끊음. 형식이 틀리면 false
	bool handleClientHello(const std::shared_ptr<Connection>& connection, PacketReader& body);

	// ClientHello로 버전 확인을 마쳤는지
	bool isGreeted(const Connection& connection) const;

private:
	const Settings m_settings;
	const std::shared_ptr<NetworkStats> m_stats;
	const Disconnect m_disconnect;
	ReceiveRateLimiter m_receiveRateLimiter;
	LoginTimeoutMonitor m_loginTimeoutMonitor;
	std::unordered_set<Connection::ConnectionId> m_greetedConnections;
};
}
