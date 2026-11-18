#pragma once

#include "ConnectionEvents.hpp"
#include "NetworkStats.hpp"
#include "ServerTransport.hpp"
#include "tcpengine/server/ServerSettings.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <memory>

namespace tcpengine
{
// settings.transport에 맞는 구현을 만듦. 새 프로토콜은 여기에 추가
std::unique_ptr<ServerTransport> makeServerTransport(boost::asio::any_io_executor executor, const ServerSettings& settings,
	std::shared_ptr<const ConnectionEvents> events, std::shared_ptr<NetworkStats> stats, ServerTransport::CanAccept canAccept);
}
