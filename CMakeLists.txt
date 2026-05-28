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
    NAME SQLiteCpp
    GITHUB_REPOSITORY SRombauts/SQLiteCpp
    GIT_TAG 3.3.3
    OPTIONS
        "SQLITECPP_BUILD_EXAMPLES OFF"
        "SQLITECPP_BUILD_TESTS OFF"
)

CPMAddPackage(
    NAME SFML
    GITHUB_REPOSITORY SFML/SFML
    GIT_TAG 3.0.0
    OPTIONS
        "SFML_BUILD_AUDIO OFF"
        "SFML_BUILD_NETWORK ON"
)

# Shared modules
set(SHARED_MODULES
    src/shared/network.cppm
)

# Client executable
add_executable(client)

target_sources(client
    PUBLIC
        src/client/main.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/shared/network.cppm
            src/client/button.cppm
            src/client/inputbox.cppm
)

target_link_libraries(client PRIVATE fmt::fmt SFML::Graphics SFML::Window SFML::System SFML::Network)

# Accounts server executable
add_executable(accounts)

target_sources(accounts
    PUBLIC
        src/accounts/main.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/shared/network.cppm
)

target_link_libraries(accounts PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network)

# Matchmaking server executable
add_executable(matchmaking)

target_sources(matchmaking
    PUBLIC
        src/matchmaking/main.cpp
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/shared/network.cppm
)

target_link_libraries(matchmaking PRIVATE fmt::fmt SFML::System SFML::Network)
