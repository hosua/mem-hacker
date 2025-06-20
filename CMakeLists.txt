cmake_minimum_required(VERSION 3.10)
project(mem-hacker)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(mem-hacker main.cc mem_tool.cc mem_tool.hh)

target_link_libraries(mem-hacker ${CURSES_LIBRARIES})
