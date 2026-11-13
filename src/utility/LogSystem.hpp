#pragma once

#include "Logger.hpp"
#include "tcpengine/utility/LogSettings.hpp"

#include <chrono>
#include <optional>
#include <string>

// spdlog 위에 엔진, 응용 로거와 출력할 곳, 로그 스레드를 구성함. Logger가 쓰는 내부 구현
// spdlog는 이 파일의 cpp 안에만 둠
namespace tcpengine::logsystem
{
// 설정대로 싱크를 만들어 바꿔 끼움. 설정이 틀리거나 파일을 열 수 없으면 예외
void start(const LogSettings& settings);

// 잠금 없이 레벨만 봄
bool isEnabled(LogSource source, LogLevel level);

void write(LogSource source, LogLevel level, const std::string& message);

// 지금까지 쓴 로그가 모두 출력될 때까지 기다림. timeout이 지나면 포기하고 false
bool flush(std::optional<std::chrono::milliseconds> timeout);
}
