#pragma once

#include <SFML/Network.hpp>

#include <memory>
#include <optional>
#include <string>

namespace bayou::tls
{
// TLS-protected, length-framed transport for the game's existing sf::Packet
// payloads. Client connections verify both the certificate chain and server
// name; server connections load their certificate and key from the environment.
class Socket
{
public:
    Socket();
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) noexcept;
    Socket& operator=(Socket&&) noexcept;

    sf::Socket::Status connect(const sf::IpAddress& address, unsigned short port);
    sf::Socket::Status send(const sf::Packet& packet);
    sf::Socket::Status receive(sf::Packet& packet);

    void disconnect();
    void setBlocking(bool blocking);
    [[nodiscard]] bool isBlocking() const;
    [[nodiscard]] std::optional<sf::IpAddress> getRemoteAddress() const;
    [[nodiscard]] unsigned short getRemotePort() const;

    // Must be called before connect when the certificate identity differs from
    // BAYOU_TLS_SERVER_NAME (which defaults to localhost for development).
    void setServerName(std::string serverName);

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    friend class Listener;
};

class Listener
{
public:
    Listener();
    ~Listener();

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    sf::Socket::Status listen(unsigned short port);
    sf::Socket::Status accept(Socket& socket);
    void setBlocking(bool blocking);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
}
