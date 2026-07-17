module;

#include <SFML/Network.hpp>
#include "tls_socket.hpp"

#include "client_config.hpp"
#include "deck_collection.hpp"

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

export module client_services;

import network;

export namespace bayou::client
{
struct ServerResult
{
    bool success = false;
    std::string message;
    std::shared_ptr<bayou::tls::Socket> gameSocket;
    std::string username;
    std::string accessToken;
    std::string rememberToken;
    bool rejectStoredCredential = false;
    bool cancelled = false;
};

struct MatchmakingCancelState
{
    std::atomic<bool> requested{false};
    std::atomic<bool> sent{false};
    std::atomic<bool> aiRequested{false};
    std::atomic<bool> aiSent{false};
};

struct CardListResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
};

struct DeckListResult
{
    bool success = false;
    std::string message;
    std::vector<deck_data::Deck> decks;
};

struct AccountStateResult
{
    bool success = false;
    std::string message;
    int coins = 0;
    int rating = 0;
    bool isAdmin = false;
    std::vector<account_data::CollectionCard> collection;
};

struct DeckEditorLoadResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
    std::vector<deck_data::Deck> decks;
    int coins = 0;
    std::vector<account_data::CollectionCard> collection;
};

struct DeckCommandResult
{
    bool success = false;
    std::string message;
    std::string originalName;
    deck_data::Deck deck;
};

struct AccountCommandResult
{
    bool success = false;
    std::string message;
    int coins = 0;
    std::string cardTitle;
};

struct ShopLoadResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
    int coins = 0;
    std::vector<account_data::CollectionCard> collection;
};

struct StarterDeckLoadResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
    deck_data::Deck deck;
};

struct AdminUsersLoadResult
{
    bool success = false;
    std::string message;
    std::uint32_t totalCount = 0;
    std::uint32_t page = 0;
    std::uint32_t pageSize = 0;
    std::vector<network::AdminUserSummary> users;
};

struct AdminUserPrivilegeResult
{
    bool success = false;
    std::string message;
    bool targetIsAdmin = false;
};

struct AdminUserGoldResult
{
    bool success = false;
    std::string message;
    std::string targetUsername;
    int targetGold = 0;
};

struct AdminUserDeleteResult
{
    bool success = false;
    std::string message;
    std::string targetUsername;
};

struct AdminUserCardResult
{
    bool success = false;
    std::string message;
    std::string targetUsername;
    std::string cardTitle;
};


void sendDisconnect(bayou::tls::Socket& socket)
{
    sf::Packet disconnectPacket;
    disconnectPacket << static_cast<std::uint8_t>(network::MessageType::Disconnect);
    [[maybe_unused]] auto disconnectResult = socket.send(disconnectPacket);
    socket.disconnect();
}

void sendSubmitDeck(bayou::tls::Socket& socket, const std::vector<card_data::Card>& cards)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(network::MessageType::SubmitDeck);
    packet << static_cast<std::uint32_t>(cards.size());
    for (const card_data::Card& card : cards)
    {
        card_data::writeCard(packet, card);
    }
    [[maybe_unused]] auto result = socket.send(packet);
}

ServerResult sendAccountRequest(
    network::MessageType requestType,
    network::MessageType expectedResponseType,
    const std::string& username,
    const std::string& password,
    bool rememberMe = false)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(requestType);
    packet << username;
    packet << password;
    if (requestType == network::MessageType::Login)
    {
        packet << rememberMe;
    }

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send to account server"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from account server"};
    }

    uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;

    if (static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        socket.disconnect();
        return {false, "Unexpected account server response"};
    }

    std::string authenticatedUsername;
    std::string accessToken;
    std::string rememberToken;
    if (requestType == network::MessageType::Login ||
        (requestType == network::MessageType::CreateAccount && success))
    {
        response >> authenticatedUsername >> accessToken >> rememberToken;
        if (!response)
        {
            socket.disconnect();
            return {false, "Invalid account server response"};
        }
    }

    sendDisconnect(socket);
    ServerResult result;
    result.success = success;
    result.message = std::move(message);
    result.username = std::move(authenticatedUsername);
    result.accessToken = std::move(accessToken);
    result.rememberToken = std::move(rememberToken);
    return result;
}

