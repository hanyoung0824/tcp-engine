# 클라이언트 엔진: 클라이언트 프로그램의 뼈대(Client), TCP 클라이언트(TcpClient), 접속하기(Connector)
add_library(tcpengine_client STATIC
	Client.hpp
	Client.cpp
	TcpClient.hpp
	TcpClient.cpp
	Connector.hpp
	Connector.cpp
)
add_library(tcp::client ALIAS tcpengine_client)

target_include_directories(tcpengine_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcpengine_client PUBLIC tcpengine_common)

if(MSVC)
	target_compile_options(tcpengine_client PRIVATE /utf-8)
endif()
