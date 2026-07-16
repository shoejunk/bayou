// End-to-end integration test: drives two players through matchmaking and a
// full game on the real game server, asserting the core mechanics behave.
// Requires the matchmaking server (55001) and game server coordinator (55002)
// to be running.
#include <SFML/Network.hpp>
#include "tls_socket.hpp"
#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/game_data.hpp"
#include "../shared/ranking.hpp"
#include "../gameserver/game_engine.hpp"

#include "../shared/network.hpp"

using namespace network;
using namespace game_data;

namespace
{
constexpr unsigned short MatchmakingPort = 55001;
constexpr unsigned short AccountPort = 55000;
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

bool connectWithRetry(bayou::tls::Socket& socket, unsigned short port, int attempts)
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

std::string tryLogin(const std::string& username, const std::string& password)
{
    bayou::tls::Socket socket;
    if (!connectWithRetry(socket, AccountPort, 20))
    {
        return {};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::Login)
            << username << password << false;
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
    bool success = false;
    std::string message;
    std::string authenticatedUsername;
    std::string accessToken;
    std::string rememberToken;
    response >> type >> success >> message
             >> authenticatedUsername >> accessToken >> rememberToken;
    return response &&
        static_cast<MessageType>(type) == MessageType::LoginResponse &&
        success
        ? accessToken
        : std::string();
}

bool createAccountForTest(const std::string& username, const std::string& password)
{
    bayou::tls::Socket socket;
    if (!connectWithRetry(socket, AccountPort, 20))
    {
        return false;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::CreateAccount)
            << username << password;
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
    response >> type >> success >> message;
    return response &&
        static_cast<MessageType>(type) == MessageType::CreateAccountResponse &&
        success;
}

// Logs the test account in, creating it first when it does not exist yet so
// the test can run against a fresh accounts database.
std::string loginForTest(const std::string& username, const std::string& password)
{
    const std::string token = tryLogin(username, password);
    if (!token.empty())
    {
        return token;
    }
    if (!createAccountForTest(username, password))
    {
        return {};
    }
    return tryLogin(username, password);
}

// Fetches the account's saved decks and returns the first one as title-only
// cards; the game server resolves each title against its configured
// authoritative card catalog, so the titles are all a legitimate client needs
// to submit.
std::vector<card_data::Card> fetchDeckCards(const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectWithRetry(socket, AccountPort, 20))
    {
        return {};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::DeckListRequest) << accessToken;
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
    bool success = false;
    std::string message;
    std::uint32_t deckCount = 0;
    response >> type >> success >> message >> deckCount;
    if (!response ||
        static_cast<MessageType>(type) != MessageType::DeckListResponse ||
        !success ||
        deckCount == 0)
    {
        return {};
    }

    deck_data::Deck deck;
    if (!deck_data::readDeck(response, deck))
    {
        return {};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(deck.cardTitles.size());
    for (const std::string& title : deck.cardTitles)
    {
        card_data::Card card;
        card.title = title;
        cards.push_back(card);
    }
    return cards;
}

void send(bayou::tls::Socket& socket, sf::Packet& packet)
{
    [[maybe_unused]] auto result = socket.send(packet);
}

// Drains all pending packets, keeping the most recent snapshot.
bool pumpSnapshot(bayou::tls::Socket& socket, Snapshot& latest)
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
void settle(bayou::tls::Socket& a, bayou::tls::Socket& b, Snapshot& sa, Snapshot& sb, int timeoutMs = 800)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        pumpSnapshot(a, sa);
        pumpSnapshot(b, sb);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

void sendPlaceHero(bayou::tls::Socket& socket, int row, int column, int heroIndex = 0)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::PlaceHero) << heroIndex << row << column;
    send(socket, packet);
}

void sendPlayCard(bayou::tls::Socket& socket, int handIndex, int row, int column)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::PlayCard) << handIndex << row << column;
    send(socket, packet);
}

void sendEndTurn(bayou::tls::Socket& socket)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::EndTurn);
    send(socket, packet);
}

void sendDiscardCard(bayou::tls::Socket& socket, int handIndex)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(MessageType::DiscardCard) << handIndex;
    send(socket, packet);
}
}

