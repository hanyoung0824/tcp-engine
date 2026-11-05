# 서버 엔진: 서버 프로그램의 뼈대(Server), TCP 서버(TcpServer), 접속 받기(Listener), 세션 목록(SessionManager)
add_library(tcpengine_server STATIC
	Server.hpp
	Server.cpp
	TcpServer.hpp
	TcpServer.cpp
	Listener.hpp
	Listener.cpp
	SessionManager.hpp
	SessionManager.cpp
)
add_library(tcp::server ALIAS tcpengine_server)

target_include_directories(tcpengine_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcpengine_server PUBLIC tcpengine_common)

if(MSVC)
	target_compile_options(tcpengine_server PRIVATE /utf-8)
endif()
