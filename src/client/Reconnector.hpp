#pragma once

#include "ClientTransport.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <functional>

namespace tcpengine
{
// 연결이 끊긴 뒤 1, 2, 4초…로 늘려 가며 maxDelay까지 기다렸다가 다시 접속을 시도함
// 시도가 실패하면 스스로 다음 시도를 예약함. 네트워크 스레드에서만 사용
class Reconnector
{
public:
	using ConnectResult = ClientTransport::ConnectResult;
	using Attempt = std::function<void(ConnectResult onResult)>;

	Reconnector(boost::asio::any_io_executor executor, std::chrono::seconds maxDelay, Attempt attempt);

	Reconnector(const Reconnector&) = delete;
	Reconnector& operator=(const Reconnector&) = delete;

	void schedule();

	// 접속에 성공함. 다음에 끊기면 1초부터 다시 셈
	void reset() { m_attempts = 0; }

	// 더 시도하지 않음. 진행 중인 시도의 결과도 무시함
	void cancel();

private:
	boost::asio::steady_timer m_timer;
	const std::chrono::seconds m_maxDelay;
	const Attempt m_attempt;
	int m_attempts = 0;
	bool m_cancelled = false;
};
}
