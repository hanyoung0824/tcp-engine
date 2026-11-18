#include "ServerTransportFactory.hpp"

#include "TcpServerTransport.hpp"

namespace tcpengine
{
std::unique_ptr<ServerTransport> makeServerTransport(boost::asio::any_io_executor executor, const ServerSettings& settings,
	std::shared_ptr<const ConnectionEvents> events, std::shared_ptr<NetworkStats> stats, ServerTransport::CanAccept canAccept)
{
	switch (settings.transport) {
	case TransportType::Tcp:
		return std::make_unique<TcpServerTransport>(std::move(executor), settings.port,
			ConnectionOptions{ settings.closeTimeout, settings.sendQueueLimit, std::move(stats) }, std::move(canAccept), std::move(events));
	}
	return nullptr;
}
}
