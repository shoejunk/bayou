module;

#include <SFML/Network.hpp>
#include "tls_socket.hpp"

#include "client_config.hpp"
#include "../shared/conquest_data.hpp"
#include "../shared/conquest_event_data.hpp"
#include "../shared/socket_timeout.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

export module conquest_services;

import network;

namespace
{
using network::MessageType;

constexpr unsigned short GameCoordinatorPort = 55002;
constexpr auto AccountResponseTimeout = std::chrono::seconds(8);
constexpr auto EventWatchResponseTimeout = std::chrono::seconds(25);
constexpr auto BattleReadyTimeout = std::chrono::seconds(12);

bool beginAccountRequest(
    bayou::tls::Socket& socket,
    sf::Packet& request,
    MessageType type,
    const std::string& accessToken)
{
    if (!bayou::client::connectToEndpoint(socket, bayou::client::clientConfig().account))
    {
        return false;
    }
    request << static_cast<std::uint8_t>(type) << accessToken;
    return true;
}

bool exchange(
    bayou::tls::Socket& socket,
    sf::Packet& request,
    sf::Packet& response)
{
    if (socket.send(request) != sf::Socket::Status::Done ||
        socket_timeout::receivePacket(socket, response, AccountResponseTimeout) !=
            sf::Socket::Status::Done)
    {
        socket.disconnect();
        return false;
    }
    socket.disconnect();
    return true;
}

bool watchExchange(
    bayou::tls::Socket& socket,
    sf::Packet& request,
    sf::Packet& response)
{
    if (socket.send(request) != sf::Socket::Status::Done ||
        socket_timeout::receivePacket(socket, response, EventWatchResponseTimeout) !=
            sf::Socket::Status::Done)
    {
        socket.disconnect();
        return false;
    }
    socket.disconnect();
    return true;
}

bool readHeader(
    sf::Packet& response,
    MessageType expected,
    bool& success,
    std::string& message)
{
    std::uint8_t rawType = 0;
    response >> rawType >> success >> message;
    return response && static_cast<MessageType>(rawType) == expected;
}
}

export namespace bayou::client
{
struct ConquestCommandResult
{
    bool success = false;
    std::string message;

    ConquestCommandResult() = default;
    ConquestCommandResult(bool wasSuccessful, std::string resultMessage)
        : success(wasSuccessful), message(std::move(resultMessage))
    {
    }
};

struct ConquestLoadoutResult : ConquestCommandResult
{
    using ConquestCommandResult::ConquestCommandResult;
    std::vector<conquest_data::ConquestDeck> decks;
    conquest_data::ConquestArmy army;
};

struct ConquestDeckResult : ConquestCommandResult
{
    using ConquestCommandResult::ConquestCommandResult;
    conquest_data::ConquestDeck deck;
};

struct ConquestArmyResult : ConquestCommandResult
{
    using ConquestCommandResult::ConquestCommandResult;
    conquest_data::ConquestArmy army;
};

struct ConquestEventListResult : ConquestCommandResult
{
    using ConquestCommandResult::ConquestCommandResult;
    std::vector<conquest_data::EventSummary> events;
};

struct ConquestEventStateResult : ConquestCommandResult
{
    using ConquestCommandResult::ConquestCommandResult;
    conquest_data::EventState state;
};

struct ConquestBattleJoinResult : ConquestCommandResult
{
    using ConquestCommandResult::ConquestCommandResult;
    std::shared_ptr<bayou::tls::Socket> socket;
    std::uint64_t battleId = 0;
    int playerNumber = 0;
};

ConquestLoadoutResult fetchConquestLoadout(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestLoadoutRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }

    ConquestLoadoutResult result;
    if (!readHeader(response, MessageType::ConquestLoadoutResponse,
                    result.success, result.message) ||
        !conquest_data::readConquestDeckList(response, result.decks) ||
        !conquest_data::readConquestArmy(response, result.army))
    {
        return {false, "Invalid Conquest loadout response"};
    }
    return result;
}

ConquestDeckResult saveConquestDeck(
    const std::string& accessToken,
    const conquest_data::ConquestDeck& deck)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestDeckSaveRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    if (!conquest_data::writeConquestDeck(request, deck))
    {
        socket.disconnect();
        return {false, "Conquest deck is too large"};
    }
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }

    ConquestDeckResult result;
    if (!readHeader(response, MessageType::ConquestDeckSaveResponse,
                    result.success, result.message) ||
        !conquest_data::readConquestDeck(response, result.deck))
    {
        return {false, "Invalid Conquest deck response"};
    }
    return result;
}

ConquestCommandResult deleteConquestDeck(
    const std::string& accessToken,
    std::int64_t deckId,
    std::uint32_t revision)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestDeckDeleteRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << deckId << revision;
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::ConquestDeckDeleteResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest deck delete response"};
    }
    return result;
}

ConquestArmyResult saveConquestArmy(
    const std::string& accessToken,
    const conquest_data::ConquestArmy& army)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestArmySaveRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    if (!conquest_data::writeConquestArmy(request, army))
    {
        socket.disconnect();
        return {false, "Army can contain at most 10 decks"};
    }
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestArmyResult result;
    if (!readHeader(response, MessageType::ConquestArmySaveResponse,
                    result.success, result.message) ||
        !conquest_data::readConquestArmy(response, result.army))
    {
        return {false, "Invalid Conquest army response"};
    }
    return result;
}

