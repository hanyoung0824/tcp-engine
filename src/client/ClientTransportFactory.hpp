#pragma once

#include "ClientTransport.hpp"
#include "tcpengine/client/ClientSettings.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <memory>

namespace tcpengine
{
// settings.transport에 맞는 구현을 만듦. 새 프로토콜은 여기에 추가
std::unique_ptr<ClientTransport> makeClientTransport(boost::asio::any_io_executor executor,
                                                     const ClientSettings &settings);
} // namespace tcpengine
