cmake_minimum_required(VERSION 3.25)
project(tcp_engine LANGUAGES CXX)

find_package(boost_asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
