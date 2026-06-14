cmake_minimum_required(VERSION 3.28)
project(CppModulesExample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BAYOU_BUILD_CLIENTS "Build graphical client and card editor executables" ON)
option(BAYOU_BUILD_SERVERS "Build server executables" ON)
option(BAYOU_BUILD_ACCOUNTS "Build the accounts server executable" ON)
option(BAYOU_BUILD_MATCHMAKING "Build the matchmaking server executable" ON)
option(BAYOU_BUILD_GAMESERVER "Build the game server executable" ON)
option(BAYOU_BUILD_CARDSERVER "Build the card server executable" ON)

if(BAYOU_BUILD_CLIENTS OR BAYOU_BUILD_ACCOUNTS OR BAYOU_BUILD_MATCHMAKING OR BAYOU_BUILD_GAMESERVER)
    # Enable modules for targets that still import src/shared/network.cppm.
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
else()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

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
        "SFML_BUILD_WINDOW ${BAYOU_BUILD_CLIENTS}"
        "SFML_BUILD_GRAPHICS ${BAYOU_BUILD_CLIENTS}"
)

# Shared modules
set(SHARED_MODULES
    src/shared/network.cppm
)

if(BAYOU_BUILD_CLIENTS)
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

    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/client.cfg"
            "$<TARGET_FILE_DIR:client>/client.cfg")
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_ACCOUNTS)
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
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_MATCHMAKING)
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
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_GAMESERVER)
    # Game server executable
    add_executable(gameserver)

    target_sources(gameserver
        PUBLIC
            src/gameserver/main.cpp
        PUBLIC
            FILE_SET CXX_MODULES FILES
                src/shared/network.cppm
    )

    target_link_libraries(gameserver PRIVATE fmt::fmt SFML::System SFML::Network)
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_CARDSERVER)
    # Card server executable
    add_executable(cardserver)

    target_sources(cardserver
        PUBLIC
            src/cardserver/main.cpp
    )

    target_link_libraries(cardserver PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network)

    install(TARGETS cardserver RUNTIME DESTINATION bin)
endif()

if(BAYOU_BUILD_CLIENTS)
    # Card editor executable
    add_executable(cardeditor)

    target_sources(cardeditor
        PUBLIC
            src/cardeditor/main.cpp
        PUBLIC
            FILE_SET CXX_MODULES FILES
                src/shared/network.cppm
    )

    target_link_libraries(cardeditor PRIVATE fmt::fmt SFML::Graphics SFML::Window SFML::System SFML::Network)

    add_custom_command(TARGET cardeditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cardeditor.cfg"
            "$<TARGET_FILE_DIR:cardeditor>/cardeditor.cfg")
endif()
