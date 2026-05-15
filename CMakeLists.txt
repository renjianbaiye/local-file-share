cmake_minimum_required(VERSION 3.16)

project(LocalFileShare
    VERSION 0.1.0
    DESCRIPTION "A lightweight C++ local network file sharing tool"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(LocalFileShare
    src/main.cpp
)

target_include_directories(LocalFileShare PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if (WIN32)
    target_link_libraries(LocalFileShare PRIVATE ws2_32)
endif()

if (MSVC)
    target_compile_options(LocalFileShare PRIVATE /utf-8)
endif()
