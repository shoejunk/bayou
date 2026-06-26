#pragma once

#include <SFML/Network.hpp>

#include "card_data.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Shared tactical-game model used by both the game server (authoritative) and
// the client (rendering / input). Everything here is header-only inline code so
// the two executables stay in sync without an extra module.
namespace game_data
{
constexpr int BoardSize = 8;
constexpr int BoardSquares = BoardSize * BoardSize;

// Deck-building constraints.
constexpr int DeckCardCount = 20;   // non-hero cards
constexpr int MaxCardCopies = 2;
constexpr int MaxHeroCopies = 1;
constexpr int MinHeroes = 1;
constexpr int MaxHeroes = 4;
constexpr int HeroCostLimit = 100;
constexpr int DamageDisabledTurns = 1;

inline std::optional<std::string> deckRulesError(const std::vector<card_data::Card>& cards)
{
    int cardCount = 0;
    int heroCount = 0;
    int heroCost = 0;
    std::unordered_map<std::string, int> used;

    for (const card_data::Card& card : cards)
    {
        if (card.title.empty())
        {
            return "Deck contains a card with no title";
        }

        const bool isHero = card.type == "Hero";
        const int count = ++used[card.title];
        const int copyLimit = isHero ? MaxHeroCopies : MaxCardCopies;
        if (count > copyLimit)
        {
            return "Deck can contain at most " + std::to_string(copyLimit) + " " +
                (isHero ? "copy of hero " : "copies of card ") + card.title;
        }

        if (isHero)
        {
            ++heroCount;
            for (const card_data::KeyIntPair& value : card.integerValues)
            {
                if (value.key == "heroCost")
                {
                    heroCost += value.value;
                    break;
                }
            }
        }
        else
        {
            ++cardCount;
        }
    }

    if (cardCount != DeckCardCount)
    {
        return "Deck must contain exactly " + std::to_string(DeckCardCount) + " non-hero cards";
    }
    if (heroCount < MinHeroes || heroCount > MaxHeroes)
    {
        return "Deck must contain " + std::to_string(MinHeroes) + "-" +
            std::to_string(MaxHeroes) + " heroes";
    }
    if (heroCost > HeroCostLimit)
    {
        return "Hero cost exceeds limit " + std::to_string(HeroCostLimit);
    }

    return std::nullopt;
}

// Hand / steam tuning.
constexpr int StartingHandSize = 4;
constexpr int MaxHandSize = 4;
constexpr int DiscardSteamGain = 10;  // steam gained by discarding a card (ends the turn)

enum class Phase : std::uint8_t
{
    HeroPlacement,
    Playing,
    GameOver
};

enum class MovePattern : std::uint8_t
{
    None = 0,
    Ortho = 1,  // rook-like, straight lines
    Diag = 2,   // bishop-like, diagonals
    Omni = 3,   // king/queen-like, all eight directions
    Jump = 4,   // knight-like, fixed L offsets
    Horizontal = 5,
    Vertical = 6
};

enum class ActionKind : std::uint8_t
{
    Slide = 0,
    Ranged = 1,
    Hop = 2,
    Teleport = 3,
    Tunnel = 4
};

struct ActionProfile
{
    std::string name;
    std::uint8_t kind = static_cast<std::uint8_t>(ActionKind::Slide);
    std::uint8_t pattern = static_cast<std::uint8_t>(MovePattern::Omni);
    int state = 0;
    int minRange = 1;
    int maxRange = 1;
    int damage = 0;
    int statusTurns = 0;
    int cooldownTurns = 0;
    bool canMove = true;
    bool canAttack = false;
    bool passThrough = false;
    bool lineOfSight = false;
};

inline std::uint8_t parseMovePattern(const std::string& value)
{
    if (value == "ortho")
    {
        return static_cast<std::uint8_t>(MovePattern::Ortho);
    }
    if (value == "diag")
    {
        return static_cast<std::uint8_t>(MovePattern::Diag);
    }
    if (value == "jump")
    {
        return static_cast<std::uint8_t>(MovePattern::Jump);
    }
    if (value == "horizontal")
    {
        return static_cast<std::uint8_t>(MovePattern::Horizontal);
    }
    if (value == "vertical")
    {
        return static_cast<std::uint8_t>(MovePattern::Vertical);
    }
    if (value == "none")
    {
        return static_cast<std::uint8_t>(MovePattern::None);
    }
    return static_cast<std::uint8_t>(MovePattern::Omni);
}

inline std::string movePatternName(std::uint8_t pattern)
{
    switch (static_cast<MovePattern>(pattern))
    {
        case MovePattern::Ortho: return "Orthogonal";
        case MovePattern::Diag: return "Diagonal";
        case MovePattern::Omni: return "Any direction";
        case MovePattern::Jump: return "Knight jump";
        case MovePattern::Horizontal: return "Horizontal";
        case MovePattern::Vertical: return "Vertical";
        default: return "Stationary";
    }
}

// Card attribute lookups against the flexible key/value card model.
inline int cardInt(const card_data::Card& card, const std::string& key, int fallback = 0)
{
    for (const card_data::KeyIntPair& item : card.integerValues)
    {
        if (item.key == key)
        {
            return item.value;
        }
    }
    return fallback;
}

inline std::string cardStr(const card_data::Card& card, const std::string& key, const std::string& fallback = "")
{
    for (const card_data::KeyStringPair& item : card.stringValues)
    {
        if (item.key == key)
        {
            return item.value;
        }
    }
    return fallback;
}

inline std::vector<std::string> cardList(const card_data::Card& card, const std::string& key)
{
    for (const card_data::KeyStringList& item : card.stringLists)
    {
        if (item.key == key)
        {
            return item.values;
        }
    }
    return {};
}

inline std::uint8_t parseActionKind(const std::string& value)
{
    if (value == "ranged")
    {
        return static_cast<std::uint8_t>(ActionKind::Ranged);
    }
    if (value == "hop")
    {
        return static_cast<std::uint8_t>(ActionKind::Hop);
    }
    if (value == "teleport")
    {
        return static_cast<std::uint8_t>(ActionKind::Teleport);
    }
    if (value == "tunnel")
    {
        return static_cast<std::uint8_t>(ActionKind::Tunnel);
    }
    return static_cast<std::uint8_t>(ActionKind::Slide);
}

inline bool actionLooksLikeAttackingMove(const card_data::Action& action)
{
    return action.state == 0 && action.canMove && action.canAttack;
}

inline bool isHeroCard(const card_data::Card& card)
{
    return card.type == "Hero";
}

inline bool isUnitCard(const card_data::Card& card)
{
    return card.type == "Unit";
}

// A playable card resolved from a card definition into the values the game uses.
struct GameCard
{
    std::string title;
    std::string type;      // "Unit", "Spell", or "Hero"
    std::vector<std::string> keywords;
    std::string imagePath;
    std::string walkAnimPath;
    std::string blueTokenPath;
    std::string redTokenPath;
    std::string blueWalkAnimPath;
    std::string redWalkAnimPath;
    int walkAnimFrames = 4;
    int cost = 1;          // steam cost (units / spells)
    int heroCost = 0;      // hero-cost budget contribution (heroes)
    int health = 1;
    int attack = 0;
    int attackRange = 0;
    std::uint8_t movePattern = static_cast<std::uint8_t>(MovePattern::None);
    int moveRange = 0;
    bool attackingMove = false;
    std::string effect = "none";  // spells: "damage", "heal", "steam"
    std::string target = "none";  // spells: "enemy", "ally", "none"
    int power = 0;                // spell magnitude
    bool canControl = true;
    int growTurns = 0;
    std::vector<ActionProfile> actions;
    std::string ability;
    std::vector<std::string> abilityLabels;
    int abilityUses = 0;
};

inline GameCard toGameCard(const card_data::Card& card)
{
    GameCard g;
    g.title = card.title;
    g.type = card.type;
    g.keywords = card.keywords;
    g.imagePath = card.imagePath;
    g.walkAnimPath = cardStr(card, "WalkAnim");
    g.blueTokenPath = cardStr(card, "TokenBlue");
    g.redTokenPath = cardStr(card, "TokenRed");
    g.blueWalkAnimPath = cardStr(card, "WalkAnimBlue");
    g.redWalkAnimPath = cardStr(card, "WalkAnimRed");
    if (g.blueWalkAnimPath.empty() && g.redWalkAnimPath.empty())
    {
        g.blueWalkAnimPath = g.walkAnimPath;
        g.redWalkAnimPath = g.walkAnimPath;
    }
    g.walkAnimFrames = std::max(1, cardInt(card, "WalkAnimFrames", 4));
    g.cost = cardInt(card, "cost", 1);
    g.heroCost = cardInt(card, "heroCost", 0);
    g.health = cardInt(card, "health", 1);
    g.effect = cardStr(card, "effect", "none");
    g.target = cardStr(card, "target", "none");
    g.power = cardInt(card, "power", 0);
    g.canControl = cardInt(card, "canControl", 1) != 0;
    g.growTurns = cardInt(card, "growTurns", 0);
    g.ability = cardStr(card, "ability");
    g.abilityLabels = cardList(card, "abilityLabels");
    g.abilityUses = cardInt(card, "abilityUses", 0);

    for (const card_data::Action& definition : card.actions)
    {
        ActionProfile action;
        action.name = definition.name;
        action.state = definition.state;
        action.kind = parseActionKind(definition.kind);
        action.pattern = parseMovePattern(definition.pattern);
        action.minRange = definition.minRange;
        action.maxRange = definition.maxRange;
        action.damage = definition.damage;
        action.canMove = definition.canMove;
        action.canAttack = definition.canAttack;
        action.passThrough = definition.passThrough;
        action.lineOfSight = definition.lineOfSight;
        action.statusTurns = definition.statusTurns;
        action.cooldownTurns = definition.cooldownTurns;
        g.actions.push_back(action);
        if (actionLooksLikeAttackingMove(definition))
        {
            g.attackingMove = true;
        }
    }
    if (g.actions.empty() && (g.type == "Unit" || g.type == "Hero"))
    {
        const std::uint8_t legacyPattern = parseMovePattern(cardStr(card, "movement", "omni"));
        const int legacyMove = cardInt(card, "move", 1);
        const int legacyAttack = cardInt(card, "attack", 0);
        const int legacyRange = cardInt(card, "range", 1);
        if (legacyMove > 0)
        {
            ActionProfile moveAction;
            moveAction.name = g.title + " Move";
            moveAction.pattern = legacyPattern;
            moveAction.maxRange = legacyMove;
            moveAction.damage = 0;
            moveAction.canMove = true;
            moveAction.canAttack = false;
            g.actions.push_back(moveAction);
        }
        if (legacyAttack > 0)
        {
            ActionProfile attackAction;
            attackAction.name = g.title + " Attack";
            attackAction.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
            attackAction.pattern = static_cast<std::uint8_t>(MovePattern::Omni);
            attackAction.maxRange = std::max(1, legacyRange);
            attackAction.damage = legacyAttack;
            attackAction.canMove = false;
            attackAction.canAttack = true;
            attackAction.lineOfSight = true;
            g.actions.push_back(attackAction);
        }
    }

    bool foundMoveAction = false;
    bool foundAttackAction = false;
    for (const ActionProfile& action : g.actions)
    {
        if (!foundMoveAction && action.canMove)
        {
            g.movePattern = action.pattern;
            g.moveRange = action.maxRange;
            foundMoveAction = true;
        }
        if (action.canAttack && (!foundAttackAction || action.damage > g.attack))
        {
            g.attack = action.damage;
            g.attackRange = action.maxRange;
            foundAttackAction = true;
        }
    }
    return g;
}

// A unit / hero standing on the board.
struct Piece
{
    int id = 0;
    int owner = 0;  // 1 or 2
    int row = 0;
    int column = 0;
    std::string name;
    std::vector<std::string> keywords;
    std::string imagePath;
    std::string walkAnimPath;
    std::string blueTokenPath;
    std::string redTokenPath;
    std::string blueWalkAnimPath;
    std::string redWalkAnimPath;
    int walkAnimFrames = 4;
    int maxHealth = 1;
    int health = 1;
    int attack = 0;
    int attackRange = 1;
    std::uint8_t movePattern = static_cast<std::uint8_t>(MovePattern::Omni);
    int moveRange = 1;
    bool attackingMove = false;
    bool canControl = true;
    int growTurnsRemaining = 0;
    int disabledTurns = 0;
    int sleepTurnsRemaining = 0;
    std::vector<ActionProfile> actions;
    int actionState = 0;
    std::string ability;
    std::vector<std::string> abilityLabels;
    int abilityUses = 0;
    bool hidden = false;
    bool isHero = false;
    bool hasActed = false;
};

inline int disabledTurnsForDamage(int damage, int statusTurns)
{
    return std::max(statusTurns, damage > 0 ? DamageDisabledTurns : 0);
}

inline void applyDamageStatus(Piece& target, int damage, int statusTurns)
{
    if (damage > 0)
    {
        target.sleepTurnsRemaining = std::max(target.sleepTurnsRemaining, 1);
    }
    target.disabledTurns = std::max(target.disabledTurns, disabledTurnsForDamage(damage, statusTurns));
}

// Returns the card keywords not supplied by any surviving friendly hero.
// Keywords are requirements only when initially playing a Unit or Spell.
inline std::vector<std::string> missingHeroKeywords(
    const std::vector<Piece>& pieces,
    int playerNumber,
    const GameCard& card)
{
    std::vector<std::string> missing;
    for (const std::string& required : card.keywords)
    {
        bool supplied = false;
        for (const Piece& piece : pieces)
        {
            if (piece.owner == playerNumber && piece.isHero &&
                std::find(piece.keywords.begin(), piece.keywords.end(), required) != piece.keywords.end())
            {
                supplied = true;
                break;
            }
        }
        if (!supplied && std::find(missing.begin(), missing.end(), required) == missing.end())
        {
            missing.push_back(required);
        }
    }
    return missing;
}

inline bool heroKeywordsAllowCard(
    const std::vector<Piece>& pieces,
    int playerNumber,
    const GameCard& card)
{
    return missingHeroKeywords(pieces, playerNumber, card).empty();
}

// Per-player summary visible to both players.
struct PlayerSnapshot
{
    int steam = 0;
    int controlledSquares = 0;
    int handCount = 0;
    int heroesToPlace = 0;
    int heroesAlive = 0;
    int drawPileCount = 0;
};

// The full view of the game tailored to one recipient (their hand is included,
// the opponent's hand is conveyed only as a count).
struct Snapshot
{
    std::uint8_t phase = static_cast<std::uint8_t>(Phase::HeroPlacement);
    int activePlayer = 1;
    int yourPlayer = 1;
    int winner = 0;  // 0 = none
    std::array<PlayerSnapshot, 2> players{};
    std::array<std::uint8_t, BoardSquares> control{};  // 0 neutral, 1, 2
    std::array<std::uint8_t, BoardSquares> holes{};
    std::vector<Piece> pieces;
    std::vector<GameCard> hand;  // recipient's hand
    std::string status;
};

// ---- serialization helpers -------------------------------------------------

inline void writeGameCard(sf::Packet& packet, const GameCard& card)
{
    packet << card.title << card.type;
    card_data::writeStringVector(packet, card.keywords);
    packet << card.imagePath << card.walkAnimPath
           << card.blueTokenPath << card.redTokenPath
           << card.blueWalkAnimPath << card.redWalkAnimPath << card.walkAnimFrames
           << card.cost << card.heroCost
           << card.health << card.attack << card.attackRange
           << card.movePattern << card.moveRange << card.attackingMove
           << card.effect << card.target << card.power
           << card.canControl << card.growTurns;
    packet << static_cast<std::uint32_t>(card.actions.size());
    for (const ActionProfile& action : card.actions)
    {
        packet << action.name << action.kind << action.pattern << action.state << action.minRange << action.maxRange
               << action.damage << action.statusTurns << action.cooldownTurns
               << action.canMove << action.canAttack << action.passThrough << action.lineOfSight;
    }
    packet << card.ability;
    card_data::writeStringVector(packet, card.abilityLabels);
    packet << card.abilityUses;
}

inline bool readGameCard(sf::Packet& packet, GameCard& card)
{
    packet >> card.title >> card.type;
    if (!packet || !card_data::readStringVector(packet, card.keywords))
    {
        return false;
    }
    packet >> card.imagePath >> card.walkAnimPath
           >> card.blueTokenPath >> card.redTokenPath
           >> card.blueWalkAnimPath >> card.redWalkAnimPath >> card.walkAnimFrames
           >> card.cost >> card.heroCost
           >> card.health >> card.attack >> card.attackRange
           >> card.movePattern >> card.moveRange >> card.attackingMove
           >> card.effect >> card.target >> card.power
           >> card.canControl >> card.growTurns;
    std::uint32_t actionCount = 0;
    packet >> actionCount;
    card.actions.clear();
    card.actions.reserve(actionCount);
    for (std::uint32_t i = 0; i < actionCount; ++i)
    {
        ActionProfile action;
        packet >> action.name >> action.kind >> action.pattern >> action.state >> action.minRange >> action.maxRange
               >> action.damage >> action.statusTurns >> action.cooldownTurns
               >> action.canMove >> action.canAttack >> action.passThrough >> action.lineOfSight;
        if (!packet)
        {
            return false;
        }
        card.actions.push_back(action);
    }
    packet >> card.ability;
    if (!packet || !card_data::readStringVector(packet, card.abilityLabels))
    {
        return false;
    }
    packet >> card.abilityUses;
    return static_cast<bool>(packet);
}

inline void writePiece(sf::Packet& packet, const Piece& piece)
{
    packet << piece.id << piece.owner << piece.row << piece.column << piece.name;
    card_data::writeStringVector(packet, piece.keywords);
    packet << piece.imagePath << piece.walkAnimPath
           << piece.blueTokenPath << piece.redTokenPath
           << piece.blueWalkAnimPath << piece.redWalkAnimPath << piece.walkAnimFrames
           << piece.maxHealth << piece.health << piece.attack << piece.attackRange
           << piece.movePattern << piece.moveRange << piece.attackingMove
           << piece.canControl << piece.growTurnsRemaining << piece.disabledTurns << piece.sleepTurnsRemaining
           << piece.actionState << piece.ability << piece.abilityUses << piece.hidden
           << piece.isHero << piece.hasActed;
    packet << static_cast<std::uint32_t>(piece.actions.size());
    for (const ActionProfile& action : piece.actions)
    {
        packet << action.name << action.kind << action.pattern << action.state << action.minRange << action.maxRange
               << action.damage << action.statusTurns << action.cooldownTurns
               << action.canMove << action.canAttack << action.passThrough << action.lineOfSight;
    }
    card_data::writeStringVector(packet, piece.abilityLabels);
}

inline bool readPiece(sf::Packet& packet, Piece& piece)
{
    packet >> piece.id >> piece.owner >> piece.row >> piece.column >> piece.name;
    if (!packet || !card_data::readStringVector(packet, piece.keywords))
    {
        return false;
    }
    packet >> piece.imagePath >> piece.walkAnimPath
           >> piece.blueTokenPath >> piece.redTokenPath
           >> piece.blueWalkAnimPath >> piece.redWalkAnimPath >> piece.walkAnimFrames
           >> piece.maxHealth >> piece.health >> piece.attack >> piece.attackRange
           >> piece.movePattern >> piece.moveRange >> piece.attackingMove
           >> piece.canControl >> piece.growTurnsRemaining >> piece.disabledTurns >> piece.sleepTurnsRemaining
           >> piece.actionState >> piece.ability >> piece.abilityUses >> piece.hidden
           >> piece.isHero >> piece.hasActed;
    std::uint32_t actionCount = 0;
    packet >> actionCount;
    piece.actions.clear();
    piece.actions.reserve(actionCount);
    for (std::uint32_t i = 0; i < actionCount; ++i)
    {
        ActionProfile action;
        packet >> action.name >> action.kind >> action.pattern >> action.state >> action.minRange >> action.maxRange
               >> action.damage >> action.statusTurns >> action.cooldownTurns
               >> action.canMove >> action.canAttack >> action.passThrough >> action.lineOfSight;
        if (!packet)
        {
            return false;
        }
        piece.actions.push_back(action);
    }
    return packet && card_data::readStringVector(packet, piece.abilityLabels);
}

inline void writePlayerSnapshot(sf::Packet& packet, const PlayerSnapshot& player)
{
    packet << player.steam << player.controlledSquares << player.handCount
           << player.heroesToPlace << player.heroesAlive << player.drawPileCount;
}

inline bool readPlayerSnapshot(sf::Packet& packet, PlayerSnapshot& player)
{
    packet >> player.steam >> player.controlledSquares >> player.handCount
           >> player.heroesToPlace >> player.heroesAlive >> player.drawPileCount;
    return static_cast<bool>(packet);
}

// Writes a complete snapshot payload (without the leading message-type byte).
inline void writeSnapshot(sf::Packet& packet, const Snapshot& snapshot)
{
    packet << snapshot.phase << snapshot.activePlayer << snapshot.yourPlayer << snapshot.winner;
    writePlayerSnapshot(packet, snapshot.players[0]);
    writePlayerSnapshot(packet, snapshot.players[1]);

    for (std::uint8_t control : snapshot.control)
    {
        packet << control;
    }
    for (std::uint8_t hole : snapshot.holes)
    {
        packet << hole;
    }

    packet << static_cast<std::uint32_t>(snapshot.pieces.size());
    for (const Piece& piece : snapshot.pieces)
    {
        writePiece(packet, piece);
    }

    packet << static_cast<std::uint32_t>(snapshot.hand.size());
    for (const GameCard& card : snapshot.hand)
    {
        writeGameCard(packet, card);
    }

    packet << snapshot.status;
}

inline bool readSnapshot(sf::Packet& packet, Snapshot& snapshot)
{
    packet >> snapshot.phase >> snapshot.activePlayer >> snapshot.yourPlayer >> snapshot.winner;
    if (!packet || !readPlayerSnapshot(packet, snapshot.players[0]) ||
        !readPlayerSnapshot(packet, snapshot.players[1]))
    {
        return false;
    }

    for (std::uint8_t& control : snapshot.control)
    {
        packet >> control;
        if (!packet)
        {
            return false;
        }
    }
    for (std::uint8_t& hole : snapshot.holes)
    {
        packet >> hole;
        if (!packet)
        {
            return false;
        }
    }

    std::uint32_t pieceCount = 0;
    packet >> pieceCount;
    if (!packet)
    {
        return false;
    }
    snapshot.pieces.clear();
    snapshot.pieces.reserve(pieceCount);
    for (std::uint32_t i = 0; i < pieceCount; ++i)
    {
        Piece piece;
        if (!readPiece(packet, piece))
        {
            return false;
        }
        snapshot.pieces.push_back(piece);
    }

    std::uint32_t handCount = 0;
    packet >> handCount;
    if (!packet)
    {
        return false;
    }
    snapshot.hand.clear();
    snapshot.hand.reserve(handCount);
    for (std::uint32_t i = 0; i < handCount; ++i)
    {
        GameCard card;
        if (!readGameCard(packet, card))
        {
            return false;
        }
        snapshot.hand.push_back(card);
    }

    packet >> snapshot.status;
    return static_cast<bool>(packet);
}

// ---- geometry helpers ------------------------------------------------------

inline int squareIndex(int row, int column)
{
    return row * BoardSize + column;
}

inline bool inBounds(int row, int column)
{
    return row >= 0 && row < BoardSize && column >= 0 && column < BoardSize;
}

inline int chebyshev(int r1, int c1, int r2, int c2)
{
    const int dr = r1 > r2 ? r1 - r2 : r2 - r1;
    const int dc = c1 > c2 ? c1 - c2 : c2 - c1;
    return dr > dc ? dr : dc;
}

// The four home squares a player starts controlling.
inline std::array<std::pair<int, int>, 4> homeSquares(int playerNumber)
{
    const int column = playerNumber == 1 ? 0 : BoardSize - 1;
    constexpr int FirstMiddleRow = (BoardSize - 4) / 2;
    return {{{FirstMiddleRow, column},
             {FirstMiddleRow + 1, column},
             {FirstMiddleRow + 2, column},
             {FirstMiddleRow + 3, column}}};
}

inline int absInt(int value)
{
    return value < 0 ? -value : value;
}

inline const Piece* findPieceAt(const std::vector<Piece>& pieces, int row, int column)
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

// Shared rules used by both the authoritative server and the client (for
// highlighting reachable squares). The server remains the source of truth.
inline bool hasLegalMoveGeometry(
    const std::vector<Piece>& pieces,
    const Piece& piece,
    int toRow,
    int toColumn,
    bool allowOccupiedDestination)
{
    if (!inBounds(toRow, toColumn) ||
        (!allowOccupiedDestination && findPieceAt(pieces, toRow, toColumn) != nullptr))
    {
        return false;
    }

    const int dr = toRow - piece.row;
    const int dc = toColumn - piece.column;
    if (dr == 0 && dc == 0)
    {
        return false;
    }

    const MovePattern pattern = static_cast<MovePattern>(piece.movePattern);
    if (pattern == MovePattern::None)
    {
        return false;
    }

    if (pattern == MovePattern::Jump)
    {
        const int adr = absInt(dr);
        const int adc = absInt(dc);
        return (adr == 1 && adc == 2) || (adr == 2 && adc == 1);
    }

    const bool straight = (dr == 0 || dc == 0);
    const bool diagonal = (absInt(dr) == absInt(dc));
    if (pattern == MovePattern::Ortho && !straight)
    {
        return false;
    }
    if (pattern == MovePattern::Diag && !diagonal)
    {
        return false;
    }
    if (pattern == MovePattern::Omni && !(straight || diagonal))
    {
        return false;
    }

    const int steps = absInt(dr) > absInt(dc) ? absInt(dr) : absInt(dc);
    if (steps > piece.moveRange)
    {
        return false;
    }

    const int stepR = (dr > 0) - (dr < 0);
    const int stepC = (dc > 0) - (dc < 0);
    int currentRow = piece.row + stepR;
    int currentColumn = piece.column + stepC;
    while (currentRow != toRow || currentColumn != toColumn)
    {
        if (findPieceAt(pieces, currentRow, currentColumn) != nullptr)
        {
            return false;
        }
        currentRow += stepR;
        currentColumn += stepC;
    }
    return true;
}

inline bool isLegalMove(const std::vector<Piece>& pieces, const Piece& piece, int toRow, int toColumn)
{
    return hasLegalMoveGeometry(pieces, piece, toRow, toColumn, false);
}

inline bool isLegalAttackingMove(
    const std::vector<Piece>& pieces,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    const Piece* target = findPieceAt(pieces, toRow, toColumn);
    return piece.attackingMove && target != nullptr && target->owner != piece.owner &&
        hasLegalMoveGeometry(pieces, piece, toRow, toColumn, true);
}

inline std::pair<int, int> attackingMoveFallbackSquare(const Piece& piece, int targetRow, int targetColumn)
{
    if (static_cast<MovePattern>(piece.movePattern) == MovePattern::Jump)
    {
        return {piece.row, piece.column};
    }

    const int stepR = (targetRow > piece.row) - (targetRow < piece.row);
    const int stepC = (targetColumn > piece.column) - (targetColumn < piece.column);
    return {targetRow - stepR, targetColumn - stepC};
}

inline bool isLegalAttack(const Piece& attacker, const Piece& target)
{
    if (target.owner == attacker.owner || attacker.attack <= 0)
    {
        return false;
    }
    return chebyshev(attacker.row, attacker.column, target.row, target.column) <= attacker.attackRange;
}

struct ActionResolution
{
    bool legal = false;
    bool moves = false;
    bool attacks = false;
    int actionIndex = -1;
    int targetId = 0;
    int damage = 0;
    int statusTurns = 0;
    int cooldownTurns = 0;
    int stagingRow = 0;
    int stagingColumn = 0;
};

inline bool actionPatternMatches(
    std::uint8_t patternValue,
    int deltaRow,
    int deltaColumn,
    int minRange,
    int maxRange)
{
    if (deltaRow == 0 && deltaColumn == 0)
    {
        return false;
    }

    const int absoluteRow = absInt(deltaRow);
    const int absoluteColumn = absInt(deltaColumn);
    const int distance = absoluteRow > absoluteColumn ? absoluteRow : absoluteColumn;
    if (distance < minRange || distance > maxRange)
    {
        return false;
    }

    const MovePattern pattern = static_cast<MovePattern>(patternValue);
    if (pattern == MovePattern::None)
    {
        return true;
    }
    if (pattern == MovePattern::Jump)
    {
        return (absoluteRow == 1 && absoluteColumn == 2) ||
               (absoluteRow == 2 && absoluteColumn == 1);
    }

    const bool straight = deltaRow == 0 || deltaColumn == 0;
    const bool diagonal = absoluteRow == absoluteColumn;
    if (pattern == MovePattern::Ortho)
    {
        return straight;
    }
    if (pattern == MovePattern::Diag)
    {
        return diagonal;
    }
    if (pattern == MovePattern::Horizontal)
    {
        return deltaRow == 0;
    }
    if (pattern == MovePattern::Vertical)
    {
        return deltaColumn == 0;
    }
    return straight || diagonal;
}

inline bool actionPathClear(
    const std::vector<Piece>& pieces,
    const Piece& piece,
    int toRow,
    int toColumn,
    bool passThrough)
{
    if (passThrough)
    {
        return true;
    }

    const int deltaRow = toRow - piece.row;
    const int deltaColumn = toColumn - piece.column;
    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
    int row = piece.row + stepRow;
    int column = piece.column + stepColumn;
    while (row != toRow || column != toColumn)
    {
        if (findPieceAt(pieces, row, column) != nullptr)
        {
            return false;
        }
        row += stepRow;
        column += stepColumn;
    }
    return true;
}

inline ActionResolution resolvePieceAction(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    ActionResolution best;
    if (!inBounds(toRow, toColumn) || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
    {
        return best;
    }

    const Piece* destination = findPieceAt(pieces, toRow, toColumn);
    const int deltaRow = toRow - piece.row;
    const int deltaColumn = toColumn - piece.column;

    for (std::size_t index = 0; index < piece.actions.size(); ++index)
    {
        const ActionProfile& action = piece.actions[index];
        if (action.state != piece.actionState)
        {
            continue;
        }

        ActionResolution candidate;
        candidate.actionIndex = static_cast<int>(index);
        candidate.damage = action.damage;
        candidate.statusTurns = action.statusTurns;
        candidate.cooldownTurns = action.cooldownTurns;
        candidate.stagingRow = piece.row;
        candidate.stagingColumn = piece.column;

        const ActionKind kind = static_cast<ActionKind>(action.kind);
        if (kind == ActionKind::Teleport)
        {
            if (action.canMove && destination == nullptr && (deltaRow != 0 || deltaColumn != 0))
            {
                candidate.legal = true;
                candidate.moves = true;
            }
        }
        else if (kind == ActionKind::Tunnel)
        {
            const std::size_t fromIndex = static_cast<std::size_t>(squareIndex(piece.row, piece.column));
            const std::size_t toIndex = static_cast<std::size_t>(squareIndex(toRow, toColumn));
            if (action.canMove && destination == nullptr && holes[fromIndex] != 0 && holes[toIndex] != 0 &&
                (deltaRow != 0 || deltaColumn != 0))
            {
                candidate.legal = true;
                candidate.moves = true;
            }
        }
        else if (kind == ActionKind::Hop)
        {
            const int absoluteRow = absInt(deltaRow);
            const int absoluteColumn = absInt(deltaColumn);
            const bool hopGeometry =
                (absoluteRow == 2 && deltaColumn == 0) ||
                (absoluteColumn == 2 && deltaRow == 0) ||
                (absoluteRow == 2 && absoluteColumn == 2);
            if (action.canMove && destination == nullptr && hopGeometry)
            {
                const Piece* pivot = findPieceAt(
                    pieces,
                    piece.row + deltaRow / 2,
                    piece.column + deltaColumn / 2);
                if (pivot != nullptr)
                {
                    candidate.legal = true;
                    candidate.moves = true;
                    if (action.canAttack && pivot->owner != piece.owner)
                    {
                        candidate.attacks = true;
                        candidate.targetId = pivot->id;
                    }
                }
            }
        }
        else if (kind == ActionKind::Ranged)
        {
            if (action.canAttack && destination != nullptr && destination->owner != piece.owner &&
                actionPatternMatches(
                    action.pattern,
                    deltaRow,
                    deltaColumn,
                    action.minRange,
                    action.maxRange) &&
                (!action.lineOfSight || actionPathClear(pieces, piece, toRow, toColumn, false)))
            {
                candidate.legal = true;
                candidate.attacks = true;
                candidate.targetId = destination->id;
            }
        }
        else
        {
            if (!actionPatternMatches(
                    action.pattern,
                    deltaRow,
                    deltaColumn,
                    action.minRange,
                    action.maxRange))
            {
                continue;
            }

            const bool jumping = static_cast<MovePattern>(action.pattern) == MovePattern::Jump;
            if (!jumping && !actionPathClear(pieces, piece, toRow, toColumn, action.passThrough))
            {
                continue;
            }

            if (destination == nullptr && action.canMove)
            {
                candidate.legal = true;
                candidate.moves = true;
            }
            else if (destination != nullptr && destination->owner != piece.owner && action.canAttack)
            {
                candidate.legal = true;
                candidate.attacks = true;
                candidate.moves = action.canMove;
                candidate.targetId = destination->id;
                if (!jumping && !action.passThrough)
                {
                    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
                    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
                    candidate.stagingRow = toRow - stepRow;
                    candidate.stagingColumn = toColumn - stepColumn;
                }
            }
        }

        if (!candidate.legal || (candidate.moves && piece.sleepTurnsRemaining > 0))
        {
            continue;
        }

        const int candidateImpact = candidate.attacks
            ? candidate.damage + candidate.statusTurns
            : 0;
        const int bestImpact = best.attacks ? best.damage + best.statusTurns : 0;
        if (!best.legal || candidateImpact > bestImpact)
        {
            best = candidate;
        }
    }

    return best;
}

inline bool isLegalPieceMove(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    const ActionResolution action = resolvePieceAction(pieces, holes, piece, toRow, toColumn);
    return action.legal && action.moves;
}

inline bool isLegalPieceAttack(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    const ActionResolution action = resolvePieceAction(pieces, holes, piece, toRow, toColumn);
    return action.legal && action.attacks;
}

inline std::string pieceAbilityLabel(const Piece& piece)
{
    if (piece.ability.empty())
    {
        return "";
    }
    if (!piece.abilityLabels.empty())
    {
        const std::size_t index = static_cast<std::size_t>(
            piece.actionState % static_cast<int>(piece.abilityLabels.size()));
        return piece.abilityLabels[index];
    }
    if (piece.ability == "dig")
    {
        return "Dig";
    }
    return "Use Ability";
}

inline bool pieceAbilityAvailable(const Piece& piece)
{
    if (piece.ability.empty() || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
    {
        return false;
    }
    if (piece.ability == "dig")
    {
        return piece.abilityUses != 0;
    }
    return true;
}
}
