#include "ai_player.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace
{
constexpr int AiMaxCandidateActions = 80;

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
        case AiActionKind::DrawCard: return 550;
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
    // Plan against the board this player can see: dematerialized enemy
    // pieces neither block nor present targets (the engine adjudicates any
    // collision when the action executes).
    const std::vector<Piece> visiblePieces = piecesVisibleTo(pieces, playerNumber);
    const auto commanderFound = std::find_if(
        pieces.begin(),
        pieces.end(),
        [&](const Piece& piece) { return piece.id == engine.commandingPiece(); });
    const Piece* commander = commanderFound == pieces.end() ? nullptr : &*commanderFound;
    const int relentlessPieceId = engine.relentlessPiece();
    const bool normalPieceActionAvailable =
        !engine.playerState(playerNumber).pieceActionUsedThisTurn;
    for (const Piece& piece : pieces)
    {
        if (piece.owner != playerNumber || piece.hasActed || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
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
        if (!normalPieceActionAvailable && commander == nullptr &&
            relentlessPieceId == 0 && piece.repeatActionIndex < 0)
        {
            continue;
        }
        if (pieceAbilityAvailable(pieces, piece))
        {
            actions.push_back({AiActionKind::UseAbility, piece.id});
        }
        for (int row = 0; row < BoardSize; ++row)
        {
            for (int column = 0; column < BoardSize; ++column)
            {
                const ActionResolution resolution = resolvePieceAction(visiblePieces, holes, piece, row, column);
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

    if (allowCards && commander == nullptr && relentlessPieceId == 0)
    {
        const GameEngine::EnginePlayer& player = engine.playerState(playerNumber);
        for (int handIndex = 0; handIndex < static_cast<int>(player.hand.size()); ++handIndex)
        {
            const GameCard& card = player.hand[static_cast<std::size_t>(handIndex)];
            if (card.cost > player.resources || !heroTraitsAllowCard(pieces, playerNumber, card))
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
                if (isResourcesEffect(card))
                {
                    actions.push_back({AiActionKind::PlayCard, 0, handIndex, 0, 0});
                }
                else
                {
                    for (const Piece& target : visiblePieces)
                    {
                        if ((card.effect == "damage" && target.owner != playerNumber) ||
                            (card.effect == "heal" && target.owner == playerNumber && target.health < target.maxHealth))
                        {
                            actions.push_back({AiActionKind::PlayCard, 0, handIndex, target.row, target.column});
                        }
                    }
                }
            }
            else if (card.type == "Enchantment")
            {
                if (card.target == "player" && card.effect == "resourceDrain")
                {
                    actions.push_back({
                        AiActionKind::PlayCard,
                        0,
                        handIndex,
                        -1,
                        playerNumber == 1 ? 2 : 1});
                }
                else if (card.target == "square" && card.effect == "resources")
                {
                    for (int row = 0; row < BoardSize; ++row)
                    {
                        for (int column = 0; column < BoardSize; ++column)
                        {
                            if (engine.boardControl()[static_cast<std::size_t>(squareIndex(row, column))] == playerNumber &&
                                engine.boardHoles()[static_cast<std::size_t>(squareIndex(row, column))] == 0)
                            {
                                actions.push_back({AiActionKind::PlayCard, 0, handIndex, row, column});
                            }
                        }
                    }
                }
                else if (card.target == "piece" && card.effect == "damage")
                {
                    for (const Piece& target : visiblePieces)
                    {
                        if (target.owner == playerNumber)
                        {
                            actions.push_back({
                                AiActionKind::PlayCard,
                                0,
                                handIndex,
                                target.row,
                                target.column});
                        }
                    }
                }
            }
        }
        if (player.discardsThisTurn < MaxDiscardsPerTurn)
        {
            for (int handIndex = 0; handIndex < static_cast<int>(player.hand.size()); ++handIndex)
            {
                actions.push_back({AiActionKind::DiscardCard, 0, handIndex});
            }
        }
        if (player.resources >= DrawCardResourceCost &&
            static_cast<int>(player.hand.size()) < MaxHandSize &&
            !player.drawPile.empty())
        {
            actions.push_back({AiActionKind::DrawCard});
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

} // namespace

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
        case AiActionKind::DrawCard:
            if (engine.drawCard(playerNumber) && engine.hasPendingForesightChoice(playerNumber))
            {
                engine.chooseForesightCard(playerNumber, 0);
            }
            break;
        case AiActionKind::DiscardCard:
            engine.discardCard(playerNumber, action.handIndex);
            break;
        case AiActionKind::EndTurn:
            engine.endTurn(playerNumber);
            break;
    }
}

namespace
{

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
            if (action.state != piece.actionState)
            {
                continue;
            }
            value += (action.damage + action.heal) * 3 + action.statusTurns * 4;
            if (action.canMove)
            {
                value += action.maxRange;
            }
        }
        const std::vector<Piece> visiblePieces =
            piecesVisibleTo(engine.boardPieces(), piece.owner);
        int legalMoves = 0;
        int legalAttacks = 0;
        for (int row = 0; row < BoardSize; ++row)
        {
            for (int column = 0; column < BoardSize; ++column)
            {
                const ActionResolution resolution = resolvePieceAction(
                    visiblePieces, engine.boardHoles(), piece, row, column);
                if (!resolution.legal)
                {
                    continue;
                }
                legalMoves += resolution.moves ? 1 : 0;
                legalAttacks += resolution.attacks ? 1 : 0;
            }
        }
        value += std::min(legalMoves, 8) + std::min(legalAttacks, 4) * 3;
        if (piece.hidden)
        {
            value += 8;
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
    score += (aiSnapshot.resources - opponentSnapshot.resources) * 2;
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

} // namespace

AiAction chooseAiAction(const GameEngine& engine, int aiPlayer, int searchPlies)
{
    searchPlies = std::max(1, searchPlies);
    std::vector<AiAction> actions = legalAiActions(engine, aiPlayer, true);
    AiAction bestAction;
    int bestScore = std::numeric_limits<int>::min();
    for (const AiAction& action : actions)
    {
        GameEngine next = engine;
        applyAiAction(next, aiPlayer, action);
        int score = searchAiPosition(next, searchPlies - 1, aiPlayer);
        if (action.kind == AiActionKind::UseAbility)
        {
            const auto piece = std::find_if(
                engine.boardPieces().begin(),
                engine.boardPieces().end(),
                [&](const Piece& candidate) { return candidate.id == action.pieceId; });
            if (piece != engine.boardPieces().end())
            {
                if (piece->ability == "summon")
                {
                    score += 700;
                }
                else if (piece->ability == "dematerialize" && !piece->hidden)
                {
                    score += 80;
                }
                else if (piece->ability == "transform")
                {
                    score += 35;
                }
            }
        }
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