ConquestEventListResult fetchConquestEvents(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestEventListRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }

    ConquestEventListResult result;
    if (!readHeader(response, MessageType::ConquestEventListResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest event list response"};
    }
    std::uint32_t count = 0;
    response >> count;
    if (!response || count > conquest_data::MaxConquestEvents)
    {
        return {false, "Invalid Conquest event list response"};
    }
    result.events.resize(count);
    for (conquest_data::EventSummary& event : result.events)
    {
        if (!conquest_data::readEventSummary(response, event))
        {
            return {false, "Invalid Conquest event list response"};
        }
    }
    return result;
}

ConquestEventStateResult fetchConquestEventState(
    const std::string& accessToken,
    std::uint64_t eventId)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestEventStateRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << eventId;
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestEventStateResult result;
    if (!readHeader(response, MessageType::ConquestEventStateResponse,
                    result.success, result.message) ||
        !conquest_data::readEventState(response, result.state))
    {
        return {false, "Invalid Conquest event response"};
    }
    return result;
}

ConquestCommandResult watchConquestEvent(
    const std::string& accessToken,
    std::uint64_t eventId,
    std::uint64_t stateFingerprint)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestEventWatchRequest, accessToken))
    {
        return {false, "Could not watch the Conquest event"};
    }
    request << eventId << stateFingerprint;
    sf::Packet response;
    if (!watchExchange(socket, request, response))
    {
        return {false, "Conquest event watch disconnected"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::ConquestEventWatchResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest event watch response"};
    }
    return result;
}

ConquestCommandResult joinConquestEvent(
    const std::string& accessToken,
    std::uint64_t eventId,
    const std::vector<conquest_data::StartingPlacement>& placements)
{
    if (placements.empty() || placements.size() > 2)
    {
        return {false, "Choose one or two starting placements"};
    }
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestEventJoinRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << eventId << static_cast<std::uint32_t>(placements.size());
    for (const conquest_data::StartingPlacement& placement : placements)
    {
        conquest_data::writeStartingPlacement(request, placement);
    }
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::ConquestEventJoinResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest join response"};
    }
    return result;
}

ConquestCommandResult submitConquestOrders(
    const std::string& accessToken,
    std::uint64_t eventId,
    const std::vector<conquest_data::MoveOrder>& orders)
{
    if (orders.size() > conquest_data::MaxConquestOrders)
    {
        return {false, "Too many Conquest move orders"};
    }
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestOrdersSubmitRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << eventId << static_cast<std::uint32_t>(orders.size());
    for (const conquest_data::MoveOrder& order : orders)
    {
        conquest_data::writeMoveOrder(request, order);
    }
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::ConquestOrdersSubmitResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest orders response"};
    }
    return result;
}

ConquestCommandResult reinforceConquestEvent(
    const std::string& accessToken,
    std::uint64_t eventId,
    std::uint64_t eventDeckId,
    int regionId)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::ConquestReinforceRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << eventId << eventDeckId << regionId;
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::ConquestReinforceResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest reinforcement response"};
    }
    return result;
}

ConquestCommandResult forceStartConquestEvent(
    const std::string& accessToken,
    std::uint64_t eventId)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::AdminConquestEventStartRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << eventId;
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::AdminConquestEventStartResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest start response"};
    }
    return result;
}

ConquestCommandResult createConquestEvent(
    const std::string& accessToken,
    const std::string& name,
    std::int64_t registrationSeconds,
    std::int64_t turnSeconds,
    std::int64_t reinforcementCooldownSeconds)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::AdminConquestEventCreateRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << name << registrationSeconds << turnSeconds
            << reinforcementCooldownSeconds;
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::AdminConquestEventCreateResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest creation response"};
    }
    return result;
}

ConquestCommandResult forceEndConquestEvent(
    const std::string& accessToken,
    std::uint64_t eventId)
{
    bayou::tls::Socket socket;
    sf::Packet request;
    if (!beginAccountRequest(
            socket, request, MessageType::AdminConquestEventEndRequest, accessToken))
    {
        return {false, "Could not reach the account server"};
    }
    request << eventId;
    sf::Packet response;
    if (!exchange(socket, request, response))
    {
        return {false, "No response from the account server"};
    }
    ConquestCommandResult result;
    if (!readHeader(response, MessageType::AdminConquestEventEndResponse,
                    result.success, result.message))
    {
        return {false, "Invalid Conquest end response"};
    }
    return result;
}

ConquestBattleJoinResult joinConquestBattle(
    const std::string& accessToken,
    std::uint64_t battleId)
{
    auto socket = std::make_shared<bayou::tls::Socket>();
    if (!connectToHostPort(
            *socket, clientConfig().gameServerHost, GameCoordinatorPort))
    {
        return {false, "Could not reach the game coordinator"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::JoinConquestBattle)
            << battleId << accessToken;
    if (socket->send(request) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "Could not join the Conquest battle"};
    }
    sf::Packet response;
    if (socket_timeout::receivePacket(*socket, response, BattleReadyTimeout) !=
        sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "No response from the game coordinator"};
    }

    std::uint8_t rawType = 0;
    ConquestBattleJoinResult result;
    response >> rawType >> result.success >> result.battleId
             >> result.playerNumber >> result.message;
    if (!response || static_cast<MessageType>(rawType) != MessageType::ConquestBattleReady ||
        result.battleId != battleId ||
        (result.success && result.playerNumber != 1 && result.playerNumber != 2))
    {
        socket->disconnect();
        return {false, "Invalid Conquest battle response"};
    }
    if (!result.success)
    {
        socket->disconnect();
        return result;
    }
    result.socket = std::move(socket);
    return result;
}

void leaveConquestBattle(bayou::tls::Socket& socket)
{
    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::LeaveConquestBattle);
    [[maybe_unused]] const sf::Socket::Status status = socket.send(request);
    socket.disconnect();
}
}
