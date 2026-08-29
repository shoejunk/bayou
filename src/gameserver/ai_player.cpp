#include "ai_player.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

// The planner works in three layers.
//
//  * Action generation lists everything the acting player may legally do right
//    now, resolved against the board that player can actually see.
//  * Turn expansion strings those actions into whole turns with a beam search,
//    because a turn is not one action: a player may deploy, draw, take one
//    piece action and then pass.
//  * Turn-level minimax scores each candidate turn by what the opponent can do
//    to it afterwards, which is the only way to notice that a move hangs a
//    hero.
//
// Everything below plans on a redacted copy of the engine (see
// GameEngine::redactForPlanning), so opposing dematerialized pieces are simply
// not there. The AI has to bump into them like anybody else.

namespace
{
// ---- evaluation weights ---------------------------------------------------
// One point of unit health is the unit of account; every other weight is
// expressed relative to it.
constexpr int UnitHealthValue = 5;
constexpr int HeroHealthValue = 20;
constexpr int HeroAliveValue = 1500;
constexpr int ActionDamageValue = 4;
constexpr int ActionHealValue = 3;
constexpr int ActionStatusValue = 3;
constexpr int ActionControlValue = 6;
constexpr int ActionPushValue = 2;
constexpr int ActionReachValue = 2;
constexpr int ControlledSquareValue = 7;
constexpr int HiddenPieceValue = 25;
constexpr int ImpairedTurnPenalty = 20;
constexpr int UnitAdvanceValue = 4;
constexpr int HeroAdvancePenalty = 2;
constexpr int CentreRowValue = 6;
constexpr int PassiveIncomeValue = 6;
constexpr int HealingAuraValue = 8;
constexpr int LastHeroInPerilPenalty = 40000;
constexpr int WinScore = 1000000;
constexpr int WorstScore = -WinScore * 8;
constexpr int BestScore = WinScore * 8;

// Resources only matter until there are enough of them to act on; a hoard past
// that point is worth less than what it could have bought, which is what makes
// paying for a draw look sensible when the hand has run dry.
constexpr int ComfortableResources = 120;

// A card in hand is a piece waiting for resources. The first two are worth the
// most because the hand caps out at MaxHandSize.
constexpr int FirstHandCardValue = 22;
constexpr int ExtraHandCardValue = 10;
constexpr int AffordableCardBonus = 8;
constexpr int EmptyHandPenalty = 45;

// Threats are discounted for the side about to move, which still gets a turn to
// step out of the way, and counted nearly in full against the side that does
// not.
constexpr int ThreatWeightToMove = 20;
constexpr int ThreatWeightWaiting = 75;

// A turn is at most a handful of actions; the cap only guards against a chain
// of Command or Relentless activations running away inside the search.
constexpr int MaxTurnActions = 10;

// ---- search shape ---------------------------------------------------------
struct SearchProfile
{
    int beamWidth = 3;
    int maxCandidates = 32;
    int maxPlans = 6;
};

constexpr SearchProfile RootProfile{4, 44, 8};
constexpr SearchProfile OwnTurnProfile{3, 28, 5};
constexpr SearchProfile OpponentTurnProfile{3, 32, 6};

struct AiContext
{
    int aiPlayer = 1;
    int opponent = 2;
    // Opposing heroes the redaction removed. Their existence is public even
    // though their squares are not, so the score has to keep counting them.
    int concealedOpponentHeroes = 0;
    long long nodes = 0;
    long long nodeBudget = 0;
};

bool budgetExhausted(const AiContext& context)
{
    return context.nodes >= context.nodeBudget;
}

// ---- piece appraisal ------------------------------------------------------
int actionSquareReach(const ActionProfile& action)
{
    switch (static_cast<ActionKind>(action.kind))
    {
        case ActionKind::Teleport:
        case ActionKind::Tunnel:
            return BoardSize;
        case ActionKind::Hop:
            return 2;
        default:
            break;
    }
    if (static_cast<MovePattern>(action.pattern) == MovePattern::Jump)
    {
        return 2;
    }
    return std::max(1, action.maxRange);
}

// How far from its own square a piece can name a destination this turn. Used
// to bound square scans; it never under-reports.
int pieceSquareReach(const Piece& piece)
{
    int reach = 0;
    for (const ActionProfile& action : piece.actions)
    {
        if (action.state == piece.actionState)
        {
            reach = std::max(reach, actionSquareReach(action));
        }
    }
    return reach;
}

int pieceBestDamage(const Piece& piece)
{
    int damage = 0;
    for (const ActionProfile& action : piece.actions)
    {
        if (action.state == piece.actionState && action.canAttack)
        {
            damage = std::max(damage, action.damage);
        }
    }
    return damage;
}

int pieceActionPower(const Piece& piece)
{
    int best = 0;
    for (const ActionProfile& action : piece.actions)
    {
        if (action.state != piece.actionState)
        {
            continue;
        }
        int power = action.damage * ActionDamageValue + action.heal * ActionHealValue +
            action.statusTurns * ActionStatusValue + action.control * ActionControlValue +
            action.push * ActionPushValue;
        if (action.canMove)
        {
            power += std::min(action.maxRange, 4) * ActionReachValue;
        }
        best = std::max(best, power);
    }
    return best;
}

int pieceWorth(const Piece& piece)
{
    const int healthValue = piece.isHero ? HeroHealthValue : UnitHealthValue;
    int worth = std::max(0, piece.health) * healthValue + pieceActionPower(piece);
    if (piece.isHero)
    {
        worth += HeroAliveValue;
    }
    worth += (piece.gatherResources + piece.tax) * PassiveIncomeValue;
    worth += piece.healingAura * HealingAuraValue;
    if (piece.hidden)
    {
        worth += HiddenPieceValue;
    }
    worth -= (piece.disabledTurns + piece.growTurnsRemaining) * ImpairedTurnPenalty;
    return worth;
}

// Where a piece stands, independent of what it is. Units are pushed toward the
// opponent's edge because squares are the income; heroes are nudged the other
// way, since losing the last one ends the match.
int piecePlacementValue(const Piece& piece)
{
    const int advance = piece.owner == 1 ? piece.column : BoardSize - 1 - piece.column;
    const int centreRow = std::min(piece.row, BoardSize - 1 - piece.row);
    const int centring = std::min(centreRow, 3) * CentreRowValue;
    return centring + advance * (piece.isHero ? -HeroAdvancePenalty : UnitAdvanceValue);
}

int resourceValue(int resources)
{
    const int clamped = std::max(0, resources);
    if (clamped <= ComfortableResources)
    {
        return clamped;
    }
    return ComfortableResources + (clamped - ComfortableResources) * 2 / 5;
}

int handValue(const GameEngine& engine, int playerNumber)
{
    const GameEngine::EnginePlayer& player = engine.playerState(playerNumber);
    int value = 0;
    int index = 0;
    for (const GameCard& card : player.hand)
    {
        value += index < 2 ? FirstHandCardValue : ExtraHandCardValue;
        if (card.cost <= player.resources &&
            heroTraitsAllowCard(engine.boardPieces(), playerNumber, card))
        {
            value += AffordableCardBonus;
        }
        ++index;
    }
    if (player.hand.empty() && !player.drawPile.empty())
    {
        value -= EmptyHandPenalty;
    }
    return value;
}

// ---- static evaluation ----------------------------------------------------
int terminalScore(const GameEngine& engine, const AiContext& context, bool& terminal)
{
    terminal = engine.phase() == Phase::GameOver;
    if (!terminal)
    {
        return 0;
    }
    if (engine.winner() == context.aiPlayer)
    {
        // A win declared while an opposing hero is still concealed is only a
        // win of the redacted board: the hidden hero keeps the match alive.
        return context.concealedOpponentHeroes > 0 ? WinScore / 8 : WinScore;
    }
    if (engine.winner() == context.opponent)
    {
        return -WinScore;
    }
    return 0;
}

// Material, position and economy. Deliberately cheap: the beam calls it for
// every candidate action it considers.
int quickEvaluate(const GameEngine& engine, const AiContext& context)
{
    bool terminal = false;
    const int decided = terminalScore(engine, context, terminal);
    if (terminal)
    {
        return decided;
    }

    int score = 0;
    for (const Piece& piece : engine.boardPieces())
    {
        const int worth = pieceWorth(piece) + piecePlacementValue(piece);
        score += piece.owner == context.aiPlayer ? worth : -worth;
    }
    // Heroes the planner cannot see still hold the match open for the opponent.
    score -= context.concealedOpponentHeroes * (HeroAliveValue + 10 * HeroHealthValue);

    score += (engine.controlledSquares(context.aiPlayer) -
              engine.controlledSquares(context.opponent)) *
        ControlledSquareValue;
    score += resourceValue(engine.playerState(context.aiPlayer).resources) -
        resourceValue(engine.playerState(context.opponent).resources);
    // The opponent's hand is redacted away, so only this side's is appraised.
    score += handValue(engine, context.aiPlayer);
    return score;
}

// Most damage each piece can take from a single enemy action next turn. Only
// one piece may take a normal action per turn, so the worst case is a maximum
// over attackers rather than a sum.
std::vector<int> incomingDamage(const GameEngine& engine)
{
    const std::vector<Piece>& pieces = engine.boardPieces();
    std::vector<int> incoming(pieces.size(), 0);
    if (pieces.empty())
    {
        return incoming;
    }

    const std::array<std::vector<Piece>, 2> views = {
        piecesVisibleTo(pieces, 1), piecesVisibleTo(pieces, 2)};
    for (const Piece& attacker : pieces)
    {
        if (attacker.growTurnsRemaining > 0 || attacker.disabledTurns > 0 ||
            attacker.owner < 1 || attacker.owner > 2 || pieceBestDamage(attacker) <= 0)
        {
            continue;
        }
        const std::vector<Piece>& view = views[static_cast<std::size_t>(attacker.owner - 1)];
        const int reach = pieceSquareReach(attacker) + std::max(attacker.width, attacker.height);
        const int bonus = pieceEnchantmentDamageBonus(engine.boardEnchantments(), attacker.id);
        for (std::size_t index = 0; index < pieces.size(); ++index)
        {
            const Piece& victim = pieces[index];
            // A dematerialized piece is not a target its opponent can pick.
            if (victim.owner == attacker.owner || victim.hidden)
            {
                continue;
            }
            if (chebyshev(attacker.row, attacker.column, victim.row, victim.column) > reach)
            {
                continue;
            }
            const ActionResolution resolution = resolvePieceAction(
                view, engine.boardHoles(), attacker, victim.row, victim.column);
            if (!resolution.legal || !resolution.attacks ||
                std::find(resolution.targetIds.begin(), resolution.targetIds.end(), victim.id) ==
                    resolution.targetIds.end())
            {
                continue;
            }
            incoming[index] = std::max(incoming[index], resolution.damage + bonus);
        }
    }
    return incoming;
}

// quickEvaluate plus what the position is about to cost. This is what search
// leaves are scored with, and it is the term that stops the AI parking a hero
// where anything can reach it.
int evaluate(const GameEngine& engine, const AiContext& context)
{
    bool terminal = false;
    const int decided = terminalScore(engine, context, terminal);
    if (terminal)
    {
        return decided;
    }

    int score = quickEvaluate(engine, context);
    const std::vector<Piece>& pieces = engine.boardPieces();
    const std::vector<int> incoming = incomingDamage(engine);
    const int sideToMove = engine.currentPlayer();
    std::array<int, 2> heroCount{0, 0};
    for (const Piece& piece : pieces)
    {
        if (piece.isHero)
        {
            const int owner = pieceOriginalOwner(piece);
            if (owner >= 1 && owner <= 2)
            {
                ++heroCount[static_cast<std::size_t>(owner - 1)];
            }
        }
    }
    heroCount[static_cast<std::size_t>(context.opponent - 1)] += context.concealedOpponentHeroes;

    // Only one piece may take a normal action per turn, so exposure costs what
    // the single worst answer takes, not the sum over everything in reach. The
    // rest is folded in at a fraction: standing several pieces in range is
    // still worse than standing one there.
    std::array<int, 2> worstRisk{0, 0};
    std::array<int, 2> totalRisk{0, 0};
    for (std::size_t index = 0; index < pieces.size(); ++index)
    {
        const int damage = incoming[index];
        if (damage <= 0)
        {
            continue;
        }
        const Piece& piece = pieces[index];
        if (piece.owner < 1 || piece.owner > 2)
        {
            continue;
        }
        const bool lethal = damage >= piece.health;
        int risk = lethal
            ? pieceWorth(piece)
            : damage * (piece.isHero ? HeroHealthValue : UnitHealthValue);
        if (lethal && piece.isHero &&
            heroCount[static_cast<std::size_t>(pieceOriginalOwner(piece) - 1)] <= 1)
        {
            risk += LastHeroInPerilPenalty;
        }
        const std::size_t side = static_cast<std::size_t>(piece.owner - 1);
        worstRisk[side] = std::max(worstRisk[side], risk);
        totalRisk[side] += risk;
    }

    for (int owner = 1; owner <= 2; ++owner)
    {
        const std::size_t side = static_cast<std::size_t>(owner - 1);
        const int exposure = worstRisk[side] + (totalRisk[side] - worstRisk[side]) / 8;
        const int weight = owner == sideToMove ? ThreatWeightToMove : ThreatWeightWaiting;
        const int penalty = exposure * weight / 100;
        score += owner == context.aiPlayer ? -penalty : penalty;
    }
    return score;
}

// ---- candidate actions ----------------------------------------------------
struct AiCandidate
{
    AiAction action;
    int order = 0;
};

int cardPromise(const GameCard& card)
{
    if (card.type == "Unit")
    {
        int promise = card.health * UnitHealthValue;
        for (const ActionProfile& action : card.actions)
        {
            promise += action.damage * ActionDamageValue + action.heal * ActionHealValue;
        }
        return promise;
    }
    if (card.type == "Spell" || card.type == "Enchantment")
    {
        return card.power * 6;
    }
    return card.cost;
}

bool canDeployCard(const GameEngine& engine, int playerNumber, const GameCard& card, int row, int column)
{
    if (row < 0 || column < 0 || row + card.height > BoardSize || column + card.width > BoardSize)
    {
        return false;
    }
    for (int r = row; r < row + card.height; ++r)
    {
        for (int c = column; c < column + card.width; ++c)
        {
            const std::size_t index = static_cast<std::size_t>(squareIndex(r, c));
            if (engine.boardControl()[index] != playerNumber ||
                findPieceAt(engine.boardPieces(), r, c) != nullptr)
            {
                return false;
            }
        }
    }
    return true;
}

int abilityOrderBonus(const Piece& piece)
{
    const std::string ability = normalizedAbility(piece.ability);
    if (ability == "summon")
    {
        // A summon is a free piece; nothing else on a normal turn matches it.
        return 900;
    }
    if (ability == "command")
    {
        return 500;
    }
    if (ability == "dematerialize")
    {
        return piece.hidden ? 60 : 200;
    }
    if (ability == "transform")
    {
        return 120;
    }
    if (ability == "dig")
    {
        return 40;
    }
    return 30;
}

std::vector<AiCandidate> generateCandidates(
    const GameEngine& engine, int playerNumber, bool allowCards, int maxCandidates)
{
    std::vector<AiCandidate> candidates;
    if (engine.phase() != Phase::Playing || engine.currentPlayer() != playerNumber)
    {
        return candidates;
    }

    const std::vector<Piece>& pieces = engine.boardPieces();
    const std::array<std::uint8_t, BoardSquares>& holes = engine.boardHoles();
    // Plan against the board this player can see: an opposing dematerialized
    // piece neither blocks nor offers a target, and the engine adjudicates the
    // collision if the action runs into one.
    const std::vector<Piece> visiblePieces = piecesVisibleTo(pieces, playerNumber);

    const auto pieceById = [&](int id) -> const Piece* {
        const auto found = std::find_if(
            pieces.begin(), pieces.end(), [&](const Piece& piece) { return piece.id == id; });
        return found == pieces.end() ? nullptr : &*found;
    };
    const Piece* commander = pieceById(engine.commandingPiece());
    const int relentlessPieceId = engine.relentlessPiece();
    const auto repeating = std::find_if(pieces.begin(), pieces.end(), [&](const Piece& piece) {
        return piece.owner == playerNumber && piece.repeatActionIndex >= 0;
    });
    const int repeatingPieceId = repeating == pieces.end() ? 0 : repeating->id;
    const bool normalPieceActionAvailable =
        !engine.playerState(playerNumber).pieceActionUsedThisTurn;

    for (const Piece& piece : pieces)
    {
        if (piece.owner != playerNumber || piece.hasActed || piece.growTurnsRemaining > 0 ||
            piece.disabledTurns > 0)
        {
            continue;
        }
        if (repeatingPieceId != 0 && piece.id != repeatingPieceId)
        {
            continue;
        }
        if (relentlessPieceId != 0 && piece.id != relentlessPieceId)
        {
            continue;
        }
        if (commander != nullptr && !pieceCanReceiveCommand(*commander, piece))
        {
            continue;
        }
        if (!normalPieceActionAvailable && commander == nullptr && relentlessPieceId == 0 &&
            piece.repeatActionIndex < 0)
        {
            continue;
        }

        if (piece.repeatActionIndex < 0 && pieceAbilityAvailable(pieces, piece))
        {
            candidates.push_back({{AiActionKind::UseAbility, piece.id}, abilityOrderBonus(piece)});
        }

        const int reach = pieceSquareReach(piece);
        const int firstRow = std::max(0, piece.row - reach);
        const int lastRow = std::min(BoardSize - 1, piece.row + reach);
        const int firstColumn = std::max(0, piece.column - reach);
        const int lastColumn = std::min(BoardSize - 1, piece.column + reach);
        for (int row = firstRow; row <= lastRow; ++row)
        {
            for (int column = firstColumn; column <= lastColumn; ++column)
            {
                const ActionResolution resolution = resolvePieceAction(
                    visiblePieces, holes, piece, row, column, false, piece.repeatActionIndex);
                if (!resolution.legal)
                {
                    continue;
                }

                int order = 0;
                if (resolution.attacks)
                {
                    order = 200 + resolution.damage * 12 + resolution.heal * 8 +
                        resolution.statusTurns * 10 + resolution.control * 30 +
                        resolution.push * 6;
                    for (int targetId : resolution.targetIds)
                    {
                        const Piece* target = pieceById(targetId);
                        if (target == nullptr || target->owner == playerNumber)
                        {
                            continue;
                        }
                        if (resolution.damage >= target->health)
                        {
                            order += target->isHero ? 4000 : 600;
                        }
                        else if (target->isHero)
                        {
                            order += 250;
                        }
                    }
                }
                else
                {
                    Piece moved = piece;
                    moved.row = row;
                    moved.column = column;
                    order = piecePlacementValue(moved) - piecePlacementValue(piece);
                }
                candidates.push_back(
                    {{resolution.attacks ? AiActionKind::AttackPiece : AiActionKind::MovePiece,
                      piece.id,
                      0,
                      row,
                      column},
                     order});
            }
        }
    }

    const bool cardsAllowed = allowCards && commander == nullptr && relentlessPieceId == 0 &&
        repeatingPieceId == 0;
    if (cardsAllowed)
    {
        const GameEngine::EnginePlayer& player = engine.playerState(playerNumber);
        for (int handIndex = 0; handIndex < static_cast<int>(player.hand.size()); ++handIndex)
        {
            const GameCard& card = player.hand[static_cast<std::size_t>(handIndex)];
            if (card.cost > player.resources || !heroTraitsAllowCard(pieces, playerNumber, card))
            {
                continue;
            }
            const int promise = cardPromise(card) - card.cost;
            if (card.type == "Unit")
            {
                for (int row = 0; row < BoardSize; ++row)
                {
                    for (int column = 0; column < BoardSize; ++column)
                    {
                        if (!canDeployCard(engine, playerNumber, card, row, column))
                        {
                            continue;
                        }
                        Piece deployed;
                        deployed.owner = playerNumber;
                        deployed.row = row;
                        deployed.column = column;
                        candidates.push_back(
                            {{AiActionKind::PlayCard, 0, handIndex, row, column},
                             150 + promise + piecePlacementValue(deployed)});
                    }
                }
            }
            else if (card.type == "Spell")
            {
                if (isResourcesEffect(card))
                {
                    candidates.push_back(
                        {{AiActionKind::PlayCard, 0, handIndex, 0, 0},
                         resourceValue(card.power) - card.cost});
                }
                else
                {
                    for (const Piece& target : visiblePieces)
                    {
                        if (card.effect == "damage" && target.owner != playerNumber)
                        {
                            const bool kills = card.power >= target.health;
                            candidates.push_back(
                                {{AiActionKind::PlayCard, 0, handIndex, target.row, target.column},
                                 100 + card.power * 10 +
                                     (kills ? (target.isHero ? 4000 : 600) : 0)});
                        }
                        else if (
                            card.effect == "heal" && target.owner == playerNumber &&
                            target.health < target.maxHealth)
                        {
                            const int healed = std::min(card.power, target.maxHealth - target.health);
                            candidates.push_back(
                                {{AiActionKind::PlayCard, 0, handIndex, target.row, target.column},
                                 healed * (target.isHero ? HeroHealthValue : UnitHealthValue)});
                        }
                    }
                }
            }
            else if (card.type == "Enchantment")
            {
                if (card.target == "player" && card.effect == "resourceDrain")
                {
                    candidates.push_back(
                        {{AiActionKind::PlayCard, 0, handIndex, -1, playerNumber == 1 ? 2 : 1},
                         120 + card.power * 8});
                }
                else if (card.target == "square" && card.effect == "resources")
                {
                    for (int row = 0; row < BoardSize; ++row)
                    {
                        for (int column = 0; column < BoardSize; ++column)
                        {
                            const std::size_t index = static_cast<std::size_t>(squareIndex(row, column));
                            if (engine.boardControl()[index] == playerNumber &&
                                engine.boardHoles()[index] == 0)
                            {
                                candidates.push_back(
                                    {{AiActionKind::PlayCard, 0, handIndex, row, column},
                                     100 + card.power * 8});
                            }
                        }
                    }
                }
                else if (card.target == "piece" && card.effect == "damage")
                {
                    for (const Piece& target : visiblePieces)
                    {
                        if (target.owner == playerNumber && pieceBestDamage(target) > 0)
                        {
                            candidates.push_back(
                                {{AiActionKind::PlayCard, 0, handIndex, target.row, target.column},
                                 80 + card.power * 8});
                        }
                    }
                }
            }
        }

        if (player.resources >= DrawCardResourceCost &&
            static_cast<int>(player.hand.size()) < MaxHandSize && !player.drawPile.empty())
        {
            candidates.push_back({{AiActionKind::DrawCard}, player.hand.empty() ? 300 : 20});
        }

        // Cycling is only worth an action when a card is dead weight: too
        // expensive to ever cast, or blocked by the traits of the living heroes.
        if (player.discardsThisTurn < MaxDiscardsPerTurn &&
            static_cast<int>(player.drawPile.size()) > 1)
        {
            for (int handIndex = 0; handIndex < static_cast<int>(player.hand.size()); ++handIndex)
            {
                const GameCard& card = player.hand[static_cast<std::size_t>(handIndex)];
                if (!heroTraitsAllowCard(pieces, playerNumber, card))
                {
                    candidates.push_back({{AiActionKind::DiscardCard, 0, handIndex}, 60});
                }
            }
        }
    }

    candidates.push_back({{AiActionKind::EndTurn}, 0});
    std::stable_sort(
        candidates.begin(), candidates.end(), [](const AiCandidate& left, const AiCandidate& right) {
            return left.order > right.order;
        });
    if (static_cast<int>(candidates.size()) > maxCandidates)
    {
        // Passing must survive the cut: it is the only action that always works.
        const bool keepsEndTurn = std::any_of(
            candidates.begin(),
            candidates.begin() + maxCandidates,
            [](const AiCandidate& candidate) {
                return candidate.action.kind == AiActionKind::EndTurn;
            });
        candidates.resize(static_cast<std::size_t>(maxCandidates));
        if (!keepsEndTurn)
        {
            candidates.back() = {{AiActionKind::EndTurn}, 0};
        }
    }
    return candidates;
}

// ---- turn expansion -------------------------------------------------------
struct TurnPlan
{
    AiAction firstAction;
    GameEngine after;
    int quick = 0;
};

struct BeamState
{
    GameEngine engine;
    AiAction firstAction;
    bool started = false;
    int quick = 0;
};

// Builds whole turns for `player`, ending each with a pass, and returns the
// positions they lead to. A beam keeps the search from exploding across every
// ordering of the deploys, draws and piece action a single turn allows.
std::vector<TurnPlan> expandTurn(
    const GameEngine& engine,
    int player,
    AiContext& context,
    const SearchProfile& profile,
    bool allowCards)
{
    std::vector<TurnPlan> plans;
    if (engine.phase() != Phase::Playing || engine.currentPlayer() != player)
    {
        return plans;
    }

    const int sign = player == context.aiPlayer ? 1 : -1;
    std::vector<BeamState> frontier;
    frontier.push_back({engine, AiAction{}, false, 0});

    for (int step = 0; step < MaxTurnActions && !frontier.empty(); ++step)
    {
        std::vector<BeamState> next;
        for (BeamState& state : frontier)
        {
            if (budgetExhausted(context))
            {
                break;
            }
            const std::vector<AiCandidate> candidates =
                generateCandidates(state.engine, player, allowCards, profile.maxCandidates);
            for (const AiCandidate& candidate : candidates)
            {
                if (budgetExhausted(context))
                {
                    break;
                }
                GameEngine child = state.engine;
                if (!applyAiAction(child, player, candidate.action))
                {
                    continue;
                }
                ++context.nodes;
                const AiAction first = state.started ? state.firstAction : candidate.action;
                const bool turnOver =
                    child.phase() != Phase::Playing || child.currentPlayer() != player;
                const int quick = quickEvaluate(child, context);
                if (turnOver)
                {
                    plans.push_back({first, std::move(child), quick});
                }
                else
                {
                    next.push_back({std::move(child), first, true, quick});
                }
            }
        }
        if (next.empty())
        {
            break;
        }
        std::stable_sort(
            next.begin(), next.end(), [sign](const BeamState& left, const BeamState& right) {
                return sign * left.quick > sign * right.quick;
            });
        if (static_cast<int>(next.size()) > profile.beamWidth)
        {
            next.erase(next.begin() + profile.beamWidth, next.end());
        }
        frontier = std::move(next);
    }

    // Anything still mid-turn when the cap or the budget ran out is finished by
    // passing, so every plan describes a complete turn.
    for (BeamState& state : frontier)
    {
        if (!state.started)
        {
            continue;
        }
        GameEngine child = std::move(state.engine);
        if (child.phase() == Phase::Playing && child.currentPlayer() == player &&
            !child.endTurn(player))
        {
            continue;
        }
        const int quick = quickEvaluate(child, context);
        plans.push_back({state.firstAction, std::move(child), quick});
    }

    std::stable_sort(plans.begin(), plans.end(), [sign](const TurnPlan& left, const TurnPlan& right) {
        return sign * left.quick > sign * right.quick;
    });
    if (static_cast<int>(plans.size()) > profile.maxPlans)
    {
        plans.erase(plans.begin() + profile.maxPlans, plans.end());
    }
    return plans;
}

// ---- turn-level minimax ---------------------------------------------------
int searchFromTurn(
    const GameEngine& engine, int turnsLeft, int alpha, int beta, AiContext& context)
{
    if (turnsLeft <= 0 || engine.phase() != Phase::Playing || budgetExhausted(context))
    {
        return evaluate(engine, context);
    }

    const int player = engine.currentPlayer();
    const bool maximizing = player == context.aiPlayer;
    const std::vector<TurnPlan> plans = expandTurn(
        engine,
        player,
        context,
        maximizing ? OwnTurnProfile : OpponentTurnProfile,
        maximizing);
    if (plans.empty())
    {
        return evaluate(engine, context);
    }

    int best = maximizing ? WorstScore : BestScore;
    for (const TurnPlan& plan : plans)
    {
        const int score = searchFromTurn(plan.after, turnsLeft - 1, alpha, beta, context);
        if (maximizing)
        {
            best = std::max(best, score);
            alpha = std::max(alpha, best);
        }
        else
        {
            best = std::min(best, score);
            beta = std::min(beta, best);
        }
        if (beta <= alpha)
        {
            break;
        }
    }
    return best;
}

int cardKeepValue(const GameCard& card, const GameEngine& engine, int playerNumber)
{
    const GameEngine::EnginePlayer& player = engine.playerState(playerNumber);
    int value = cardPromise(card) - card.cost;
    if (card.cost > player.resources)
    {
        value -= 40;
    }
    if (!heroTraitsAllowCard(engine.boardPieces(), playerNumber, card))
    {
        value -= 400;
    }
    return value;
}
} // namespace

