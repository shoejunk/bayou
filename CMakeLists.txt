cmake_minimum_required(VERSION 3.28)
project(CppModulesExample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable modules (CMake 3.28+)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_executable(app)

target_sources(app
    PUBLIC
        src/main.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/math.cppm
)