ServerResult sendRememberLogin(const std::string& token)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Could not restore saved login; account server is unavailable"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::RememberLogin) << token;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Could not restore saved login"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Could not restore saved login"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::string username;
    std::string accessToken;
    std::string replacementToken;
    response >> responseType >> success >> message >> username >> accessToken >> replacementToken;
    socket.disconnect();
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::RememberLoginResponse)
    {
        return {false, "Unexpected account server response"};
    }

    ServerResult result;
    result.success = success;
    result.message = std::move(message);
    result.username = std::move(username);
    result.accessToken = std::move(accessToken);
    result.rememberToken = std::move(replacementToken);
    result.rejectStoredCredential = !success;
    return result;
}

void revokeLoginTokens(const std::string& rememberToken, const std::string& accessToken)
{
    if (rememberToken.empty() && accessToken.empty())
    {
        return;
    }

    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::RevokeRememberToken)
            << rememberToken << accessToken;
    if (socket.send(request) == sf::Socket::Status::Done)
    {
        sf::Packet response;
        [[maybe_unused]] auto status = socket.receive(response);
    }
    socket.disconnect();
}

CardListResult fetchCards()
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().card))
    {
        return {false, "Failed to connect to card server " + endpointText(clientConfig().card)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send card list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::CardListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected card list response"};
    }

    if (!success)
    {
        socket.disconnect();
        return {false, message};
    }

    std::uint32_t count = 0;
    bool legacyFormat = false;
    bool actionIncludesNextState = false;
    if (!card_data::readCardListHeader(
            response, count, legacyFormat, &actionIncludesNextState))
    {
        socket.disconnect();
        return {false, "Unsupported card list payload"};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        card_data::Card card;
        if (!card_data::readListedCard(
                response, card, legacyFormat, actionIncludesNextState))
        {
            socket.disconnect();
            return {false, "Invalid card list payload"};
        }
        cards.push_back(card);
    }

    socket.disconnect();
    return {success, message, cards};
}

DeckListResult fetchDecks(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckListRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No deck list response from account server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::DeckListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected deck list response"};
    }

    std::vector<deck_data::Deck> decks;
    decks.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        deck_data::Deck deck;
        if (!deck_data::readDeck(response, deck))
        {
            socket.disconnect();
            return {false, "Invalid deck list payload"};
        }
        decks.push_back(deck);
    }

    sendDisconnect(socket);
    return {success, message, decks};
}

AccountStateResult fetchAccountState(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AccountStateRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send account state request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No account state response from account server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    account_data::AccountState accountState;
    response >> responseType >> success >> message;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::AccountStateResponse)
    {
        socket.disconnect();
        return {false, "Unexpected account state response"};
    }

    if (!account_data::readAccountState(response, accountState))
    {
        socket.disconnect();
        return {false, "Invalid account state payload"};
    }

    sendDisconnect(socket);
    return {
        success,
        message,
        accountState.coins,
        accountState.rating,
        accountState.isAdmin,
        std::move(accountState.collection)};
}

DeckCommandResult readDeckCommandResponse(
    bayou::tls::Socket& socket,
    network::MessageType expectedResponseType,
    const std::string& fallbackMessage,
    const std::string& originalName,
    const deck_data::Deck& deck)
{
    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, fallbackMessage, originalName, deck};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response || static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        return {false, "Unexpected deck command response", originalName, deck};
    }

    return {success, message, originalName, deck};
}

DeckCommandResult saveDeckToAccount(
    const std::string& accessToken,
    const std::string& originalName,
    const deck_data::Deck& deck)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), originalName, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckSaveRequest);
    request << accessToken << originalName;
    deck_data::writeDeck(request, deck);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck save request", originalName, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::DeckSaveResponse,
        "No deck save response from account server",
        originalName,
        deck);
    sendDisconnect(socket);
    return result;
}

DeckCommandResult deleteDeckFromAccount(const std::string& accessToken, const std::string& deckName)
{
    deck_data::Deck deck;
    deck.name = deckName;

    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), deckName, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckDeleteRequest);
    request << accessToken << deckName;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck delete request", deckName, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::DeckDeleteResponse,
        "No deck delete response from account server",
        deckName,
        deck);
    sendDisconnect(socket);
    return result;
}

