// End-to-end integration test: drives two players through matchmaking and a
// full game on the real game server, asserting the core mechanics behave.
// Requires the matchmaking server (55001) and game server coordinator (55002)
// to be running.
#include <SFML/Network.hpp>
#include <fmt/core.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include "../shared/network.hpp"

using namespace network;
using namespace game_data;

namespace
{
constexpr unsigned short MatchmakingPort = 55001;
const char* Host = "127.0.0.1";

int failures = 0;

void check(bool condition, const std::string& label)
{
    fmt::println("[{}] {}", condition ? "PASS" : "FAIL", label);
    if (!condition)
    {
        ++failures;
    }
}

card_data::Card makeCard(
    const std::string& title,
    const std::string& type,
    std::vector<card_data::KeyIntPair> ints,
    std::vector<card_data::KeyStringPair> strings = {})
{
    card_data::Card card;
    card.title = title;
    card.type = type;
    card.integerValues = std::move(ints);
    card.stringValues = std::move(strings);
    return card;
}

std::vector<card_data::Card> makeTestDeck()
{
    std::vector<card_data::Card> deck;
    // Two heroes (hero cost 4 + 2 = 6, within the limit of 10).
    deck.push_back(makeCard("Gear Knight", "Hero",
        {{"heroCost", 4}, {"health", 18}, {"attack", 6}, {"range", 1}, {"move", 1}},
        {{"movement", "jump"}}));
    deck.push_back(makeCard("Cog Tinker", "Hero",
        {{"heroCost", 2}, {"health", 9}, {"attack", 3}, {"range", 1}, {"move", 1}},
        {{"movement", "omni"}}));
    // Twenty cheap units.
    for (int i = 0; i < 20; ++i)
    {
        deck.push_back(makeCard("Brass Pawn", "Unit",
            {{"cost", 1}, {"health", 4}, {"attack", 2}, {"range", 1}, {"move", 1}},
            {{"movement", "ortho"}}));
    }
    return deck;
}

bool connectWithRetry(sf::TcpSocket& socket, unsigned short port, int attempts)
{
    const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(Host);
    if (!address)
    {
        return false;
    }
    for (int i = 0; i < attempts; ++i)
    {
        if (socket.connect(*address, port) == sf::Socket::Status::Done)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void send(sf::TcpSocket& socket, sf::Packet& packet)
{
    [[maybe_unused]] auto result = socket.send(packet);
}

// Drains all pending packets, keeping the most recent snapshot.
bool pumpSnapshot(sf::TcpSocket& socket, Snapshot& latest)
{
    bool updated = false;
    sf::Packet packet;
    while (socket.receive(packet) == sf::Socket::Status::Done)
    {
        std::uint8_t type = 0;
        packet >> type;
        if (static_cast<MessageType>(type) == MessageType::GameStateUpdate)
        {
            Snapshot snapshot;
            if (readSnapshot(packet, snapshot))
            {
                latest = snapshot;
                updated = true;
            }
        }
        packet.clear();
    }
    return updated;
}

// Polls both sockets for up to timeoutMs, returning the latest snapshots.
void settle(sf::TcpSocket& a, sf::TcpSocket& b, Snapshot& sa, Snapshot& sb, int timeoutMs = 800)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        pumpSnapshot(a, sa);
        pumpSnapshot(b, sb);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

void sendPlaceHero(sf::TcpSocket& socket, int row, int column)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::PlaceHero) << 0 << row << column;
    send(socket, packet);
}

void sendPlayCard(sf::TcpSocket& socket, int handIndex, int row, int column)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::PlayCard) << handIndex << row << column;
    send(socket, packet);
}

void sendEndTurn(sf::TcpSocket& socket)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::EndTurn);
    send(socket, packet);
}
}

