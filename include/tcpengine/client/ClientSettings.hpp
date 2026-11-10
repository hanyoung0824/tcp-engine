#pragma once

#include <string>

namespace tcpengine
{
// Client::run()에 넘긴 설정 파일에서 엔진이 읽어 둔 값. 응용은 getSettings()로 읽기만 함
// 하트비트는 서버가 접속 직후 알려 주는 값을 씀
struct ClientSettings
{
	std::string host;
	unsigned short port = 0;
};
}
