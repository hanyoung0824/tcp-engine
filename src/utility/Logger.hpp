#pragma once

#include "tcpengine/utility/Log.hpp"
#include "tcpengine/utility/LogSettings.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace tcpengine
{
enum class LogSource
{
	Engine,
	App,
	MaxCount,  // 출처 개수. 출처가 아님
};

// 엔진 안에서 로그 시스템을 시작하고 flush하는 곳. 프로세스에 하나뿐이라 모두 static
// 엔진 코드가 로그를 남길 때는 EngineLog.hpp를 씀
// spdlog 비동기 로거 위에서 동작하며 spdlog는 LogSystem.cpp 안에만 두어 엔진의 다른 층과 응용은 모름
// 엔진 로그와 응용 로그는 로그 스레드 하나와 큐 하나를 함께 써서 쓴 순서대로 출력됨
class Logger
{
public:
	Logger() = delete;

	// 설정대로 콘솔과 파일 싱크를 만들고 출력을 시작함. 다시 부르면 싱크를 바꿈. 그 전의 로그는 버림
	// 설정이 틀리거나 파일을 열 수 없으면 로그로 남길 수 없으므로 stderr에 출력하고 false
	static bool start(const LogSettings& settings);

	// 잠금 없이 레벨만 봄
	static bool isEnabled(LogSource source, LogLevel level);

	static void write(LogSource source, LogLevel level, std::string message);

	// 지금까지 쓴 로그가 모두 출력될 때까지 기다림. timeout이 지나면 포기하고 false
	static bool flush(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

	// 잡히지 않은 예외 등으로 std::terminate()가 불리면 로그를 남기고 flush한 뒤 abort()하여 덤프를 남김
	// 예외를 던진 지점에서 스택을 되감지 않고 불리므로 덤프에 던진 위치가 남음
	static void installTerminateHandler();
};

// 소멸 시 로그를 flush. Server::run()과 Client::run()이 어떤 경로로 끝나도 남은 로그를 출력하기 위함
class ScopedLogFlush
{
public:
	ScopedLogFlush() = default;
	~ScopedLogFlush();

	ScopedLogFlush(const ScopedLogFlush&) = delete;
	ScopedLogFlush& operator=(const ScopedLogFlush&) = delete;
};
}
