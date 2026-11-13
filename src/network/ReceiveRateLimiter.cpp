#include "ReceiveRateLimiter.hpp"

#include "EngineLog.hpp"

namespace tcpengine
{
ReceiveRateLimiter::ReceiveRateLimiter(double packetsPerSecond)
	: m_packetsPerSecond(packetsPerSecond)
{
}

void ReceiveRateLimiter::start(const Connection& connection)
{
	m_buckets.insert_or_assign(connection.getId(), TokenBucket(m_packetsPerSecond, m_packetsPerSecond));
}

bool ReceiveRateLimiter::tryReceive(const Connection& connection)
{
	const auto it = m_buckets.find(connection.getId());
	if (it == m_buckets.end() || it->second.tryTake()) {
		return true;
	}
	engineLogWarning("Receive rate over ", m_packetsPerSecond, " packets/s from ", connection.getAddress(), ", resetting");
	return false;
}

void ReceiveRateLimiter::stop(const Connection& connection)
{
	m_buckets.erase(connection.getId());
}
}
