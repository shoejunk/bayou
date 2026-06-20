#include <SFML/Network.hpp>
#include <fmt/core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include "../shared/network.hpp"

using namespace network;
using namespace game_data;

namespace
{
constexpr unsigned short GameServerPort = 55002;
constexpr unsigned short FirstGamePort = 56000;

struct JoinedPlayer
{
    std::unique_ptr<sf::TcpSocket> socket;
    int playerNumber = 0;
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

bool spawnGameProcess(int matchId, unsigned short port)
{
#ifdef _WIN32
    std::string command = fmt::format("\"{}\" --game {} {}", executablePath(), matchId, port);
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
    const std::string command = fmt::format("{} --game {} {} &", executablePath(), matchId, port);
    return std::system(command.c_str()) == 0;
#endif
}
}

// ---------------------------------------------------------------------------
// Authoritative tactical-game engine.
// ---------------------------------------------------------------------------
class GameEngine
{
public:
    struct EnginePlayer
    {
        int number = 0;
        std::vector<GameCard> drawPile;
        std::vector<GameCard> hand;
        std::vector<GameCard> heroesToPlace;
        int steam = 0;
        bool deckSubmitted = false;
    };

    explicit GameEngine(unsigned int seed)
        : rng(seed)
    {
        players[0].number = 1;
        players[1].number = 2;
        initializeControl();
    }

    bool bothDecksSubmitted() const
    {
        return players[0].deckSubmitted && players[1].deckSubmitted;
    }

    Phase phase() const { return phaseValue; }
    int winner() const { return winnerValue; }

    // Splits a submitted deck into a shuffled draw pile and a hero roster.
    void submitDeck(int playerNumber, const std::vector<card_data::Card>& cards)
    {
        EnginePlayer& player = playerRef(playerNumber);
        player.drawPile.clear();
        player.heroesToPlace.clear();

        for (const card_data::Card& card : cards)
        {
            GameCard resolved = toGameCard(card);
            if (isHeroCard(card))
            {
                if (static_cast<int>(player.heroesToPlace.size()) < MaxHeroes)
                {
                    player.heroesToPlace.push_back(resolved);
                }
            }
            else
            {
                player.drawPile.push_back(resolved);
            }
        }

        std::shuffle(player.drawPile.begin(), player.drawPile.end(), rng);
        player.deckSubmitted = true;

        if (bothDecksSubmitted())
        {
            status = "Place your heroes on your starting squares.";
        }
    }

    // Heroes are placed in deck order (the client always offers the next one),
    // so the heroIndex hint is ignored and the front hero is consumed.
    void placeHero(int playerNumber, int /*heroIndex*/, int row, int column)
    {
        if (phaseValue != Phase::HeroPlacement)
        {
            return;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (player.heroesToPlace.empty())
        {
            return;
        }

        if (!isStartingSquare(playerNumber, row, column) || pieceAt(row, column) != nullptr)
        {
            setStatusFor(playerNumber, "Heroes must go on an empty starting square.");
            return;
        }

        spawnPiece(playerNumber, player.heroesToPlace.front(), row, column, true);
        player.heroesToPlace.erase(player.heroesToPlace.begin());

        if (players[0].heroesToPlace.empty() && players[1].heroesToPlace.empty())
        {
            beginPlay();
        }
    }

    void playCard(int playerNumber, int handIndex, int targetRow, int targetColumn)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (handIndex < 0 || handIndex >= static_cast<int>(player.hand.size()))
        {
            return;
        }

        const GameCard card = player.hand[static_cast<std::size_t>(handIndex)];
        if (card.cost > player.steam)
        {
            setStatusFor(playerNumber, "Not enough steam to play that card.");
            return;
        }

        if (card.type == "Unit")
        {
            if (!inBounds(targetRow, targetColumn) ||
                control[static_cast<std::size_t>(squareIndex(targetRow, targetColumn))] != playerNumber ||
                pieceAt(targetRow, targetColumn) != nullptr)
            {
                setStatusFor(playerNumber, "Units deploy onto an empty square you control.");
                return;
            }

            spawnPiece(playerNumber, card, targetRow, targetColumn, false);
            // Summoned units cannot act the turn they arrive.
            pieces.back().hasActed = true;
        }
        else  // Spell
        {
            if (!resolveSpell(playerNumber, card, targetRow, targetColumn))
            {
                return;
            }
        }

        player.steam -= card.cost;
        player.hand.erase(player.hand.begin() + handIndex);
        advanceTurn(fmt::format("Player {} played {}.", playerNumber, card.title));
    }

