cmake_minimum_required(VERSION 3.28)
project(Bayou LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BAYOU_BUILD_CLIENTS "Build graphical client executable" ON)
option(BAYOU_BUILD_SERVERS "Build server executables" ON)
option(BAYOU_BUILD_ACCOUNTS "Build the accounts server executable" ON)
option(BAYOU_BUILD_MATCHMAKING "Build the matchmaking server executable" ON)
option(BAYOU_BUILD_GAMESERVER "Build the game server executable" ON)
option(BAYOU_BUILD_CARDSERVER "Build the card server executable" ON)

if(BAYOU_BUILD_CLIENTS)
    # Enable modules for the graphical targets.
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
        "SFML_BUILD_AUDIO ${BAYOU_BUILD_CLIENTS}"
        "SFML_BUILD_NETWORK ON"
        "SFML_BUILD_WINDOW ${BAYOU_BUILD_CLIENTS}"
        "SFML_BUILD_GRAPHICS ${BAYOU_BUILD_CLIENTS}"
)

CPMAddPackage(
    NAME asio_source
    GITHUB_REPOSITORY chriskohlhoff/asio
    GIT_TAG asio-1-30-2
    DOWNLOAD_ONLY YES
)

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
CPMAddPackage(
    NAME MbedTLS
    GITHUB_REPOSITORY Mbed-TLS/mbedtls
    GIT_TAG mbedtls-3.6.6
)

