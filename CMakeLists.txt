cmake_minimum_required(VERSION 3.25)
project(net_tcp_engine LANGUAGES CXX)

# Boost.Asio 기반 TCP 네트워크 엔진.
# 혼자 빌드할 수도 있고, 다른 프로젝트에 submodule로 넣어 add_subdirectory()로 쓸 수도 있다.
# 패킷 틀과 로그 창구는 TCP와 무관하다. UDP 같은 다른 전송 방식의 엔진이 생기면 공용 저장소(net-core)로 뗀다.
#
# 쓰는 쪽이 링크하는 타깃:
# - tcp::common: 연결(Session), 패킷 틀(IPacket, PacketManager, PacketReader/Writer), 로그 창구
# - tcp::server: 서버 프로그램의 뼈대(Server)와 접속 받기, 세션 목록
# - tcp::client: 클라이언트 프로그램의 뼈대(Client)와 접속하기, 네트워크 스레드
#
# C++20, Windows 설정 같은 필요 조건은 각 타깃이 공개하므로, 쓰는 쪽은 타깃만 링크하면 된다.

# Boost.Asio 1.88 (vcpkg). 다른 프로젝트 안에서 쓸 때는 그 프로젝트의 vcpkg.json에도 boost-asio가 있어야 한다.
find_package(boost_asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
