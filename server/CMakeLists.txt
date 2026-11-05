# tcp::server: 서버 앱은 이 타깃을 링크하고 Server를 상속한다.
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
