#pragma once

#include "Log.hpp"

#include <iostream>
#include <string_view>

// 엔진 로그를 콘솔에 출력하도록 등록하는 기본 로그 함수. 쓸지 말지, 어느 레벨부터 출력할지는 응용이 정한다.
// minLevel보다 낮은 레벨은 버린다. Warning 이상은 std::cerr, 나머지는 std::cout으로 보낸다.
// 시작할 때(네트워크 스레드가 생기기 전에) 한 번만 호출한다.
inline void setConsoleLogHandler(LogLevel minLevel)
{
	setLogHandler([minLevel](LogLevel level, std::string_view message) {
		if (level < minLevel) {
			return;
		}

		std::ostream& out = level >= LogLevel::Warning ? std::cerr : std::cout;
		switch (level) {
		case LogLevel::Debug: out << "[DEBUG] "; break;
		case LogLevel::Info: out << "[INFO] "; break;
		case LogLevel::Warning: out << "[WARN] "; break;
		case LogLevel::Error: out << "[ERROR] "; break;
		}
		out << message << '\n';
	});
}
