#pragma once

#include "ClientTransport.hpp"
#include "ConnectionEvents.hpp"
#include "ConnectionOptions.hpp"
#include "TcpConnection.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <string>

namespace tcpengine
{
    class TcpClientTransport final : public ClientTransport
    {
    public:
        TcpClientTransport(boost::asio::any_io_executor executor, ConnectionOptions connectionOptions);

        TcpClientTransport(const TcpClientTransport &) = delete;
        TcpClientTransport &operator=(const TcpClientTransport &) = delete;

        void connect(const std::string &host, unsigned short port, std::chrono::seconds timeout,
                     std::shared_ptr<const ConnectionEvents> events, ConnectResult onResult) override;

        void send(std::shared_ptr<const Connection::Buffer> data) override;

        void close() override;

    private:
        boost::asio::any_io_executor m_executor;
        const ConnectionOptions m_connectionOptions;
        // 네트워크 스레드에서만 사용
        std::shared_ptr<TcpConnection> m_connection;
        Connection::ConnectionId m_nextConnectionId = 1;
    };
} // namespace tcpengine