bool applyAiAction(GameEngine& engine, int playerNumber, const AiAction& action)
{
    switch (action.kind)
    {
        case AiActionKind::MovePiece:
            return engine.movePiece(playerNumber, action.pieceId, action.row, action.column);
        case AiActionKind::AttackPiece:
            return engine.attackPiece(playerNumber, action.pieceId, action.row, action.column);
        case AiActionKind::UseAbility:
            return engine.useAbility(playerNumber, action.pieceId);
        case AiActionKind::PlayCard:
            return engine.playCard(playerNumber, action.handIndex, action.row, action.column);
        case AiActionKind::DrawCard:
            if (!engine.drawCard(playerNumber))
            {
                return false;
            }
            if (engine.hasPendingForesightChoice(playerNumber))
            {
                engine.chooseForesightCard(
                    playerNumber, chooseAiForesightCard(engine, playerNumber));
            }
            return true;
        case AiActionKind::DiscardCard:
            return engine.discardCard(playerNumber, action.handIndex);
        case AiActionKind::EndTurn:
            return engine.endTurn(playerNumber);
    }
    return false;
}

int chooseAiForesightCard(const GameEngine& engine, int playerNumber)
{
    if (playerNumber < 1 || playerNumber > 2)
    {
        return 0;
    }
    const std::vector<GameCard>& choices = engine.playerState(playerNumber).foresightChoices;
    int bestIndex = 0;
    int bestValue = std::numeric_limits<int>::min();
    for (std::size_t index = 0; index < choices.size(); ++index)
    {
        const int value = cardKeepValue(choices[index], engine, playerNumber);
        if (value > bestValue)
        {
            bestValue = value;
            bestIndex = static_cast<int>(index);
        }
    }
    return bestIndex;
}

