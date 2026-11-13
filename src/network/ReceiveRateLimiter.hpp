#pragma once

#include "tcpengine/network/Connection.hpp"
#include "tcpengine/utility/TokenBucket.hpp"

#include <unordered_map>

namespace tcpengine
{
// 연결마다 받는 패킷 수를 토큰 버킷으로 제한함. 1초 분량까지는 몰아서 받아도 됨
// 패킷 종류와 상관없이 비정상적인 양만 막으므로 정상 사용자가 걸리지 않을 만큼 넉넉하게 정함
// 네트워크 스레드에서만 사용
class ReceiveRateLimiter
{
public:
	explicit ReceiveRateLimiter(double packetsPerSecond);

	ReceiveRateLimiter(const ReceiveRateLimiter&) = delete;
	ReceiveRateLimiter& operator=(const ReceiveRateLimiter&) = delete;

	void start(const Connection& connection);

	// 패킷 하나를 받아도 되는지. 넘으면 로그를 남기고 false
	bool tryReceive(const Connection& connection);

	void stop(const Connection& connection);

private:
	const double m_packetsPerSecond;
	std::unordered_map<Connection::ConnectionId, TokenBucket> m_buckets;
};
}
