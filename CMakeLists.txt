cmake_minimum_required(VERSION 3.10)
project(mem-hacker)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

add_executable(mem-hacker main.cc mem_tool.cc mem_tool.hh)