AiAction chooseAiAction(const GameEngine& engine, int aiPlayer, int searchTurns)
{
    const AiAction pass{AiActionKind::EndTurn};
    if (engine.phase() != Phase::Playing || engine.currentPlayer() != aiPlayer)
    {
        return pass;
    }

    AiContext context;
    context.aiPlayer = aiPlayer;
    context.opponent = aiPlayer == 1 ? 2 : 1;

    // Everything from here on reasons about the redacted board, so the planner
    // cannot see - or accidentally exploit - an opposing dematerialized piece.
    GameEngine planning = engine;
    context.concealedOpponentHeroes = planning.redactForPlanning(aiPlayer);

    const int turns = std::clamp(searchTurns, 1, 6);
    // Deeper searches earn their cost up to four turns; past that the extra
    // plies stop paying. The absolute cap keeps a crowded board from turning
    // one decision into a visible pause.
    context.nodeBudget = std::min(60000LL * turns, 300000LL);

    const std::vector<TurnPlan> plans =
        expandTurn(planning, aiPlayer, context, RootProfile, true);
    if (plans.empty())
    {
        return pass;
    }

    std::vector<std::pair<int, AiAction>> ranked;
    ranked.reserve(plans.size());
    int alpha = WorstScore;
    for (const TurnPlan& plan : plans)
    {
        const int score = searchFromTurn(plan.after, turns - 1, alpha, BestScore, context);
        alpha = std::max(alpha, score);
        ranked.emplace_back(score, plan.firstAction);
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        return left.first > right.first;
    });

    // The plan was built without the opposing dematerialized pieces, so the
    // real engine may still refuse its opening action. Take the best one it
    // accepts rather than repeating a move that cannot be made.
    for (const auto& [score, action] : ranked)
    {
        (void)score;
        if (action.kind == AiActionKind::EndTurn)
        {
            return action;
        }
        GameEngine probe = engine;
        probe.setNarrationEnabled(false);
        if (applyAiAction(probe, aiPlayer, action))
        {
            return action;
        }
    }
    return pass;
}