    void movePiece(int playerNumber, int pieceId, int toRow, int toColumn)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return;
        }

        Piece* piece = pieceById(pieceId);
        if (piece == nullptr || piece->owner != playerNumber || piece->hasActed)
        {
            return;
        }

        if (!game_data::isLegalMove(pieces, *piece, toRow, toColumn))
        {
            setStatusFor(playerNumber, "That piece cannot reach there.");
            return;
        }

        piece->row = toRow;
        piece->column = toColumn;
        piece->hasActed = true;
        advanceTurn(fmt::format("{} moved.", piece->name));
    }

    void attackPiece(int playerNumber, int attackerId, int targetRow, int targetColumn)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return;
        }

        Piece* attacker = pieceById(attackerId);
        Piece* target = pieceAt(targetRow, targetColumn);
        if (attacker == nullptr || target == nullptr || attacker->owner != playerNumber ||
            attacker->hasActed)
        {
            return;
        }

        if (!game_data::isLegalAttack(*attacker, *target))
        {
            setStatusFor(playerNumber, "Target is out of range.");
            return;
        }

        target->health -= attacker->attack;
        attacker->hasActed = true;

        const std::string attackerName = attacker->name;
        const std::string targetName = target->name;
        if (target->health <= 0)
        {
            const int victimOwner = target->owner;
            removePiece(target->id);
            checkForWinner(victimOwner);
            if (phaseValue == Phase::GameOver)
            {
                return;
            }
            advanceTurn(fmt::format("{} destroyed {}!", attackerName, targetName));
        }
        else
        {
            advanceTurn(fmt::format("{} hit {} for {}.", attackerName, targetName, attacker->attack));
        }
    }

    void endTurn(int playerNumber)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return;
        }

        advanceTurn(fmt::format("Player {} passed.", playerNumber));
    }

    // Builds the view tailored to one player (their hand only).
    Snapshot snapshotFor(int playerNumber) const
    {
        Snapshot snapshot;
        snapshot.phase = static_cast<std::uint8_t>(phaseValue);
        snapshot.activePlayer = activePlayer;
        snapshot.yourPlayer = playerNumber;
        snapshot.winner = winnerValue;
        snapshot.control = control;
        snapshot.pieces = pieces;
        snapshot.hand = playerRef(playerNumber).hand;
        snapshot.status = status;

        for (int p = 0; p < 2; ++p)
        {
            const EnginePlayer& player = players[static_cast<std::size_t>(p)];
            PlayerSnapshot& view = snapshot.players[static_cast<std::size_t>(p)];
            view.steam = player.steam;
            view.controlledSquares = controlledCount(p + 1);
            view.handCount = static_cast<int>(player.hand.size());
            view.heroesToPlace = static_cast<int>(player.heroesToPlace.size());
            view.heroesAlive = heroesAlive(p + 1);
            view.drawPileCount = static_cast<int>(player.drawPile.size());
        }

        return snapshot;
    }

    void resign(int playerNumber)
    {
        if (phaseValue == Phase::GameOver)
        {
            return;
        }
        winnerValue = playerNumber == 1 ? 2 : 1;
        phaseValue = Phase::GameOver;
        status = fmt::format("Player {} left. Player {} wins!", playerNumber, winnerValue);
    }

