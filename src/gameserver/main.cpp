#include <SFML/Network.hpp>
#include "tls_socket.hpp"
#include <fmt/core.h>

#include "ai_deck.hpp"
#include "ai_player.hpp"
#include "game_engine.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/card_server_client.hpp"
#include "../shared/card_source_config.hpp"
#include "../shared/game_data.hpp"

#include "../shared/listener_retry.hpp"
#include "../shared/network.hpp"
#include "../shared/socket_timeout.hpp"

using namespace network;
using namespace game_data;

namespace
{
constexpr unsigned short GameServerPort = 55002;
constexpr unsigned short FirstGamePort = 56000;
constexpr unsigned short LastGamePort = 59999;
constexpr unsigned short AccountServerPort = 55000;
constexpr auto InitialRequestTimeout = std::chrono::seconds(2);
// A deck is 20 non-hero cards plus up to 4 heroes; anything past this bound is
// rejected before allocating, so a crafted count cannot exhaust memory.
constexpr std::uint32_t MaxDeckPacketCards = 64;
constexpr int AiPlayerNumber = 2;
constexpr const char* AiOpponentName = "Bayou Automaton";

struct JoinedPlayer
{
    std::unique_ptr<bayou::tls::Socket> socket;
    int playerNumber = 0;
    std::string username;
    std::string accessToken;
    int rating = 0;
};

struct MatchResult
{
    bool success = false;
    std::string message;
    std::array<int, 2> ratings{};
    std::array<int, 2> ratingChanges{};
    int winnerCoins = 0;
    bool selfMatch = false;
};

std::string executablePath()
{
#ifdef _WIN32
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::string(buffer.data(), length);
#else
    std::array<char, PATH_MAX> buffer{};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0)
    {
        return std::string(buffer.data(), static_cast<std::size_t>(length));
    }
    return "gameserver";
#endif
}

bool loadRankedPlayer(const std::string& accessToken, std::string& username, int& rating)
{
    bayou::tls::Socket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return false;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::RankedPlayerRequest) << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return false;
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return false;
    }

    std::uint8_t type = 0;
    bool success = false;
    std::string message;
    response >> type >> success >> message >> username >> rating;
    return response &&
        static_cast<MessageType>(type) == MessageType::RankedPlayerResponse &&
        success;
}

// Fetches the authoritative card collection for the account behind accessToken
// from the account server; a submitted deck may only use cards it contains.
bool loadPlayerCollection(
    const std::string& accessToken,
    std::vector<account_data::CollectionCard>& collection)
{
    bayou::tls::Socket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return false;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::AccountStateRequest) << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return false;
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return false;
    }

    std::uint8_t type = 0;
    bool success = false;
    std::string message;
    account_data::AccountState state;
    response >> type >> success >> message;
    if (!response ||
        static_cast<MessageType>(type) != MessageType::AccountStateResponse ||
        !success ||
        !account_data::readAccountState(response, state))
    {
        return false;
    }

    collection = std::move(state.collection);
    return true;
}

MatchResult submitRankedResult(int matchId, const std::string& resultToken, int winner)
{
    bayou::tls::Socket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return {};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::SubmitRankedResult)
            << matchId << resultToken << winner;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {};
    }

    std::uint8_t type = 0;
    MatchResult result;
    response >> type >> result.success >> result.message
             >> result.ratings[0] >> result.ratings[1]
             >> result.ratingChanges[0] >> result.ratingChanges[1]
             >> result.winnerCoins >> result.selfMatch;
    if (!response ||
        static_cast<MessageType>(type) != MessageType::SubmitRankedResultResponse)
    {
        return {};
    }

    if (result.success)
    {
        fmt::println(
            "Match {} ratings updated to {} ({:+}) and {} ({:+})",
            matchId,
            result.ratings[0],
            result.ratingChanges[0],
            result.ratings[1],
            result.ratingChanges[1]);
    }
    else
    {
        fmt::println("Match {} reward update failed: {}", matchId, result.message);
    }
    return result;
}

