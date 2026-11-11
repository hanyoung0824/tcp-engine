#include "ClientTransportFactory.hpp"

#include "TcpClientTransport.hpp"

namespace tcpengine
{
std::unique_ptr<ClientTransport> makeClientTransport(TransportType type)
{
	switch (type) {
	case TransportType::Tcp:
		return std::make_unique<TcpClientTransport>();
	}
	return nullptr;
}
}