AccountCommandResult sendCoinCommand(
    network::MessageType requestType,
    network::MessageType expectedResponseType,
    const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(requestType);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send account command"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No account command response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    int coins = 0;
    response >> responseType >> success >> message >> coins;
    if (!response || static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        socket.disconnect();
        return {false, "Unexpected account command response"};
    }

    sendDisconnect(socket);
    return {success, message, coins};
}

AccountCommandResult purchaseRandomCard(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::ShopPurchaseRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send shop purchase request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No shop purchase response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    int coins = 0;
    std::string cardTitle;
    response >> responseType >> success >> message >> coins >> cardTitle;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::ShopPurchaseResponse)
    {
        socket.disconnect();
        return {false, "Unexpected shop purchase response"};
    }

    sendDisconnect(socket);
    return {success, message, coins, cardTitle};
}

AccountCommandResult changePassword(
    const std::string& accessToken,
    const std::string& currentPassword,
    const std::string& newPassword)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::ChangePasswordRequest)
            << accessToken << currentPassword << newPassword;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send password change request"};
    }

    socket.setBlocking(false);
    sf::Packet response;
    const auto responseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    sf::Socket::Status receiveStatus = sf::Socket::Status::NotReady;
    while (std::chrono::steady_clock::now() < responseDeadline)
    {
        receiveStatus = socket.receive(response);
        if (receiveStatus != sf::Socket::Status::NotReady)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (receiveStatus != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {
            false,
            receiveStatus == sf::Socket::Status::NotReady
                ? "Password change timed out; restart the account server with the latest build"
                : "No password change response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::ChangePasswordResponse)
    {
        socket.disconnect();
        return {false, "Unexpected password change response"};
    }

    sendDisconnect(socket);
    return {success, message};
}

AdminUsersLoadResult loadAdminUsers(
    const std::string& accessToken,
    const std::string& search,
    std::uint32_t page,
    std::uint32_t pageSize)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserListRequest);
    request << accessToken << search << page << pageSize;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send admin user list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No admin user list response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t totalCount = 0;
    std::uint32_t responsePage = 0;
    std::uint32_t responsePageSize = 0;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> totalCount >> responsePage >> responsePageSize >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected admin user list response"};
    }

    std::vector<network::AdminUserSummary> users;
    users.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        network::AdminUserSummary user;
        response >> user.username >> user.isAdmin >> user.gold;
        if (!response)
        {
            socket.disconnect();
            return {false, "Invalid admin user list payload"};
        }
        users.push_back(std::move(user));
    }

    sendDisconnect(socket);
    return {success, message, totalCount, responsePage, responsePageSize, std::move(users)};
}

AdminUserPrivilegeResult updateAdminUserPrivilege(
    const std::string& accessToken,
    const std::string& targetUsername,
    bool makeAdmin)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserPrivilegeRequest);
    request << accessToken << targetUsername << makeAdmin;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send admin privilege request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No admin privilege response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    bool targetIsAdmin = false;
    response >> responseType >> success >> message >> targetIsAdmin;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserPrivilegeResponse)
    {
        socket.disconnect();
        return {false, "Unexpected admin privilege response"};
    }

    sendDisconnect(socket);
    return {success, message, targetIsAdmin};
}

AdminUserGoldResult updateAdminUserGold(
    const std::string& accessToken,
    const std::string& targetUsername,
    int amount)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), targetUsername};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserGoldRequest);
    request << accessToken << targetUsername << amount;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send admin gold request", targetUsername};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No admin gold response", targetUsername};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    int targetGold = 0;
    response >> responseType >> success >> message >> targetGold;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserGoldResponse)
    {
        socket.disconnect();
        return {false, "Unexpected admin gold response", targetUsername};
    }

    sendDisconnect(socket);
    return {success, message, targetUsername, targetGold};
}

