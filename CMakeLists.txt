cmake_minimum_required(VERSION 3.14)

project(MyProject)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_executable(app src/main.cpp)

target_link_libraries(app PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)