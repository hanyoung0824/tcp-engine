#pragma once

#include "tcpengine/utility/Log.hpp"

#include <atomic>

namespace tcpengine
{
// 로그를 내보낼 곳. 콘솔 외에 파일, 윈도우, DB 등은 상속해서 write()를 구현
// 로그는 모두 쌓이고, 여기서 정한 레벨 이상만 write()로 넘어옴
class LogInterface
{
public:
	virtual ~LogInterface() = default;

	// 기본값 Info. 어느 스레드에서 불러도 됨
	void setLevel(LogLevel level) { m_level.store(level, std::memory_order_relaxed); }

	LogLevel getLevel() const { return m_level.load(std::memory_order_relaxed); }

	// 로그 스레드가 호출
	void output(const LogEntry& entry)
	{
		if (entry.level >= getLevel()) {
			write(entry);
		}
	}

protected:
	// 로그 스레드 하나에서만 호출됨
	virtual void write(const LogEntry& entry) = 0;

private:
	std::atomic<LogLevel> m_level{ LogLevel::Info };
};
}