add_library(bayou_transport
    src/shared/tls_socket.cpp
)
target_include_directories(bayou_transport
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shared"
        "${asio_source_SOURCE_DIR}/asio/include"
)
target_compile_definitions(bayou_transport PUBLIC ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_compile_definitions(
    bayou_transport
    PRIVATE
        $<$<CONFIG:Debug>:BAYOU_DEVELOPMENT_TLS_DEFAULTS>
        $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00>
)
target_link_libraries(bayou_transport PUBLIC SFML::Network MbedTLS::mbedtls)

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_ACCOUNTS)
    set(SODIUM_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    CPMAddPackage(
        NAME Sodium
        GITHUB_REPOSITORY robinlinden/libsodium-cmake
        GIT_TAG cfebfd3da486d5a86c644c8b47067e5411c7599c
    )
endif()

if(BAYOU_BUILD_CLIENTS)
    # Client executable
    add_executable(client)

    set(BAYOU_CLIENT_BUILD_IDENTITY_HEADER
        "${CMAKE_CURRENT_BINARY_DIR}/generated/client_build_identity.hpp")
    add_custom_target(client_build_identity
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_FILE=${BAYOU_CLIENT_BUILD_IDENTITY_HEADER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateClientBuildIdentity.cmake"
        BYPRODUCTS "${BAYOU_CLIENT_BUILD_IDENTITY_HEADER}"
        COMMENT "Embedding client source provenance"
        VERBATIM)
    add_dependencies(client client_build_identity)
    target_include_directories(client PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

    target_sources(client
        PUBLIC
            src/client/main.cpp
            src/client/card_editor_assets.cpp
            src/client/client_board_layout.cpp
            src/client/client_card_text.cpp
            src/client/client_config.cpp
            src/client/client_display.cpp
            src/client/client_sandbox.cpp
            src/client/client_story.cpp
            src/client/client_story_catalog.cpp
            src/client/client_story_cards.cpp
            src/client/client_string.cpp
            src/client/client_textures.cpp
            src/client/client_ui.cpp
            src/client/client_ui_capture.cpp
            src/client/deck_collection.cpp
            src/gameserver/ai_player.cpp
        PUBLIC
            FILE_SET CXX_MODULES FILES
                src/shared/network.cppm
                src/shared/inputbox.cppm
                src/client/button.cppm
                src/client/client_controls.cppm
                src/client/client_services.cppm
                src/client/conquest_services.cppm
                src/client/conquest_screen.cppm
                src/client/card_editor_screen.cppm
    )

    if(MSVC)
        # main.cpp plus the imported screen modules exceed the default COFF
        # section limit.
        target_compile_options(client PRIVATE /bigobj)
    endif()

    target_link_libraries(client PRIVATE fmt::fmt SFML::Audio SFML::Graphics SFML::Window SFML::System SFML::Network bayou_transport)
    if(WIN32)
        target_link_libraries(client PRIVATE Crypt32)
    endif()
    set_target_properties(client PROPERTIES OUTPUT_NAME "SteamTactics")

    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/client_debug.cfg"
            "$<TARGET_FILE_DIR:client>/client_debug.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/client_release.cfg"
            "$<TARGET_FILE_DIR:client>/client_release.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/deploy/ca/isrg-root-x1.pem"
            "$<TARGET_FILE_DIR:client>/bayou-ca.pem")

    add_executable(storymodetest
        src/storymodetest/main.cpp
        src/client/client_story.cpp
        src/client/client_story_catalog.cpp
        src/client/client_story_cards.cpp
    )
    target_link_libraries(storymodetest PRIVATE fmt::fmt SFML::System SFML::Network)
    target_compile_definitions(storymodetest PRIVATE BAYOU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    # Network-backed drift gate for the deployed Story Mode card definitions.
    # Keep this separate from storymodetest: the offline suite must remain
    # deterministic and runnable when the authoritative card service is down.
    add_executable(storylivecatalogtest
        src/storylivecatalogtest/main.cpp
        src/client/client_story.cpp
        src/client/client_story_catalog.cpp
        src/client/client_story_cards.cpp
        src/gameserver/ai_player.cpp
        src/shared/card_source_config.cpp
    )
    target_link_libraries(
        storylivecatalogtest
        PRIVATE fmt::fmt SFML::System SFML::Network bayou_transport)
    target_compile_definitions(
        storylivecatalogtest PRIVATE BAYOU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_command(TARGET storylivecatalogtest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/client_debug.cfg"
            "$<TARGET_FILE_DIR:storylivecatalogtest>/client_debug.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/client_release.cfg"
            "$<TARGET_FILE_DIR:storylivecatalogtest>/client_release.cfg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/deploy/ca/isrg-root-x1.pem"
            "$<TARGET_FILE_DIR:storylivecatalogtest>/bayou-ca.pem")
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_ACCOUNTS)
    # Accounts server executable
    add_executable(accounts)

    target_sources(accounts
        PUBLIC
            src/accounts/main.cpp
            src/accounts/account_catalog.cpp
            src/accounts/account_conquest.cpp
            src/accounts/account_conquest_events.cpp
            src/accounts/account_decks.cpp
            src/accounts/account_rate_limiter.cpp
            src/accounts/account_security.cpp
            src/accounts/account_tokens.cpp
            src/accounts/starter_deck_database.cpp
            src/shared/card_database.cpp
            src/shared/card_source_config.cpp
            src/shared/card_server_client.cpp
    )

    target_link_libraries(accounts PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network sodium bayou_transport)
    install(TARGETS accounts RUNTIME DESTINATION bin)

    # Deterministic Conquest persistence and strategy-rule test suite.
    add_executable(conquesttest
        src/conquesttest/main.cpp
        src/accounts/account_catalog.cpp
        src/accounts/account_conquest.cpp
        src/accounts/account_conquest_events.cpp
        src/accounts/account_decks.cpp
    )
    target_link_libraries(
        conquesttest
        PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network sodium)

    add_executable(starterdecktest
        src/starterdecktest/main.cpp
        src/accounts/starter_deck_database.cpp
    )
    target_link_libraries(starterdecktest PRIVATE SQLiteCpp)
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_MATCHMAKING)
    # Matchmaking server executable
    add_executable(matchmaking)

    target_sources(matchmaking
        PUBLIC
            src/matchmaking/main.cpp
    )

    target_link_libraries(matchmaking PRIVATE fmt::fmt SFML::System SFML::Network bayou_transport)
    install(TARGETS matchmaking RUNTIME DESTINATION bin)
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_GAMESERVER)
    # Game server executable
    add_executable(gameserver)

    target_sources(gameserver
        PUBLIC
            src/gameserver/main.cpp
            src/gameserver/ai_deck.cpp
            src/gameserver/ai_player.cpp
            src/gameserver/conquest_battle_manager.cpp
            src/accounts/account_catalog.cpp
            src/accounts/account_decks.cpp
            src/shared/card_database.cpp
            src/shared/card_source_config.cpp
            src/shared/card_server_client.cpp
    )

    target_link_libraries(gameserver PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network bayou_transport)
    install(TARGETS gameserver RUNTIME DESTINATION bin)
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_GAMESERVER)
    # End-to-end integration test driver
    add_executable(gametest)

    target_sources(gametest
        PUBLIC
            src/gametest/main.cpp
            src/gameserver/ai_player.cpp
    )

    target_link_libraries(gametest PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network bayou_transport)
    # This integration driver intentionally keeps dozens of independent
    # GameEngine fixtures alive in one function so failures can be reported in
    # a single run. MSVC's default 1 MiB stack is too small for that Debug-only
    # test shape as the authoritative scenario matrix grows.
    if(MSVC)
        target_link_options(gametest PRIVATE /STACK:8388608)
    endif()
endif()

if(BAYOU_BUILD_SERVERS AND BAYOU_BUILD_CARDSERVER)
    # Card server executable
    add_executable(cardserver)

    target_sources(cardserver
        PUBLIC
            src/cardserver/main.cpp
            src/shared/card_database.cpp
    )

    target_link_libraries(cardserver PRIVATE fmt::fmt SQLiteCpp SFML::System SFML::Network bayou_transport)

    install(TARGETS cardserver RUNTIME DESTINATION bin)
endif()
