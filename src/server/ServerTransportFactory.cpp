#include "ServerTransportFactory.hpp"

#include "TcpServerTransport.hpp"

namespace tcpengine
{
std::unique_ptr<ServerTransport> makeServerTransport(const ServerSettings& settings,
	std::shared_ptr<const ConnectionEventHandler> handler)
{
	switch (settings.transport) {
	case TransportType::Tcp:
		return std::make_unique<TcpServerTransport>(settings.port, std::move(handler));
	}
	return nullptr;
}
}
