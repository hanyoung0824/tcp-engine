#pragma once

#include "tcpengine/network/TransportType.hpp"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>

namespace tcpengine
{
// Client 생성자에 넘기는 엔진 설정. 응용은 getSettings()로 읽을 수 있음
// 하트비트는 서버가 접속 직후 알려 주는 값을 씀
struct ClientSettings
{
	TransportType transport = TransportType::Tcp;
	std::string host;
	unsigned short port = 0;

	// 틀리면 이유를 담은 std::invalid_argument를 던짐
	void validate() const;

	// 설정 파일 전체를 받아 "engine" 영역을 읽음. 실패하면 std::runtime_error (메시지에 경로 없음)
	static ClientSettings fromJson(const nlohmann::json& file);

	// 응용 설정이 없을 때 파일을 읽어 바로 만듦. 실패하면 "경로: 이유"를 담은 std::runtime_error
	static ClientSettings create(const std::filesystem::path& path);
};
}
