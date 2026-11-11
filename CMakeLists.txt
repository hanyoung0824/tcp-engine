cmake_minimum_required(VERSION 3.25)
project(tcp_engine LANGUAGES CXX)

find_package(boost_asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

# 층 구조: utility, packet ← network ← server, client
# 공개 헤더는 include/tcpengine/<층>/에 두고 "tcpengine/<층>/이름.hpp"로 include
# src/<층>/은 엔진 안에서만 씀. 다른 층의 내부 헤더가 필요하면 *_internal 타깃을 링크
# 응용은 protocol이 tcp::packet, 서버가 tcp::server, 클라이언트가 tcp::client만 링크

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/tcpengine)
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 빌드 속도용. 각 파일은 쓰는 헤더를 직접 include함
set(PCH_HEADERS
	<boost/asio.hpp>
	<nlohmann/json.hpp>
	<atomic>
	<chrono>
	<functional>
	<memory>
	<mutex>
	<optional>
	<span>
	<sstream>
	<string>
	<string_view>
	<vector>
)

function(tcp_engine_add_layer target layer)
	add_library(${target} STATIC ${ARGN})
	target_include_directories(${target}
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
		PRIVATE ${SOURCE_DIR}/${layer}
	)
	target_compile_features(${target} PUBLIC cxx_std_20)
	if(MSVC)
		target_compile_options(${target} PRIVATE /utf-8)
	endif()
endfunction()

function(tcp_engine_add_internal_headers target layer)
	add_library(${target} INTERFACE)
	target_include_directories(${target} INTERFACE ${SOURCE_DIR}/${layer})
endfunction()

# utility
tcp_engine_add_layer(tcp_engine_utility utility
	${INCLUDE_DIR}/utility/ConsoleSink.hpp
	${INCLUDE_DIR}/utility/JsonConfig.hpp
	${INCLUDE_DIR}/utility/Log.hpp
	${INCLUDE_DIR}/utility/LogSink.hpp
	${SOURCE_DIR}/utility/ConsoleSink.cpp
	${SOURCE_DIR}/utility/JsonConfig.cpp
	${SOURCE_DIR}/utility/Logger.hpp
	${SOURCE_DIR}/utility/Logger.cpp
)
add_library(tcp::utility ALIAS tcp_engine_utility)
target_link_libraries(tcp_engine_utility PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
tcp_engine_add_internal_headers(tcp_engine_utility_internal utility)

# packet (응용 protocol이 연결 등을 몰라도 되도록 network와 별도 타깃)
tcp_engine_add_layer(tcp_engine_packet network/packet
	${INCLUDE_DIR}/network/packet/Packet.hpp
	${INCLUDE_DIR}/network/packet/PacketReader.hpp
	${INCLUDE_DIR}/network/packet/PacketWriter.hpp
	${SOURCE_DIR}/network/packet/Packet.cpp
	${SOURCE_DIR}/network/packet/PacketReader.cpp
	${SOURCE_DIR}/network/packet/PacketWriter.cpp
)
add_library(tcp::packet ALIAS tcp_engine_packet)

# network
tcp_engine_add_layer(tcp_engine_network network
	${INCLUDE_DIR}/network/HeartbeatSettings.hpp
	${INCLUDE_DIR}/network/PacketDispatcher.hpp
	${INCLUDE_DIR}/network/TransportType.hpp
	${INCLUDE_DIR}/network/Connection.hpp
	${SOURCE_DIR}/network/PacketHeader.hpp
	${SOURCE_DIR}/network/PacketManager.hpp
	${SOURCE_DIR}/network/PacketManager.cpp
	${SOURCE_DIR}/network/ConnectionEventHandler.hpp
	${SOURCE_DIR}/network/HeartbeatMonitor.hpp
	${SOURCE_DIR}/network/HeartbeatMonitor.cpp
	${SOURCE_DIR}/network/TcpConnection.hpp
	${SOURCE_DIR}/network/TcpConnection.cpp
)
add_library(tcp::network ALIAS tcp_engine_network)
target_link_libraries(tcp_engine_network
	PUBLIC tcp::packet tcp::utility Boost::asio Threads::Threads
	PRIVATE tcp_engine_utility_internal
)
if(WIN32)
	target_compile_definitions(tcp_engine_network PUBLIC _WIN32_WINNT=0x0A00)
endif()
target_precompile_headers(tcp_engine_network PRIVATE ${PCH_HEADERS})
tcp_engine_add_internal_headers(tcp_engine_network_internal network)

# server
tcp_engine_add_layer(tcp_engine_server server
	${INCLUDE_DIR}/server/Server.hpp
	${INCLUDE_DIR}/server/ServerSettings.hpp
	${INCLUDE_DIR}/server/Session.hpp
	${SOURCE_DIR}/server/Listener.hpp
	${SOURCE_DIR}/server/Listener.cpp
	${SOURCE_DIR}/server/Server.cpp
	${SOURCE_DIR}/server/ServerSettings.cpp
	${SOURCE_DIR}/server/ServerTransport.hpp
	${SOURCE_DIR}/server/ServerTransportFactory.hpp
	${SOURCE_DIR}/server/ServerTransportFactory.cpp
	${SOURCE_DIR}/server/SessionManager.hpp
	${SOURCE_DIR}/server/SessionManager.cpp
	${SOURCE_DIR}/server/ConnectionManager.hpp
	${SOURCE_DIR}/server/ConnectionManager.cpp
	${SOURCE_DIR}/server/TcpServerTransport.hpp
	${SOURCE_DIR}/server/TcpServerTransport.cpp
)
add_library(tcp::server ALIAS tcp_engine_server)
target_link_libraries(tcp_engine_server
	PUBLIC tcp::network
	PRIVATE tcp_engine_network_internal tcp_engine_utility_internal
)
target_precompile_headers(tcp_engine_server PRIVATE ${PCH_HEADERS})

# client
tcp_engine_add_layer(tcp_engine_client client
	${INCLUDE_DIR}/client/Client.hpp
	${INCLUDE_DIR}/client/ClientSettings.hpp
	${SOURCE_DIR}/client/Client.cpp
	${SOURCE_DIR}/client/ClientSettings.cpp
	${SOURCE_DIR}/client/ClientTransport.hpp
	${SOURCE_DIR}/client/ClientTransportFactory.hpp
	${SOURCE_DIR}/client/ClientTransportFactory.cpp
	${SOURCE_DIR}/client/TcpClientTransport.hpp
	${SOURCE_DIR}/client/TcpClientTransport.cpp
)
add_library(tcp::client ALIAS tcp_engine_client)
target_link_libraries(tcp_engine_client
	PUBLIC tcp::network
	PRIVATE tcp_engine_network_internal tcp_engine_utility_internal
)
target_precompile_headers(tcp_engine_client PRIVATE ${PCH_HEADERS})
