cmake_minimum_required(VERSION 3.16)
project(BlackHoleSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML CONFIG REQUIRED COMPONENTS Graphics Window System)

add_executable(BlackHoleSimulator main.cpp)

target_link_libraries(BlackHoleSimulator PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)