int main(int argc, char** argv)
{
    fmt::println("=== Resources Tactics integration test ===");

    card_data::Card largeDefinition;
    largeDefinition.title = "Large Unit";
    largeDefinition.type = "Unit";
    largeDefinition.integerValues = {{"width", 2}, {"height", 3}};
    const GameCard largeCard = toGameCard(largeDefinition);
    check(largeCard.width == 2 && largeCard.height == 3,
          "unit width and height resolve from card data");
    Piece largePiece;
    largePiece.id = 100;
    largePiece.row = 2;
    largePiece.column = 3;
    populatePieceFromCard(largePiece, largeCard, false);
    std::vector<Piece> footprintPieces{largePiece};
    check(findPieceAt(footprintPieces, 4, 4) != nullptr &&
              findPieceAt(footprintPieces, 5, 4) == nullptr,
          "every square inside a multi-square footprint is occupied");
    Piece footprintBlocker;
    footprintBlocker.id = 101;
    footprintBlocker.row = 3;
    footprintBlocker.column = 6;
    footprintPieces.push_back(footprintBlocker);
    check(!pieceFootprintFree(footprintPieces, footprintPieces[0], 3, 5),
          "multi-square movement rejects a blocker under any covered square");

    Piece largeRangedAttacker;
    largeRangedAttacker.id = 110;
    largeRangedAttacker.owner = 1;
    largeRangedAttacker.row = 2;
    largeRangedAttacker.column = 1;
    largeRangedAttacker.width = 2;
    largeRangedAttacker.height = 2;
    ActionProfile rangedProfile;
    rangedProfile.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    rangedProfile.pattern = static_cast<std::uint8_t>(MovePattern::Omni);
    rangedProfile.minRange = 1;
    rangedProfile.maxRange = 2;
    rangedProfile.canMove = false;
    rangedProfile.canAttack = true;
    rangedProfile.damage = 1;
    largeRangedAttacker.actions = {rangedProfile};
    Piece rangedTarget;
    rangedTarget.id = 111;
    rangedTarget.owner = 2;
    rangedTarget.row = 2;
    rangedTarget.column = 4;
    std::array<std::uint8_t, BoardSquares> footprintBoard{};
    std::vector<Piece> rangedPieces{largeRangedAttacker, rangedTarget};
    check(resolvePieceAction(rangedPieces, footprintBoard, rangedPieces[0], 2, 4).legal,
          "large ranged attacker measures range from its closest footprint square");
    rangedPieces[1].actions = {rangedProfile};
    rangedPieces[1].owner = 1;
    rangedPieces[0].owner = 2;
    check(resolvePieceAction(rangedPieces, footprintBoard, rangedPieces[1], 2, 1).legal,
          "ranged attacks against a large target use its closest footprint square");

    Piece sweepingAttacker;
    sweepingAttacker.id = 120;
    sweepingAttacker.owner = 1;
    sweepingAttacker.row = 2;
    sweepingAttacker.column = 1;
    sweepingAttacker.width = 3;
    sweepingAttacker.height = 3;
    ActionProfile sweepingMove;
    sweepingMove.kind = static_cast<std::uint8_t>(ActionKind::Slide);
    sweepingMove.pattern = static_cast<std::uint8_t>(MovePattern::Horizontal);
    sweepingMove.minRange = 1;
    sweepingMove.maxRange = 1;
    sweepingMove.damage = 2;
    sweepingMove.canMove = true;
    sweepingMove.canAttack = true;
    sweepingAttacker.actions = {sweepingMove};
    std::vector<Piece> sweepingPieces{sweepingAttacker};
    for (int row = 2; row <= 4; ++row)
    {
        Piece target;
        target.id = 121 + row;
        target.owner = 2;
        target.row = row;
        target.column = 4;
        sweepingPieces.push_back(target);
    }
    const ActionResolution sweep =
        resolvePieceAction(sweepingPieces, footprintBoard, sweepingPieces[0], 2, 2);
    check(sweep.legal && sweep.moves && sweep.attacks && sweep.targetIds.size() == 3,
          "attacking move targets every enemy overlapped by the destination footprint");

    Piece slidingAttacker;
    slidingAttacker.owner = 1;
    slidingAttacker.row = 3;
    slidingAttacker.column = 1;
    slidingAttacker.attack = 4;
    slidingAttacker.movePattern = static_cast<std::uint8_t>(MovePattern::Ortho);
    slidingAttacker.moveRange = 4;
    slidingAttacker.attackingMove = true;
    Piece slidingTarget;
    slidingTarget.owner = 2;
    slidingTarget.row = 3;
    slidingTarget.column = 4;
    std::vector<Piece> movementTestPieces = {slidingAttacker, slidingTarget};
    check(isLegalAttackingMove(movementTestPieces, movementTestPieces[0], 3, 4),
          "attacking slide can target a reachable enemy");
    check(attackingMoveFallbackSquare(movementTestPieces[0], 3, 4) == std::pair<int, int>{3, 3},
          "failed attacking slide stops before the enemy");
    movementTestPieces[0].movePattern = static_cast<std::uint8_t>(MovePattern::Jump);
    movementTestPieces[1].row = 5;
    movementTestPieces[1].column = 2;
    check(isLegalAttackingMove(movementTestPieces, movementTestPieces[0], 5, 2),
          "attacking jump can target a knight-reachable enemy");
    check(attackingMoveFallbackSquare(movementTestPieces[0], 5, 2) == std::pair<int, int>{3, 1},
          "failed attacking jump stays at its starting square");

    std::array<std::uint8_t, BoardSquares> holes{};
    Piece profilePiece;
    profilePiece.id = 10;
    profilePiece.owner = 1;
    profilePiece.row = 3;
    profilePiece.column = 3;

    ActionProfile horizontal;
    horizontal.pattern = static_cast<std::uint8_t>(MovePattern::Horizontal);
    horizontal.canMove = true;
    profilePiece.actions = {horizontal};
    check(resolvePieceAction({profilePiece}, holes, profilePiece, 3, 4).legal,
          "horizontal profile moves along its row");
    check(!resolvePieceAction({profilePiece}, holes, profilePiece, 4, 3).legal,
          "horizontal profile rejects vertical movement");

    ActionProfile teleport;
    teleport.kind = static_cast<std::uint8_t>(ActionKind::Teleport);
    teleport.canMove = true;
    profilePiece.actions = {teleport};
    check(resolvePieceAction({profilePiece}, holes, profilePiece, 7, 7).legal,
          "teleport reaches any empty square");

    Piece hopTarget;
    hopTarget.id = 11;
    hopTarget.owner = 2;
    hopTarget.row = 3;
    hopTarget.column = 4;
    ActionProfile hop;
    hop.kind = static_cast<std::uint8_t>(ActionKind::Hop);
    hop.canMove = true;
    hop.canAttack = true;
    hop.damage = 1;
    profilePiece.actions = {hop};
    const std::vector<Piece> hopPieces = {profilePiece, hopTarget};
    const ActionResolution hopResult =
        resolvePieceAction(hopPieces, holes, hopPieces[0], 3, 5);
    check(hopResult.legal && hopResult.moves && hopResult.attacks && hopResult.targetId == 11,
          "hop moves to an empty landing square and attacks the pivot enemy");

    ActionProfile tunnel;
    tunnel.kind = static_cast<std::uint8_t>(ActionKind::Tunnel);
    tunnel.canMove = true;
    profilePiece.actions = {tunnel};
    holes[static_cast<std::size_t>(squareIndex(3, 3))] = 1;
    holes[static_cast<std::size_t>(squareIndex(6, 6))] = 1;
    check(resolvePieceAction({profilePiece}, holes, profilePiece, 6, 6).legal,
          "tunnel connects decorated hole squares");
    check(!resolvePieceAction({profilePiece}, holes, profilePiece, 6, 5).legal,
          "tunnel rejects a destination without a hole");
    profilePiece.ability = "dig";
    profilePiece.abilityUses = -1;
    check(pieceAbilityAvailable(profilePiece), "negative dig uses mean unlimited ability uses");
    profilePiece.abilityUses = 0;
    check(!pieceAbilityAvailable(profilePiece), "zero dig uses means the ability is spent");
    profilePiece.ability = "summon";
    profilePiece.summonTitle = "Test Unit";
    profilePiece.owner = 1;
    profilePiece.row = 3;
    profilePiece.column = 3;
    profilePiece.growTurnsRemaining = 0;
    profilePiece.disabledTurns = 0;
    check(pieceAbilityAvailable({profilePiece}, profilePiece),
          "summon ability is available when player 1 has an empty square to the right");
    Piece summonBlocker = profilePiece;
    summonBlocker.id = 42;
    summonBlocker.column = 4;
    check(!pieceAbilityAvailable({profilePiece, summonBlocker}, profilePiece),
          "summon ability is unavailable when the front square is occupied");
    profilePiece.owner = 2;
    profilePiece.column = 3;
    summonBlocker.column = 2;
    check(!pieceAbilityAvailable({profilePiece, summonBlocker}, profilePiece),
          "summon ability checks the left square for player 2");
    profilePiece.ability.clear();
    profilePiece.keywords = {"tRaIl"};
    check(pieceHasTrailAbility(profilePiece) &&
              !pieceAbilityAvailable(profilePiece) &&
              !pieceAbilityAvailable({profilePiece}, profilePiece),
          "Trail is a passive keyword and does not expose an active ability action");
    profilePiece.ability = "dematerialize";
    check(pieceHasTrailAbility(profilePiece) &&
              pieceAbilityAvailable(profilePiece) &&
              pieceAbilityLabel(profilePiece) == "Dematerialize",
          "Trail can coexist with a clearly labeled active ability");
    profilePiece.owner = 1;
    profilePiece.ability = "command";
    profilePiece.keywords.clear();
    Piece commandedPiece = profilePiece;
    commandedPiece.id = 43;
    commandedPiece.ability.clear();
    commandedPiece.column = 4;
    check(pieceAbilityLabel(profilePiece) == "Command",
          "command ability has a descriptive button label");
    check(pieceAbilityAvailable({profilePiece, commandedPiece}, profilePiece),
          "command is available with a ready adjacent friendly piece");
    commandedPiece.hasActed = true;
    check(!pieceAbilityAvailable({profilePiece, commandedPiece}, profilePiece),
          "command rejects an adjacent friendly piece that already acted");
    commandedPiece.hasActed = false;
    commandedPiece.column = 5;
    check(!pieceAbilityAvailable({profilePiece, commandedPiece}, profilePiece),
          "command requires the friendly piece to be adjacent");
    commandedPiece.column = 4;
    commandedPiece.owner = 2;
    check(!pieceAbilityAvailable({profilePiece, commandedPiece}, profilePiece),
          "command cannot activate an adjacent enemy piece");
    profilePiece.ability.clear();

    Piece readyNextTurn = profilePiece;
    readyNextTurn.hasActed = true;
    readyNextTurn.growTurnsRemaining = 1;
    beginPieceTurn(readyNextTurn);
    check(!readyNextTurn.hasActed && readyNextTurn.growTurnsRemaining == 0,
          "a piece finishing growth is ready at the start of its next turn");
    Piece stunnedNextTurn = profilePiece;
    stunnedNextTurn.disabledTurns = 1;
    beginPieceTurn(stunnedNextTurn);
    check(stunnedNextTurn.hasActed && stunnedNextTurn.disabledTurns == 0,
          "a piece whose final disabled turn starts remains stunned for that turn");

    ActionProfile paralyze;
    paralyze.pattern = static_cast<std::uint8_t>(MovePattern::Omni);
    paralyze.maxRange = 2;
    paralyze.canMove = true;
    paralyze.canAttack = true;
    paralyze.statusTurns = 2;
    profilePiece.actions = {paralyze};
    Piece adjacentEnemy = hopTarget;
    adjacentEnemy.row = 4;
    adjacentEnemy.column = 4;
    const std::vector<Piece> paralyzePieces = {profilePiece, adjacentEnemy};
    const ActionResolution paralyzeResult =
        resolvePieceAction(paralyzePieces, holes, paralyzePieces[0], 4, 4);
    check(paralyzeResult.legal && paralyzeResult.attacks && paralyzeResult.statusTurns == 2,
          "status-only attacking movement is legal");

    ActionProfile capture;
    capture.kind = static_cast<std::uint8_t>(ActionKind::Capture);
    capture.pattern = static_cast<std::uint8_t>(MovePattern::Diag);
    capture.maxRange = 1;
    capture.damage = 2;
    capture.canMove = true;
    capture.canAttack = true;
    profilePiece.actions = {capture};
    check(!resolvePieceAction({profilePiece}, holes, profilePiece, 2, 2).legal,
          "capture movement cannot target an empty square");
    const std::vector<Piece> capturePieces = {profilePiece, adjacentEnemy};
    const ActionResolution captureResult =
        resolvePieceAction(capturePieces, holes, capturePieces[0], 4, 4);
    check(captureResult.legal && captureResult.moves && captureResult.attacks &&
              captureResult.targetId == adjacentEnemy.id &&
              captureResult.stagingRow == profilePiece.row &&
              captureResult.stagingColumn == profilePiece.column,
          "capture movement can move and attack only when an enemy occupies the target square");

    ActionProfile passThroughAttack;
    passThroughAttack.pattern = static_cast<std::uint8_t>(MovePattern::Ortho);
    passThroughAttack.minRange = 3;
    passThroughAttack.maxRange = 3;
    passThroughAttack.damage = 1;
    passThroughAttack.canMove = true;
    passThroughAttack.canAttack = true;
    passThroughAttack.passThrough = true;
    profilePiece.actions = {passThroughAttack};
    Piece passThroughBlocker = hopTarget;
    passThroughBlocker.owner = 1;
    passThroughBlocker.row = 3;
    passThroughBlocker.column = 4;
    Piece passThroughTarget = hopTarget;
    passThroughTarget.row = 3;
    passThroughTarget.column = 6;
    const std::vector<Piece> passThroughPieces = {
        profilePiece,
        passThroughBlocker,
        passThroughTarget,
    };
    const ActionResolution passThroughResult =
        resolvePieceAction(passThroughPieces, holes, passThroughPieces[0], 3, 6);
    check(passThroughResult.legal && passThroughResult.moves && passThroughResult.attacks &&
              passThroughResult.stagingRow == profilePiece.row &&
              passThroughResult.stagingColumn == profilePiece.column,
          "failed pass-through attack returns the attacker to its original square");

    ActionProfile stateOne = horizontal;
    stateOne.state = 1;
    profilePiece.actions = {stateOne};
    profilePiece.actionState = 0;
    check(!resolvePieceAction({profilePiece}, holes, profilePiece, 3, 4).legal,
          "inactive transform state actions are unavailable");
    profilePiece.actionState = 1;
    check(resolvePieceAction({profilePiece}, holes, profilePiece, 3, 4).legal,
          "active transform state actions are available");

    Piece gunner;
    gunner.id = 20;
    gunner.owner = 1;
    gunner.row = 3;
    gunner.column = 3;
    ActionProfile loweredGun;
    loweredGun.state = 0;
    loweredGun.pattern = static_cast<std::uint8_t>(MovePattern::Ortho);
    loweredGun.maxRange = 2;
    loweredGun.canMove = true;
    ActionProfile raisedGun;
    raisedGun.state = 1;
    raisedGun.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    raisedGun.pattern = static_cast<std::uint8_t>(MovePattern::Omni);
    raisedGun.damage = 3;
    raisedGun.canMove = false;
    raisedGun.canAttack = true;
    gunner.actions = {loweredGun, raisedGun};
    Piece gunTarget;
    gunTarget.id = 21;
    gunTarget.owner = 2;
    gunTarget.row = 3;
    gunTarget.column = 4;
    const std::vector<Piece> gunPieces = {gunner, gunTarget};
    const ActionResolution loweredMove =
        resolvePieceAction(gunPieces, holes, gunPieces[0], 2, 3);
    const ActionResolution loweredAttack =
        resolvePieceAction(gunPieces, holes, gunPieces[0], 3, 4);
    check(loweredMove.legal && loweredMove.moves && !loweredMove.attacks,
          "lowered gun can move without dealing damage");
    check(!loweredAttack.legal,
          "lowered gun cannot attack");

    std::vector<Piece> raisedGunPieces = gunPieces;
    raisedGunPieces[0].actionState = 1;
    const ActionResolution raisedMove =
        resolvePieceAction(raisedGunPieces, holes, raisedGunPieces[0], 2, 3);
    const ActionResolution raisedAttack =
        resolvePieceAction(raisedGunPieces, holes, raisedGunPieces[0], 3, 4);
    check(!raisedMove.legal,
          "raised gun cannot move");
    check(raisedAttack.legal && !raisedAttack.moves && raisedAttack.attacks &&
              raisedAttack.damage == 3,
          "raised gun can deal damage without moving");

    Piece healer = gunner;
    healer.id = 24;
    healer.owner = 1;
    healer.actionState = 0;
    ActionProfile healingAction = raisedGun;
    healingAction.state = 0;
    healingAction.damage = 0;
    healingAction.heal = 3;
    ActionProfile weakHealingAction = healingAction;
    weakHealingAction.heal = 1;
    healer.actions = {weakHealingAction, healingAction};
    Piece woundedFriendly = gunTarget;
    woundedFriendly.id = 25;
    woundedFriendly.owner = 1;
    woundedFriendly.maxHealth = 5;
    woundedFriendly.health = 4;
    Piece healingEnemy = gunTarget;
    healingEnemy.id = 26;
    healingEnemy.owner = 2;
    healingEnemy.row = 4;
    healingEnemy.column = 3;
    std::vector<Piece> healingPieces = {healer, woundedFriendly, healingEnemy};
    const ActionResolution healingResult =
        resolvePieceAction(healingPieces, holes, healingPieces[0], 3, 4);
    check(healingResult.legal && healingResult.attacks &&
              healingResult.targetId == woundedFriendly.id &&
              healingResult.damage == 0 && healingResult.heal == 3,
          "positive action healing targets a friendly piece");
    check(!resolvePieceAction(healingPieces, holes, healingPieces[0], 4, 3).legal,
          "healing-only actions cannot target an enemy piece");
    applyActionHealing(healingPieces[1], healingResult.heal, healingResult.statusTurns);
    check(healingPieces[1].health == healingPieces[1].maxHealth &&
              healingPieces[1].disabledTurns == 0 && healingPieces[1].sleepTurnsRemaining == 0,
          "positive action healing stops at maximum health without applying damage status");
    check(!resolvePieceAction(healingPieces, holes, healingPieces[0], 3, 4).legal,
          "healing-only actions cannot target a friendly piece at maximum health");

    raisedGun.maxRange = 3;
    raisedGun.lineOfSight = false;
    gunner.actions = {raisedGun};
    gunner.actionState = 1;
    gunTarget.row = 3;
    gunTarget.column = 6;
    Piece friendlyBlocker;
    friendlyBlocker.id = 22;
    friendlyBlocker.owner = 1;
    friendlyBlocker.row = 3;
    friendlyBlocker.column = 4;
    const std::vector<Piece> friendlyBlockedShotPieces = {gunner, friendlyBlocker, gunTarget};
    check(!resolvePieceAction(friendlyBlockedShotPieces, holes, friendlyBlockedShotPieces[0], 3, 6).legal,
          "ranged attacks cannot shoot through friendly pieces");
    Piece enemyBlocker = friendlyBlocker;
    enemyBlocker.id = 23;
    enemyBlocker.owner = 2;
    const std::vector<Piece> enemyBlockedShotPieces = {gunner, enemyBlocker, gunTarget};
    check(!resolvePieceAction(enemyBlockedShotPieces, holes, enemyBlockedShotPieces[0], 3, 6).legal,
          "ranged attacks cannot shoot through enemy pieces");

    std::vector<Piece> sleepingGunPieces = gunPieces;
    sleepingGunPieces[0].sleepTurnsRemaining = 1;
    const ActionResolution sleepingMove =
        resolvePieceAction(sleepingGunPieces, holes, sleepingGunPieces[0], 2, 3);
    sleepingGunPieces[0].actionState = 1;
    const ActionResolution sleepingAttack =
        resolvePieceAction(sleepingGunPieces, holes, sleepingGunPieces[0], 3, 4);
    check(!sleepingMove.legal,
          "sleeping pieces cannot move");
    check(sleepingAttack.legal && !sleepingAttack.moves && sleepingAttack.attacks,
          "sleeping pieces can still make stationary attacks");

    Piece damagedTarget;
    applyDamageStatus(damagedTarget, 2, 0);
    check(damagedTarget.disabledTurns == DamageDisabledTurns && damagedTarget.sleepTurnsRemaining == 1,
          "positive damage applies a one-turn disabled status");
    applyDamageStatus(damagedTarget, 0, 2);
    check(damagedTarget.disabledTurns == 2,
          "explicit status duration can exceed the damage disabled duration");

    Piece protectedPiece;
    protectedPiece.id = 30;
    protectedPiece.owner = 2;
    protectedPiece.row = 3;
    protectedPiece.column = 3;
    protectedPiece.health = 10;
    protectedPiece.maxHealth = 10;
    auto makeBodyguard = [](int id, int owner, int row, int column) {
        Piece bodyguard;
        bodyguard.id = id;
        bodyguard.owner = owner;
        bodyguard.row = row;
        bodyguard.column = column;
        bodyguard.health = 10;
        bodyguard.maxHealth = 10;
        bodyguard.keywords = {"BoDyGuArD"};
        return bodyguard;
    };
    Piece bodyguardA = makeBodyguard(31, 2, 2, 2);
    Piece bodyguardB = makeBodyguard(32, 2, 2, 3);
    Piece bodyguardC = makeBodyguard(33, 2, 2, 4);
    Piece enemyBodyguard = makeBodyguard(34, 1, 3, 4);
    Piece distantBodyguard = makeBodyguard(35, 2, 0, 0);
    std::vector<Piece> bodyguardPieces = {
        protectedPiece,
        bodyguardA,
        bodyguardB,
        bodyguardC,
        enemyBodyguard,
        distantBodyguard};
    std::mt19937 bodyguardRandom(23);
    const std::vector<DamageAssignment> splitAssignments =
        applyDamageWithBodyguards(bodyguardPieces, protectedPiece.id, 5, 0, bodyguardRandom);
    std::vector<int> bodyguardDamage;
    for (int id : {bodyguardA.id, bodyguardB.id, bodyguardC.id})
    {
        const auto found = std::find_if(
            bodyguardPieces.begin(),
            bodyguardPieces.end(),
            [id](const Piece& piece) { return piece.id == id; });
        bodyguardDamage.push_back(10 - found->health);
    }
    std::sort(bodyguardDamage.begin(), bodyguardDamage.end());
    check(splitAssignments.size() == 3 &&
              bodyguardPieces[0].health == 10 &&
              bodyguardDamage == std::vector<int>({1, 2, 2}) &&
              bodyguardPieces[4].health == 10 &&
              bodyguardPieces[5].health == 10,
          "Bodyguard splits damage evenly among adjacent friendly Bodyguards only");

    const std::array<int, 3> healthBeforeBodyguardHit = {
        bodyguardPieces[1].health,
        bodyguardPieces[2].health,
        bodyguardPieces[3].health};
    applyDamageWithBodyguards(
        bodyguardPieces, bodyguardB.id, 5, 0, bodyguardRandom);
    check(bodyguardPieces[1].health == healthBeforeBodyguardHit[0] &&
              bodyguardPieces[2].health == healthBeforeBodyguardHit[1] - 5 &&
              bodyguardPieces[3].health == healthBeforeBodyguardHit[2],
          "damage dealt to a Bodyguard is not redirected to adjacent Bodyguards");

    card_data::Card encodedCard;
    encodedCard.title = "Encoded";
    encodedCard.type = "Unit";
    encodedCard.traits = {"corrupt", "fey"};
    encodedCard.keywords = {"future-rule"};
    encodedCard.integerValues = {{"attack", 9}, {"range", 5}, {"FidgetAnimFrames", 3}, {"Tax", 4}};
    encodedCard.stringValues = {{"FidgetAnim", "animations/fidget/test.png"}};
    encodedCard.actionNames = {"Diagonal Charge"};
    encodedCard.actions.push_back({
        "Diagonal Charge",
        0,
        "slide",
        "diag",
        1,
        7,
        2,
        0,
        true,
        true,
        false,
        false,
        0,
        0,
    });
    const GameCard decodedCard = toGameCard(encodedCard);
    check(decodedCard.actions.size() == 1 &&
              decodedCard.traits == encodedCard.traits &&
              decodedCard.keywords == encodedCard.keywords &&
              decodedCard.attackingMove &&
              decodedCard.actions[0].name == "Diagonal Charge" &&
              decodedCard.actions[0].damage == 2 &&
              decodedCard.actions[0].heal == 0 &&
              decodedCard.actions[0].canMove &&
              decodedCard.actions[0].canAttack &&
              decodedCard.fidgetAnimPath == "animations/fidget/test.png" &&
              decodedCard.fidgetAnimFrames == 3 &&
              decodedCard.tax == 4,
          "referenced action object resolves into gameplay data without a legacy fallback attack");

    card_data::Action encodedHealingAction = encodedCard.actions[0];
    encodedHealingAction.damage = 0;
    encodedHealingAction.heal = 4;
    sf::Packet actionPacket;
    card_data::writeAction(actionPacket, encodedHealingAction);
    card_data::Action roundTrippedAction;
    check(card_data::readAction(actionPacket, roundTrippedAction) &&
              roundTrippedAction.damage == 0 && roundTrippedAction.heal == 4,
          "card-server action serialization keeps healing separate from damage");

    sf::Packet legacyCardListPacket;
    legacyCardListPacket << static_cast<std::uint32_t>(1);
    legacyCardListPacket << std::string("Legacy") << std::string("Unit") << std::string("legacy.png");
    card_data::writeStringVector(
        legacyCardListPacket,
        std::vector<std::string>{"corrupt", "fey"});
    legacyCardListPacket << static_cast<std::uint32_t>(0); // integer fields
    legacyCardListPacket << static_cast<std::uint32_t>(0); // string fields
    legacyCardListPacket << static_cast<std::uint32_t>(0); // string lists
    card_data::writeStringVector(legacyCardListPacket, std::vector<std::string>{}); // action names
    legacyCardListPacket << static_cast<std::uint32_t>(0); // actions
    std::uint32_t legacyCardCount = 0;
    bool legacyCardFormat = false;
    card_data::Card legacyCard;
    check(card_data::readCardListHeader(legacyCardListPacket, legacyCardCount, legacyCardFormat) &&
              legacyCardCount == 1 && legacyCardFormat &&
              card_data::readListedCard(legacyCardListPacket, legacyCard, legacyCardFormat) &&
              legacyCard.traits == std::vector<std::string>({"corrupt", "fey"}) &&
              legacyCard.keywords.empty(),
          "legacy card-server keywords decode as traits");

    sf::Packet currentCardListHeader;
    card_data::writeCardListHeader(currentCardListHeader, 3);
    std::uint32_t currentCardCount = 0;
    bool currentCardFormatIsLegacy = true;
    check(card_data::readCardListHeader(currentCardListHeader, currentCardCount, currentCardFormatIsLegacy) &&
              currentCardCount == 3 && !currentCardFormatIsLegacy,
          "versioned card-list headers select the traits-and-keywords format");

    GameCard serializedCard = decodedCard;
    serializedCard.tokenPath = "characters/test.png";
    serializedCard.walkAnimPath = "animations/test.png";
    serializedCard.idleAnimPath = "animations/idle/test.png";
    serializedCard.attackAnimPath = "animations/attack/test.png";
    serializedCard.damagedAnimPath = "animations/damaged/test.png";
    serializedCard.killedAnimPath = "animations/killed/test.png";
    serializedCard.fidgetAnimPath = "animations/fidget/test.png";
    serializedCard.pieceBaseBluePath = "characters/bases/blue.png";
    serializedCard.pieceBaseRedPath = "characters/bases/red.png";
    serializedCard.walkAnimFrames = 7;
    serializedCard.idleAnimFrames = 5;
    serializedCard.attackAnimFrames = 6;
    serializedCard.damagedAnimFrames = 8;
    serializedCard.killedAnimFrames = 9;
    serializedCard.fidgetAnimFrames = 10;
    serializedCard.ability = "transform";
    serializedCard.summonTitle = "Serialized Summon";
    serializedCard.abilityLabels = {"Ready", "Lower"};
    serializedCard.abilityUses = 2;
    serializedCard.tax = 4;
    serializedCard.actions[0].heal = 3;
    sf::Packet cardPacket;
    writeGameCard(cardPacket, serializedCard);
    GameCard roundTrippedCard;
    check(readGameCard(cardPacket, roundTrippedCard) &&
              roundTrippedCard.actions.size() == 1 &&
              roundTrippedCard.actions[0].name == "Diagonal Charge" &&
              roundTrippedCard.actions[0].damage == 2 &&
              roundTrippedCard.actions[0].heal == 3 &&
              roundTrippedCard.traits == encodedCard.traits &&
              roundTrippedCard.keywords == encodedCard.keywords &&
              roundTrippedCard.tokenPath == "characters/test.png" &&
              roundTrippedCard.walkAnimPath == "animations/test.png" &&
              roundTrippedCard.idleAnimPath == "animations/idle/test.png" &&
              roundTrippedCard.attackAnimPath == "animations/attack/test.png" &&
              roundTrippedCard.damagedAnimPath == "animations/damaged/test.png" &&
              roundTrippedCard.killedAnimPath == "animations/killed/test.png" &&
              roundTrippedCard.fidgetAnimPath == "animations/fidget/test.png" &&
              roundTrippedCard.pieceBaseBluePath == "characters/bases/blue.png" &&
              roundTrippedCard.pieceBaseRedPath == "characters/bases/red.png" &&
              roundTrippedCard.walkAnimFrames == 7 &&
              roundTrippedCard.idleAnimFrames == 5 &&
              roundTrippedCard.attackAnimFrames == 6 &&
              roundTrippedCard.damagedAnimFrames == 8 &&
              roundTrippedCard.killedAnimFrames == 9 &&
              roundTrippedCard.fidgetAnimFrames == 10 &&
              roundTrippedCard.summonTitle == "Serialized Summon" &&
              roundTrippedCard.abilityLabels.size() == 2 &&
              roundTrippedCard.abilityUses == 2 &&
              roundTrippedCard.tax == 4,
          "extended game card fields survive network serialization");

    Piece serializedPiece = profilePiece;
    serializedPiece.ability = "dig";
    serializedPiece.summonTitle = "Serialized Summon";
    serializedPiece.traits = {"corrupt"};
    serializedPiece.keywords = {"future-rule"};
    serializedPiece.tokenPath = "characters/test.png";
    serializedPiece.walkAnimPath = "animations/test.png";
    serializedPiece.idleAnimPath = "animations/idle/test.png";
    serializedPiece.attackAnimPath = "animations/attack/test.png";
    serializedPiece.damagedAnimPath = "animations/damaged/test.png";
    serializedPiece.killedAnimPath = "animations/killed/test.png";
    serializedPiece.fidgetAnimPath = "animations/fidget/test.png";
    serializedPiece.pieceBaseBluePath = "characters/bases/blue.png";
    serializedPiece.pieceBaseRedPath = "characters/bases/red.png";
    serializedPiece.walkAnimFrames = 6;
    serializedPiece.idleAnimFrames = 4;
    serializedPiece.attackAnimFrames = 5;
    serializedPiece.damagedAnimFrames = 7;
    serializedPiece.killedAnimFrames = 8;
    serializedPiece.fidgetAnimFrames = 9;
    serializedPiece.abilityLabels = {"Dig"};
    serializedPiece.abilityUses = 1;
    serializedPiece.growTurnsRemaining = 2;
    serializedPiece.disabledTurns = 1;
    serializedPiece.sleepTurnsRemaining = 1;
    if (!serializedPiece.actions.empty())
    {
        serializedPiece.actions[0].name = "Serialized Action";
        serializedPiece.actions[0].heal = 2;
    }
    sf::Packet piecePacket;
    writePiece(piecePacket, serializedPiece);
    Piece roundTrippedPiece;
    check(readPiece(piecePacket, roundTrippedPiece) &&
              roundTrippedPiece.actions.size() == 1 &&
              roundTrippedPiece.actions[0].name == "Serialized Action" &&
              roundTrippedPiece.actions[0].heal == 2 &&
              roundTrippedPiece.traits == serializedPiece.traits &&
              roundTrippedPiece.keywords == serializedPiece.keywords &&
              roundTrippedPiece.tokenPath == "characters/test.png" &&
              roundTrippedPiece.walkAnimPath == "animations/test.png" &&
              roundTrippedPiece.idleAnimPath == "animations/idle/test.png" &&
              roundTrippedPiece.attackAnimPath == "animations/attack/test.png" &&
              roundTrippedPiece.damagedAnimPath == "animations/damaged/test.png" &&
              roundTrippedPiece.killedAnimPath == "animations/killed/test.png" &&
              roundTrippedPiece.fidgetAnimPath == "animations/fidget/test.png" &&
              roundTrippedPiece.pieceBaseBluePath == "characters/bases/blue.png" &&
              roundTrippedPiece.pieceBaseRedPath == "characters/bases/red.png" &&
              roundTrippedPiece.walkAnimFrames == 6 &&
              roundTrippedPiece.idleAnimFrames == 4 &&
              roundTrippedPiece.attackAnimFrames == 5 &&
              roundTrippedPiece.damagedAnimFrames == 7 &&
              roundTrippedPiece.killedAnimFrames == 8 &&
              roundTrippedPiece.fidgetAnimFrames == 9 &&
              roundTrippedPiece.ability == "dig" &&
              roundTrippedPiece.summonTitle == "Serialized Summon" &&
              roundTrippedPiece.growTurnsRemaining == 2 &&
              roundTrippedPiece.disabledTurns == 1 &&
              roundTrippedPiece.sleepTurnsRemaining == 1,
          "extended piece fields survive network serialization");

    Snapshot serializedSnapshot;
    serializedSnapshot.commandingPieceId = 43;
    serializedSnapshot.relentlessPieceId = 44;
    serializedSnapshot.status = "Command pending";
    sf::Packet snapshotPacket;
    writeSnapshot(snapshotPacket, serializedSnapshot);
    Snapshot roundTrippedSnapshot;
    check(readSnapshot(snapshotPacket, roundTrippedSnapshot) &&
              roundTrippedSnapshot.commandingPieceId == 43 &&
              roundTrippedSnapshot.relentlessPieceId == 44 &&
              roundTrippedSnapshot.status == "Command pending",
          "pending command and Relentless states survive snapshot serialization");

    card_data::Card taxHeroCard;
    taxHeroCard.title = "Tax Hero";
    taxHeroCard.type = "Hero";
    taxHeroCard.integerValues = {{"health", 4}, {"Tax", 2}};
    card_data::Card plainHeroCard;
    plainHeroCard.title = "Plain Hero";
    plainHeroCard.type = "Hero";
    plainHeroCard.integerValues = {{"health", 4}};
    GameEngine taxEngine(17, {taxHeroCard, plainHeroCard});
    taxEngine.submitDeck(1, {taxHeroCard});
    taxEngine.submitDeck(2, {plainHeroCard});
    taxEngine.placeHero(1, 0, homeSquares(1)[0].first, homeSquares(1)[0].second);
    taxEngine.placeHero(2, 0, homeSquares(2)[0].first, homeSquares(2)[0].second);
    const auto taxHero = std::find_if(
        taxEngine.boardPieces().begin(), taxEngine.boardPieces().end(),
        [](const Piece& piece) { return piece.name == "Tax Hero"; });
    check(taxHero != taxEngine.boardPieces().end() && taxHero->tax == 2,
          "Tax card field resolves onto the owned piece");
    taxEngine.endTurn(1);
    const int playerTwoResourcesBeforeTax =
        taxEngine.snapshotFor(2).players[1].resources;
    taxEngine.endTurn(2);
    const Snapshot taxedSnapshot = taxEngine.snapshotFor(1);
    check(playerTwoResourcesBeforeTax > 0 &&
              taxedSnapshot.players[1].resources == playerTwoResourcesBeforeTax - 2 &&
              taxedSnapshot.players[0].resources >= 2,
          "Tax transfers up to its amount from the opponent to the owner each turn");

    card_data::Card trailHeroCard;
    trailHeroCard.title = "Trail Hero";
    trailHeroCard.type = "Hero";
    trailHeroCard.integerValues = {{"health", 5}};
    trailHeroCard.keywords = {"Trail"};
    trailHeroCard.stringValues = {{"ability", "dematerialize"}, {"summon", "Seedling"}};
    card_data::Action trailStep;
    trailStep.name = "Trail Step";
    trailStep.kind = "slide";
    trailStep.pattern = "omni";
    trailStep.minRange = 1;
    trailStep.maxRange = 1;
    trailStep.canMove = true;
    card_data::Action trailShot;
    trailShot.name = "Trail Shot";
    trailShot.kind = "ranged";
    trailShot.pattern = "none";
    trailShot.minRange = 1;
    trailShot.maxRange = 7;
    trailShot.damage = 1;
    trailShot.canMove = false;
    trailShot.canAttack = true;
    trailHeroCard.actions = {trailStep, trailShot};

    card_data::Card seedlingCard;
    seedlingCard.title = "Seedling";
    seedlingCard.type = "Unit";
    seedlingCard.integerValues = {{"health", 1}, {"move", 0}, {"canControl", 0}};
    card_data::Card trailEnemyCard;
    trailEnemyCard.title = "Trail Enemy";
    trailEnemyCard.type = "Hero";
    trailEnemyCard.integerValues = {{"health", 5}};

    GameEngine trailEngine(23, {trailHeroCard, seedlingCard, trailEnemyCard});
    trailEngine.submitDeck(1, {trailHeroCard});
    trailEngine.submitDeck(2, {trailEnemyCard});
    const auto trailOrigin = homeSquares(1)[0];
    const auto trailEnemyOrigin = homeSquares(2)[0];
    trailEngine.placeHero(1, 0, trailOrigin.first, trailOrigin.second);
    trailEngine.placeHero(2, 0, trailEnemyOrigin.first, trailEnemyOrigin.second);
    const int trailHeroId = trailEngine.boardPieces().front().id;
    trailEngine.movePiece(1, trailHeroId, trailOrigin.first, trailOrigin.second + 1);
    const auto trailSeed = std::find_if(
        trailEngine.boardPieces().begin(), trailEngine.boardPieces().end(),
        [](const Piece& piece) { return piece.name == "Seedling"; });
    const auto movedTrailHero = std::find_if(
        trailEngine.boardPieces().begin(), trailEngine.boardPieces().end(),
        [](const Piece& piece) { return piece.name == "Trail Hero"; });
    check(trailSeed != trailEngine.boardPieces().end() &&
              trailSeed->owner == 1 && trailSeed->row == trailOrigin.first &&
              trailSeed->column == trailOrigin.second && trailSeed->hasActed &&
              movedTrailHero != trailEngine.boardPieces().end() &&
              movedTrailHero->column == trailOrigin.second + 1,
          "trail summons its configured unit at the mover's former position");

    trailEngine.endTurn(2);
    trailEngine.attackPiece(
        1, trailHeroId, trailEnemyOrigin.first, trailEnemyOrigin.second);
    const int seedlingCount = static_cast<int>(std::count_if(
        trailEngine.boardPieces().begin(), trailEngine.boardPieces().end(),
        [](const Piece& piece) { return piece.name == "Seedling"; }));
    check(seedlingCount == 1,
          "trail does not summon when an action attacks without moving the piece");

    auto commandTestHero = [](const std::string& title, bool commands) {
        card_data::Card card;
        card.title = title;
        card.type = "Hero";
        card.integerValues = {{"health", 5}};
        if (commands)
        {
            card.stringValues = {{"ability", "command"}};
        }
        card_data::Action step;
        step.name = title + " Step";
        step.kind = "slide";
        step.pattern = "omni";
        step.minRange = 1;
        step.maxRange = 7;
        step.damage = 1;
        step.canMove = true;
        step.canAttack = true;
        card.actions = {step};
        return card;
    };

    const card_data::Card commanderCard = commandTestHero("Test Commander", true);
    const card_data::Card commandedCard = commandTestHero("Commanded Friend", false);
    const card_data::Card normalCard = commandTestHero("Normal Friend", false);
    const card_data::Card enemyCard = commandTestHero("Enemy Hero", false);
    const std::vector<card_data::Card> commandLibrary = {
        commanderCard, commandedCard, normalCard, enemyCard};
    GameEngine commandEngine(7, commandLibrary);
    commandEngine.submitDeck(1, {commanderCard, commandedCard, normalCard});
    commandEngine.submitDeck(2, {enemyCard});
    const auto player1Home = homeSquares(1);
    const auto player2Home = homeSquares(2);
    commandEngine.placeHero(1, 0, player1Home[0].first, player1Home[0].second);
    commandEngine.placeHero(1, 0, player1Home[1].first, player1Home[1].second);
    commandEngine.placeHero(1, 0, player1Home[2].first, player1Home[2].second);
    commandEngine.placeHero(2, 0, player2Home[1].first, player2Home[1].second);

    auto boardPieceNamed = [&](const std::string& name) -> const Piece* {
        const auto found = std::find_if(
            commandEngine.boardPieces().begin(),
            commandEngine.boardPieces().end(),
            [&](const Piece& piece) { return piece.name == name; });
        return found == commandEngine.boardPieces().end() ? nullptr : &*found;
    };
    const Piece* testCommander = boardPieceNamed("Test Commander");
    const Piece* testCommanded = boardPieceNamed("Commanded Friend");
    const Piece* testNormal = boardPieceNamed("Normal Friend");
    check(testCommander && testCommanded && testNormal,
          "command engine test placed its friendly pieces");
    if (testCommander && testCommanded && testNormal)
    {
        const int commanderId = testCommander->id;
        const int commandedId = testCommanded->id;
        const int normalId = testNormal->id;
        commandEngine.useAbility(1, commanderId);
        check(commandEngine.currentPlayer() == 1 && commandEngine.commandingPiece() == commanderId,
              "using Command keeps the turn and waits for an adjacent friendly action");
        const Piece* testEnemyBefore = boardPieceNamed("Enemy Hero");
        const int enemyHealthBefore = testEnemyBefore ? testEnemyBefore->health : 0;
        commandEngine.attackPiece(1, commandedId, player2Home[1].first, player2Home[1].second);
        const Piece* movedCommandTarget = boardPieceNamed("Commanded Friend");
        const Piece* testEnemyAfter = boardPieceNamed("Enemy Hero");
        check(commandEngine.currentPlayer() == 1 && commandEngine.commandingPiece() == 0 &&
                  movedCommandTarget && movedCommandTarget->hasActed &&
                  testEnemyAfter && testEnemyAfter->health == enemyHealthBefore - 1,
              "the commanded friendly piece attacks without ending the turn");
        commandEngine.movePiece(1, normalId, player1Home[2].first, 1);
        check(commandEngine.currentPlayer() == 2,
              "a normal move after the commanded action ends the turn");
    }

    auto relentlessTestHero = [](const std::string& title, bool relentless, bool canDig) {
        card_data::Card card;
        card.title = title;
        card.type = "Hero";
        card.integerValues = {{"health", 3}};
        if (relentless)
        {
            card.keywords = {"ReLeNtLeSs"};
        }
        if (canDig)
        {
            card.stringValues = {{"ability", "dig"}};
            card.integerValues.push_back({"abilityUses", 1});
        }

        card_data::Action step;
        step.name = title + " Step";
        step.kind = "slide";
        step.pattern = "omni";
        step.maxRange = 1;
        step.canMove = true;

        card_data::Action shot;
        shot.name = title + " Shot";
        shot.kind = "ranged";
        shot.pattern = "none";
        shot.maxRange = 7;
        shot.damage = 3;
        shot.canMove = false;
        shot.canAttack = true;
        card.actions = {step, shot};
        return card;
    };

    const card_data::Card relentlessCard =
        relentlessTestHero("Relentless Hero", true, true);
    const card_data::Card relentlessAllyCard =
        relentlessTestHero("Relentless Ally", false, false);
    const card_data::Card relentlessVictimA =
        relentlessTestHero("Relentless Victim A", false, false);
    const card_data::Card relentlessVictimB =
        relentlessTestHero("Relentless Victim B", false, false);
    const card_data::Card relentlessSurvivor =
        relentlessTestHero("Relentless Survivor", false, false);
    const std::vector<card_data::Card> relentlessLibrary = {
        relentlessCard,
        relentlessAllyCard,
        relentlessVictimA,
        relentlessVictimB,
        relentlessSurvivor};
    GameEngine relentlessEngine(11, relentlessLibrary);
    relentlessEngine.submitDeck(1, {relentlessCard, relentlessAllyCard});
    relentlessEngine.submitDeck(2, {
        relentlessVictimA,
        relentlessVictimB,
        relentlessSurvivor});
    relentlessEngine.placeHero(1, 0, player1Home[0].first, player1Home[0].second);
    relentlessEngine.placeHero(1, 0, player1Home[1].first, player1Home[1].second);
    relentlessEngine.placeHero(2, 0, player2Home[0].first, player2Home[0].second);
    relentlessEngine.placeHero(2, 0, player2Home[1].first, player2Home[1].second);
    relentlessEngine.placeHero(2, 0, player2Home[2].first, player2Home[2].second);

    auto relentlessPieceNamed = [&](const GameEngine& engine, const std::string& name) -> const Piece* {
        const auto found = std::find_if(
            engine.boardPieces().begin(),
            engine.boardPieces().end(),
            [&](const Piece& piece) { return piece.name == name; });
        return found == engine.boardPieces().end() ? nullptr : &*found;
    };
    const Piece* relentlessAttacker = relentlessPieceNamed(relentlessEngine, "Relentless Hero");
    const Piece* relentlessAlly = relentlessPieceNamed(relentlessEngine, "Relentless Ally");
    check(relentlessAttacker && relentlessAlly,
          "Relentless engine test placed its friendly pieces");
    if (relentlessAttacker && relentlessAlly)
    {
        const int attackerId = relentlessAttacker->id;
        const int allyId = relentlessAlly->id;
        relentlessEngine.attackPiece(
            1, attackerId, player2Home[0].first, player2Home[0].second);
        const Piece* readyRelentless = relentlessPieceNamed(relentlessEngine, "Relentless Hero");
        check(relentlessEngine.currentPlayer() == 1 &&
                  relentlessEngine.relentlessPiece() == attackerId &&
                  readyRelentless && !readyRelentless->hasActed &&
                  relentlessPieceNamed(relentlessEngine, "Relentless Victim A") == nullptr,
              "a case-insensitive Relentless kill readies that piece without ending the turn");

        GameEngine relentlessMoveBranch = relentlessEngine;
        relentlessMoveBranch.movePiece(
            1, attackerId, player1Home[0].first, player1Home[0].second + 1);
        const Piece* movedRelentless =
            relentlessPieceNamed(relentlessMoveBranch, "Relentless Hero");
        check(relentlessMoveBranch.currentPlayer() == 2 &&
                  relentlessMoveBranch.relentlessPiece() == 0 &&
                  movedRelentless && movedRelentless->column == 1,
              "the immediate Relentless action may be a normal move and then ends the turn");

        relentlessEngine.movePiece(
            1, allyId, player1Home[1].first, player1Home[1].second + 1);
        const Piece* blockedAlly = relentlessPieceNamed(relentlessEngine, "Relentless Ally");
        check(relentlessEngine.currentPlayer() == 1 &&
                  relentlessEngine.relentlessPiece() == attackerId &&
                  blockedAlly && blockedAlly->column == player1Home[1].second,
              "another piece cannot consume a pending Relentless action");

        relentlessEngine.attackPiece(
            1, attackerId, player2Home[1].first, player2Home[1].second);
        check(relentlessEngine.currentPlayer() == 1 &&
                  relentlessEngine.relentlessPiece() == attackerId &&
                  relentlessPieceNamed(relentlessEngine, "Relentless Victim B") == nullptr,
              "a second Relentless kill chains another immediate action");

        relentlessEngine.useAbility(1, attackerId);
        check(relentlessEngine.currentPlayer() == 2 &&
                  relentlessEngine.relentlessPiece() == 0 &&
                  relentlessEngine.boardHoles()[static_cast<std::size_t>(squareIndex(
                      player1Home[0].first,
                      player1Home[0].second))] != 0,
              "the immediate Relentless action may use an ability and then ends the turn");
    }

    Piece corruptHero;
    corruptHero.owner = 1;
    corruptHero.isHero = true;
    corruptHero.traits = {"corrupt"};
    Piece feyHero;
    feyHero.owner = 1;
    feyHero.isHero = true;
    feyHero.traits = {"fey"};
    Piece enemyHero;
    enemyHero.owner = 2;
    enemyHero.isHero = true;
    enemyHero.traits = {"fey"};
    GameCard unrestrictedCard;
    unrestrictedCard.type = "Unit";
    GameCard corruptFeyCard;
    corruptFeyCard.type = "Unit";
    corruptFeyCard.traits = {"corrupt", "fey"};
    check(heroTraitsAllowCard({corruptHero}, 1, unrestrictedCard),
          "units without traits need no hero trait");
    check(!heroTraitsAllowCard({corruptHero, enemyHero}, 1, corruptFeyCard),
          "enemy heroes cannot supply a card trait");
    check(heroTraitsAllowCard({corruptHero, feyHero}, 1, corruptFeyCard),
          "multiple friendly heroes can collectively supply all card traits");
    check(!heroTraitsAllowCard({corruptHero}, 1, corruptFeyCard),
          "a unit becomes unavailable when a required hero trait is no longer supplied");
    GameCard traitSpell;
    traitSpell.type = "Spell";
    traitSpell.traits = {"ancient"};
    check(heroTraitsAllowCard({}, 1, traitSpell),
          "spells do not require matching hero traits");
    GameCard keywordUnit;
    keywordUnit.type = "Unit";
    keywordUnit.keywords = {"corrupt"};
    check(heroTraitsAllowCard({}, 1, keywordUnit),
          "keywords do not gate unit play");
    check(CardTraitLabels.size() == 9,
          "the card model exposes nine supported traits");

    const auto [equalWinnerRating, equalLoserRating] =
        ranking::ratingsAfterMatch(0, 0, 1);
    check(equalWinnerRating == 16 && equalLoserRating == 0,
          "equal zero-rated players update to 16 and 0");
    const ranking::MatchRewards selfMatchRewards =
        ranking::rewardsAfterMatch(250, 250, 1, true, 10);
    check(
        selfMatchRewards.ratings[0] == 250 &&
            selfMatchRewards.ratings[1] == 250 &&
            selfMatchRewards.ratingChanges[0] == 0 &&
            selfMatchRewards.ratingChanges[1] == 0,
        "self-match produces zero rating change");
    check(selfMatchRewards.winnerCoins == 0,
          "self-match produces zero gold");
    check(ranking::matchmakingRange(std::chrono::seconds(0)) == 10,
          "matchmaking starts at +/- 10");
    check(ranking::matchmakingRange(std::chrono::seconds(30)) == 640,
          "matchmaking reaches +/- 640 after 30 seconds");

    // --- dematerialized piece interception ---------------------------------
    std::array<std::uint8_t, BoardSquares> flatBoard{};
    Piece charger;
    charger.id = 30;
    charger.owner = 1;
    charger.row = 3;
    charger.column = 1;
    ActionProfile chargeAttack;
    chargeAttack.pattern = static_cast<std::uint8_t>(MovePattern::Ortho);
    chargeAttack.maxRange = 6;
    chargeAttack.damage = 2;
    chargeAttack.canMove = true;
    chargeAttack.canAttack = true;
    charger.actions = {chargeAttack};

    Piece lurker;
    lurker.id = 31;
    lurker.owner = 2;
    lurker.row = 3;
    lurker.column = 4;
    lurker.hidden = true;
    lurker.actionState = 1;
    lurker.health = 5;

    const std::vector<Piece> hiddenPieces = {charger, lurker};
    check(piecesVisibleTo(hiddenPieces, 1).size() == 1 &&
              piecesVisibleTo(hiddenPieces, 2).size() == 2,
          "hidden pieces are filtered from the opponent's view but not their owner's");
    check(resolvePieceActionThroughHidden(hiddenPieces, flatBoard, hiddenPieces[0], 3, 6)
              .action.legal,
          "moves through a hidden piece resolve as if the square were empty");

    const PieceActionOutcome hiddenStrike =
        resolvePieceActionThroughHidden(hiddenPieces, flatBoard, hiddenPieces[0], 3, 6);
    check(hiddenStrike.action.legal && hiddenStrike.action.attacks &&
              hiddenStrike.action.targetId == lurker.id &&
              hiddenStrike.revealedPieceId == lurker.id &&
              hiddenStrike.destinationRow == 3 && hiddenStrike.destinationColumn == 4 &&
              hiddenStrike.action.stagingRow == 3 && hiddenStrike.action.stagingColumn == 3,
          "attacking move through a hidden piece strikes it and stages just short of it");

    Piece walker = charger;
    walker.actions[0].damage = 0;
    walker.actions[0].canAttack = false;
    const std::vector<Piece> walkerPieces = {walker, lurker};
    const PieceActionOutcome hiddenBump =
        resolvePieceActionThroughHidden(walkerPieces, flatBoard, walkerPieces[0], 3, 6);
    check(hiddenBump.action.legal && !hiddenBump.action.attacks && hiddenBump.action.moves &&
              hiddenBump.revealedPieceId == lurker.id &&
              hiddenBump.destinationRow == 3 && hiddenBump.destinationColumn == 3,
          "non-attacking move halts just short of a hidden piece without damage");

    Piece hopper;
    hopper.id = 32;
    hopper.owner = 1;
    hopper.row = 3;
    hopper.column = 3;
    ActionProfile vault;
    vault.kind = static_cast<std::uint8_t>(ActionKind::Hop);
    vault.damage = 1;
    vault.canMove = true;
    vault.canAttack = true;
    hopper.actions = {vault};
    Piece pivot;
    pivot.id = 33;
    pivot.owner = 2;
    pivot.row = 3;
    pivot.column = 4;
    Piece landingLurker = lurker;
    landingLurker.id = 34;
    landingLurker.row = 3;
    landingLurker.column = 5;
    const std::vector<Piece> hopHiddenPieces = {hopper, pivot, landingLurker};
    const PieceActionOutcome hopBlocked =
        resolvePieceActionThroughHidden(hopHiddenPieces, flatBoard, hopHiddenPieces[0], 3, 5);
    check(hopBlocked.action.legal && !hopBlocked.action.attacks && !hopBlocked.action.moves &&
              hopBlocked.revealedPieceId == landingLurker.id &&
              hopBlocked.destinationRow == 3 && hopBlocked.destinationColumn == 3,
          "hop onto a hidden landing square fails without damaging the hopped piece");

    Piece knight;
    knight.id = 35;
    knight.owner = 1;
    knight.row = 3;
    knight.column = 3;
    ActionProfile knightMove;
    knightMove.pattern = static_cast<std::uint8_t>(MovePattern::Jump);
    knightMove.minRange = 1;
    knightMove.maxRange = 2;
    knightMove.damage = 1;
    knightMove.canMove = true;
    knightMove.canAttack = true;
    knight.actions = {knightMove};
    Piece jumpLurker = lurker;
    jumpLurker.id = 36;
    jumpLurker.row = 4;
    jumpLurker.column = 5;
    const std::vector<Piece> jumpPieces = {knight, jumpLurker};
    const PieceActionOutcome jumpStrike =
        resolvePieceActionThroughHidden(jumpPieces, flatBoard, jumpPieces[0], 4, 5);
    check(jumpStrike.action.legal && jumpStrike.action.attacks && jumpStrike.action.moves &&
              jumpStrike.action.targetId == jumpLurker.id &&
              jumpStrike.destinationRow == 4 && jumpStrike.destinationColumn == 5 &&
              jumpStrike.action.stagingRow == 3 && jumpStrike.action.stagingColumn == 3,
          "attacking jump onto a hidden piece strikes it and stays put unless it dies");

    check(!pieceExertsControl(lurker),
          "dematerialized pieces exert no square control");
    Piece materializedLurker = lurker;
    materializedLurker.hidden = false;
    check(pieceExertsControl(materializedLurker),
          "materialized pieces control squares normally");

    Piece revealedPiece = lurker;
    materializeRevealedPiece(revealedPiece);
    check(!revealedPiece.hidden && revealedPiece.actionState == 0 &&
              revealedPiece.disabledTurns >= HiddenRevealStunTurns,
          "revealed pieces materialize into action state zero, stunned");

    if (argc == 2 && std::string(argv[1]) == "--movement-only")
    {
        return failures == 0 ? 0 : 1;
    }

    // --- matchmaking -------------------------------------------------------
    const char* testPassword = std::getenv("BAYOU_TEST_PASSWORD");
    if (testPassword == nullptr)
    {
        testPassword = std::getenv("BAYOU_SEED_PASSWORD");
    }
    if (testPassword == nullptr)
    {
        fmt::println("Set BAYOU_TEST_PASSWORD or BAYOU_SEED_PASSWORD for the end-to-end test.");
        return 1;
    }
    const std::string tokenA = loginForTest("alpha", testPassword);
    const std::string tokenB = loginForTest("bravo", testPassword);
    check(!tokenA.empty() && !tokenB.empty(), "test players authenticated");
    if (tokenA.empty() || tokenB.empty())
    {
        return 1;
    }

    bayou::tls::Socket mmA;
    bayou::tls::Socket mmB;
    if (!connectWithRetry(mmA, MatchmakingPort, 20))
    {
        fmt::println("Could not reach matchmaking server on {}. Is it running?", MatchmakingPort);
        return 1;
    }
    sf::Packet joinA;
    joinA << static_cast<std::uint8_t>(MessageType::JoinMatchmaking) << tokenA;
    send(mmA, joinA);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    if (!connectWithRetry(mmB, MatchmakingPort, 20))
    {
        fmt::println("Second player could not reach matchmaking");
        return 1;
    }
    sf::Packet joinB;
    joinB << static_cast<std::uint8_t>(MessageType::JoinMatchmaking) << tokenB;
    send(mmB, joinB);

    auto readMatch = [](bayou::tls::Socket& socket, int& matchId, int& playerNumber, unsigned short& gamePort) -> bool {
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
    bayou::tls::Socket gameA;
    bayou::tls::Socket gameB;
    check(connectWithRetry(gameA, portA, 40), "player A connected to game");
    check(connectWithRetry(gameB, portB, 40), "player B connected to game");

    // The game process only sends GameReady once both players have joined, so
    // send both JoinGame messages before blocking on either response.
    auto sendJoin = [](bayou::tls::Socket& socket, int matchId, int playerNumber, const std::string& token) {
        sf::Packet join;
        join << static_cast<std::uint8_t>(MessageType::JoinGame)
             << matchId << playerNumber << token;
        send(socket, join);
    };
    auto readReady = [](bayou::tls::Socket& socket) -> bool {
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

    sendJoin(gameA, matchA, pnumA, tokenA);
    sendJoin(gameB, matchB, pnumB, tokenB);
    check(readReady(gameA), "player A game ready");
    check(readReady(gameB), "player B game ready");

    // Determine which socket is player 1 / player 2.
    bayou::tls::Socket& p1 = (pnumA == 1) ? gameA : gameB;
    bayou::tls::Socket& p2 = (pnumA == 1) ? gameB : gameA;

    // --- submit decks ------------------------------------------------------
    auto submitDeck = [](bayou::tls::Socket& socket, const std::vector<card_data::Card>& deck) {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(MessageType::SubmitDeck);
        packet << static_cast<std::uint32_t>(deck.size());
        for (const card_data::Card& card : deck)
        {
            card_data::writeCard(packet, card);
        }
        send(socket, packet);
    };
    // The game server only accepts decks whose titles resolve against its
    // authoritative card catalog and that the account actually owns, so submit
    // each player's saved starter deck.
    const std::vector<card_data::Card> deck1 = fetchDeckCards(pnumA == 1 ? tokenA : tokenB);
    const std::vector<card_data::Card> deck2 = fetchDeckCards(pnumA == 1 ? tokenB : tokenA);
    check(!deck1.empty() && !deck2.empty(), "fetched saved decks for both players");
    if (deck1.empty() || deck2.empty())
    {
        return 1;
    }
    submitDeck(p1, deck1);
    submitDeck(p2, deck2);

    gameA.setBlocking(false);
    gameB.setBlocking(false);

    Snapshot s1;
    Snapshot s2;
    settle(p1, p2, s1, s2, 1000);
    check(s1.phase == static_cast<std::uint8_t>(Phase::HeroPlacement), "phase is HeroPlacement after decks");
    const int heroCount1 = s1.players[0].heroesToPlace;
    const int heroCount2 = s1.players[1].heroesToPlace;
    check(heroCount1 >= MinHeroes && heroCount1 <= MaxHeroes, "player 1 hero count obeys the deck rules");
    check(heroCount2 >= MinHeroes && heroCount2 <= MaxHeroes, "player 2 hero count obeys the deck rules");
    check(static_cast<int>(s1.hand.size()) == heroCount1, "player 1 sees hero cards in hand during placement");
    check(static_cast<int>(s2.hand.size()) == heroCount2, "player 2 sees hero cards in hand during placement");
    bool placementHandAllHeroes = !s1.hand.empty();
    for (const GameCard& card : s1.hand)
    {
        placementHandAllHeroes = placementHandAllHeroes && card.type == "Hero";
    }
    check(placementHandAllHeroes, "placement hand contains only heroes");

    // Normal turn actions cannot start or mutate the placement hand.
    sendEndTurn(p1);
    sendPlayCard(p1, 0, 2, 0);
    sendDiscardCard(p1, 0);
    settle(p1, p2, s1, s2, 400);
    check(s1.phase == static_cast<std::uint8_t>(Phase::HeroPlacement), "turn actions cannot end hero placement");
    check(s1.players[0].heroesToPlace == heroCount1 && static_cast<int>(s1.hand.size()) == heroCount1,
          "heroes cannot be played or discarded during placement");

    // --- hero placement ----------------------------------------------------
    // Player 1 home column 0 middle rows; player 2 home column 7 middle rows.
    const auto p1Home = homeSquares(1);
    const auto p2Home = homeSquares(2);
    const int firstPlacementIndex = heroCount1 > 1 ? 1 : 0;
    const std::string firstPlacedTitle =
        s1.hand[static_cast<std::size_t>(firstPlacementIndex)].title;
    sendPlaceHero(p1, p1Home[0].first, p1Home[0].second, firstPlacementIndex);
    settle(p1, p2, s1, s2, 400);
    const Piece* firstPlacedHero = findPieceAt(s1.pieces, p1Home[0].first, p1Home[0].second);
    check(firstPlacedHero && firstPlacedHero->name == firstPlacedTitle,
          "placing a hero uses the selected hand index");
    check(findPieceAt(s2.pieces, p1Home[0].first, p1Home[0].second) == nullptr,
          "opponent cannot see a hero placed during setup");
    check(static_cast<int>(s1.hand.size()) == heroCount1 - 1,
          "placed hero is removed from the placement hand");
    for (int i = 1; i < heroCount1; ++i)
    {
        const auto& [row, column] = p1Home[static_cast<std::size_t>(i)];
        sendPlaceHero(p1, row, column);
    }
    sendPlaceHero(p2, p2Home[0].first, p2Home[0].second);
    settle(p1, p2, s1, s2, 400);
    check(findPieceAt(s1.pieces, p2Home[0].first, p2Home[0].second) == nullptr,
          "player 1 cannot see player 2's placement before setup finishes");
    bool opponentSeesP1Setup = false;
    for (int i = 0; i < heroCount1; ++i)
    {
        const auto& [row, column] = p1Home[static_cast<std::size_t>(i)];
        opponentSeesP1Setup = opponentSeesP1Setup || findPieceAt(s2.pieces, row, column) != nullptr;
    }
    check(!opponentSeesP1Setup,
          "player 2 cannot see player 1's completed placement while still placing");
    check(findPieceAt(s2.pieces, p2Home[0].first, p2Home[0].second) != nullptr,
          "players can see their own placed heroes during setup");
    for (int i = 1; i < heroCount2; ++i)
    {
        const auto& [row, column] = p2Home[static_cast<std::size_t>(i)];
        sendPlaceHero(p2, row, column);
    }
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
    check(p1Heroes == heroCount1 && p2Heroes == heroCount2, "all heroes are on the board");
    int visibleToPlayer2 = 0;
    for (const Piece& piece : s2.pieces)
    {
        if (piece.isHero)
        {
            ++visibleToPlayer2;
        }
    }
    check(visibleToPlayer2 == heroCount1 + heroCount2, "both placements are revealed when setup finishes");
    check(s1.players[0].resources == s1.players[0].controlledSquares, "player 1 Resources equals controlled squares");
    check(s1.players[0].handCount >= StartingHandSize, "player 1 drew an opening hand");

    // --- player 1 deploys a unit ------------------------------------------
    // Resources accrue every turn, so pass turns until the cheapest unit in
    // player 1's hand is affordable.
    int unitIndex = -1;
    int unitCost = 0;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        unitIndex = -1;
        for (std::size_t i = 0; i < s1.hand.size(); ++i)
        {
            if (s1.hand[i].type == "Unit" && (unitIndex < 0 || s1.hand[i].cost < unitCost))
            {
                unitIndex = static_cast<int>(i);
                unitCost = s1.hand[i].cost;
            }
        }
        if (unitIndex >= 0 && unitCost <= s1.players[0].resources)
        {
            break;
        }
        sendEndTurn(p1);
        settle(p1, p2, s1, s2, 400);
        sendEndTurn(p2);
        settle(p1, p2, s1, s2, 400);
    }
    const bool unitAffordable = unitIndex >= 0 && unitCost <= s1.players[0].resources;
    check(unitAffordable, "player 1 has an affordable unit card in hand");

    const int p1ControlBefore = s1.players[0].controlledSquares;
    const int p1ResourcesBefore = s1.players[0].resources;
    const int p1PiecesBefore = static_cast<int>(s1.pieces.size());

    // An empty controlled square to deploy the unit onto. Find one.
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

    if (deployRow >= 0 && unitAffordable)
    {
        const int p2ResourcesBefore = s2.players[1].resources;
        sendPlayCard(p1, unitIndex, deployRow, deployCol);
        settle(p1, p2, s1, s2, 800);
        check(static_cast<int>(s1.pieces.size()) == p1PiecesBefore + 1, "deploying a unit added a piece");
        check(s1.players[0].resources == p1ResourcesBefore - unitCost, "deploying spent Resources");
        check(s1.activePlayer == 2, "playing a card immediately ended player 1's turn");
        check(s2.activePlayer == 2, "turn passed to player 2 after player 1 played a card");
        check(s2.players[1].resources == p2ResourcesBefore + s2.players[1].controlledSquares,
              "player 2 gained Resources on its turn");
    }

    // Player 1's extra piece should have expanded or maintained its territory.
    check(s1.players[0].controlledSquares >= p1ControlBefore, "player 1 territory did not shrink after deploying");

    if (s2.activePlayer == 2 && !s2.hand.empty())
    {
        const int p2ResourcesBeforeDiscard = s2.players[1].resources;
        const int p2HandBeforeDiscard = s2.players[1].handCount;
        const int p2DrawPileBeforeDiscard = s2.players[1].drawPileCount;

        sendDiscardCard(p2, 0);
        settle(p1, p2, s1, s2, 800);
        check(s2.activePlayer == 2, "discarding a card does not end the turn");
        check(s2.players[1].resources == p2ResourcesBeforeDiscard, "discarding a card grants no Resources");
        check(s2.players[1].handCount == p2HandBeforeDiscard - 1,
              "discarding removes the card from hand");
        check(s2.players[1].drawPileCount == p2DrawPileBeforeDiscard + 1,
              "discarded card returns to the draw pile");
        check(s2.players[1].discardsThisTurn == 1, "discard count is tracked for the active turn");

        const int p2HandAfterDiscard = s2.players[1].handCount;
        const int p2DrawPileAfterDiscard = s2.players[1].drawPileCount;
        sendDiscardCard(p2, 0);
        settle(p1, p2, s1, s2, 800);
        check(s2.players[1].handCount == p2HandAfterDiscard &&
                  s2.players[1].drawPileCount == p2DrawPileAfterDiscard,
              "a second discard in the same turn is rejected");
    }

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
