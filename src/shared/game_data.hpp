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

inline std::string cardRarity(const card_data::Card& card)
{
    for (const card_data::KeyStringPair& item : card.stringValues)
    {
        if (item.key == "rarity")
        {
            if (item.value == "rare" || item.value == "legendary" || item.value == "token")
            {
                return item.value;
            }
            break;
        }
    }
    return "common";
}

inline bool isTokenCard(const card_data::Card& card)
{
    return cardRarity(card) == "token";
}

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
        if (isTokenCard(card))
        {
            return "Token card cannot be in a deck: " + card.title;
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
constexpr int MaxDiscardsPerTurn = 1;

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
    Tunnel = 4,
    Capture = 5
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
    if (value == "capture")
    {
        return static_cast<std::uint8_t>(ActionKind::Capture);
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
    std::string idleAnimPath;
    std::string attackAnimPath;
    std::string damagedAnimPath;
    std::string killedAnimPath;
    std::string tokenPath;
    std::string blueTokenPath;
    std::string redTokenPath;
    std::string blueWalkAnimPath;
    std::string redWalkAnimPath;
    std::string pieceBaseBluePath;
    std::string pieceBaseRedPath;
    bool separateBaseArt = false;
    int walkAnimFrames = 4;
    int idleAnimFrames = 4;
    int attackAnimFrames = 1;
    int damagedAnimFrames = 1;
    int killedAnimFrames = 1;
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
    std::string summonTitle;
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
    g.idleAnimPath = cardStr(card, "IdleAnim");
    g.attackAnimPath = cardStr(card, "AttackAnim");
    g.damagedAnimPath = cardStr(card, "DamagedAnim");
    g.killedAnimPath = cardStr(card, "KilledAnim");
    g.tokenPath = cardStr(card, "Token");
    g.blueTokenPath = cardStr(card, "TokenBlue");
    g.redTokenPath = cardStr(card, "TokenRed");
    g.blueWalkAnimPath = cardStr(card, "WalkAnimBlue");
    g.redWalkAnimPath = cardStr(card, "WalkAnimRed");
    g.pieceBaseBluePath = cardStr(card, "PieceBaseBlue");
    g.pieceBaseRedPath = cardStr(card, "PieceBaseRed");
    g.separateBaseArt = cardStr(card, "PieceArtStyle") == "separateBase";
    if (g.blueWalkAnimPath.empty() && g.redWalkAnimPath.empty())
    {
        g.blueWalkAnimPath = g.walkAnimPath;
        g.redWalkAnimPath = g.walkAnimPath;
    }
    g.walkAnimFrames = std::max(1, cardInt(card, "WalkAnimFrames", 4));
    g.idleAnimFrames = std::max(1, cardInt(card, "IdleAnimFrames", g.walkAnimFrames));
    g.attackAnimFrames = std::max(1, cardInt(card, "AttackAnimFrames", 1));
    g.damagedAnimFrames = std::max(1, cardInt(card, "DamagedAnimFrames", 1));
    g.killedAnimFrames = std::max(1, cardInt(card, "KilledAnimFrames", 1));
    g.cost = cardInt(card, "cost", 1);
    g.heroCost = cardInt(card, "heroCost", 0);
    g.health = cardInt(card, "health", 1);
    g.effect = cardStr(card, "effect", "none");
    g.target = cardStr(card, "target", "none");
    g.power = cardInt(card, "power", 0);
    g.canControl = cardInt(card, "canControl", 1) != 0;
    g.growTurns = cardInt(card, "growTurns", 0);
    g.ability = cardStr(card, "ability");
    g.summonTitle = cardStr(card, "summon");
    if (g.summonTitle.empty())
    {
        g.summonTitle = cardStr(card, "summonUnit");
    }
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
    std::string idleAnimPath;
    std::string attackAnimPath;
    std::string damagedAnimPath;
    std::string killedAnimPath;
    std::string tokenPath;
    std::string blueTokenPath;
    std::string redTokenPath;
    std::string blueWalkAnimPath;
    std::string redWalkAnimPath;
    std::string pieceBaseBluePath;
    std::string pieceBaseRedPath;
    bool separateBaseArt = false;
    int walkAnimFrames = 4;
    int idleAnimFrames = 4;
    int attackAnimFrames = 1;
    int damagedAnimFrames = 1;
    int killedAnimFrames = 1;
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
    std::string summonTitle;
    std::vector<std::string> abilityLabels;
    int abilityUses = 0;
    bool hidden = false;
    bool isHero = false;
    bool hasActed = false;
};

inline void populatePieceFromCard(Piece& piece, const GameCard& card, bool isHero)
{
    piece.name = card.title;
    piece.keywords = card.keywords;
    piece.imagePath = card.imagePath;
    piece.walkAnimPath = card.walkAnimPath;
    piece.idleAnimPath = card.idleAnimPath;
    piece.attackAnimPath = card.attackAnimPath;
    piece.damagedAnimPath = card.damagedAnimPath;
    piece.killedAnimPath = card.killedAnimPath;
    piece.tokenPath = card.tokenPath;
    piece.blueTokenPath = card.blueTokenPath;
    piece.redTokenPath = card.redTokenPath;
    piece.blueWalkAnimPath = card.blueWalkAnimPath;
    piece.redWalkAnimPath = card.redWalkAnimPath;
    piece.pieceBaseBluePath = card.pieceBaseBluePath;
    piece.pieceBaseRedPath = card.pieceBaseRedPath;
    piece.separateBaseArt = card.separateBaseArt;
    piece.walkAnimFrames = card.walkAnimFrames;
    piece.idleAnimFrames = card.idleAnimFrames;
    piece.attackAnimFrames = card.attackAnimFrames;
    piece.damagedAnimFrames = card.damagedAnimFrames;
    piece.killedAnimFrames = card.killedAnimFrames;
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
    piece.summonTitle = card.summonTitle;
    piece.abilityLabels = card.abilityLabels;
    piece.abilityUses = card.abilityUses;
    piece.isHero = isHero;
}

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
    int discardsThisTurn = 0;
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
    packet << card.imagePath << card.walkAnimPath << card.idleAnimPath
           << card.attackAnimPath << card.damagedAnimPath << card.killedAnimPath << card.tokenPath
           << card.blueTokenPath << card.redTokenPath
           << card.blueWalkAnimPath << card.redWalkAnimPath
           << card.pieceBaseBluePath << card.pieceBaseRedPath << card.separateBaseArt
           << card.walkAnimFrames << card.idleAnimFrames
           << card.attackAnimFrames << card.damagedAnimFrames << card.killedAnimFrames
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
    packet << card.ability << card.summonTitle;
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
    packet >> card.imagePath >> card.walkAnimPath >> card.idleAnimPath
           >> card.attackAnimPath >> card.damagedAnimPath >> card.killedAnimPath >> card.tokenPath
           >> card.blueTokenPath >> card.redTokenPath
           >> card.blueWalkAnimPath >> card.redWalkAnimPath
           >> card.pieceBaseBluePath >> card.pieceBaseRedPath >> card.separateBaseArt
           >> card.walkAnimFrames >> card.idleAnimFrames
           >> card.attackAnimFrames >> card.damagedAnimFrames >> card.killedAnimFrames
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
    packet >> card.ability >> card.summonTitle;
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
    packet << piece.imagePath << piece.walkAnimPath << piece.idleAnimPath
           << piece.attackAnimPath << piece.damagedAnimPath << piece.killedAnimPath << piece.tokenPath
           << piece.blueTokenPath << piece.redTokenPath
           << piece.blueWalkAnimPath << piece.redWalkAnimPath
           << piece.pieceBaseBluePath << piece.pieceBaseRedPath << piece.separateBaseArt
           << piece.walkAnimFrames << piece.idleAnimFrames
           << piece.attackAnimFrames << piece.damagedAnimFrames << piece.killedAnimFrames
           << piece.maxHealth << piece.health << piece.attack << piece.attackRange
           << piece.movePattern << piece.moveRange << piece.attackingMove
           << piece.canControl << piece.growTurnsRemaining << piece.disabledTurns << piece.sleepTurnsRemaining
           << piece.actionState << piece.ability << piece.summonTitle << piece.abilityUses << piece.hidden
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
    packet >> piece.imagePath >> piece.walkAnimPath >> piece.idleAnimPath
           >> piece.attackAnimPath >> piece.damagedAnimPath >> piece.killedAnimPath >> piece.tokenPath
           >> piece.blueTokenPath >> piece.redTokenPath
           >> piece.blueWalkAnimPath >> piece.redWalkAnimPath
           >> piece.pieceBaseBluePath >> piece.pieceBaseRedPath >> piece.separateBaseArt
           >> piece.walkAnimFrames >> piece.idleAnimFrames
           >> piece.attackAnimFrames >> piece.damagedAnimFrames >> piece.killedAnimFrames
           >> piece.maxHealth >> piece.health >> piece.attack >> piece.attackRange
           >> piece.movePattern >> piece.moveRange >> piece.attackingMove
           >> piece.canControl >> piece.growTurnsRemaining >> piece.disabledTurns >> piece.sleepTurnsRemaining
           >> piece.actionState >> piece.ability >> piece.summonTitle >> piece.abilityUses >> piece.hidden
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
           << player.heroesToPlace << player.heroesAlive << player.drawPileCount
           << player.discardsThisTurn;
}

inline bool readPlayerSnapshot(sf::Packet& packet, PlayerSnapshot& player)
{
    packet >> player.steam >> player.controlledSquares >> player.handCount
           >> player.heroesToPlace >> player.heroesAlive >> player.drawPileCount
           >> player.discardsThisTurn;
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
}

#include "game_rules.hpp"
