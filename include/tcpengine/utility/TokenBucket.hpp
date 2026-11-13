#pragma once

#include <chrono>

namespace tcpengine
{
// 초당 rate개씩 채워지고 capacity개까지 모이는 토큰. 몰아서 쓰는 것은 capacity만큼 허용하고 계속 빠른 것만 막음
// 타이머 없이 꺼낼 때 지난 시간만큼 채움. 스레드 안전하지 않음
class TokenBucket
{
public:
	// 가득 찬 상태로 시작. rate와 capacity는 0보다 커야 함
	TokenBucket(double rate, double capacity);

	// count개를 꺼냄. 모자라면 꺼내지 않고 false
	bool tryTake(double count = 1);

private:
	void refill();

private:
	double m_rate;
	double m_capacity;
	double m_tokens;
	std::chrono::steady_clock::time_point m_lastRefill;
};
}
