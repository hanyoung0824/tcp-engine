#include "PacketManager.hpp"

#include "Log.hpp"
#include "PacketHeader.hpp"
#include "PacketReader.hpp"
#include "Session.hpp"

bool PacketManager::addFactory(std::uint16_t id, PacketFactory factory)
{
	// emplace는 같은 키가 이미 있으면 넣지 않는다 (덮어쓰지 않는다).
	if (!m_factories.emplace(id, std::move(factory)).second) {
		writeLog(LogLevel::Error, "Packet id ", id, " is already registered");
		return false;
	}
	return true;
}

std::optional<std::size_t> PacketManager::onReceive(const std::shared_ptr<Session>& session,
	std::span<const std::uint8_t> data) const
{
	// 한 번에 받은 바이트에 패킷이 여러 개 붙어 있을 수 있으므로, 완성된 패킷이 있는 동안 반복한다.
	// 패킷 처리 중에 세션이 닫히면(예: handle()에서 close() 호출) 남은 패킷은 처리하지 않는다.
	std::size_t processed = 0;
	while (!session->isClosed()) {
		const std::span<const std::uint8_t> rest = data.subspan(processed);

		// 1. 헤더(size + id)가 다 오지 않았으면 다음에 이어서 처리한다.
		if (rest.size() < packetHeaderSize) {
			break;
		}

		PacketReader header(rest.first(packetHeaderSize));
		std::uint16_t size = 0;
		std::uint16_t id = 0;
		header.readUInt16(size);
		header.readUInt16(id);

		// 2. size 검사: 헤더보다 작거나 최대 크기보다 크면 잘못된(또는 악의적인) 패킷이다.
		// size가 잘못되면 이후 패킷의 경계도 알 수 없다.
		if (size < packetHeaderSize || size > maxPacketSize) {
			writeLog(LogLevel::Warning, "Invalid packet size ", size, " from ", session->getName());
			return std::nullopt;
		}

		// 3. 패킷 전체가 다 오지 않았으면 다음에 이어서 처리한다.
		if (rest.size() < size) {
			break;
		}

		// 4. 등록된 패킷 클래스 찾기. 없으면 모르는 패킷이거나 상대가 보내면 안 되는 패킷이다
		// (예: 클라이언트가 S2C_ChatNotify를 보냄. 서버는 S2C_ 패킷을 등록하지 않는다).
		const auto it = m_factories.find(id);
		if (it == m_factories.end()) {
			writeLog(LogLevel::Warning, "Unknown packet id ", id, " from ", session->getName());
			return std::nullopt;
		}

		// 5. 이 패킷 전용 객체를 만들어 본문을 읽는다. 형식이 틀렸거나 본문이 남으면 잘못된 패킷이다.
		std::unique_ptr<IPacket> packet = it->second();
		PacketReader body(rest.subspan(packetHeaderSize, size - packetHeaderSize));
		if (!packet->read(body) || !body.isEnd()) {
			writeLog(LogLevel::Warning, "Invalid packet id ", id, " from ", session->getName());
			return std::nullopt;
		}

		// 6. 처리
		packet->handle(session);

		processed += size;
	}
	return processed;
}
