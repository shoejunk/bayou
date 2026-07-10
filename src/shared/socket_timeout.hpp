#pragma once

#include <SFML/Network.hpp>
#include "tls_socket.hpp"

#include <chrono>
#include <thread>

namespace socket_timeout
{
inline sf::Socket::Status receivePacket(
    bayou::tls::Socket& socket,
    sf::Packet& packet,
    std::chrono::milliseconds timeout,
    std::chrono::milliseconds pollInterval = std::chrono::milliseconds(25))
{
    const bool wasBlocking = socket.isBlocking();
    socket.setBlocking(false);

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const sf::Socket::Status status = socket.receive(packet);
        if (status != sf::Socket::Status::NotReady)
        {
            socket.setBlocking(wasBlocking);
            return status;
        }

        std::this_thread::sleep_for(pollInterval);
    }

    socket.setBlocking(wasBlocking);
    return sf::Socket::Status::NotReady;
}
}
