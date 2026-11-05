#pragma once

#include "Log.hpp"
#include "PacketReader.hpp"
#include "PacketWriter.hpp"

#include <cstdint>
#include <memory>

class Session;

class IPacket
{
public:
	virtual ~IPacket() = default;

	virtual std::uint16_t getPacketId() const = 0;

	virtual bool read(PacketReader& reader) = 0;

	virtual void write(PacketWriter& writer) const = 0;

	virtual void handle(const std::shared_ptr<Session>& session) = 0;
};

// 보내는 쪽도 객체를 만들 수 있도록 handle()을 순수 가상 대신 기본 구현으로 둔다.

class ClientToServerPacket : public IPacket
{
public:
	void handle(const std::shared_ptr<Session>&) override
	{
		writeLog(LogLevel::Warning, "Packet id ", getPacketId(), " has no handler on this side");
	}
};

class ServerToClientPacket : public IPacket
{
public:
	void handle(const std::shared_ptr<Session>&) override
	{
		writeLog(LogLevel::Warning, "Packet id ", getPacketId(), " has no handler on this side");
	}
};