private:
    std::mt19937 rng;
    Phase phaseValue = Phase::HeroPlacement;
    int activePlayer = 1;
    int winnerValue = 0;
    std::array<std::uint8_t, BoardSquares> control{};
    std::vector<Piece> pieces;
    std::array<EnginePlayer, 2> players{};
    int nextPieceId = 1;
    std::string status = "Waiting for both decks...";

    EnginePlayer& playerRef(int playerNumber)
    {
        return players[static_cast<std::size_t>(playerNumber - 1)];
    }
    const EnginePlayer& playerRef(int playerNumber) const
    {
        return players[static_cast<std::size_t>(playerNumber - 1)];
    }

    void initializeControl()
    {
        control.fill(0);
        for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
        {
            for (const auto& [row, column] : homeSquares(playerNumber))
            {
                control[static_cast<std::size_t>(squareIndex(row, column))] =
                    static_cast<std::uint8_t>(playerNumber);
            }
        }
    }

    bool isStartingSquare(int playerNumber, int row, int column) const
    {
        for (const auto& [r, c] : homeSquares(playerNumber))
        {
            if (r == row && c == column)
            {
                return true;
            }
        }
        return false;
    }

    Piece* pieceAt(int row, int column)
    {
        for (Piece& piece : pieces)
        {
            if (piece.row == row && piece.column == column)
            {
                return &piece;
            }
        }
        return nullptr;
    }
    const Piece* pieceAt(int row, int column) const
    {
        for (const Piece& piece : pieces)
        {
            if (piece.row == row && piece.column == column)
            {
                return &piece;
            }
        }
        return nullptr;
    }

    Piece* pieceById(int id)
    {
        for (Piece& piece : pieces)
        {
            if (piece.id == id)
            {
                return &piece;
            }
        }
        return nullptr;
    }

    void removePiece(int id)
    {
        pieces.erase(
            std::remove_if(pieces.begin(), pieces.end(), [id](const Piece& p) { return p.id == id; }),
            pieces.end());
    }

    void spawnPiece(int playerNumber, const GameCard& card, int row, int column, bool isHero)
    {
        Piece piece;
        piece.id = nextPieceId++;
        piece.owner = playerNumber;
        piece.row = row;
        piece.column = column;
        piece.name = card.title;
        piece.imagePath = card.imagePath;
        piece.walkAnimPath = card.walkAnimPath;
        piece.maxHealth = card.health;
        piece.health = card.health;
        piece.attack = card.attack;
        piece.attackRange = card.attackRange;
        piece.movePattern = card.movePattern;
        piece.moveRange = card.moveRange;
        piece.isHero = isHero;
        piece.hasActed = false;
        pieces.push_back(piece);
    }

    int controlledCount(int playerNumber) const
    {
        int count = 0;
        for (std::uint8_t owner : control)
        {
            if (owner == playerNumber)
            {
                ++count;
            }
        }
        return count;
    }

    int heroesAlive(int playerNumber) const
    {
        int count = 0;
        for (const Piece& piece : pieces)
        {
            if (piece.owner == playerNumber && piece.isHero)
            {
                ++count;
            }
        }
        return count;
    }

    void beginPlay()
    {
        for (int p = 1; p <= 2; ++p)
        {
            EnginePlayer& player = playerRef(p);
            for (int i = 0; i < StartingHandSize; ++i)
            {
                drawCard(player);
            }
        }

        recomputeControl();
        phaseValue = Phase::Playing;
        activePlayer = 1;
        startTurn(1);
    }

    void startTurn(int playerNumber)
    {
        EnginePlayer& player = playerRef(playerNumber);
        player.steam += controlledCount(playerNumber);
        drawCard(player);

        for (Piece& piece : pieces)
        {
            if (piece.owner == playerNumber)
            {
                piece.hasActed = false;
            }
        }

        status = fmt::format("Player {}'s turn. +{} steam.", playerNumber, controlledCount(playerNumber));
    }

    void drawCard(EnginePlayer& player)
    {
        if (player.drawPile.empty() || static_cast<int>(player.hand.size()) >= MaxHandSize)
        {
            return;
        }
        player.hand.push_back(player.drawPile.back());
        player.drawPile.pop_back();
    }

    void advanceTurn(const std::string& actionStatus)
    {
        recomputeControl();
        if (phaseValue == Phase::GameOver)
        {
            return;
        }

        activePlayer = activePlayer == 1 ? 2 : 1;
        startTurn(activePlayer);
        if (!actionStatus.empty())
        {
            status = actionStatus + " " + status;
        }
    }

    bool resolveSpell(int playerNumber, const GameCard& card, int targetRow, int targetColumn)
    {
        if (card.effect == "steam")
        {
            playerRef(playerNumber).steam += card.power;
            return true;
        }

        Piece* target = inBounds(targetRow, targetColumn) ? pieceAt(targetRow, targetColumn) : nullptr;
        if (target == nullptr)
        {
            setStatusFor(playerNumber, "That spell needs a target.");
            return false;
        }

        if (card.effect == "damage")
        {
            if (target->owner == playerNumber)
            {
                setStatusFor(playerNumber, "Target an enemy piece.");
                return false;
            }
            target->health -= card.power;
            if (target->health <= 0)
            {
                const int victimOwner = target->owner;
                removePiece(target->id);
                checkForWinner(victimOwner);
            }
            return true;
        }

        if (card.effect == "heal")
        {
            if (target->owner != playerNumber)
            {
                setStatusFor(playerNumber, "Target a friendly piece.");
                return false;
            }
            target->health = std::min(target->maxHealth, target->health + card.power);
            return true;
        }

        return true;
    }

    void checkForWinner(int victimOwner)
    {
        if (heroesAlive(victimOwner) == 0)
        {
            winnerValue = victimOwner == 1 ? 2 : 1;
            phaseValue = Phase::GameOver;
            status = fmt::format("All of Player {}'s heroes fell. Player {} wins!", victimOwner, winnerValue);
        }
    }

    void setStatusFor(int playerNumber, const std::string& message)
    {
        status = message;
        (void)playerNumber;
    }

    // Recomputes whole-board control: occupied squares belong to the occupant;
    // empty squares go to whoever has more adjacent pieces; ties hold.
    void recomputeControl()
    {
        std::array<std::uint8_t, BoardSquares> next = control;
        for (int row = 0; row < BoardSize; ++row)
        {
            for (int column = 0; column < BoardSize; ++column)
            {
                const std::size_t index = static_cast<std::size_t>(squareIndex(row, column));
                if (const Piece* occupant = pieceAt(row, column))
                {
                    next[index] = static_cast<std::uint8_t>(occupant->owner);
                    continue;
                }

                int influence1 = 0;
                int influence2 = 0;
                for (int dr = -1; dr <= 1; ++dr)
                {
                    for (int dc = -1; dc <= 1; ++dc)
                    {
                        if (dr == 0 && dc == 0)
                        {
                            continue;
                        }
                        const Piece* neighbor = inBounds(row + dr, column + dc)
                            ? pieceAt(row + dr, column + dc)
                            : nullptr;
                        if (neighbor == nullptr)
                        {
                            continue;
                        }
                        if (neighbor->owner == 1)
                        {
                            ++influence1;
                        }
                        else if (neighbor->owner == 2)
                        {
                            ++influence2;
                        }
                    }
                }

                if (influence1 > influence2)
                {
                    next[index] = 1;
                }
                else if (influence2 > influence1)
                {
                    next[index] = 2;
                }
                // tie: keep existing controller (already copied into next)
            }
        }
        control = next;
    }
};