struct AiMatchResult
{
    bool success = false;
    std::string message;
    int coinsAwarded = 0;
};

AiMatchResult submitAiResult(int matchId, const std::string& accessToken, bool humanWon)
{
    if (accessToken.empty())
    {
        return {};
    }

    bayou::tls::Socket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return {};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::SubmitAiResult) << accessToken << humanWon;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {};
    }

    std::uint8_t type = 0;
    AiMatchResult result;
    response >> type >> result.success >> result.message >> result.coinsAwarded;
    if (!response || static_cast<MessageType>(type) != MessageType::SubmitAiResultResponse)
    {
        return {};
    }

    if (!result.success)
    {
        fmt::println("AI match {} reward update failed: {}", matchId, result.message);
    }
    return result;
}

bool spawnGameProcess(
    int matchId,
    unsigned short port,
    const std::string& playerOne,
    const std::string& playerTwo,
    const std::string& resultToken,
    const std::string& aiAccessToken,
    bool aiOpponent,
    const std::filesystem::path& configPath)
{
#ifdef _WIN32
    std::string command = aiOpponent
        ? fmt::format(
            "\"{}\" --config \"{}\" --game-ai {} {} {} {}",
            executablePath(),
            configPath.string(),
            matchId,
            port,
            playerOne,
            aiAccessToken)
        : fmt::format(
            "\"{}\" --config \"{}\" --game {} {} {} {} {}",
            executablePath(),
            configPath.string(),
            matchId,
            port,
            playerOne,
            playerTwo,
            resultToken);
    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::vector<char> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back('\0');

    if (!CreateProcessA(
            nullptr,
            commandBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo))
    {
        fmt::println("Failed to start game process for match {}", matchId);
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
#else
    const std::string command = aiOpponent
        ? fmt::format(
            "{} --config {} --game-ai {} {} {} {} &",
            executablePath(),
            configPath.string(),
            matchId,
            port,
            playerOne,
            aiAccessToken)
        : fmt::format(
            "{} --config {} --game {} {} {} {} {} &",
            executablePath(),
            configPath.string(),
            matchId,
            port,
            playerOne,
            playerTwo,
            resultToken);
    return std::system(command.c_str()) == 0;
#endif
}

}

// ---------------------------------------------------------------------------
// Authoritative tactical-game engine.
// ---------------------------------------------------------------------------
class GameServerCoordinator
{
public:
    GameServerCoordinator(unsigned short port, std::filesystem::path configPath)
        : configPath(std::move(configPath)),
          listener(std::make_unique<bayou::tls::Listener>())
    {
        if (!listener_retry::listenWithRetry(*listener, port))
        {
            return;
        }

        listening = true;
        fmt::println("Game server coordinator listening on port {}", port);
    }

    bool isListening() const
    {
        return listening;
    }

    void run()
    {
        if (!listening)
        {
            return;
        }

        running = true;
        while (running)
        {
            auto client = std::make_unique<bayou::tls::Socket>();
            if (listener->accept(*client) == sf::Socket::Status::Done)
            {
                handleClient(*client);
            }
        }
    }

private:
    std::filesystem::path configPath;
    std::unique_ptr<bayou::tls::Listener> listener;
    std::atomic<bool> running{false};
    bool listening = false;
    unsigned short nextGamePort = FirstGamePort;

