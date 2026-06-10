#include <SFML/Network.hpp>
#include <fmt/core.h>

#include "../shared/card_data.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

import network;

using namespace network;

namespace
{
constexpr unsigned short CardServerPort = 55004;

struct CardListResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
};

struct CommandResult
{
    bool success = false;
    std::string message;
};

std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (first >= last)
    {
        return "";
    }

    return std::string(first, last);
}

std::vector<std::string> splitCommaSeparated(const std::string& line)
{
    std::vector<std::string> values;
    std::stringstream stream(line);
    std::string item;
    while (std::getline(stream, item, ','))
    {
        item = trim(item);
        if (!item.empty())
        {
            values.push_back(item);
        }
    }

    return values;
}

bool connectToServer(sf::TcpSocket& socket)
{
    return socket.connect(sf::IpAddress::LocalHost, CardServerPort) == sf::Socket::Status::Done;
}

CardListResult fetchCards()
{
    sf::TcpSocket socket;
    if (!connectToServer(socket))
    {
        return {false, "Failed to connect to card server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {false, "Failed to send card list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<MessageType>(responseType) != MessageType::CardListResponse)
    {
        return {false, "Unexpected card list response"};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        card_data::Card card;
        if (!card_data::readCard(response, card))
        {
            return {false, "Invalid card list payload"};
        }
        cards.push_back(card);
    }

    return {success, message, cards};
}

CommandResult saveCard(const card_data::Card& card)
{
    sf::TcpSocket socket;
    if (!connectToServer(socket))
    {
        return {false, "Failed to connect to card server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::CardUpsertRequest);
    card_data::writeCard(request, card);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {false, "Failed to send card save request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response || static_cast<MessageType>(responseType) != MessageType::CardUpsertResponse)
    {
        return {false, "Unexpected card save response"};
    }

    return {success, message};
}

void printCard(const card_data::Card& card)
{
    fmt::println("Title: {}", card.title);
    fmt::println("  Type: {}", card_data::toString(card.type));
    fmt::println("  Image: {}", card.imagePath);

    fmt::print("  Keywords:");
    for (const std::string& keyword : card.keywords)
    {
        fmt::print(" {}", keyword);
    }
    fmt::println("");

    for (const card_data::KeyIntPair& item : card.integerValues)
    {
        fmt::println("  Int {} = {}", item.key, item.value);
    }

    for (const card_data::KeyStringPair& item : card.stringValues)
    {
        fmt::println("  Text {} = {}", item.key, item.value);
    }

    for (const card_data::KeyStringList& item : card.stringLists)
    {
        fmt::print("  List {} =", item.key);
        for (const std::string& value : item.values)
        {
            fmt::print(" {}", value);
        }
        fmt::println("");
    }
}

void printCards(const std::vector<card_data::Card>& cards)
{
    if (cards.empty())
    {
        fmt::println("No cards found.");
        return;
    }

    for (std::size_t i = 0; i < cards.size(); ++i)
    {
        fmt::println("{}. {}", i + 1, cards[i].title);
        printCard(cards[i]);
    }
}

std::string readLine(const std::string& prompt, const std::string& currentValue = "")
{
    if (currentValue.empty())
    {
        fmt::print("{}", prompt);
    }
    else
    {
        fmt::print("{} [{}]: ", prompt, currentValue);
    }

    std::string line;
    std::getline(std::cin, line);
    if (line.empty())
    {
        return currentValue;
    }

    return line;
}

int readInt(const std::string& prompt, int currentValue = 0)
{
    while (true)
    {
        const std::string line = readLine(prompt, std::to_string(currentValue));
        try
        {
            return std::stoi(line);
        }
        catch (const std::exception&)
        {
            fmt::println("Enter a whole number.");
        }
    }
}

card_data::CardType readCardType(card_data::CardType currentType)
{
    while (true)
    {
        fmt::println("Card type:");
        fmt::println("  1. Unit");
        fmt::println("  2. Spell");
        fmt::println("  3. Artifact");
        fmt::println("  4. Reaction");
        const std::string line = readLine("Choose type", std::to_string(static_cast<int>(currentType) + 1));
        const std::optional<card_data::CardType> type = card_data::cardTypeFromIndex(std::atoi(line.c_str()));
        if (type)
        {
            return *type;
        }
        fmt::println("Choose a type from 1 to 4.");
    }
}

std::vector<std::string> readKeywords(const std::vector<std::string>& current)
{
    fmt::print("Keywords comma separated");
    if (!current.empty())
    {
        fmt::print(" [");
        for (std::size_t i = 0; i < current.size(); ++i)
        {
            fmt::print("{}{}", i == 0 ? "" : ",", current[i]);
        }
        fmt::print("]");
    }
    fmt::print(": ");

    std::string line;
    std::getline(std::cin, line);
    if (line.empty())
    {
        return current;
    }
    return splitCommaSeparated(line);
}

std::vector<card_data::KeyIntPair> readIntegerPairs(const std::vector<card_data::KeyIntPair>& current)
{
    std::vector<card_data::KeyIntPair> values;
    fmt::println("Integer fields. Leave key empty when done.");
    for (const card_data::KeyIntPair& item : current)
    {
        fmt::println("  Existing: {} = {}", item.key, item.value);
    }

    while (true)
    {
        const std::string key = trim(readLine("  Key: "));
        if (key.empty())
        {
            break;
        }
        values.push_back({key, readInt("  Value", 0)});
    }

    return values.empty() ? current : values;
}

std::vector<card_data::KeyStringPair> readStringPairs(const std::vector<card_data::KeyStringPair>& current)
{
    std::vector<card_data::KeyStringPair> values;
    fmt::println("String fields. Leave key empty when done.");
    for (const card_data::KeyStringPair& item : current)
    {
        fmt::println("  Existing: {} = {}", item.key, item.value);
    }

    while (true)
    {
        const std::string key = trim(readLine("  Key: "));
        if (key.empty())
        {
            break;
        }
        values.push_back({key, readLine("  Value: ")});
    }

    return values.empty() ? current : values;
}

std::vector<card_data::KeyStringList> readStringLists(const std::vector<card_data::KeyStringList>& current)
{
    std::vector<card_data::KeyStringList> values;
    fmt::println("String lists. Leave key empty when done. Enter list values comma separated.");
    for (const card_data::KeyStringList& item : current)
    {
        fmt::print("  Existing: {} =", item.key);
        for (const std::string& value : item.values)
        {
            fmt::print(" {}", value);
        }
        fmt::println("");
    }

    while (true)
    {
        const std::string key = trim(readLine("  Key: "));
        if (key.empty())
        {
            break;
        }
        values.push_back({key, splitCommaSeparated(readLine("  Values: "))});
    }

    return values.empty() ? current : values;
}

card_data::Card readCardFromConsole(const card_data::Card& current = {})
{
    card_data::Card card = current;
    while (card.title.empty())
    {
        card.title = trim(readLine("Title: ", current.title));
        if (card.title.empty())
        {
            fmt::println("Title is required.");
        }
    }

    card.type = readCardType(current.type);
    card.imagePath = readLine("Image path: ", current.imagePath);
    card.keywords = readKeywords(current.keywords);
    card.integerValues = readIntegerPairs(current.integerValues);
    card.stringValues = readStringPairs(current.stringValues);
    card.stringLists = readStringLists(current.stringLists);
    return card;
}

card_data::Card makeSampleCard(const std::string& title, int revision)
{
    card_data::Card card;
    card.title = title;
    card.type = revision == 1 ? card_data::CardType::Unit : card_data::CardType::Spell;
    card.imagePath = fmt::format("assets/cards/{}_rev_{}.png", title, revision);
    card.keywords = {"starter", "sample", fmt::format("revision{}", revision)};
    card.integerValues = {{"cost", revision}, {"power", revision + 1}};
    card.stringValues = {{"faction", "bayou"}, {"rules", fmt::format("Sample rules revision {}", revision)}};
    card.stringLists = {{"tags", {"developer", "test"}}, {"slots", {"hand", "board"}}};
    return card;
}

int handleListCommand()
{
    const CardListResult result = fetchCards();
    if (!result.success)
    {
        fmt::println("{}", result.message);
        return 1;
    }

    printCards(result.cards);
    return 0;
}

int handleSaveCommand(const card_data::Card& card)
{
    const CommandResult result = saveCard(card);
    fmt::println("{}", result.message);
    return result.success ? 0 : 1;
}

std::optional<card_data::Card> chooseCardForEdit()
{
    const CardListResult result = fetchCards();
    if (!result.success)
    {
        fmt::println("{}", result.message);
        return std::nullopt;
    }

    if (result.cards.empty())
    {
        fmt::println("No cards exist yet.");
        return std::nullopt;
    }

    for (std::size_t i = 0; i < result.cards.size(); ++i)
    {
        fmt::println("{}. {}", i + 1, result.cards[i].title);
    }

    const int choice = readInt("Choose card", 1);
    if (choice < 1 || static_cast<std::size_t>(choice) > result.cards.size())
    {
        fmt::println("Invalid card selection.");
        return std::nullopt;
    }

    return result.cards[static_cast<std::size_t>(choice - 1)];
}

int runInteractive()
{
    while (true)
    {
        fmt::println("");
        fmt::println("Card Editor");
        fmt::println("1. View all cards");
        fmt::println("2. Create new card");
        fmt::println("3. Edit existing card");
        fmt::println("4. Quit");

        const int choice = readInt("Choose option", 1);
        if (choice == 1)
        {
            (void)handleListCommand();
        }
        else if (choice == 2)
        {
            (void)handleSaveCommand(readCardFromConsole());
        }
        else if (choice == 3)
        {
            std::optional<card_data::Card> card = chooseCardForEdit();
            if (card)
            {
                (void)handleSaveCommand(readCardFromConsole(*card));
            }
        }
        else if (choice == 4)
        {
            return 0;
        }
        else
        {
            fmt::println("Choose an option from 1 to 4.");
        }
    }
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--list")
    {
        return handleListCommand();
    }

    if (argc == 3 && std::string(argv[1]) == "--create-sample")
    {
        return handleSaveCommand(makeSampleCard(argv[2], 1));
    }

    if (argc == 3 && std::string(argv[1]) == "--edit-sample")
    {
        return handleSaveCommand(makeSampleCard(argv[2], 2));
    }

    fmt::println("Connects to cardserver.exe on localhost:{}.", CardServerPort);
    return runInteractive();
}