class GameServerCoordinator
{
public:
    explicit GameServerCoordinator(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>())
    {
        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Failed to listen on port {}", port);
            return;
        }

        listening = true;
        fmt::println("Game server coordinator listening on port {}", port);
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
            auto client = std::make_unique<sf::TcpSocket>();
            if (listener->accept(*client) == sf::Socket::Status::Done)
            {
                handleClient(*client);
            }
        }
    }

private:
    std::unique_ptr<sf::TcpListener> listener;
    std::atomic<bool> running{false};
    bool listening = false;
    unsigned short nextGamePort = FirstGamePort;

    void handleClient(sf::TcpSocket& client)
    {
        sf::Packet packet;
        if (client.receive(packet) != sf::Socket::Status::Done)
        {
            return;
        }

        uint8_t msgType = 0;
        int matchId = 0;
        packet >> msgType >> matchId;

        if (static_cast<MessageType>(msgType) != MessageType::CreateGameSession)
        {
            return;
        }

        const unsigned short port = nextGamePort++;
        const bool started = spawnGameProcess(matchId, port);

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
    GameProcess(int matchId, unsigned short port)
        : matchId(matchId), port(port), listener(std::make_unique<sf::TcpListener>())
    {
        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Game {} failed to listen on port {}", matchId, port);
            return;
        }

        listening = true;
        fmt::println("Game {} listening on port {}", matchId, port);
    }

    void run()
    {
        if (!listening)
        {
            return;
        }

        auto playerOne = acceptPlayer();
        auto playerTwo = acceptPlayer();
        if (!playerOne || !playerTwo)
        {
            fmt::println("Game {} did not receive both players", matchId);
            return;
        }

        sendGameReady(*playerOne->socket, playerOne->playerNumber);
        sendGameReady(*playerTwo->socket, playerTwo->playerNumber);

        const unsigned int seed =
            static_cast<unsigned int>(matchId) ^
            static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
        GameEngine engine(seed);

        if (!receiveDeck(*playerOne->socket, engine, playerOne->playerNumber) ||
            !receiveDeck(*playerTwo->socket, engine, playerTwo->playerNumber))
        {
            fmt::println("Game {} did not receive both decks", matchId);
            return;
        }

        fmt::println("Game {} started", matchId);
        broadcast(engine, *playerOne, *playerTwo);

        playerOne->socket->setBlocking(false);
        playerTwo->socket->setBlocking(false);

        runGameLoop(engine, *playerOne, *playerTwo);
        fmt::println("Game {} ended", matchId);
    }