AdminUserDeleteResult deleteAdminUser(
    const std::string& accessToken,
    const std::string& targetUsername)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), targetUsername};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserDeleteRequest);
    request << accessToken << targetUsername;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send delete user request", targetUsername};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No delete user response", targetUsername};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserDeleteResponse)
    {
        socket.disconnect();
        return {false, "Unexpected delete user response", targetUsername};
    }

    sendDisconnect(socket);
    return {success, message, targetUsername};
}

AdminUserCardResult addCardToAdminUser(
    const std::string& accessToken,
    const std::string& targetUsername,
    const std::string& cardTitle)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {
            false,
            "Failed to connect to account server " + endpointText(clientConfig().account),
            targetUsername,
            cardTitle};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserCardRequest);
    request << accessToken << targetUsername << cardTitle;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send add card request", targetUsername, cardTitle};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No add card response", targetUsername, cardTitle};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserCardResponse)
    {
        socket.disconnect();
        return {false, "Unexpected add card response", targetUsername, cardTitle};
    }

    sendDisconnect(socket);
    return {success, message, targetUsername, cardTitle};
}

DeckCommandResult saveStarterDeckToAccount(const std::string& accessToken, const deck_data::Deck& deck)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), deck.name, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminStarterDeckSaveRequest);
    request << accessToken;
    deck_data::writeDeck(request, deck);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send starter deck save request", deck.name, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::AdminStarterDeckSaveResponse,
        "No starter deck save response from account server",
        deck.name,
        deck);
    sendDisconnect(socket);
    return result;
}

StarterDeckLoadResult fetchAdminStarterDeck(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminStarterDeckRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send starter deck request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No starter deck response from account server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    deck_data::Deck deck;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminStarterDeckResponse ||
        !deck_data::readDeck(response, deck))
    {
        socket.disconnect();
        return {false, "Unexpected starter deck response"};
    }

    sendDisconnect(socket);
    return {success, message, {}, std::move(deck)};
}

StarterDeckLoadResult loadStarterDeckEditorData(const std::string& accessToken)
{
    CardListResult cardResult = fetchCards();
    if (!cardResult.success)
    {
        return {false, cardResult.message};
    }

    StarterDeckLoadResult deckResult = fetchAdminStarterDeck(accessToken);
    if (!deckResult.success)
    {
        return {false, deckResult.message};
    }

    deckResult.cards = std::move(cardResult.cards);
    deckResult.message =
        "Loaded starter deck and " + std::to_string(deckResult.cards.size()) + " cards";
    return deckResult;
}

DeckEditorLoadResult loadDeckEditorData(const std::string& accessToken)
{
    CardListResult cardResult = fetchCards();
    if (!cardResult.success)
    {
        return {false, cardResult.message};
    }

    AccountStateResult accountResult = fetchAccountState(accessToken);
    if (!accountResult.success)
    {
        return {false, accountResult.message};
    }

    DeckListResult deckResult = fetchDecks(accessToken);
    if (!deckResult.success)
    {
        return {
            false,
            deckResult.message,
            ownedCardsFromCollection(cardResult.cards, accountResult.collection),
            {},
            accountResult.coins,
            std::move(accountResult.collection)};
    }

    std::vector<card_data::Card> ownedCards = ownedCardsFromCollection(cardResult.cards, accountResult.collection);
    const std::string message =
        "Loaded " + std::to_string(ownedCards.size()) + " owned cards and " +
        std::to_string(deckResult.decks.size()) + " decks";
    return {
        true,
        message,
        std::move(ownedCards),
        std::move(deckResult.decks),
        accountResult.coins,
        std::move(accountResult.collection)};
}

ShopLoadResult loadShopData(const std::string& accessToken)
{
    CardListResult cardResult = fetchCards();
    if (!cardResult.success)
    {
        return {false, cardResult.message};
    }

    AccountStateResult accountResult = fetchAccountState(accessToken);
    if (!accountResult.success)
    {
        return {false, accountResult.message, std::move(cardResult.cards)};
    }

    return {
        true,
        "Shop loaded",
        std::move(cardResult.cards),
        accountResult.coins,
        std::move(accountResult.collection)};
}

