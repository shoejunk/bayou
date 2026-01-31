cmake_minimum_required(VERSION 3.28)
project(CppModulesExample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable modules (CMake 3.28+)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Include CPM.cmake
include(cmake/CPM.cmake)

# Add dependencies via CPM
CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 11.0.2
)

CPMAddPackage(
    NAME SFML
    GITHUB_REPOSITORY SFML/SFML
    GIT_TAG 3.0.0
    OPTIONS
        "SFML_BUILD_AUDIO OFF"
        "SFML_BUILD_NETWORK OFF"
)

add_executable(app)

target_sources(app
    PUBLIC
        src/main.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/math.cppm
)

target_link_libraries(app PRIVATE fmt::fmt SFML::Graphics SFML::Window SFML::System)
