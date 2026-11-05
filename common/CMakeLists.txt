# 네트워크 공용 (서버 엔진과 클라이언트 엔진이 함께 쓴다). 패킷 목록은 모르므로 어느 프로젝트에도 그대로 쓸 수 있다.
# - Session: 연결 하나 (바이트만 다룬다)
# - 패킷 틀: 형식(PacketHeader), 만들기(PacketWriter), 읽기(PacketReader),
#   받은 바이트를 패킷 객체로 바꿔 처리하기(IPacket, PacketManager), 보내기 도우미(PacketHelper)
# - 로그 창구(Log): 엔진은 직접 출력하지 않고 응용이 등록한 함수로 로그를 넘긴다
#   ConsoleLog는 콘솔로 출력하는 기본 로그 함수 (응용이 쓸지 고른다)
add_library(tcpengine_common STATIC
	Log.hpp
	Log.cpp
	ConsoleLog.hpp
	Session.hpp
	Session.cpp
	PacketHeader.hpp
	IPacket.hpp
	PacketHelper.hpp
	PacketWriter.hpp
	PacketWriter.cpp
	PacketReader.hpp
	PacketReader.cpp
	PacketManager.hpp
	PacketManager.cpp
)
add_library(tcp::common ALIAS tcpengine_common)

# 링크하는 쪽에서 #include "Session.hpp"처럼 쓸 수 있도록 이 폴더를 include 경로로 공개한다.
# 헤더가 Asio를 쓰므로 Asio와 스레드 라이브러리도 함께 공개한다.
target_include_directories(tcpengine_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcpengine_common PUBLIC Boost::asio Threads::Threads)

# 헤더가 C++20(std::span 등)을 쓰므로 링크하는 쪽도 C++20으로 빌드되게 한다.
target_compile_features(tcpengine_common PUBLIC cxx_std_20)

if(WIN32)
	# Windows 10 이상 대상 (Asio의 "Please define _WIN32_WINNT" 경고를 없앤다). 헤더에서 쓰이므로 공개한다.
	target_compile_definitions(tcpengine_common PUBLIC _WIN32_WINNT=0x0A00)
endif()

if(MSVC)
	# 소스가 UTF-8이다 (한글 주석).
	target_compile_options(tcpengine_common PRIVATE /utf-8)
endif()
