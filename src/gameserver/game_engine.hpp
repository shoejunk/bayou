#pragma once

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <random>
#include <string>
#include <vector>

using namespace game_data;

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
        int discardsThisTurn = 0;
        bool deckSubmitted = false;
    };

    GameEngine(unsigned int seed, const std::vector<card_data::Card>& cardLibrary)
        : rng(seed)
    {
        loadSummonCatalog(cardLibrary);
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
            rememberSummonCard(resolved);
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

        const GameCard& hero = player.heroesToPlace[static_cast<std::size_t>(heroIndex)];
        if (!footprintCanDeploy(playerNumber, hero, row, column, true))
        {
            setStatusFor(playerNumber, "Heroes must go on an empty starting square.");
            return;
        }

        spawnPiece(
            playerNumber,
            hero,
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

        const std::vector<std::string> missingTraits =
            missingHeroTraits(pieces, playerNumber, card);
        if (!missingTraits.empty())
        {
            std::string message = "Your living heroes must supply traits:";
            for (std::size_t i = 0; i < missingTraits.size(); ++i)
            {
                message += i == 0 ? " " : ", ";
                message += missingTraits[i];
            }
            message += " to play that card.";
            setStatusFor(playerNumber, message);
            return;
        }

        if (card.type == "Unit")
        {
            if (!footprintCanDeploy(playerNumber, card, targetRow, targetColumn, false))
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

    // Discarding sends the card to the bottom of the draw pile without ending the turn.
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
        if (player.discardsThisTurn >= MaxDiscardsPerTurn)
        {
            setStatusFor(playerNumber, "You can discard only one card each turn.");
            return;
        }

        const GameCard card = player.hand[static_cast<std::size_t>(handIndex)];
        player.hand.erase(player.hand.begin() + handIndex);
        // The draw pile is drawn from the back, so the front is the bottom of the deck.
        player.drawPile.insert(player.drawPile.begin(), card);
        ++player.discardsThisTurn;
        status = fmt::format("Player {} discarded {} to the bottom of the deck.", playerNumber, card.title);
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
            !pieceAbilityAvailable(pieces, *piece))
        {
            return;
        }

        const std::string abilityLabel = pieceAbilityLabel(*piece);
        const std::string actingPieceName = piece->name;
        const int actingPieceId = piece->id;
        if (piece->ability == "dig")
        {
            if (piece->abilityUses == 0)
            {
                setStatusFor(playerNumber, "That piece has already dug its hole.");
                return;
            }
            holes[static_cast<std::size_t>(squareIndex(piece->row, piece->column))] = 1;
            if (piece->abilityUses > 0)
            {
                --piece->abilityUses;
            }
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
        else if (piece->ability == "summon")
        {
            const GameCard* summonCard = summonCardByTitle(piece->summonTitle);
            if (summonCard == nullptr || summonCard->type != "Unit")
            {
                setStatusFor(playerNumber, "That summon does not name a valid unit.");
                return;
            }
            const auto [row, column] = summonDestination(*piece);
            if (!pieceSummonDestinationFree(pieces, *piece))
            {
                setStatusFor(playerNumber, "That summon needs an empty space in front.");
                return;
            }
            spawnPiece(playerNumber, *summonCard, row, column, false);
            pieces.back().hasActed = true;
        }
        else
        {
            return;
        }

        Piece* actingPiece = pieceById(actingPieceId);
        if (actingPiece == nullptr)
        {
            return;
        }
        actingPiece->hasActed = true;
        advanceTurn(fmt::format("{} used {}.", actingPieceName, abilityLabel));
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
            view.discardsThisTurn = player.discardsThisTurn;
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
    std::vector<GameCard> summonCatalog;
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
            if (row >= piece.row && row < piece.row + piece.height &&
                column >= piece.column && column < piece.column + piece.width)
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
            if (row >= piece.row && row < piece.row + piece.height &&
                column >= piece.column && column < piece.column + piece.width)
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

    void rememberSummonCard(const GameCard& card)
    {
        const auto found = std::find_if(
            summonCatalog.begin(),
            summonCatalog.end(),
            [&](const GameCard& existing) { return existing.title == card.title; });
        if (found == summonCatalog.end())
        {
            summonCatalog.push_back(card);
            return;
        }
        *found = card;
    }

    void loadSummonCatalog(const std::vector<card_data::Card>& cardLibrary)
    {
        for (const card_data::Card& card : cardLibrary)
        {
            rememberSummonCard(toGameCard(card));
        }
    }

    bool footprintCanDeploy(int playerNumber, const GameCard& card, int row, int column, bool starting) const
    {
        if (row < 0 || column < 0 || row + card.height > BoardSize || column + card.width > BoardSize)
            return false;
        for (int r = row; r < row + card.height; ++r)
            for (int c = column; c < column + card.width; ++c)
            {
                if ((starting ? !isStartingSquare(playerNumber, r, c)
                              : control[static_cast<std::size_t>(squareIndex(r, c))] != playerNumber) ||
                    pieceAt(r, c) != nullptr)
                    return false;
            }
        return true;
    }

    const GameCard* summonCardByTitle(const std::string& title) const
    {
        const auto found = std::find_if(
            summonCatalog.begin(),
            summonCatalog.end(),
            [&](const GameCard& card) { return card.title == title; });
        return found == summonCatalog.end() ? nullptr : &*found;
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

        // Resolve against the acting player's view of the board so hidden
        // enemy pieces do not block or betray their squares; a collision with
        // one is adjusted into a strike or a harmless bump below.
        const PieceActionOutcome outcome =
            resolvePieceActionThroughHidden(pieces, holes, *piece, toRow, toColumn);
        const ActionResolution& action = outcome.action;
        if (!action.legal)
        {
            setStatusFor(playerNumber, "That piece cannot act there.");
            return;
        }
        const int destinationRow = outcome.destinationRow;
        const int destinationColumn = outcome.destinationColumn;

        const int attackerId = piece->id;
        const std::string attackerName = piece->name;
        std::vector<std::string> targetNames;
        std::vector<int> defeatedOwners;
        bool anyTargetDestroyed = false;
        bool anyTargetWasHidden = false;

        if (action.attacks)
        {
            const std::vector<int> targetIds = action.targetIds.empty()
                ? std::vector<int>{action.targetId}
                : action.targetIds;
            for (int targetId : targetIds)
            {
                Piece* target = pieceById(targetId);
                if (target == nullptr)
                    continue;
                targetNames.push_back((target->hidden ? "a hidden " : "") + target->name);
                anyTargetWasHidden = anyTargetWasHidden || target->hidden;
                const int victimOwner = target->owner;
                target->health -= action.damage;
                applyDamageStatus(*target, action.damage, action.statusTurns);
                if (target->health <= 0)
                {
                    anyTargetDestroyed = true;
                    defeatedOwners.push_back(victimOwner);
                    removePiece(targetId);
                }
            }
            if (targetNames.empty()) return;
        }

        // A hidden piece that was struck or bumped into materializes stunned.
        std::string revealedName;
        if (outcome.revealedPieceId != 0)
        {
            if (Piece* revealed = pieceById(outcome.revealedPieceId))
            {
                revealedName = revealed->name;
                materializeRevealedPiece(*revealed);
            }
        }

        Piece* survivingAttacker = pieceById(attackerId);
        if (survivingAttacker == nullptr)
        {
            return;
        }

        if (action.moves)
        {
            if (pieceFootprintFree(pieces, *survivingAttacker, destinationRow, destinationColumn))
            {
                survivingAttacker->row = destinationRow;
                survivingAttacker->column = destinationColumn;
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

        for (int defeatedOwner : defeatedOwners)
        {
            checkForWinner(defeatedOwner);
            if (phaseValue == Phase::GameOver)
            {
                return;
            }
        }

        if (action.attacks)
        {
            const int effectiveDisabledTurns = disabledTurnsForDamage(action.damage, action.statusTurns);
            std::string joinedTargets;
            for (std::size_t i = 0; i < targetNames.size(); ++i)
            {
                if (i > 0) joinedTargets += i + 1 == targetNames.size() ? " and " : ", ";
                joinedTargets += targetNames[i];
            }
            std::string result = fmt::format("{} hit {} for {} each", attackerName, joinedTargets, action.damage);
            if (effectiveDisabledTurns > 0)
            {
                result += fmt::format(" and disabled surviving targets for {} turn(s)", effectiveDisabledTurns);
            }
            result += anyTargetDestroyed ? "; at least one was destroyed!" : ".";
            if (anyTargetWasHidden)
            {
                result += " It materialized!";
            }
            advanceTurn(result);
        }
        else if (!revealedName.empty())
        {
            advanceTurn(fmt::format(
                "{} bumped into a hidden {}! It materialized, stunned.",
                attackerName,
                revealedName));
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
        populatePieceFromCard(piece, card, isHero);
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
        player.discardsThisTurn = 0;
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
            applyDamageStatus(*target, card.power, 0);
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
                    if (pieceExertsControl(*occupant))
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
                        if (!pieceExertsControl(*neighbor))
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

