#include "ClientTransportFactory.hpp"

#include "TcpClientTransport.hpp"

namespace tcpengine
{
std::unique_ptr<ClientTransport> makeClientTransport(const ClientSettings& settings)
{
	switch (settings.transport) {
	case TransportType::Tcp:
		// 클라이언트는 자기가 보낸 것만 쌓이고 서버가 멈추면 하트비트가 끊으므로 보낼 큐는 제한하지 않음
		return std::make_unique<TcpClientTransport>(ConnectionOptions{ settings.closeTimeout, std::nullopt });
	}
	return nullptr;
}
}
