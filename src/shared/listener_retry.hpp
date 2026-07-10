#pragma once

#include <SFML/Network.hpp>
#include "tls_socket.hpp"
#include <fmt/core.h>

#include <chrono>
#include <thread>

namespace listener_retry
{
// Binding right after a restart can race the old process releasing the port,
// so retry briefly before giving up.
inline bool listenWithRetry(
    bayou::tls::Listener& listener,
    unsigned short port,
    std::chrono::milliseconds timeout = std::chrono::seconds(5),
    std::chrono::milliseconds retryInterval = std::chrono::milliseconds(250))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (listener.listen(port) != sf::Socket::Status::Done)
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            fmt::println(
                "Failed to listen on port {} after {} seconds",
                port,
                std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
            return false;
        }
        fmt::println("Port {} is temporarily unavailable; retrying...", port);
        std::this_thread::sleep_for(retryInterval);
    }
    return true;
}
}
