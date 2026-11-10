#pragma once

#include "ClientTransport.hpp"
#include "SessionConfig.hpp"
#include "TcpSession.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <thread>

namespace tcpengine
{
class TcpClientTransport final : public ClientTransport
{
public:
	TcpClientTransport() = default;

	~TcpClientTransport() override;

	TcpClientTransport(const TcpClientTransport&) = delete;
	TcpClientTransport& operator=(const TcpClientTransport&) = delete;

	bool connect(const std::string& host, unsigned short port, std::shared_ptr<const SessionConfig> config) override;

	void startHeartbeat(const HeartbeatSettings& heartbeat, std::shared_ptr<const Session::Buffer> pingData) override;

	void send(std::shared_ptr<const Session::Buffer> data) override;

	void close() override;

	bool isConnected() const override { return m_session && !m_session->isClosed(); }

private:
	boost::asio::io_context m_ioContext;
	std::shared_ptr<TcpSession> m_session;
	std::thread m_networkThread;
};
}
