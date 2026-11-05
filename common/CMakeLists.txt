# tcp::common: 서버와 클라이언트 엔진이 함께 쓰는 공용 부분이다.
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

target_include_directories(tcpengine_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcpengine_common PUBLIC Boost::asio Threads::Threads)

target_compile_features(tcpengine_common PUBLIC cxx_std_20)

if(WIN32)
	target_compile_definitions(tcpengine_common PUBLIC _WIN32_WINNT=0x0A00)
endif()

if(MSVC)
	target_compile_options(tcpengine_common PRIVATE /utf-8)
endif()
