# tcp::client: 클라이언트 앱은 이 타깃을 링크하고 Client를 상속한다.
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
