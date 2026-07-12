#include "card_server_client.hpp"

#include "card_database.hpp"
#include "network.hpp"
#include "socket_timeout.hpp"
#include "tls_socket.hpp"

#include <SFML/Network.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <utility>

namespace card_server_client
{
namespace
{
constexpr auto CardServerRequestTimeout = std::chrono::seconds(5);

bool connectToCardServer(
    bayou::tls::Socket& socket,
    const card_source_config::Config& config,
    std::string& error)
{
    const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(config.cardServerHost);
    if (!address)
    {
        error = "could not resolve card server " + config.cardServerHost;
        return false;
    }

    socket.setServerName(config.cardServerHost);
    if (socket.connect(*address, config.cardServerPort) != sf::Socket::Status::Done)
    {
        error = "could not connect to card server " + config.cardServerHost + ":" +
            std::to_string(config.cardServerPort);
        return false;
    }
    return true;
}

std::vector<card_data::Card> loadFromCardServer(
    const card_source_config::Config& config,
    std::string& error)
{
    bayou::tls::Socket socket;
    if (!connectToCardServer(socket, config, error))
    {
        return {};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        error = "could not send card list request";
        return {};
    }

    sf::Packet response;
    if (socket_timeout::receivePacket(socket, response, CardServerRequestTimeout) != sf::Socket::Status::Done)
    {
        error = "timed out waiting for card server response";
        return {};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::CardListResponse)
    {
        error = "unexpected card server response";
        return {};
    }
    if (!success)
    {
        error = message.empty() ? "card server rejected card list request" : message;
        return {};
    }
    if (count > card_data::MaxSerializedItems)
    {
        error = "card server returned too many cards";
        return {};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        card_data::Card card;
        if (!card_data::readCard(response, card))
        {
            error = "card server returned an invalid card payload";
            return {};
        }
        cards.push_back(std::move(card));
    }
    return cards;
}
}

std::vector<card_data::Card> load(const card_source_config::Config& config, std::string& error)
{
    if (config.usesCardServer())
    {
        return loadFromCardServer(config, error);
    }

    try
    {
        return card_database::loadCardsFromFile(*config.cardsDatabasePath);
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return {};
    }
}

} // namespace card_server_client