private:
    int matchId = 0;
    unsigned short port = 0;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening = false;

    std::optional<JoinedPlayer> acceptPlayer()
    {
        auto client = std::make_unique<sf::TcpSocket>();
        if (listener->accept(*client) != sf::Socket::Status::Done)
        {
            return std::nullopt;
        }

        sf::Packet packet;
        if (client->receive(packet) != sf::Socket::Status::Done)
        {
            return std::nullopt;
        }

        uint8_t msgType = 0;
        int joinedMatchId = 0;
        int playerNumber = 0;
        packet >> msgType >> joinedMatchId >> playerNumber;

        if (static_cast<MessageType>(msgType) != MessageType::JoinGame || joinedMatchId != matchId)
        {
            return std::nullopt;
        }

        fmt::println("Player {} joined game {}", playerNumber, matchId);
        return JoinedPlayer{std::move(client), playerNumber};
    }

    void sendGameReady(sf::TcpSocket& client, int playerNumber)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::GameReady);
        response << matchId;
        response << playerNumber;
        response << std::string("Game ready");
        [[maybe_unused]] auto result = client.send(response);
    }

    bool receiveDeck(sf::TcpSocket& client, GameEngine& engine, int playerNumber)
    {
        sf::Packet packet;
        if (client.receive(packet) != sf::Socket::Status::Done)
        {
            return false;
        }

        uint8_t msgType = 0;
        std::uint32_t count = 0;
        packet >> msgType >> count;
        if (!packet || static_cast<MessageType>(msgType) != MessageType::SubmitDeck)
        {
            return false;
        }

        std::vector<card_data::Card> cards;
        cards.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            card_data::Card card;
            if (!card_data::readCard(packet, card))
            {
                return false;
            }
            cards.push_back(card);
        }

        engine.submitDeck(playerNumber, cards);
        fmt::println("Game {} received deck for player {} ({} cards)", matchId, playerNumber, count);
        return true;
    }

    void sendSnapshot(sf::TcpSocket& client, const GameEngine& engine, int playerNumber)
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
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
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
    if (argc == 4 && std::string(argv[1]) == "--game")
    {
        const int matchId = std::stoi(argv[2]);
        const unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));
        GameProcess game(matchId, port);
        game.run();
        return 0;
    }

    fmt::println("Starting Game Server Coordinator...");

    GameServerCoordinator coordinator(GameServerPort);
    coordinator.run();

    return 0;
}
