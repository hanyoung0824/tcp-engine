#include "ServerTransportFactory.hpp"

#include "TcpServerTransport.hpp"

namespace tcpengine
{
std::unique_ptr<ServerTransport> makeServerTransport(boost::asio::any_io_executor executor, const ServerSettings& settings,
	std::shared_ptr<const ConnectionEventHandler> handler)
{
	switch (settings.transport) {
	case TransportType::Tcp:
		return std::make_unique<TcpServerTransport>(std::move(executor), settings.port, settings.closeTimeout, std::move(handler));
	}
	return nullptr;
}
}