void placeAiHeroes(GameEngine& engine, int aiPlayer)
{
    // Heroes go on the edge column, furthest from anything that could reach
    // them, and take the middle rows first so they are not cornered.
    const std::array<std::pair<int, int>, 8> home = homeSquares(aiPlayer);
    std::vector<std::pair<int, int>> ordered(home.begin(), home.end());
    std::stable_sort(
        ordered.begin(), ordered.end(), [aiPlayer](const auto& left, const auto& right) {
            const int leftAdvance = aiPlayer == 1 ? left.second : BoardSize - 1 - left.second;
            const int rightAdvance = aiPlayer == 1 ? right.second : BoardSize - 1 - right.second;
            if (leftAdvance != rightAdvance)
            {
                return leftAdvance < rightAdvance;
            }
            const int leftCentre = std::min(left.first, BoardSize - 1 - left.first);
            const int rightCentre = std::min(right.first, BoardSize - 1 - right.first);
            return leftCentre > rightCentre;
        });

    std::size_t next = 0;
    while (engine.phase() == Phase::HeroPlacement &&
           !engine.playerState(aiPlayer).heroesToPlace.empty() && next < ordered.size())
    {
        const auto [row, column] = ordered[next];
        if (findPieceAt(engine.boardPieces(), row, column) == nullptr)
        {
            engine.placeHero(aiPlayer, 0, row, column);
        }
        ++next;
    }
}