ServerResult joinGameServer(
    int matchId,
    int playerNumber,
    unsigned short gamePort,
    const std::string& accessToken)
{
    if (gamePort == 0)
    {
        return {false, "Game server did not assign a game"};
    }

    auto socket = std::make_shared<bayou::tls::Socket>();
    bool connected = false;
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        if (connectToHostPort(*socket, clientConfig().gameServerHost, gamePort))
        {
            connected = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!connected)
    {
        socket->disconnect();
        return {false, "Failed to connect to game server"};
    }

    sf::Packet joinRequest;
    joinRequest << static_cast<uint8_t>(network::MessageType::JoinGame);
    joinRequest << matchId;
    joinRequest << playerNumber;
    joinRequest << accessToken;

    if (socket->send(joinRequest) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "Failed to join game"};
    }

    sf::Packet response;
    if (socket->receive(response) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "No game server response"};
    }

    uint8_t responseType = 0;
    int responseMatchId = 0;
    int responsePlayerNumber = 0;
    std::string message;
    response >> responseType >> responseMatchId >> responsePlayerNumber >> message;
    if (static_cast<network::MessageType>(responseType) != network::MessageType::GameReady ||
        responseMatchId != matchId ||
        responsePlayerNumber != playerNumber)
    {
        socket->disconnect();
        return {false, "Unexpected game server response"};
    }

    return {true, message, socket};
}

ServerResult joinMatchmaking(
    const std::string& accessToken,
    std::shared_ptr<MatchmakingCancelState> cancelState)
{
    bayou::tls::Socket socket;
    if (!connectToEndpoint(socket, clientConfig().matchmaking))
    {
        return {false, "Failed to connect to matchmaking " + endpointText(clientConfig().matchmaking)};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(network::MessageType::JoinMatchmaking);
    packet << accessToken;

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to join matchmaking"};
    }

    socket.setBlocking(false);
    sf::Packet cancelPacket;
    cancelPacket << static_cast<uint8_t>(network::MessageType::CancelMatchmaking);
    sf::Packet aiPacket;
    aiPacket << static_cast<uint8_t>(network::MessageType::PlayAiMatchmaking);

    while (true)
    {
        if (cancelState &&
            cancelState->aiRequested.load() &&
            !cancelState->aiSent.load())
        {
            const sf::Socket::Status aiStatus = socket.send(aiPacket);
            if (aiStatus == sf::Socket::Status::Done)
            {
                cancelState->aiSent.store(true);
            }
            else if (aiStatus != sf::Socket::Status::NotReady &&
                     aiStatus != sf::Socket::Status::Partial)
            {
                socket.disconnect();
                return {false, "Failed to request AI match"};
            }
        }

        if (cancelState &&
            cancelState->requested.load() &&
            !cancelState->sent.load())
        {
            const sf::Socket::Status cancelStatus = socket.send(cancelPacket);
            if (cancelStatus == sf::Socket::Status::Done)
            {
                cancelState->sent.store(true);
            }
            else if (cancelStatus != sf::Socket::Status::NotReady &&
                     cancelStatus != sf::Socket::Status::Partial)
            {
                socket.disconnect();
                return {false, "Failed to cancel matchmaking"};
            }
        }

        sf::Packet response;
        const sf::Socket::Status receiveStatus = socket.receive(response);
        if (receiveStatus == sf::Socket::Status::NotReady)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }

        if (receiveStatus != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Disconnected from matchmaking"};
        }

        uint8_t responseType = 0;
        response >> responseType;
        const auto messageType = static_cast<network::MessageType>(responseType);
        if (messageType == network::MessageType::CancelMatchmakingResponse)
        {
            bool cancelled = false;
            std::string message;
            response >> cancelled >> message;
            socket.disconnect();
            if (!response || !cancelled)
            {
                return {false, message.empty() ? "Matchmaking cancel was rejected" : message};
            }

            ServerResult result;
            result.message = message.empty() ? "Matchmaking cancelled." : message;
            result.cancelled = true;
            return result;
        }

        if (messageType != network::MessageType::MatchFound)
        {
            socket.disconnect();
            return {false, "Unexpected matchmaking response"};
        }

        int matchId = 0;
        int playerNumber = 0;
        unsigned short gamePort = 0;
        response >> matchId >> playerNumber >> gamePort;
        socket.disconnect();
        return joinGameServer(matchId, playerNumber, gamePort, accessToken);
    }
}

}