    void handleClient(bayou::tls::Socket& client)
    {
        sf::Packet packet;
        if (socket_timeout::receivePacket(client, packet, InitialRequestTimeout) != sf::Socket::Status::Done)
        {
            return;
        }

        uint8_t msgType = 0;
        int matchId = 0;
        std::string playerOne;
        std::string playerTwo;
        std::string resultToken;
        std::string playerOneToken;
        packet >> msgType >> matchId;
        const MessageType type = static_cast<MessageType>(msgType);
        if (type == MessageType::CreateGameSession)
        {
            packet >> playerOne >> playerTwo >> resultToken;
        }
        else if (type == MessageType::CreateAiGameSession)
        {
            packet >> playerOne >> playerOneToken;
            playerTwo = AiOpponentName;
        }
        else
        {
            return;
        }

        if (!packet || playerOne.empty() || playerTwo.empty() ||
            (type == MessageType::CreateGameSession && resultToken.empty()))
        {
            return;
        }

        // Stay inside [FirstGamePort, LastGamePort] so a long-lived coordinator
        // never overflows into port 0 or the well-known service ports. A
        // recycled port that is still held by a running match simply fails to
        // listen in the child process, like any other in-use port.
        const unsigned short port = nextGamePort;
        nextGamePort = port >= LastGamePort
            ? FirstGamePort
            : static_cast<unsigned short>(port + 1);
        const bool started = spawnGameProcess(
            matchId,
            port,
            playerOne,
            playerTwo,
            resultToken,
            playerOneToken,
            type == MessageType::CreateAiGameSession,
            configPath);

        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::GameSessionCreated);
        response << started;
        response << matchId;
        response << (started ? port : static_cast<unsigned short>(0));
        response << std::string(started ? "Game session created" : "Failed to create game session");
        [[maybe_unused]] auto result = client.send(response);

        if (started)
        {
            fmt::println("Started game process for match {} on port {}", matchId, port);
        }
    }
};

class GameProcess
{
public:
    GameProcess(
        int matchId,
        unsigned short port,
        std::string playerOne,
        std::string playerTwo,
        std::string resultToken,
        card_source_config::Config cardSourceConfig,
        bool aiOpponent = false,
        std::string aiAccessToken = {})
        : matchId(matchId),
          port(port),
          playerUsernames{std::move(playerOne), std::move(playerTwo)},
          resultToken(std::move(resultToken)),
          cardSourceConfig(std::move(cardSourceConfig)),
          aiAccessToken(std::move(aiAccessToken)),
          listener(std::make_unique<bayou::tls::Listener>()),
          aiOpponent(aiOpponent)
    {
        loadCardLibrary();

        if (!listener_retry::listenWithRetry(*listener, port))
        {
            fmt::println("Game {} failed to listen on port {}", matchId, port);
            return;
        }

        listening = true;
        fmt::println("Game {} listening on port {}", matchId, port);
    }

    bool isListening() const
    {
        return listening;
    }

    void run()
    {
        if (!listening)
        {
            return;
        }

        if (aiOpponent)
        {
            runAiGame();
            return;
        }

        auto playerOne = acceptPlayer();
        auto playerTwo = acceptPlayer();
        if (!playerOne || !playerTwo)
        {
            fmt::println("Game {} did not receive both players", matchId);
            return;
        }
        if (playerOne->playerNumber == playerTwo->playerNumber)
        {
            fmt::println("Game {} received the same player slot twice", matchId);
            return;
        }

        sendGameReady(*playerOne->socket, playerOne->playerNumber);
        sendGameReady(*playerTwo->socket, playerTwo->playerNumber);

        const unsigned int seed =
            static_cast<unsigned int>(matchId) ^
            static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
        GameEngine engine(seed, cardCatalog);

        if (!receiveDeck(*playerOne, engine))
        {
            fmt::println("Game {} did not receive player {}'s deck", matchId, playerOne->playerNumber);
            engine.resign(playerOne->playerNumber);
            broadcast(engine, *playerOne, *playerTwo);
            finishRankedMatch(engine, *playerOne, *playerTwo);
            return;
        }
        if (!receiveDeck(*playerTwo, engine))
        {
            fmt::println("Game {} did not receive player {}'s deck", matchId, playerTwo->playerNumber);
            engine.resign(playerTwo->playerNumber);
            broadcast(engine, *playerOne, *playerTwo);
            finishRankedMatch(engine, *playerOne, *playerTwo);
            return;
        }

        fmt::println("Game {} started", matchId);
        broadcast(engine, *playerOne, *playerTwo);

        playerOne->socket->setBlocking(false);
        playerTwo->socket->setBlocking(false);

        runGameLoop(engine, *playerOne, *playerTwo);
        fmt::println("Game {} ended", matchId);
    }

