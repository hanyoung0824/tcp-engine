#pragma once

#include "ConnectionEventHandler.hpp"
#include "ServerTransport.hpp"
#include "tcpengine/server/ServerSettings.hpp"

#include <memory>

namespace tcpengine
{
// settings.transport에 맞는 구현을 만듦. 새 프로토콜은 여기에 추가
std::unique_ptr<ServerTransport> makeServerTransport(const ServerSettings& settings,
	std::shared_ptr<const ConnectionEventHandler> handler);
}
