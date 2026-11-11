#pragma once

#include "tcpengine/utility/LogSink.hpp"

#include <string>

namespace tcpengine
{
// [시:분:초.밀리초][레벨][이름] 메시지. Warning 이상은 stderr
// 여러 ConsoleSink가 함께 써도 줄이 섞이지 않음
class ConsoleSink final : public LogSink
{
public:
	explicit ConsoleSink(std::string name);

protected:
	void write(const LogEntry& entry) override;

private:
	const std::string m_name;
};
}
