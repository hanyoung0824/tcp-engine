cmake_minimum_required(VERSION 3.25)
project(tcp_engine LANGUAGES CXX)

find_package(boost_asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

# tcp::engine: 서버/클라이언트 앱이 링크하고 Server 또는 Client를 상속
# 각 폴더의 include/만 앱에 공개하고 src/는 엔진 내부에서만 씀
add_library(tcp_engine STATIC
	common/include/HeartbeatSettings.hpp
	common/include/IPacket.hpp
	common/include/Log.hpp
	common/include/PacketReader.hpp
	common/include/PacketWriter.hpp
	common/include/Session.hpp
	common/src/ConsoleLogger.hpp
	common/src/ConsoleLogger.cpp
	common/src/IPacket.cpp
	common/src/PacketHeader.hpp
	common/src/PacketManager.hpp
	common/src/PacketManager.cpp
	common/src/PacketReader.cpp
	common/src/PacketWriter.cpp
	common/src/SessionConfig.hpp
	common/src/SessionEventHandler.hpp
	common/src/TcpSession.hpp
	common/src/TcpSession.cpp

	server/include/Server.hpp
	server/include/ServerSettings.hpp
	server/src/Server.cpp
	server/src/ServerTransport.hpp
	server/src/TcpServerTransport.hpp
	server/src/TcpServerTransport.cpp
	server/src/Listener.hpp
	server/src/Listener.cpp
	server/src/SessionManager.hpp
	server/src/SessionManager.cpp

	client/include/Client.hpp
	client/include/ClientSettings.hpp
	client/src/Client.cpp
	client/src/ClientTransport.hpp
	client/src/TcpClientTransport.hpp
	client/src/TcpClientTransport.cpp
	client/src/Connector.hpp
	client/src/Connector.cpp
)
add_library(tcp::engine ALIAS tcp_engine)

target_include_directories(tcp_engine
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/common/include
		${CMAKE_CURRENT_SOURCE_DIR}/server/include
		${CMAKE_CURRENT_SOURCE_DIR}/client/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/common/src
		${CMAKE_CURRENT_SOURCE_DIR}/server/src
		${CMAKE_CURRENT_SOURCE_DIR}/client/src
)
target_link_libraries(tcp_engine PUBLIC Boost::asio Threads::Threads)
target_compile_features(tcp_engine PUBLIC cxx_std_20)

if(WIN32)
	target_compile_definitions(tcp_engine PUBLIC _WIN32_WINNT=0x0A00)
endif()

if(MSVC)
	target_compile_options(tcp_engine PRIVATE /utf-8)
endif()
