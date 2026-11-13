#pragma once

#include "tcpengine/utility/Log.hpp"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>

namespace tcpengine
{
struct ConsoleLogSettings
{
	LogLevel level{};
};

// path가 maxSize를 넘으면 path.1, path.2…로 밀어내고 새로 씀. 밀려난 파일은 maxFiles개까지 보관
struct FileLogSettings
{
	LogLevel level{};
	std::filesystem::path path;
	std::size_t maxSize{};
	std::size_t maxFiles{};
};

// 로그 하나(엔진 또는 응용)를 출력할 곳. 없는 항목으로는 출력하지 않음
struct LogOutputSettings
{
	std::optional<ConsoleLogSettings> console;
	std::optional<FileLogSettings> file;

	// 틀리면 이유를 담은 std::invalid_argument를 던짐
	void validate() const;
};

struct LogSettings
{
	LogOutputSettings engine;
	LogOutputSettings app;

	// 틀리면 이유를 담은 std::invalid_argument를 던짐
	// 한 파일을 두 곳에서 따로 돌려 쓰면 내용이 깨지므로 엔진과 응용의 파일 경로는 달라야 함
	void validate() const;

	// 설정 파일 전체를 받아 "log" 영역을 읽음. "engine", "app" 안에 "console", "file"을 필요한 것만 둠
	// 레벨은 "debug", "info", "warning", "error", 파일 크기는 "maxSizeMB"
	// 실패하면 std::runtime_error (메시지에 경로 없음)
	static LogSettings fromJson(const nlohmann::json& file);
};
}