int main()
{
    fmt::println("=== Steam Tactics integration test ===");

    // --- matchmaking -------------------------------------------------------
    sf::TcpSocket mmA;
    sf::TcpSocket mmB;
    if (!connectWithRetry(mmA, MatchmakingPort, 20))
    {
        fmt::println("Could not reach matchmaking server on {}. Is it running?", MatchmakingPort);
        return 1;
    }
    sf::Packet joinA;
    joinA << static_cast<std::uint8_t>(MessageType::JoinMatchmaking);
    send(mmA, joinA);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    if (!connectWithRetry(mmB, MatchmakingPort, 20))
    {
        fmt::println("Second player could not reach matchmaking");
        return 1;
    }
    sf::Packet joinB;
    joinB << static_cast<std::uint8_t>(MessageType::JoinMatchmaking);
    send(mmB, joinB);

    auto readMatch = [](sf::TcpSocket& socket, int& matchId, int& playerNumber, unsigned short& gamePort) -> bool {
        sf::Packet packet;
        if (socket.receive(packet) != sf::Socket::Status::Done)
        {
            return false;
        }
        std::uint8_t type = 0;
        packet >> type >> matchId >> playerNumber >> gamePort;
        return static_cast<MessageType>(type) == MessageType::MatchFound;
    };

    int matchA = 0;
    int matchB = 0;
    int pnumA = 0;
    int pnumB = 0;
    unsigned short portA = 0;
    unsigned short portB = 0;
    const bool gotA = readMatch(mmA, matchA, pnumA, portA);
    const bool gotB = readMatch(mmB, matchB, pnumB, portB);
    check(gotA && gotB, "both players received MatchFound");
    check(matchA == matchB && matchA != 0, "shared match id");
    check(portA == portB && portA != 0, "shared game port");
    check((pnumA == 1 && pnumB == 2) || (pnumA == 2 && pnumB == 1), "distinct player numbers");
    mmA.disconnect();
    mmB.disconnect();

    if (failures != 0)
    {
        fmt::println("Aborting after matchmaking failures.");
        return 1;
    }

    // --- join game ---------------------------------------------------------
    sf::TcpSocket gameA;
    sf::TcpSocket gameB;
    check(connectWithRetry(gameA, portA, 40), "player A connected to game");
    check(connectWithRetry(gameB, portB, 40), "player B connected to game");

    // The game process only sends GameReady once both players have joined, so
    // send both JoinGame messages before blocking on either response.
    auto sendJoin = [](sf::TcpSocket& socket, int matchId, int playerNumber) {
        sf::Packet join;
        join << static_cast<std::uint8_t>(MessageType::JoinGame) << matchId << playerNumber;
        send(socket, join);
    };
    auto readReady = [](sf::TcpSocket& socket) -> bool {
        sf::Packet response;
        if (socket.receive(response) != sf::Socket::Status::Done)
        {
            return false;
        }
        std::uint8_t type = 0;
        int rid = 0;
        int rpn = 0;
        std::string message;
        response >> type >> rid >> rpn >> message;
        return static_cast<MessageType>(type) == MessageType::GameReady;
    };

    sendJoin(gameA, matchA, pnumA);
    sendJoin(gameB, matchB, pnumB);
    check(readReady(gameA), "player A game ready");
    check(readReady(gameB), "player B game ready");

    // Determine which socket is player 1 / player 2.
    sf::TcpSocket& p1 = (pnumA == 1) ? gameA : gameB;
    sf::TcpSocket& p2 = (pnumA == 1) ? gameB : gameA;

    // --- submit decks ------------------------------------------------------
    auto submitDeck = [](sf::TcpSocket& socket, const std::vector<card_data::Card>& deck) {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(MessageType::SubmitDeck);
        packet << static_cast<std::uint32_t>(deck.size());
        for (const card_data::Card& card : deck)
        {
            card_data::writeCard(packet, card);
        }
        send(socket, packet);
    };
    submitDeck(p1, makeTestDeck());
    submitDeck(p2, makeTestDeck());

    gameA.setBlocking(false);
    gameB.setBlocking(false);

    Snapshot s1;
    Snapshot s2;
    settle(p1, p2, s1, s2, 1000);
    check(s1.phase == static_cast<std::uint8_t>(Phase::HeroPlacement), "phase is HeroPlacement after decks");
    check(s1.players[0].heroesToPlace == 2, "player 1 has 2 heroes to place");
    check(s1.players[1].heroesToPlace == 2, "player 2 has 2 heroes to place");

    // --- hero placement ----------------------------------------------------
    // Player 1 home column 0 middle rows; player 2 home column 7 middle rows.
    sendPlaceHero(p1, 2, 0);
    settle(p1, p2, s1, s2, 400);
    sendPlaceHero(p1, 3, 0);
    sendPlaceHero(p2, 2, 7);
    settle(p1, p2, s1, s2, 400);
    sendPlaceHero(p2, 3, 7);
    settle(p1, p2, s1, s2, 1000);

    check(s1.phase == static_cast<std::uint8_t>(Phase::Playing), "phase advanced to Playing after all heroes placed");
    check(s1.activePlayer == 1, "player 1 acts first");

    int p1Heroes = 0;
    int p2Heroes = 0;
    for (const Piece& piece : s1.pieces)
    {
        if (piece.isHero && piece.owner == 1) ++p1Heroes;
        if (piece.isHero && piece.owner == 2) ++p2Heroes;
    }
    check(p1Heroes == 2 && p2Heroes == 2, "four heroes on the board");
    check(s1.players[0].steam == s1.players[0].controlledSquares, "player 1 steam equals controlled squares");
    check(s1.players[0].handCount >= StartingHandSize, "player 1 drew an opening hand");

    const int p1ControlBefore = s1.players[0].controlledSquares;
    const int p1SteamBefore = s1.players[0].steam;
    const int p1PiecesBefore = static_cast<int>(s1.pieces.size());

    // --- player 1 deploys a unit ------------------------------------------
    // Brass Pawn (cost 1) onto an empty controlled square. Find one.
    int deployRow = -1;
    int deployCol = -1;
    for (int r = 0; r < BoardSize && deployRow < 0; ++r)
    {
        for (int c = 0; c < BoardSize; ++c)
        {
            const std::size_t idx = static_cast<std::size_t>(squareIndex(r, c));
            if (s1.control[idx] == 1 && findPieceAt(s1.pieces, r, c) == nullptr)
            {
                deployRow = r;
                deployCol = c;
                break;
            }
        }
    }
    check(deployRow >= 0, "found an empty controlled square to deploy onto");

    // Find a Brass Pawn (Unit) in hand.
    int pawnIndex = -1;
    for (std::size_t i = 0; i < s1.hand.size(); ++i)
    {
        if (s1.hand[i].type == "Unit")
        {
            pawnIndex = static_cast<int>(i);
            break;
        }
    }
    check(pawnIndex >= 0, "player 1 has a unit card in hand");

    if (deployRow >= 0 && pawnIndex >= 0)
    {
        sendPlayCard(p1, pawnIndex, deployRow, deployCol);
        settle(p1, p2, s1, s2, 800);
        check(static_cast<int>(s1.pieces.size()) == p1PiecesBefore + 1, "deploying a unit added a piece");
        check(s1.players[0].steam == p1SteamBefore - 1, "deploying spent steam");
        check(s1.activePlayer == 2, "playing a card immediately ended player 1's turn");
        check(s2.activePlayer == 2, "turn passed to player 2 after player 1 played a card");
        check(s2.players[1].steam == s2.players[1].controlledSquares, "player 2 gained steam on its turn");
    }

    // Player 1's extra piece should have expanded or maintained its territory.
    check(s1.players[0].controlledSquares >= p1ControlBefore, "player 1 territory did not shrink after deploying");

    // --- resignation / win condition --------------------------------------
    fmt::println("Disconnecting player 1 to test win-by-default...");
    if (pnumA == 1)
    {
        gameA.disconnect();
    }
    else
    {
        gameB.disconnect();
    }

    // Player 2 should be told it won.
    settle(p2, p2, s2, s2, 1500);
    check(s2.phase == static_cast<std::uint8_t>(Phase::GameOver), "game ended after player 1 left");
    check(s2.winner == 2, "player 2 wins by default");

    if (pnumA == 1)
    {
        gameB.disconnect();
    }
    else
    {
        gameA.disconnect();
    }

    fmt::println("");
    if (failures == 0)
    {
        fmt::println("ALL CHECKS PASSED");
        return 0;
    }
    fmt::println("{} CHECK(S) FAILED", failures);
    return 1;
}
