cmake_minimum_required(VERSION 3.21)

project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_executable(app main.cpp)   # добавь WIN32, если не нужна консоль

target_link_libraries(app PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

# Статический рантайм GCC (libgcc, libstdc++, winpthread)
target_link_options(app PRIVATE -static -static-libgcc -static-libstdc++)