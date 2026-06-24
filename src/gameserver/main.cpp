#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <random>
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

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include "../shared/network.hpp"

using namespace network;
using namespace game_data;

namespace
{
constexpr unsigned short GameServerPort = 55002;
constexpr unsigned short FirstGamePort = 56000;
constexpr unsigned short AccountServerPort = 55000;
constexpr int AiPlayerNumber = 2;
constexpr int AiSearchPlies = 2;
constexpr int AiMaxCandidateActions = 80;
constexpr int StarterNonHeroKinds = game_data::DeckCardCount / game_data::MaxCardCopies;
constexpr const char* PreferredStarterHero = "Steam Baron";
constexpr const char* AiOpponentName = "Bayou Automaton";

struct JoinedPlayer
{
    std::unique_ptr<sf::TcpSocket> socket;
    int playerNumber = 0;
    std::string username;
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
    sf::TcpSocket socket;
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

MatchResult submitRankedResult(int matchId, const std::string& resultToken, int winner)
{
    sf::TcpSocket socket;
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

bool spawnGameProcess(
    int matchId,
    unsigned short port,
    const std::string& playerOne,
    const std::string& playerTwo,
    const std::string& resultToken,
    bool aiOpponent)
{
#ifdef _WIN32
    std::string command = aiOpponent
        ? fmt::format(
            "\"{}\" --game-ai {} {} {}",
            executablePath(),
            matchId,
            port,
            playerOne)
        : fmt::format(
            "\"{}\" --game {} {} {} {} {}",
            executablePath(),
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
            "{} --game-ai {} {} {} &",
            executablePath(),
            matchId,
            port,
            playerOne)
        : fmt::format(
            "{} --game {} {} {} {} {} &",
            executablePath(),
            matchId,
            port,
            playerOne,
            playerTwo,
            resultToken);
    return std::system(command.c_str()) == 0;
#endif
}

card_data::Action actionFromQuery(SQLite::Statement& query)
{
    card_data::Action action;
    action.name = query.getColumn(0).getString();
    action.state = query.getColumn(1).getInt();
    action.kind = query.getColumn(2).getString();
    action.pattern = query.getColumn(3).getString();
    action.minRange = query.getColumn(4).getInt();
    action.maxRange = query.getColumn(5).getInt();
    action.damage = query.getColumn(6).getInt();
    action.canMove = query.getColumn(7).getInt() != 0;
    action.canAttack = query.getColumn(8).getInt() != 0;
    action.passThrough = query.getColumn(9).getInt() != 0;
    action.lineOfSight = query.getColumn(10).getInt() != 0;
    action.statusTurns = query.getColumn(11).getInt();
    action.cooldownTurns = query.getColumn(12).getInt();
    return action;
}

std::vector<std::string> loadStringColumn(SQLite::Database& database, const std::string& sql, const std::string& title)
{
    std::vector<std::string> values;
    SQLite::Statement query(database, sql);
    query.bind(1, title);
    while (query.executeStep())
    {
        values.push_back(query.getColumn(0).getString());
    }
    return values;
}

std::vector<card_data::KeyIntPair> loadIntegerValues(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::KeyIntPair> values;
    SQLite::Statement query(database, "SELECT key, value FROM card_integer_values WHERE title = ? ORDER BY key");
    query.bind(1, title);
    while (query.executeStep())
    {
        values.push_back({query.getColumn(0).getString(), query.getColumn(1).getInt()});
    }
    return values;
}

std::vector<card_data::KeyStringPair> loadStringValues(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::KeyStringPair> values;
    SQLite::Statement query(database, "SELECT key, value FROM card_string_values WHERE title = ? ORDER BY key");
    query.bind(1, title);
    while (query.executeStep())
    {
        values.push_back({query.getColumn(0).getString(), query.getColumn(1).getString()});
    }
    return values;
}

std::vector<card_data::Action> loadCardActions(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::Action> actions;
    SQLite::Statement query(
        database,
        "SELECT a.name, a.state, a.kind, a.pattern, a.min_range, a.max_range, a.damage, "
        "a.can_move, a.can_attack, a.pass_through, a.line_of_sight, a.status_turns, a.cooldown_turns "
        "FROM card_actions ca JOIN actions a ON a.name = ca.action_name "
        "WHERE ca.title = ? ORDER BY ca.item_index");
    query.bind(1, title);
    while (query.executeStep())
    {
        actions.push_back(actionFromQuery(query));
    }
    return actions;
}

std::vector<card_data::Card> loadCardsFromCardsDb()
{
    std::vector<card_data::Card> cards;
    try
    {
        SQLite::Database database("cards.db", SQLite::OPEN_READONLY);
        SQLite::Statement query(database, "SELECT title, type, image_path FROM cards ORDER BY title");
        while (query.executeStep())
        {
            card_data::Card card;
            card.title = query.getColumn(0).getString();
            card.type = query.getColumn(1).getString();
            card.imagePath = query.getColumn(2).getString();
            card.keywords = loadStringColumn(database, "SELECT keyword FROM card_keywords WHERE title = ? ORDER BY keyword", card.title);
            card.integerValues = loadIntegerValues(database, card.title);
            card.stringValues = loadStringValues(database, card.title);
            card.actionNames = loadStringColumn(database, "SELECT action_name FROM card_actions WHERE title = ? ORDER BY item_index", card.title);
            card.actions = loadCardActions(database, card.title);
            cards.push_back(std::move(card));
        }
    }
    catch (const std::exception& error)
    {
        fmt::println("Could not load AI starter deck from cards.db: {}", error.what());
    }
    return cards;
}

const std::vector<std::string>& fallbackStarterNonHeroes()
{
    static const std::vector<std::string> titles = {
        "Brass Pawn",
        "Rifleman",
        "Clockwork Rook",
        "Steam Bishop",
        "Automaton Knight",
        "Dredger",
        "Spark Drone",
        "Smoke Bomb",
        "Cannon Blast",
        "Repair Kit",
        "Overpressure",
        "Gearwright",
        "Brass Medic",
        "Boiler Imp",
        "Railgunner",
        "Swamp Skiff",
        "Arc Lantern",
        "Sprocket Swarm",
        "Chain Harpoon",
        "Mudslide",
    };
    return titles;
}

card_data::Card makeFallbackUnit(
    const std::string& title,
    int cost,
    int health,
    int attack,
    int range,
    int move,
    const std::string& movement)
{
    card_data::Card card;
    card.title = title;
    card.type = "Unit";
    card.imagePath = "cards/clockwork-rook.png";
    card.integerValues = {{"attack", attack}, {"cost", cost}, {"health", health}, {"move", move}, {"range", range}};
    card.stringValues = {{"movement", movement}};
    return card;
}

std::vector<card_data::Card> fallbackStarterDeck()
{
    std::vector<card_data::Card> deck;
    card_data::Card hero;
    hero.title = PreferredStarterHero;
    hero.type = "Hero";
    hero.imagePath = "cards/steam-baron.png";
    hero.integerValues = {{"attack", 8}, {"health", 30}, {"heroCost", 10}, {"move", 1}, {"range", 1}};
    hero.stringValues = {{"movement", "omni"}};
    deck.push_back(hero);

    const std::vector<card_data::Card> units = {
        makeFallbackUnit("Brass Pawn", 1, 4, 2, 1, 1, "ortho"),
        makeFallbackUnit("Rifleman", 3, 6, 4, 3, 1, "ortho"),
        makeFallbackUnit("Clockwork Rook", 4, 12, 5, 1, 7, "ortho"),
        makeFallbackUnit("Steam Bishop", 4, 9, 5, 1, 7, "diag"),
        makeFallbackUnit("Automaton Knight", 3, 9, 4, 1, 1, "jump"),
        makeFallbackUnit("Dredger", 5, 16, 6, 1, 1, "omni"),
        makeFallbackUnit("Spark Drone", 2, 4, 3, 2, 2, "omni"),
        makeFallbackUnit("Gearwright", 2, 5, 2, 1, 2, "omni"),
        makeFallbackUnit("Brass Medic", 3, 7, 2, 1, 2, "diag"),
        makeFallbackUnit("Boiler Imp", 1, 3, 3, 1, 2, "omni"),
    };
    for (const card_data::Card& unit : units)
    {
        deck.push_back(unit);
        deck.push_back(unit);
    }
    return deck;
}

const card_data::Card* findCardByTitle(const std::vector<card_data::Card>& cards, const std::string& title)
{
    const auto found = std::find_if(cards.begin(), cards.end(), [&](const card_data::Card& card) {
        return card.title == title;
    });
    return found == cards.end() ? nullptr : &*found;
}

std::vector<card_data::Card> makeAiStarterDeck()
{
    const std::vector<card_data::Card> library = loadCardsFromCardsDb();
    if (library.empty())
    {
        return fallbackStarterDeck();
    }

    std::vector<card_data::Card> deck;
    const card_data::Card* hero = findCardByTitle(library, PreferredStarterHero);
    if (hero == nullptr || !isHeroCard(*hero))
    {
        const auto firstHero = std::find_if(library.begin(), library.end(), isHeroCard);
        if (firstHero != library.end())
        {
            hero = &*firstHero;
        }
    }
    if (hero != nullptr)
    {
        deck.push_back(*hero);
    }

    std::vector<std::string> orderedTitles;
    for (const std::string& title : fallbackStarterNonHeroes())
    {
        if (const card_data::Card* card = findCardByTitle(library, title); card != nullptr && !isHeroCard(*card))
        {
            orderedTitles.push_back(title);
        }
    }
    for (const card_data::Card& card : library)
    {
        if (!isHeroCard(card) && std::find(orderedTitles.begin(), orderedTitles.end(), card.title) == orderedTitles.end())
        {
            orderedTitles.push_back(card.title);
        }
    }

    for (std::size_t i = 0; i < orderedTitles.size() && i < static_cast<std::size_t>(StarterNonHeroKinds); ++i)
    {
        if (const card_data::Card* card = findCardByTitle(library, orderedTitles[i]))
        {
            deck.push_back(*card);
            deck.push_back(*card);
        }
    }

    if (deckRulesError(deck))
    {
        return fallbackStarterDeck();
    }
    return deck;
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
    int currentPlayer() const { return activePlayer; }
    const std::vector<Piece>& boardPieces() const { return pieces; }
    const std::array<std::uint8_t, BoardSquares>& boardControl() const { return control; }
    const std::array<std::uint8_t, BoardSquares>& boardHoles() const { return holes; }
    const EnginePlayer& playerState(int playerNumber) const { return playerRef(playerNumber); }

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

    void placeHero(int playerNumber, int heroIndex, int row, int column)
    {
        if (phaseValue != Phase::HeroPlacement)
        {
            return;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (heroIndex < 0 || heroIndex >= static_cast<int>(player.heroesToPlace.size()))
        {
            return;
        }

        if (!isStartingSquare(playerNumber, row, column) || pieceAt(row, column) != nullptr)
        {
            setStatusFor(playerNumber, "Heroes must go on an empty starting square.");
            return;
        }

        spawnPiece(
            playerNumber,
            player.heroesToPlace[static_cast<std::size_t>(heroIndex)],
            row,
            column,
            true);
        player.heroesToPlace.erase(player.heroesToPlace.begin() + heroIndex);

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

        const std::vector<std::string> missingKeywords =
            missingHeroKeywords(pieces, playerNumber, card);
        if (!missingKeywords.empty())
        {
            std::string message = "Your living heroes must supply:";
            for (std::size_t i = 0; i < missingKeywords.size(); ++i)
            {
                message += i == 0 ? " " : ", ";
                message += missingKeywords[i];
            }
            message += " to play that card.";
            setStatusFor(playerNumber, message);
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

    // Discarding sends the card to the bottom of the draw pile, grants steam, and
    // counts as the turn's action (it ends the turn like any other play).
    void discardCard(int playerNumber, int handIndex)
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
        player.hand.erase(player.hand.begin() + handIndex);
        // The draw pile is drawn from the back, so the front is the bottom of the deck.
        player.drawPile.insert(player.drawPile.begin(), card);
        player.steam += DiscardSteamGain;
        advanceTurn(fmt::format(
            "Player {} discarded {} for {} steam.", playerNumber, card.title, DiscardSteamGain));
    }

    void movePiece(int playerNumber, int pieceId, int toRow, int toColumn)
    {
        performPieceAction(playerNumber, pieceId, toRow, toColumn);
    }

    void attackPiece(int playerNumber, int attackerId, int targetRow, int targetColumn)
    {
        performPieceAction(playerNumber, attackerId, targetRow, targetColumn);
    }

    void useAbility(int playerNumber, int pieceId)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return;
        }

        Piece* piece = pieceById(pieceId);
        if (piece == nullptr || piece->owner != playerNumber || piece->hasActed ||
            piece->ability.empty() || piece->growTurnsRemaining > 0 || piece->disabledTurns > 0)
        {
            return;
        }

        const std::string abilityLabel = pieceAbilityLabel(*piece);
        if (piece->ability == "dig")
        {
            if (piece->abilityUses <= 0)
            {
                setStatusFor(playerNumber, "That piece has already dug its hole.");
                return;
            }
            holes[static_cast<std::size_t>(squareIndex(piece->row, piece->column))] = 1;
            --piece->abilityUses;
        }
        else if (piece->ability == "transform" || piece->ability == "dematerialize")
        {
            int stateCount = 1;
            for (const ActionProfile& action : piece->actions)
            {
                stateCount = std::max(stateCount, action.state + 1);
            }
            piece->actionState = (piece->actionState + 1) % stateCount;
            piece->hidden = piece->ability == "dematerialize" && piece->actionState != 0;
        }
        else
        {
            return;
        }

        piece->hasActed = true;
        advanceTurn(fmt::format("{} used {}.", piece->name, abilityLabel));
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
        snapshot.holes = holes;
        snapshot.pieces.clear();
        for (const Piece& piece : pieces)
        {
            const bool concealedOpponentPlacement =
                phaseValue == Phase::HeroPlacement &&
                piece.isHero &&
                piece.owner != playerNumber;
            if (!concealedOpponentPlacement && (!piece.hidden || piece.owner == playerNumber))
            {
                snapshot.pieces.push_back(piece);
            }
        }
        const EnginePlayer& viewingPlayer = playerRef(playerNumber);
        snapshot.hand = phaseValue == Phase::HeroPlacement
            ? viewingPlayer.heroesToPlace
            : viewingPlayer.hand;
        snapshot.status = status;

        for (int p = 0; p < 2; ++p)
        {
            const EnginePlayer& player = players[static_cast<std::size_t>(p)];
            PlayerSnapshot& view = snapshot.players[static_cast<std::size_t>(p)];
            view.steam = player.steam;
            view.controlledSquares = controlledCount(p + 1);
            view.handCount = phaseValue == Phase::HeroPlacement
                ? static_cast<int>(player.heroesToPlace.size())
                : static_cast<int>(player.hand.size());
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
    std::array<std::uint8_t, BoardSquares> holes{};
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
        holes.fill(0);
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

    void performPieceAction(int playerNumber, int pieceId, int toRow, int toColumn)
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

        const ActionResolution action = resolvePieceAction(pieces, holes, *piece, toRow, toColumn);
        if (!action.legal)
        {
            setStatusFor(playerNumber, "That piece cannot act there.");
            return;
        }

        const int attackerId = piece->id;
        const std::string attackerName = piece->name;
        std::string targetName;
        bool targetDestroyed = false;
        bool targetAtDestination = false;
        int victimOwner = 0;

        if (action.attacks)
        {
            Piece* target = pieceById(action.targetId);
            if (target == nullptr)
            {
                return;
            }

            targetName = target->name;
            targetAtDestination = target->row == toRow && target->column == toColumn;
            victimOwner = target->owner;
            target->health -= action.damage;
            if (action.damage > 0)
            {
                target->sleepTurnsRemaining = std::max(target->sleepTurnsRemaining, 1);
            }
            target->disabledTurns = std::max(target->disabledTurns, action.statusTurns);
            if (target->health <= 0)
            {
                targetDestroyed = true;
                removePiece(target->id);
            }
        }

        Piece* survivingAttacker = pieceById(attackerId);
        if (survivingAttacker == nullptr)
        {
            return;
        }

        if (action.moves)
        {
            if (!action.attacks || !targetAtDestination || targetDestroyed)
            {
                survivingAttacker->row = toRow;
                survivingAttacker->column = toColumn;
            }
            else
            {
                survivingAttacker->row = action.stagingRow;
                survivingAttacker->column = action.stagingColumn;
            }
        }
        survivingAttacker->disabledTurns =
            std::max(survivingAttacker->disabledTurns, action.cooldownTurns);
        survivingAttacker->hasActed = true;

        if (targetDestroyed)
        {
            checkForWinner(victimOwner);
            if (phaseValue == Phase::GameOver)
            {
                return;
            }
        }

        if (action.attacks)
        {
            std::string result = fmt::format(
                "{} hit {} for {}",
                attackerName,
                targetName,
                action.damage);
            if (action.statusTurns > 0)
            {
                result += fmt::format(" and disabled it for {} turn(s)", action.statusTurns);
            }
            result += targetDestroyed ? " and destroyed it!" : ".";
            advanceTurn(result);
        }
        else
        {
            advanceTurn(fmt::format("{} moved.", attackerName));
        }
    }

    void spawnPiece(int playerNumber, const GameCard& card, int row, int column, bool isHero)
    {
        Piece piece;
        piece.id = nextPieceId++;
        piece.owner = playerNumber;
        piece.row = row;
        piece.column = column;
        piece.name = card.title;
        piece.keywords = card.keywords;
        piece.imagePath = card.imagePath;
        piece.walkAnimPath = card.walkAnimPath;
        piece.blueTokenPath = card.blueTokenPath;
        piece.redTokenPath = card.redTokenPath;
        piece.blueWalkAnimPath = card.blueWalkAnimPath;
        piece.redWalkAnimPath = card.redWalkAnimPath;
        piece.walkAnimFrames = card.walkAnimFrames;
        piece.maxHealth = card.health;
        piece.health = card.health;
        piece.attack = card.attack;
        piece.attackRange = card.attackRange;
        piece.movePattern = card.movePattern;
        piece.moveRange = card.moveRange;
        piece.attackingMove = card.attackingMove;
        piece.canControl = card.canControl;
        piece.growTurnsRemaining = card.growTurns;
        piece.actions = card.actions;
        piece.ability = card.ability;
        piece.abilityLabels = card.abilityLabels;
        piece.abilityUses = card.abilityUses;
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
                if (piece.growTurnsRemaining > 0)
                {
                    --piece.growTurnsRemaining;
                    piece.hasActed = piece.growTurnsRemaining > 0;
                }
                if (piece.disabledTurns > 0)
                {
                    --piece.disabledTurns;
                    piece.hasActed = true;
                }
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
        endTurnFor(activePlayer);
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

    void endTurnFor(int playerNumber)
    {
        for (Piece& piece : pieces)
        {
            if (piece.owner == playerNumber && piece.sleepTurnsRemaining > 0)
            {
                --piece.sleepTurnsRemaining;
            }
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
            if (card.power > 0)
            {
                target->sleepTurnsRemaining = std::max(target->sleepTurnsRemaining, 1);
            }
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
                    if (occupant->canControl)
                    {
                        next[index] = static_cast<std::uint8_t>(occupant->owner);
                    }
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
                        if (!neighbor->canControl)
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

enum class AiActionKind
{
    EndTurn,
    MovePiece,
    AttackPiece,
    UseAbility,
    PlayCard,
    DiscardCard
};

struct AiAction
{
    AiActionKind kind = AiActionKind::EndTurn;
    int pieceId = 0;
    int handIndex = 0;
    int row = 0;
    int column = 0;
};

const Piece* aiPieceAt(const std::vector<Piece>& pieces, int row, int column)
{
    return findPieceAt(pieces, row, column);
}

int actionPriority(const AiAction& action)
{
    switch (action.kind)
    {
        case AiActionKind::AttackPiece: return 700;
        case AiActionKind::PlayCard: return 600;
        case AiActionKind::UseAbility: return 500;
        case AiActionKind::MovePiece: return 300;
        case AiActionKind::DiscardCard: return 100;
        case AiActionKind::EndTurn: return 0;
    }
    return 0;
}

std::vector<AiAction> legalAiActions(const GameEngine& engine, int playerNumber, bool allowCards)
{
    std::vector<AiAction> actions;
    if (engine.phase() != Phase::Playing || engine.currentPlayer() != playerNumber)
    {
        return actions;
    }

    const std::vector<Piece>& pieces = engine.boardPieces();
    const std::array<std::uint8_t, BoardSquares>& holes = engine.boardHoles();
    for (const Piece& piece : pieces)
    {
        if (piece.owner != playerNumber || piece.hasActed || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
        {
            continue;
        }
        if (!piece.ability.empty() && (piece.ability != "dig" || piece.abilityUses > 0))
        {
            actions.push_back({AiActionKind::UseAbility, piece.id});
        }
        for (int row = 0; row < BoardSize; ++row)
        {
            for (int column = 0; column < BoardSize; ++column)
            {
                const ActionResolution resolution = resolvePieceAction(pieces, holes, piece, row, column);
                if (!resolution.legal)
                {
                    continue;
                }
                actions.push_back({
                    resolution.attacks ? AiActionKind::AttackPiece : AiActionKind::MovePiece,
                    piece.id,
                    0,
                    row,
                    column});
            }
        }
    }

    if (allowCards)
    {
        const GameEngine::EnginePlayer& player = engine.playerState(playerNumber);
        for (int handIndex = 0; handIndex < static_cast<int>(player.hand.size()); ++handIndex)
        {
            const GameCard& card = player.hand[static_cast<std::size_t>(handIndex)];
            if (card.cost > player.steam || !heroKeywordsAllowCard(pieces, playerNumber, card))
            {
                continue;
            }
            if (card.type == "Unit")
            {
                for (int row = 0; row < BoardSize; ++row)
                {
                    for (int column = 0; column < BoardSize; ++column)
                    {
                        if (engine.boardControl()[static_cast<std::size_t>(squareIndex(row, column))] == playerNumber &&
                            aiPieceAt(pieces, row, column) == nullptr)
                        {
                            actions.push_back({AiActionKind::PlayCard, 0, handIndex, row, column});
                        }
                    }
                }
            }
            else if (card.type == "Spell")
            {
                if (card.effect == "steam")
                {
                    actions.push_back({AiActionKind::PlayCard, 0, handIndex, 0, 0});
                }
                else
                {
                    for (const Piece& target : pieces)
                    {
                        if ((card.effect == "damage" && target.owner != playerNumber) ||
                            (card.effect == "heal" && target.owner == playerNumber && target.health < target.maxHealth))
                        {
                            actions.push_back({AiActionKind::PlayCard, 0, handIndex, target.row, target.column});
                        }
                    }
                }
            }
        }
        for (int handIndex = 0; handIndex < static_cast<int>(player.hand.size()); ++handIndex)
        {
            actions.push_back({AiActionKind::DiscardCard, 0, handIndex});
        }
    }

    actions.push_back({AiActionKind::EndTurn});
    std::stable_sort(actions.begin(), actions.end(), [](const AiAction& left, const AiAction& right) {
        return actionPriority(left) > actionPriority(right);
    });
    if (actions.size() > static_cast<std::size_t>(AiMaxCandidateActions))
    {
        actions.resize(static_cast<std::size_t>(AiMaxCandidateActions));
    }
    return actions;
}

void applyAiAction(GameEngine& engine, int playerNumber, const AiAction& action)
{
    switch (action.kind)
    {
        case AiActionKind::MovePiece:
            engine.movePiece(playerNumber, action.pieceId, action.row, action.column);
            break;
        case AiActionKind::AttackPiece:
            engine.attackPiece(playerNumber, action.pieceId, action.row, action.column);
            break;
        case AiActionKind::UseAbility:
            engine.useAbility(playerNumber, action.pieceId);
            break;
        case AiActionKind::PlayCard:
            engine.playCard(playerNumber, action.handIndex, action.row, action.column);
            break;
        case AiActionKind::DiscardCard:
            engine.discardCard(playerNumber, action.handIndex);
            break;
        case AiActionKind::EndTurn:
            engine.endTurn(playerNumber);
            break;
    }
}

int evaluateEngine(const GameEngine& engine, int aiPlayer)
{
    const int opponent = aiPlayer == 1 ? 2 : 1;
    if (engine.phase() == Phase::GameOver)
    {
        if (engine.winner() == aiPlayer)
        {
            return 100000;
        }
        if (engine.winner() == opponent)
        {
            return -100000;
        }
    }

    int score = 0;
    for (const Piece& piece : engine.boardPieces())
    {
        int value = piece.health * (piece.isHero ? 10 : 4);
        for (const ActionProfile& action : piece.actions)
        {
            value += action.damage * 3 + action.statusTurns * 4;
            if (action.canMove)
            {
                value += action.maxRange;
            }
        }
        if (piece.disabledTurns > 0 || piece.growTurnsRemaining > 0)
        {
            value -= 12;
        }
        score += piece.owner == aiPlayer ? value : -value;
    }

    const PlayerSnapshot aiSnapshot = engine.snapshotFor(aiPlayer).players[static_cast<std::size_t>(aiPlayer - 1)];
    const PlayerSnapshot opponentSnapshot = engine.snapshotFor(aiPlayer).players[static_cast<std::size_t>(opponent - 1)];
    score += (aiSnapshot.controlledSquares - opponentSnapshot.controlledSquares) * 20;
    score += (aiSnapshot.steam - opponentSnapshot.steam) * 2;
    score += (aiSnapshot.heroesAlive - opponentSnapshot.heroesAlive) * 800;
    score += aiSnapshot.handCount * 4;
    return score;
}

int searchAiPosition(const GameEngine& engine, int depth, int aiPlayer)
{
    if (depth <= 0 || engine.phase() == Phase::GameOver)
    {
        return evaluateEngine(engine, aiPlayer);
    }

    const int current = engine.currentPlayer();
    const bool maximizing = current == aiPlayer;
    std::vector<AiAction> actions = legalAiActions(engine, current, maximizing);
    if (actions.empty())
    {
        return evaluateEngine(engine, aiPlayer);
    }

    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    for (const AiAction& action : actions)
    {
        GameEngine next = engine;
        applyAiAction(next, current, action);
        const int score = searchAiPosition(next, depth - 1, aiPlayer);
        if (maximizing)
        {
            best = std::max(best, score);
        }
        else
        {
            best = std::min(best, score);
        }
    }
    return best;
}

AiAction chooseAiAction(const GameEngine& engine, int aiPlayer)
{
    std::vector<AiAction> actions = legalAiActions(engine, aiPlayer, true);
    AiAction bestAction;
    int bestScore = std::numeric_limits<int>::min();
    for (const AiAction& action : actions)
    {
        GameEngine next = engine;
        applyAiAction(next, aiPlayer, action);
        const int score = searchAiPosition(next, AiSearchPlies - 1, aiPlayer);
        if (score > bestScore)
        {
            bestScore = score;
            bestAction = action;
        }
    }
    return bestAction;
}

void placeAiHeroes(GameEngine& engine, int aiPlayer)
{
    int homeIndex = 0;
    while (engine.phase() == Phase::HeroPlacement &&
           !engine.playerState(aiPlayer).heroesToPlace.empty() &&
           homeIndex < static_cast<int>(homeSquares(aiPlayer).size()))
    {
        const auto [row, column] = homeSquares(aiPlayer)[static_cast<std::size_t>(homeIndex)];
        if (findPieceAt(engine.boardPieces(), row, column) == nullptr)
        {
            engine.placeHero(aiPlayer, 0, row, column);
        }
        ++homeIndex;
    }
}

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
        std::string playerOne;
        std::string playerTwo;
        std::string resultToken;
        packet >> msgType >> matchId;
        const MessageType type = static_cast<MessageType>(msgType);
        if (type == MessageType::CreateGameSession)
        {
            packet >> playerOne >> playerTwo >> resultToken;
        }
        else if (type == MessageType::CreateAiGameSession)
        {
            packet >> playerOne;
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

        const unsigned short port = nextGamePort++;
        const bool started =
            spawnGameProcess(matchId, port, playerOne, playerTwo, resultToken, type == MessageType::CreateAiGameSession);

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
        bool aiOpponent = false)
        : matchId(matchId),
          port(port),
          playerUsernames{std::move(playerOne), std::move(playerTwo)},
          resultToken(std::move(resultToken)),
          listener(std::make_unique<sf::TcpListener>()),
          aiOpponent(aiOpponent)
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
        GameEngine engine(seed);

        if (!receiveDeck(*playerOne->socket, engine, playerOne->playerNumber))
        {
            fmt::println("Game {} did not receive player {}'s deck", matchId, playerOne->playerNumber);
            engine.resign(playerOne->playerNumber);
            broadcast(engine, *playerOne, *playerTwo);
            finishRankedMatch(engine, *playerOne, *playerTwo);
            return;
        }
        if (!receiveDeck(*playerTwo->socket, engine, playerTwo->playerNumber))
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
        GameEngine engine(seed);

        if (!receiveDeck(*human->socket, engine, human->playerNumber))
        {
            fmt::println("AI game {} did not receive player's deck", matchId);
            engine.resign(human->playerNumber);
            sendSnapshot(*human->socket, engine, human->playerNumber);
            sendAiMatchResult(*human, engine);
            return;
        }

        const std::vector<card_data::Card> aiDeck = makeAiStarterDeck();
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
                    action.kind != AiActionKind::EndTurn)
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
        const int coinsAwarded = humanWon ? 10 : 0;
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(MessageType::GameOver)
               << false
               << std::string(humanWon ? "Victory over the AI!" : "Defeated by the AI.")
               << 0
               << human.rating
               << coinsAwarded
               << false;
        [[maybe_unused]] auto sent = human.socket->send(packet);
    }

private:
    int matchId = 0;
    unsigned short port = 0;
    std::array<std::string, 2> playerUsernames;
    std::string resultToken;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening = false;
    bool aiOpponent = false;

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
        return JoinedPlayer{std::move(client), playerNumber, username, rating};
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

        if (const std::optional<std::string> error = game_data::deckRulesError(cards))
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
    if (argc == 7 && std::string(argv[1]) == "--game")
    {
        const int matchId = std::stoi(argv[2]);
        const unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));
        GameProcess game(matchId, port, argv[4], argv[5], argv[6]);
        game.run();
        return 0;
    }

    if (argc == 5 && std::string(argv[1]) == "--game-ai")
    {
        const int matchId = std::stoi(argv[2]);
        const unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));
        GameProcess game(matchId, port, argv[4], AiOpponentName, "", true);
        game.run();
        return 0;
    }

    fmt::println("Starting Game Server Coordinator...");

    GameServerCoordinator coordinator(GameServerPort);
    coordinator.run();

    return 0;
}
