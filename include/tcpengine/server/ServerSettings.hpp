#pragma once

#include "tcpengine/network/HeartbeatSettings.hpp"
#include "tcpengine/network/TransportType.hpp"

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <filesystem>

namespace tcpengine
{
struct SessionSettings
{
	// 연결이 끊긴 세션을 재접속을 위해 남겨 두는 시간. 0이면 끊기는 즉시 끝냄
	std::chrono::seconds resumeTimeout{ 30 };

	// 틀리면 이유를 담은 std::invalid_argument를 던짐
	void validate() const;
};

// Server 생성자에 넘기는 엔진 설정. 응용은 getSettings()로 읽을 수 있음
struct ServerSettings
{
	TransportType transport = TransportType::Tcp;
	unsigned short port = 0;
	HeartbeatSettings heartbeat;
	SessionSettings session;

	// 틀리면 이유를 담은 std::invalid_argument를 던짐. 하위 설정도 함께 검사
	void validate() const;

	// 설정 파일 전체를 받아 "engine" 영역을 읽음. 실패하면 std::runtime_error (메시지에 경로 없음)
	static ServerSettings fromJson(const nlohmann::json& file);

	// 응용 설정이 없을 때 파일을 읽어 바로 만듦. 실패하면 "경로: 이유"를 담은 std::runtime_error
	static ServerSettings create(const std::filesystem::path& path);
};
}
