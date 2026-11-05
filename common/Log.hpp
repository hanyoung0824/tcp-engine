#pragma once

#include <functional>
#include <sstream>
#include <string_view>

// 엔진(net)의 로그 창구.
// 엔진은 화면이나 파일에 직접 출력하지 않고, 응용이 setLogHandler()로 등록한 함수에 로그를 넘긴다.
// 어디에 어떻게 남길지(콘솔, 파일, 레벨 거르기)는 응용이 정한다. 등록하지 않으면 엔진 로그는 버려진다.

enum class LogLevel
{
	Debug, // 개발 중에만 보고 싶은 자세한 정보 (예: 세션 소멸)
	Info, // 정상 동작 (예: 접속, 종료)
	Warning, // 상대의 잘못이나 예상된 실패 (예: 잘못된 패킷, 접속 실패)
	Error, // 이쪽의 문제 (예: 포트 열기 실패)
};

// 로그 한 줄을 받는 함수. 여러 스레드에서 동시에 불릴 수 있다.
using LogHandler = std::function<void(LogLevel level, std::string_view message)>;

// 로그를 받을 함수를 등록한다. 시작할 때(네트워크 스레드가 생기기 전에) 한 번만 호출한다.
// 이후에는 읽기만 하므로 여러 스레드에서 로그를 남겨도 잠금이 필요 없다.
void setLogHandler(LogHandler handler);

// 등록된 함수가 있는지. 없으면 메시지를 만들 필요도 없다.
bool hasLogHandler();

// 완성된 메시지를 등록된 함수로 넘긴다.
void writeLogMessage(LogLevel level, std::string_view message);

// 인자들을 이어 붙여 로그 한 줄을 남긴다. 예: writeLog(LogLevel::Info, "Session ", id, " added");
template <typename... Args>
void writeLog(LogLevel level, const Args&... args)
{
	if (!hasLogHandler()) {
		return;
	}

	std::ostringstream message;
	(message << ... << args);
	writeLogMessage(level, message.str());
}
