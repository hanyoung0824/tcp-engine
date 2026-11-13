#include "tcpengine/utility/TokenBucket.hpp"

#include <algorithm>

namespace tcpengine
{
TokenBucket::TokenBucket(double rate, double capacity)
	: m_rate(rate),
	m_capacity(capacity),
	m_tokens(capacity),
	m_lastRefill(std::chrono::steady_clock::now())
{
}

bool TokenBucket::tryTake(double count)
{
	refill();
	if (m_tokens < count) {
		return false;
	}
	m_tokens -= count;
	return true;
}

void TokenBucket::refill()
{
	const auto now = std::chrono::steady_clock::now();
	const std::chrono::duration<double> elapsed = now - m_lastRefill;
	m_lastRefill = now;
	m_tokens = std::min(m_capacity, m_tokens + elapsed.count() * m_rate);
}
}
