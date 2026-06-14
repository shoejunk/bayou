#pragma once

#include <SFML/Network.hpp>

#include "card_data.hpp"

#include <array>
#include <cstdint>
#include <string>
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
constexpr int MinHeroes = 1;
constexpr int MaxHeroes = 4;
constexpr int HeroCostLimit = 10;

// Hand / steam tuning.
constexpr int StartingHandSize = 4;
constexpr int MaxHandSize = 8;

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
    Jump = 4    // knight-like, fixed L offsets
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
    int cost = 1;          // steam cost (units / spells)
    int heroCost = 0;      // hero-cost budget contribution (heroes)
    int health = 1;
    int attack = 0;
    int attackRange = 1;
    std::uint8_t movePattern = static_cast<std::uint8_t>(MovePattern::Omni);
    int moveRange = 1;
    std::string effect = "none";  // spells: "damage", "heal", "steam"
    std::string target = "none";  // spells: "enemy", "ally", "none"
    int power = 0;                // spell magnitude
};

inline GameCard toGameCard(const card_data::Card& card)
{
    GameCard g;
    g.title = card.title;
    g.type = card.type;
    g.cost = cardInt(card, "cost", 1);
    g.heroCost = cardInt(card, "heroCost", 0);
    g.health = cardInt(card, "health", 1);
    g.attack = cardInt(card, "attack", 0);
    g.attackRange = cardInt(card, "range", 1);
    g.movePattern = parseMovePattern(cardStr(card, "movement", "omni"));
    g.moveRange = cardInt(card, "move", 1);
    g.effect = cardStr(card, "effect", "none");
    g.target = cardStr(card, "target", "none");
    g.power = cardInt(card, "power", 0);
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
    int maxHealth = 1;
    int health = 1;
    int attack = 0;
    int attackRange = 1;
    std::uint8_t movePattern = static_cast<std::uint8_t>(MovePattern::Omni);
    int moveRange = 1;
    bool isHero = false;
    bool hasActed = false;
};

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
    std::vector<Piece> pieces;
    std::vector<GameCard> hand;  // recipient's hand
    std::string status;
};

// ---- serialization helpers -------------------------------------------------

inline void writeGameCard(sf::Packet& packet, const GameCard& card)
{
    packet << card.title << card.type << card.cost << card.heroCost
           << card.health << card.attack << card.attackRange
           << card.movePattern << card.moveRange
           << card.effect << card.target << card.power;
}

inline bool readGameCard(sf::Packet& packet, GameCard& card)
{
    packet >> card.title >> card.type >> card.cost >> card.heroCost
           >> card.health >> card.attack >> card.attackRange
           >> card.movePattern >> card.moveRange
           >> card.effect >> card.target >> card.power;
    return static_cast<bool>(packet);
}

inline void writePiece(sf::Packet& packet, const Piece& piece)
{
    packet << piece.id << piece.owner << piece.row << piece.column << piece.name
           << piece.maxHealth << piece.health << piece.attack << piece.attackRange
           << piece.movePattern << piece.moveRange << piece.isHero << piece.hasActed;
}

inline bool readPiece(sf::Packet& packet, Piece& piece)
{
    packet >> piece.id >> piece.owner >> piece.row >> piece.column >> piece.name
           >> piece.maxHealth >> piece.health >> piece.attack >> piece.attackRange
           >> piece.movePattern >> piece.moveRange >> piece.isHero >> piece.hasActed;
    return static_cast<bool>(packet);
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
    const int row = playerNumber == 1 ? 0 : BoardSize - 1;
    return {{{row, 2}, {row, 3}, {row, 4}, {row, 5}}};
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
inline bool isLegalMove(const std::vector<Piece>& pieces, const Piece& piece, int toRow, int toColumn)
{
    if (!inBounds(toRow, toColumn) || findPieceAt(pieces, toRow, toColumn) != nullptr)
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

inline bool isLegalAttack(const Piece& attacker, const Piece& target)
{
    if (target.owner == attacker.owner || attacker.attack <= 0)
    {
        return false;
    }
    return chebyshev(attacker.row, attacker.column, target.row, target.column) <= attacker.attackRange;
}
}
