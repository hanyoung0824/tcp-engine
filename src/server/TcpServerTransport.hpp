#pragma once

#include "ConnectionEvents.hpp"
#include "ConnectionOptions.hpp"
#include "Listener.hpp"
#include "ServerTransport.hpp"
#include "TcpConnectionRegistry.hpp"

#include <boost/asio.hpp>

#include <functional>
#include <memory>

namespace tcpengine
{
    class TcpServerTransport final : public ServerTransport
    {
        public:
            TcpServerTransport(boost::asio::any_io_executor executor, unsigned short port,
                               ConnectionOptions connectionOptions, CanAccept canAccept,
                               std::shared_ptr<const ConnectionEvents> events);

            TcpServerTransport(const TcpServerTransport &) = delete;
            TcpServerTransport &operator=(const TcpServerTransport &) = delete;

            bool start() override;

            void stopAccepting() override;

            void closeAll(std::shared_ptr<const Connection::Buffer> farewell,
                          std::function<void()> onAllClosed) override;

        private:
            TcpConnectionRegistry m_connectionRegistry;
            Listener m_listener;
    };
} // namespace tcpengine