    void runAiGame()
    {
        auto human = acceptPlayer();
        if (!human)
        {
            fmt::println("AI game {} did not receive the human player", matchId);
            return;
        }

        sendGameReady(*human->socket, human->playerNumber);

        const unsigned int seed =
            static_cast<unsigned int>(matchId) ^
            static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
        GameEngine engine(seed, cardCatalog);

        if (!receiveDeck(*human, engine))
        {
            fmt::println("AI game {} did not receive player's deck", matchId);
            engine.resign(human->playerNumber);
            sendSnapshot(*human->socket, engine, human->playerNumber);
            sendAiMatchResult(*human, engine);
            return;
        }

        const std::vector<card_data::Card> aiDeck = ai_deck::makeStarterDeck(cardCatalog);
        engine.submitDeck(AiPlayerNumber, aiDeck);
        fmt::println("AI game {} started ({} AI deck cards)", matchId, aiDeck.size());

        broadcastAi(engine, *human);

        human->socket->setBlocking(false);

        runAiGameLoop(engine, *human);
        fmt::println("AI game {} ended", matchId);
    }

    void broadcastAi(const GameEngine& engine, JoinedPlayer& human)
    {
        sendSnapshot(*human.socket, engine, human.playerNumber);
    }

    void runAiGameLoop(GameEngine& engine, JoinedPlayer& human)
    {
        while (true)
        {
            bool changed = false;

            if (engine.phase() == Phase::HeroPlacement &&
                !engine.playerState(AiPlayerNumber).heroesToPlace.empty())
            {
                placeAiHeroes(engine, AiPlayerNumber);
                changed = true;
            }

            if (engine.phase() == Phase::Playing &&
                engine.currentPlayer() == AiPlayerNumber)
            {
                const int beforePlayer = engine.currentPlayer();
                const AiAction action = chooseAiAction(engine, AiPlayerNumber);
                applyAiAction(engine, AiPlayerNumber, action);
                if (engine.phase() == Phase::Playing &&
                    engine.currentPlayer() == beforePlayer &&
                    action.kind != AiActionKind::EndTurn &&
                    action.kind != AiActionKind::DiscardCard)
                {
                    engine.endTurn(AiPlayerNumber);
                }
                changed = true;
            }

            sf::Packet packet;
            const auto status = human.socket->receive(packet);
            if (status == sf::Socket::Status::Disconnected)
            {
                engine.resign(human.playerNumber);
                broadcastAi(engine, human);
                sendAiMatchResult(human, engine);
                return;
            }
            if (status == sf::Socket::Status::Done)
            {
                if (handleAction(engine, human.playerNumber, packet))
                {
                    changed = true;
                }
            }

            if (changed)
            {
                broadcastAi(engine, human);
                if (engine.phase() == Phase::GameOver)
                {
                    sendAiMatchResult(human, engine);
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void sendAiMatchResult(JoinedPlayer& human, const GameEngine& engine)
    {
        const bool humanWon = engine.winner() == human.playerNumber;
        const AiMatchResult result = submitAiResult(matchId, aiAccessToken, humanWon);
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(MessageType::GameOver)
               << result.success
               << std::string(
                      result.success
                          ? (humanWon ? "Victory over the AI!" : "Defeated by the AI.")
                          : result.message)
               << 0
               << human.rating
               << result.coinsAwarded
               << false;
        [[maybe_unused]] auto sent = human.socket->send(packet);
    }

private:
    int matchId = 0;
    unsigned short port = 0;
    std::array<std::string, 2> playerUsernames;
    std::string resultToken;
    card_source_config::Config cardSourceConfig;
    std::string aiAccessToken;
    std::unique_ptr<bayou::tls::Listener> listener;
    // Authoritative card definitions keyed by title, loaded from the configured
    // card source; submitted decks are resolved against these stats.
    std::unordered_map<std::string, card_data::Card> cardLibrary;
    std::vector<card_data::Card> cardCatalog;
    bool listening = false;
    bool aiOpponent = false;

    std::optional<JoinedPlayer> acceptPlayer()
    {
        auto client = std::make_unique<bayou::tls::Socket>();
        if (listener->accept(*client) != sf::Socket::Status::Done)
        {
            return std::nullopt;
        }

        sf::Packet packet;
        if (socket_timeout::receivePacket(*client, packet, InitialRequestTimeout) != sf::Socket::Status::Done)
        {
            return std::nullopt;
        }

        uint8_t msgType = 0;
        int joinedMatchId = 0;
        int playerNumber = 0;
        std::string accessToken;
        packet >> msgType >> joinedMatchId >> playerNumber >> accessToken;

        if (!packet ||
            static_cast<MessageType>(msgType) != MessageType::JoinGame ||
            joinedMatchId != matchId ||
            (playerNumber != 1 && playerNumber != 2))
        {
            return std::nullopt;
        }

        std::string username;
        int rating = 0;
        if (!loadRankedPlayer(accessToken, username, rating) ||
            username != playerUsernames[static_cast<std::size_t>(playerNumber - 1)])
        {
            fmt::println("Rejected unauthenticated player {} for game {}", playerNumber, matchId);
            return std::nullopt;
        }

        fmt::println("{} joined game {} as player {}", username, matchId, playerNumber);
        return JoinedPlayer{std::move(client), playerNumber, username, accessToken, rating};
    }

    void sendGameReady(bayou::tls::Socket& client, int playerNumber)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::GameReady);
        response << matchId;
        response << playerNumber;
        response << std::string("Game ready");
        [[maybe_unused]] auto result = client.send(response);
    }

    // The client's deck submission is trusted only for its card titles. Every
    // title is re-resolved against this process's configured card database and checked
    // against the player's account collection, so a modified client can neither
    // forge card stats nor field cards it does not own.
    bool receiveDeck(JoinedPlayer& player, GameEngine& engine)
    {
        const int playerNumber = player.playerNumber;
        sf::Packet packet;
        if (player.socket->receive(packet) != sf::Socket::Status::Done)
        {
            return false;
        }

        uint8_t msgType = 0;
        std::uint32_t count = 0;
        packet >> msgType >> count;
        if (!packet ||
            static_cast<MessageType>(msgType) != MessageType::SubmitDeck ||
            count > MaxDeckPacketCards)
        {
            return false;
        }

        std::vector<card_data::Card> cards;
        cards.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            card_data::Card submitted;
            if (!card_data::readCard(packet, submitted))
            {
                return false;
            }

            const auto found = cardLibrary.find(submitted.title);
            if (found == cardLibrary.end())
            {
                fmt::println(
                    "Game {} rejected deck for player {}: unknown card {}",
                    matchId,
                    playerNumber,
                    submitted.title);
                return false;
            }
            cards.push_back(found->second);
        }

        if (const std::optional<std::string> error = game_data::deckRulesError(cards))
        {
            fmt::println(
                "Game {} rejected deck for player {}: {}",
                matchId,
                playerNumber,
                *error);
            return false;
        }

        if (const std::optional<std::string> error = deckOwnershipError(player, cards))
        {
            fmt::println(
                "Game {} rejected deck for player {}: {}",
                matchId,
                playerNumber,
                *error);
            return false;
        }

        engine.submitDeck(playerNumber, cards);
        fmt::println("Game {} received deck for player {} ({} cards)", matchId, playerNumber, count);
        return true;
    }

    std::optional<std::string> deckOwnershipError(
        const JoinedPlayer& player,
        const std::vector<card_data::Card>& cards) const
    {
        std::vector<account_data::CollectionCard> collection;
        if (!loadPlayerCollection(player.accessToken, collection))
        {
            return "could not load the player's card collection";
        }

        std::unordered_map<std::string, int> owned;
        for (const account_data::CollectionCard& card : collection)
        {
            owned[card.title] = card.copies;
        }

        std::unordered_map<std::string, int> used;
        for (const card_data::Card& card : cards)
        {
            if (++used[card.title] > owned[card.title])
            {
                return "deck uses more copies of " + card.title + " than the collection holds";
            }
        }

        return std::nullopt;
    }

    void loadCardLibrary()
    {
        try
        {
            std::string loadError;
            const std::vector<card_data::Card> cards = card_server_client::load(cardSourceConfig, loadError);
            if (!loadError.empty())
            {
                throw std::runtime_error(loadError);
            }

            for (const card_data::Card& card : cards)
            {
                cardCatalog.push_back(card);
                cardLibrary.emplace(card.title, card);
            }
        }
        catch (const std::exception& error)
        {
            // Fail closed: with an empty library every submitted deck is
            // rejected rather than falling back to client-supplied stats.
            fmt::println(
                "Game {} could not load configured card source {}: {}",
                matchId,
                cardSourceConfig.usesCardServer()
                    ? cardSourceConfig.cardServerHost + ":" + std::to_string(cardSourceConfig.cardServerPort)
                    : cardSourceConfig.cardsDatabasePath->string(),
                error.what());
        }
    }

    void sendSnapshot(bayou::tls::Socket& client, const GameEngine& engine, int playerNumber)
    {
        sf::Packet packet;
        packet << static_cast<uint8_t>(MessageType::GameStateUpdate);
        writeSnapshot(packet, engine.snapshotFor(playerNumber));
        [[maybe_unused]] auto result = client.send(packet);
    }

    void broadcast(const GameEngine& engine, JoinedPlayer& one, JoinedPlayer& two)
    {
        sendSnapshot(*one.socket, engine, one.playerNumber);
        sendSnapshot(*two.socket, engine, two.playerNumber);
    }

    void runGameLoop(GameEngine& engine, JoinedPlayer& one, JoinedPlayer& two)
    {
        while (true)
        {
            bool changed = false;

            for (JoinedPlayer* player : {&one, &two})
            {
                sf::Packet packet;
                const auto status = player->socket->receive(packet);
                if (status == sf::Socket::Status::Disconnected)
                {
                    engine.resign(player->playerNumber);
                    broadcast(engine, one, two);
                    finishRankedMatch(engine, one, two);
                    return;
                }
                if (status == sf::Socket::Status::Done)
                {
                    if (handleAction(engine, player->playerNumber, packet))
                    {
                        changed = true;
                    }
                }
            }

            if (changed)
            {
                broadcast(engine, one, two);
                if (engine.phase() == Phase::GameOver)
                {
                    finishRankedMatch(engine, one, two);
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    static void sendMatchResult(
        JoinedPlayer& player,
        const MatchResult& result,
        int winner)
    {
        const std::size_t index = static_cast<std::size_t>(player.playerNumber - 1);
        const int coinsAwarded =
            result.success && player.playerNumber == winner ? result.winnerCoins : 0;
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(MessageType::GameOver)
               << result.success
               << result.message
               << result.ratingChanges[index]
               << result.ratings[index]
               << coinsAwarded
               << result.selfMatch;
        [[maybe_unused]] auto sent = player.socket->send(packet);
    }

    void finishRankedMatch(
        const GameEngine& engine,
        JoinedPlayer& one,
        JoinedPlayer& two)
    {
        if (engine.winner() != 1 && engine.winner() != 2)
        {
            return;
        }
        const MatchResult result =
            submitRankedResult(matchId, resultToken, engine.winner());
        if (!result.success)
        {
            fmt::println("Game {} could not persist its match rewards", matchId);
        }
        sendMatchResult(one, result, engine.winner());
        sendMatchResult(two, result, engine.winner());
    }

    bool handleAction(GameEngine& engine, int playerNumber, sf::Packet& packet)
    {
        uint8_t msgType = 0;
        packet >> msgType;
        if (!packet)
        {
            return false;
        }

        switch (static_cast<MessageType>(msgType))
        {
            case MessageType::PlaceHero:
            {
                int heroIndex = 0;
                int row = 0;
                int column = 0;
                packet >> heroIndex >> row >> column;
                engine.placeHero(playerNumber, heroIndex, row, column);
                return true;
            }
            case MessageType::PlayCard:
            {
                int handIndex = 0;
                int row = 0;
                int column = 0;
                packet >> handIndex >> row >> column;
                engine.playCard(playerNumber, handIndex, row, column);
                return true;
            }
            case MessageType::MovePiece:
            {
                int pieceId = 0;
                int row = 0;
                int column = 0;
                packet >> pieceId >> row >> column;
                engine.movePiece(playerNumber, pieceId, row, column);
                return true;
            }
            case MessageType::AttackPiece:
            {
                int attackerId = 0;
                int row = 0;
                int column = 0;
                packet >> attackerId >> row >> column;
                engine.attackPiece(playerNumber, attackerId, row, column);
                return true;
            }
            case MessageType::UseAbility:
            {
                int pieceId = 0;
                packet >> pieceId;
                engine.useAbility(playerNumber, pieceId);
                return true;
            }
            case MessageType::DiscardCard:
            {
                int handIndex = 0;
                packet >> handIndex;
                engine.discardCard(playerNumber, handIndex);
                return true;
            }
            case MessageType::EndTurn:
                engine.endTurn(playerNumber);
                return true;
            case MessageType::Disconnect:
                engine.resign(playerNumber);
                return true;
            default:
                return false;
        }
    }
};

int main(int argc, char* argv[])
{
    std::filesystem::path configPath = "gameserver.cfg";
    int commandIndex = 1;
    if (argc >= 3 && std::string(argv[1]) == "--config")
    {
        configPath = argv[2];
        commandIndex = 3;
    }

    std::string configError;
    const std::optional<card_source_config::Config> config =
        card_source_config::load(configPath, configError);
    if (!config)
    {
        fmt::println("Game server configuration error: {}", configError);
        return 1;
    }

    if (argc == commandIndex + 6 && std::string(argv[commandIndex]) == "--game")
    {
        const int matchId = std::stoi(argv[commandIndex + 1]);
        const unsigned short port = static_cast<unsigned short>(std::stoi(argv[commandIndex + 2]));
        GameProcess game(
            matchId,
            port,
            argv[commandIndex + 3],
            argv[commandIndex + 4],
            argv[commandIndex + 5],
            *config);
        if (!game.isListening())
        {
            return 1;
        }
        game.run();
        return 0;
    }

    if (argc == commandIndex + 5 && std::string(argv[commandIndex]) == "--game-ai")
    {
        const int matchId = std::stoi(argv[commandIndex + 1]);
        const unsigned short port = static_cast<unsigned short>(std::stoi(argv[commandIndex + 2]));
        GameProcess game(
            matchId,
            port,
            argv[commandIndex + 3],
            AiOpponentName,
            "",
            *config,
            true,
            argv[commandIndex + 4]);
        if (!game.isListening())
        {
            return 1;
        }
        game.run();
        return 0;
    }

    fmt::println("Starting Game Server Coordinator...");

    GameServerCoordinator coordinator(GameServerPort, configPath);
    if (!coordinator.isListening())
    {
        return 1;
    }
    coordinator.run();

    return 0;
}
