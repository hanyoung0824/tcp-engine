#pragma once

#include <optional>
#include <string_view>

namespace tcpengine
{
// 연결에 쓸 프로토콜. 구현이 추가되면 여기에 늘어남
enum class TransportType
{
	Tcp,
};

// 설정 파일의 이름("tcp")을 바꿈. 모르는 이름이면 nullopt
inline std::optional<TransportType> parseTransportType(std::string_view name)
{
	if (name == "tcp") {
		return TransportType::Tcp;
	}
	return std::nullopt;
}
}
