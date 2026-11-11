#include "ClientTransportFactory.hpp"

#include "TcpClientTransport.hpp"

namespace tcpengine
{
std::unique_ptr<ClientTransport> makeClientTransport(const ClientSettings& settings)
{
	switch (settings.transport) {
	case TransportType::Tcp:
		return std::make_unique<TcpClientTransport>(settings.closeTimeout);
	}
	return nullptr;
}
}
