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
    static constexpr std::int64_t RegularClockMs = 15 * 60 * 1000;
    static constexpr std::int64_t FullTurnTimerMs = 2 * 60 * 1000;
    static constexpr std::int64_t ReducedTurnTimerMs = 60 * 1000;
    static constexpr std::int64_t MinimumTurnTimerMs = 30 * 1000;
    static constexpr std::int64_t ConquestClockMs = 3LL * 24 * 60 * 60 * 1000;
    static constexpr std::int64_t ConquestFullTurnTimerMs = 24LL * 60 * 60 * 1000;
    static constexpr std::int64_t ConquestReducedTurnTimerMs = 12LL * 60 * 60 * 1000;
    static constexpr std::int64_t ConquestMinimumTurnTimerMs = 6LL * 60 * 60 * 1000;

    struct EnginePlayer
    {
        int number = 0;
        std::vector<GameCard> drawPile;
        std::vector<GameCard> hand;
        std::vector<GameCard> heroesToPlace;
        int resources = 0;
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
    int commandingPiece() const { return commandingPieceId; }
    int relentlessPiece() const { return relentlessPieceId; }
    const std::vector<Piece>& boardPieces() const { return pieces; }
    const std::vector<Enchantment>& boardEnchantments() const { return enchantments; }
    const std::array<std::uint8_t, BoardSquares>& boardControl() const { return control; }
    const std::array<std::uint8_t, BoardSquares>& boardHoles() const { return holes; }
    const EnginePlayer& playerState(int playerNumber) const { return playerRef(playerNumber); }

    void enableTimers()
    {
        enableTimers(
            RegularClockMs,
            FullTurnTimerMs,
            ReducedTurnTimerMs,
            MinimumTurnTimerMs);
    }

    void enableTimers(
        std::int64_t clockMs,
        std::int64_t fullTurnTimerMs,
        std::int64_t reducedTurnTimerMs,
        std::int64_t minimumTurnTimerMs)
    {
        timersEnabled = true;
        playerClockRemainingMs.fill(clockMs);
        turnTimerLevels.fill(0);
        turnTimerDurations = {
            fullTurnTimerMs,
            reducedTurnTimerMs,
            minimumTurnTimerMs};
        turnRemainingMs = turnTimerDurations[0];
    }

    bool timersAreEnabled() const { return timersEnabled; }

    std::int64_t timeUntilNextTimerEventMs() const
    {
        if (!timersEnabled || phaseValue != Phase::Playing)
        {
            return 0;
        }
        return std::min(
            playerClockRemainingMs[static_cast<std::size_t>(activePlayer - 1)],
            turnRemainingMs);
    }

    // Advances authoritative game time. Returns true only when a timer caused
    // a turn transition or ended the game; ordinary clock countdowns are
    // exposed through snapshots without being game-state transitions.
    bool updateTimers(std::int64_t elapsedMs)
    {
        if (!timersEnabled || phaseValue != Phase::Playing || elapsedMs <= 0)
        {
            return false;
        }

        bool transitioned = false;
        while (elapsedMs > 0 && phaseValue == Phase::Playing)
        {
            const std::size_t activeIndex = static_cast<std::size_t>(activePlayer - 1);
            const std::int64_t untilEvent = std::min(
                playerClockRemainingMs[activeIndex],
                turnRemainingMs);
            const std::int64_t consumed = std::min(elapsedMs, untilEvent);
            playerClockRemainingMs[activeIndex] -= consumed;
            turnRemainingMs -= consumed;
            elapsedMs -= consumed;

            // The match clock is decisive when both deadlines land together.
            if (playerClockRemainingMs[activeIndex] <= 0)
            {
                playerClockRemainingMs[activeIndex] = 0;
                winnerValue = activePlayer == 1 ? 2 : 1;
                phaseValue = Phase::GameOver;
                status = fmt::format(
                    "Player {} ran out of time. Player {} wins!",
                    activePlayer,
                    winnerValue);
                return true;
            }

            if (turnRemainingMs <= 0)
            {
                const int timedOutPlayer = activePlayer;
                std::size_t& level = turnTimerLevels[activeIndex];
                level = std::min<std::size_t>(level + 1, 2);
                advanceTurn(fmt::format(
                    "Player {}'s turn timer expired.",
                    timedOutPlayer));
                transitioned = true;
            }
        }
        return transitioned;
    }

    // A successful play, piece action, ability, or discard restores that
    // player's next turn to two minutes. If the action keeps the turn (such as
    // a discard or Command), the current turn timer is restored immediately.
    void recordPlayerMove(int playerNumber)
    {
        if (!timersEnabled || playerNumber < 1 || playerNumber > 2)
        {
            return;
        }
        turnTimerLevels[static_cast<std::size_t>(playerNumber - 1)] = 0;
        if (phaseValue == Phase::Playing && activePlayer == playerNumber)
        {
            turnRemainingMs = turnTimerDurations[0];
        }
    }

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

    bool placeHero(int playerNumber, int heroIndex, int row, int column)
    {
        if (phaseValue != Phase::HeroPlacement)
        {
            return false;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (heroIndex < 0 || heroIndex >= static_cast<int>(player.heroesToPlace.size()))
        {
            return false;
        }

        const GameCard& hero = player.heroesToPlace[static_cast<std::size_t>(heroIndex)];
        if (!footprintCanDeploy(playerNumber, hero, row, column, true))
        {
            setStatusFor(playerNumber, "Heroes must go on an empty starting square.");
            return false;
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
        return true;
    }

    bool playCard(int playerNumber, int handIndex, int targetRow, int targetColumn)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        if (relentlessPieceId != 0)
        {
            setStatusFor(playerNumber, "The Relentless piece must act again or you must pass.");
            return false;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (handIndex < 0 || handIndex >= static_cast<int>(player.hand.size()))
        {
            return false;
        }

        const GameCard card = player.hand[static_cast<std::size_t>(handIndex)];
        if (card.cost > player.resources)
        {
            setStatusFor(playerNumber, "Not enough Resources to play that card.");
            return false;
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
            return false;
        }

        if (card.type == "Unit")
        {
            if (!footprintCanDeploy(playerNumber, card, targetRow, targetColumn, false))
            {
                setStatusFor(playerNumber, "Units deploy onto an empty square you control.");
                return false;
            }

            spawnPiece(playerNumber, card, targetRow, targetColumn, false);
            // Summoned units cannot act the turn they arrive.
            pieces.back().hasActed = true;
        }
        else if (card.type == "Enchantment")
        {
            if (!resolveEnchantment(playerNumber, card, targetRow, targetColumn))
            {
                return false;
            }
        }
        else  // Spell
        {
            if (!resolveSpell(playerNumber, card, targetRow, targetColumn))
            {
                return false;
            }
        }

        player.resources -= card.cost;
        player.hand.erase(player.hand.begin() + handIndex);
        recordPlayerMove(playerNumber);
        advanceTurn(fmt::format("Player {} played {}.", playerNumber, card.title));
        return true;
    }

    // Discarding sends the card to the bottom of the draw pile without ending the turn.
    bool discardCard(int playerNumber, int handIndex)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        if (relentlessPieceId != 0)
        {
            setStatusFor(playerNumber, "The Relentless piece must act again or you must pass.");
            return false;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (handIndex < 0 || handIndex >= static_cast<int>(player.hand.size()))
        {
            return false;
        }
        if (player.discardsThisTurn >= MaxDiscardsPerTurn)
        {
            setStatusFor(playerNumber, "You can discard only one card each turn.");
            return false;
        }

        const GameCard card = player.hand[static_cast<std::size_t>(handIndex)];
        player.hand.erase(player.hand.begin() + handIndex);
        // The draw pile is drawn from the back, so the front is the bottom of the deck.
        player.drawPile.insert(player.drawPile.begin(), card);
        ++player.discardsThisTurn;
        status = fmt::format("Player {} discarded {} to the bottom of the deck.", playerNumber, card.title);
        recordPlayerMove(playerNumber);
        return true;
    }

    bool movePiece(int playerNumber, int pieceId, int toRow, int toColumn)
    {
        const bool accepted = performPieceAction(playerNumber, pieceId, toRow, toColumn);
        if (accepted)
        {
            recordPlayerMove(playerNumber);
        }
        return accepted;
    }

    bool attackPiece(int playerNumber, int attackerId, int targetRow, int targetColumn)
    {
        const bool accepted = performPieceAction(playerNumber, attackerId, targetRow, targetColumn);
        if (accepted)
        {
            recordPlayerMove(playerNumber);
        }
        return accepted;
    }

    bool useAbility(int playerNumber, int pieceId)
    {
        const bool accepted = useAbilityWithoutRecordingMove(playerNumber, pieceId);
        if (accepted)
        {
            recordPlayerMove(playerNumber);
        }
        return accepted;
    }

private:
    bool useAbilityWithoutRecordingMove(int playerNumber, int pieceId)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }

        Piece* piece = pieceById(pieceId);
        if (piece == nullptr || piece->owner != playerNumber || piece->hasActed ||
            !pieceAbilityAvailable(pieces, *piece))
        {
            return false;
        }
        if (relentlessPieceId != 0 && piece->id != relentlessPieceId)
        {
            setStatusFor(playerNumber, "Only the Relentless piece may take the immediate action.");
            return false;
        }

        const Piece* commander = commandingPieceId != 0 ? pieceById(commandingPieceId) : nullptr;
        const bool commandedAction = commander != nullptr;
        const bool relentlessAction = relentlessPieceId != 0;
        const bool actionKeepsTurn = commandedAction ||
            (relentlessAction && relentlessActionKeepsTurn);
        if (commandedAction && !pieceCanReceiveCommand(*commander, *piece))
        {
            setStatusFor(playerNumber, "Command must activate a ready adjacent friendly piece.");
            return false;
        }

        const std::string abilityLabel = pieceAbilityLabel(*piece);
        const std::string actingPieceName = piece->name;
        const int actingPieceId = piece->id;
        const std::string commanderName = commandedAction ? commander->name : std::string();
        if (piece->ability == "dig")
        {
            if (piece->abilityUses == 0)
            {
                setStatusFor(playerNumber, "That piece has already dug its hole.");
                return false;
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
                stateCount = std::max(stateCount, actionNextState(action) + 1);
            }
            setPieceActionState(*piece, (piece->actionState + 1) % stateCount);
        }
        else if (piece->ability == "summon")
        {
            const GameCard* summonCard = summonCardByTitle(piece->summonTitle);
            if (summonCard == nullptr || summonCard->type != "Unit")
            {
                setStatusFor(playerNumber, "That summon does not name a valid unit.");
                return false;
            }
            const auto [row, column] = summonDestination(*piece);
            if (!pieceSummonDestinationFree(pieces, *piece))
            {
                setStatusFor(playerNumber, "That summon needs an empty space in front.");
                return false;
            }
            spawnPiece(playerNumber, *summonCard, row, column, false);
            pieces.back().hasActed = true;
        }
        else if (piece->ability == "command")
        {
            piece->hasActed = true;
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
            commandingPieceId = piece->id;
            status = fmt::format(
                "{} used Command. Activate one adjacent friendly piece.",
                actingPieceName);
            return true;
        }
        else
        {
            return false;
        }

        Piece* actingPiece = pieceById(actingPieceId);
        if (actingPiece == nullptr)
        {
            return true;
        }
        actingPiece->hasActed = true;
        if (relentlessAction)
        {
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
        }
        if (commandedAction)
        {
            commandingPieceId = 0;
            recomputeControl();
            status = fmt::format(
                "{} commanded {} to use {}.",
                commanderName,
                actingPieceName,
                abilityLabel);
        }
        else if (actionKeepsTurn)
        {
            recomputeControl();
            status = fmt::format("{} used {}.", actingPieceName, abilityLabel);
        }
        else
        {
            advanceTurn(fmt::format("{} used {}.", actingPieceName, abilityLabel));
        }
        return true;
    }

public:
    bool endTurn(int playerNumber)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }

        advanceTurn(fmt::format("Player {} passed.", playerNumber));
        return true;
    }

    // Builds the view tailored to one player (their hand only).
    Snapshot snapshotFor(int playerNumber) const
    {
        Snapshot snapshot;
        snapshot.phase = static_cast<std::uint8_t>(phaseValue);
        snapshot.activePlayer = activePlayer;
        snapshot.yourPlayer = playerNumber;
        snapshot.winner = winnerValue;
        snapshot.commandingPieceId = commandingPieceId;
        snapshot.relentlessPieceId = relentlessPieceId;
        snapshot.timersEnabled = timersEnabled;
        snapshot.turnRemainingMs = turnRemainingMs;
        snapshot.control = control;
        snapshot.holes = holes;
        snapshot.enchantments.clear();
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
        for (const Enchantment& enchantment : enchantments)
        {
            if (enchantment.target == static_cast<std::uint8_t>(EnchantmentTarget::Piece))
            {
                const bool targetIsVisible = std::any_of(
                    snapshot.pieces.begin(),
                    snapshot.pieces.end(),
                    [&](const Piece& piece) { return piece.id == enchantment.targetPieceId; });
                if (!targetIsVisible)
                {
                    continue;
                }
            }
            snapshot.enchantments.push_back(enchantment);
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
            view.resources = player.resources;
            view.controlledSquares = controlledCount(p + 1);
            view.handCount = phaseValue == Phase::HeroPlacement
                ? static_cast<int>(player.heroesToPlace.size())
                : static_cast<int>(player.hand.size());
            view.heroesToPlace = static_cast<int>(player.heroesToPlace.size());
            view.heroesAlive = heroesAlive(p + 1);
            view.drawPileCount = static_cast<int>(player.drawPile.size());
            view.discardsThisTurn = player.discardsThisTurn;
            view.clockRemainingMs = playerClockRemainingMs[static_cast<std::size_t>(p)];
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
    std::vector<Enchantment> enchantments;
    std::array<EnginePlayer, 2> players{};
    std::vector<GameCard> summonCatalog;
    int nextPieceId = 1;
    int nextEnchantmentId = 1;
    int commandingPieceId = 0;
    int relentlessPieceId = 0;
    bool relentlessActionKeepsTurn = false;
    bool timersEnabled = false;
    std::array<std::int64_t, 2> playerClockRemainingMs{RegularClockMs, RegularClockMs};
    std::array<std::size_t, 2> turnTimerLevels{};
    std::array<std::int64_t, 3> turnTimerDurations{
        FullTurnTimerMs,
        ReducedTurnTimerMs,
        MinimumTurnTimerMs};
    std::int64_t turnRemainingMs = FullTurnTimerMs;
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

    // Returns true when Rebirth replaced the destroyed piece. Callers use
    // that result to distinguish a lethal hit from an actual kill.
    bool destroyPiece(int id)
    {
        const auto dying = std::find_if(
            pieces.begin(),
            pieces.end(),
            [id](const Piece& piece) { return piece.id == id; });
        if (dying == pieces.end())
        {
            return false;
        }

        const Piece original = *dying;
        const GameCard* rebirthDefinition = summonCardByTitle(original.rebirthTitle);
        const bool hasValidRebirth = rebirthDefinition != nullptr &&
            (rebirthDefinition->type == "Unit" || rebirthDefinition->type == "Hero");
        const GameCard rebirthCard = hasValidRebirth ? *rebirthDefinition : GameCard{};

        pieces.erase(dying);

        int rebornPieceId = 0;
        if (hasValidRebirth &&
            cardFootprintFree(pieces, rebirthCard, original.row, original.column))
        {
            spawnPiece(
                original.owner,
                rebirthCard,
                original.row,
                original.column,
                rebirthCard.type == "Hero");
            Piece& reborn = pieces.back();
            reborn.hasActed = true;
            rebornPieceId = reborn.id;
        }

        if (rebornPieceId != 0)
        {
            for (Enchantment& enchantment : enchantments)
            {
                if (enchantment.target == static_cast<std::uint8_t>(EnchantmentTarget::Piece) &&
                    enchantment.targetPieceId == id)
                {
                    enchantment.targetPieceId = rebornPieceId;
                    enchantment.targetRow = original.row;
                    enchantment.targetColumn = original.column;
                }
            }
        }
        else
        {
            enchantments.erase(
                std::remove_if(
                    enchantments.begin(),
                    enchantments.end(),
                    [id](const Enchantment& enchantment) {
                        return enchantment.target == static_cast<std::uint8_t>(EnchantmentTarget::Piece) &&
                            enchantment.targetPieceId == id;
                    }),
                enchantments.end());
        }

        if (commandingPieceId == id)
        {
            commandingPieceId = 0;
        }
        if (relentlessPieceId == id)
        {
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
        }
        return rebornPieceId != 0;
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

    bool performPieceAction(int playerNumber, int pieceId, int toRow, int toColumn)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }

        Piece* piece = pieceById(pieceId);
        if (piece == nullptr || piece->owner != playerNumber || piece->hasActed)
        {
            return false;
        }
        if (relentlessPieceId != 0 && piece->id != relentlessPieceId)
        {
            setStatusFor(playerNumber, "Only the Relentless piece may take the immediate action.");
            return false;
        }

        const Piece* commander = commandingPieceId != 0 ? pieceById(commandingPieceId) : nullptr;
        const bool commandedAction = commander != nullptr;
        const bool relentlessAction = relentlessPieceId != 0;
        const bool actionKeepsTurn = commandedAction ||
            (relentlessAction && relentlessActionKeepsTurn);
        const std::string commanderName = commandedAction ? commander->name : std::string();
        if (commandedAction && !pieceCanReceiveCommand(*commander, *piece))
        {
            setStatusFor(playerNumber, "Command must activate a ready adjacent friendly piece.");
            return false;
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
            return false;
        }
        const int destinationRow = outcome.destinationRow;
        const int destinationColumn = outcome.destinationColumn;

        const int attackerId = piece->id;
        const int attackerOwner = piece->owner;
        const int originRow = piece->row;
        const int originColumn = piece->column;
        const std::string attackerName = piece->name;
        std::vector<std::string> damagedTargetNames;
        std::vector<std::string> healedTargetNames;
        std::vector<int> defeatedOwners;
        bool anyTargetDestroyed = false;
        bool anyTargetReborn = false;
        bool anyTargetWasHidden = false;
        int pushedSquares = 0;
        int pushCollisionDamage = 0;
        const int attackDamage = action.damage +
            pieceEnchantmentDamageBonus(enchantments, attackerId);

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
                const std::string targetName =
                    (target->hidden ? "a hidden " : "") + target->name;
                anyTargetWasHidden = anyTargetWasHidden ||
                    (target->hidden && target->owner != attackerOwner);
                if (target->owner == attackerOwner)
                {
                    healedTargetNames.push_back(targetName);
                    applyActionHealing(*target, action.heal, action.statusTurns);
                }
                else
                {
                    damagedTargetNames.push_back(targetName);
                    const std::vector<DamageAssignment> damageAssignments =
                        applyDamageWithBodyguards(
                            pieces, targetId, attackDamage, action.statusTurns, rng);
                    for (const DamageAssignment& assignment : damageAssignments)
                    {
                        Piece* damagedPiece = pieceById(assignment.pieceId);
                        if (damagedPiece != nullptr && damagedPiece->health <= 0)
                        {
                            defeatedOwners.push_back(damagedPiece->owner);
                            const bool reborn = destroyPiece(damagedPiece->id);
                            anyTargetReborn = anyTargetReborn || reborn;
                            anyTargetDestroyed = anyTargetDestroyed || !reborn;
                        }
                    }
                    const PushResult pushResult = applyActionPush(
                        pieces,
                        targetId,
                        action.stagingRow,
                        action.stagingColumn,
                        action.push);
                    pushedSquares += pushResult.movedSquares;
                    pushCollisionDamage += pushResult.preventedSquares;
                    if (Piece* pushedTarget = pieceById(targetId);
                        pushedTarget != nullptr && pushedTarget->health <= 0)
                    {
                        defeatedOwners.push_back(pushedTarget->owner);
                        const bool reborn = destroyPiece(pushedTarget->id);
                        anyTargetReborn = anyTargetReborn || reborn;
                        anyTargetDestroyed = anyTargetDestroyed || !reborn;
                    }
                }
            }
            if (damagedTargetNames.empty() && healedTargetNames.empty()) return false;
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
            return true;
        }

        if (action.moves)
        {
            if (!anyTargetReborn &&
                pieceFootprintFree(pieces, *survivingAttacker, destinationRow, destinationColumn))
            {
                survivingAttacker->row = destinationRow;
                survivingAttacker->column = destinationColumn;
            }
            else
            {
                if (pieceFootprintFree(
                        pieces,
                        *survivingAttacker,
                        action.stagingRow,
                        action.stagingColumn))
                {
                    survivingAttacker->row = action.stagingRow;
                    survivingAttacker->column = action.stagingColumn;
                }
            }
        }
        survivingAttacker->disabledTurns =
            std::max(survivingAttacker->disabledTurns, action.cooldownTurns);
        setPieceActionState(*survivingAttacker, action.nextState);
        const bool gainsRelentlessAction = anyTargetDestroyed &&
            hasKeyword(survivingAttacker->keywords, "relentless");
        survivingAttacker->hasActed = !gainsRelentlessAction;
        const bool moved = survivingAttacker->row != originRow ||
            survivingAttacker->column != originColumn;
        const bool leavesTrail = moved && pieceHasTrailAbility(*survivingAttacker);
        const std::string trailSummonTitle = survivingAttacker->summonTitle;

        if (leavesTrail)
        {
            const GameCard* trailCard = summonCardByTitle(trailSummonTitle);
            if (trailCard != nullptr && trailCard->type == "Unit" &&
                cardFootprintFree(pieces, *trailCard, originRow, originColumn))
            {
                spawnPiece(attackerOwner, *trailCard, originRow, originColumn, false);
                pieces.back().hasActed = true;
            }
        }

        for (int defeatedOwner : defeatedOwners)
        {
            checkForWinner(defeatedOwner);
            if (phaseValue == Phase::GameOver)
            {
                return true;
            }
        }

        std::string result;
        if (action.attacks)
        {
            const int effectiveDisabledTurns = damagedTargetNames.empty()
                ? std::max(0, action.statusTurns)
                : disabledTurnsForDamage(attackDamage, action.statusTurns);
            const auto joinTargets = [](const std::vector<std::string>& names) {
                std::string joined;
                for (std::size_t i = 0; i < names.size(); ++i)
                {
                    if (i > 0) joined += i + 1 == names.size() ? " and " : ", ";
                    joined += names[i];
                }
                return joined;
            };
            if (!damagedTargetNames.empty())
            {
                result = fmt::format(
                    "{} hit {} for {} each",
                    attackerName,
                    joinTargets(damagedTargetNames),
                    attackDamage);
            }
            if (!healedTargetNames.empty())
            {
                const std::string healed = fmt::format(
                    "healed {} for {} each",
                    joinTargets(healedTargetNames),
                    action.heal);
                result += result.empty()
                    ? fmt::format("{} {}", attackerName, healed)
                    : " and " + healed;
            }
            if (pushedSquares > 0)
            {
                result += fmt::format(" and pushed targets {} square(s)", pushedSquares);
            }
            if (pushCollisionDamage > 0)
            {
                result += fmt::format(
                    " and dealt {} extra collision damage",
                    pushCollisionDamage);
            }
            if (effectiveDisabledTurns > 0)
            {
                result += fmt::format(" and disabled surviving targets for {} turn(s)", effectiveDisabledTurns);
            }
            result += anyTargetDestroyed ? "; at least one was destroyed!" : ".";
            if (anyTargetReborn)
            {
                result += " Rebirth returned a piece to the board!";
            }
            if (anyTargetWasHidden)
            {
                result += " It materialized!";
            }
        }
        else if (!revealedName.empty())
        {
            result = fmt::format(
                "{} bumped into a hidden {}! It materialized, stunned.",
                attackerName,
                revealedName);
        }
        else
        {
            result = fmt::format("{} moved.", attackerName);
        }

        if (commandedAction)
        {
            commandingPieceId = 0;
        }

        if (gainsRelentlessAction)
        {
            relentlessPieceId = attackerId;
            relentlessActionKeepsTurn = actionKeepsTurn;
            recomputeControl();
            status = (commandedAction ? fmt::format("{} commanded {}", commanderName, result) : result) +
                " Relentless: it may act again immediately.";
        }
        else if (commandedAction)
        {
            recomputeControl();
            status = fmt::format("{} commanded {}", commanderName, result);
        }
        else if (actionKeepsTurn)
        {
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
            recomputeControl();
            status = result;
        }
        else
        {
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
            advanceTurn(result);
        }
        return true;
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
        if (timersEnabled)
        {
            turnRemainingMs = turnTimerDurations[turnTimerLevels[
                static_cast<std::size_t>(playerNumber - 1)]];
        }
        EnginePlayer& player = playerRef(playerNumber);
        player.discardsThisTurn = 0;
        const int controlledIncome = controlledCount(playerNumber);
        player.resources += controlledIncome;

        const int enchantedSquareResources =
            squareEnchantmentResourceBonus(enchantments, control, playerNumber);
        player.resources += enchantedSquareResources;

        int taxAmount = 0;
        int gatheredResources = 0;
        for (const Piece& piece : pieces)
        {
            if (piece.owner == playerNumber)
            {
                taxAmount += piece.tax;
                gatheredResources += piece.gatherResources;
            }
        }
        player.resources += gatheredResources;
        EnginePlayer& opponent = playerRef(playerNumber == 1 ? 2 : 1);
        const int collectedTax = std::min(std::max(0, taxAmount), opponent.resources);
        opponent.resources -= collectedTax;
        player.resources += collectedTax;
        const int resourceDrain = std::min(
            player.resources,
            playerEnchantmentResourceDrain(enchantments, playerNumber));
        player.resources -= resourceDrain;
        drawCard(player);

        for (Piece& piece : pieces)
        {
            if (piece.owner == playerNumber)
            {
                beginPieceTurn(piece);
            }
        }

        status = fmt::format(
            "Player {}'s turn. +{} Resources{}{}{}{}.",
            playerNumber,
            controlledIncome,
            enchantedSquareResources > 0
                ? fmt::format(" and +{} from enchanted squares", enchantedSquareResources)
                : "",
            gatheredResources > 0 ? fmt::format(" and gathered {} Resources", gatheredResources) : "",
            collectedTax > 0 ? fmt::format(" and collected {} Resources in Tax", collectedTax) : "",
            resourceDrain > 0 ? fmt::format(" and drained {} Resources", resourceDrain) : "");
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
        commandingPieceId = 0;
        relentlessPieceId = 0;
        relentlessActionKeepsTurn = false;
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
        if (isResourcesEffect(card))
        {
            playerRef(playerNumber).resources += card.power;
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
            const int targetId = target->id;
            const std::vector<DamageAssignment> damageAssignments =
                applyDamageWithBodyguards(pieces, targetId, card.power, 0, rng);
            for (const DamageAssignment& assignment : damageAssignments)
            {
                Piece* damagedPiece = pieceById(assignment.pieceId);
                if (damagedPiece != nullptr && damagedPiece->health <= 0)
                {
                    const int victimOwner = damagedPiece->owner;
                    destroyPiece(damagedPiece->id);
                    checkForWinner(victimOwner);
                }
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

    bool resolveEnchantment(int playerNumber, const GameCard& card, int targetRow, int targetColumn)
    {
        Enchantment enchantment;
        enchantment.id = nextEnchantmentId++;
        enchantment.owner = playerNumber;
        enchantment.title = card.title;
        enchantment.imagePath = card.imagePath;
        enchantment.effect = card.effect;
        enchantment.power = std::max(0, card.power);

        if (card.target == "player")
        {
            if (targetRow != -1 || (targetColumn != 1 && targetColumn != 2) ||
                card.effect != "resourceDrain")
            {
                setStatusFor(playerNumber, "That player enchantment must drain a player target's Resources.");
                return false;
            }
            enchantment.target = static_cast<std::uint8_t>(EnchantmentTarget::Player);
            enchantment.targetPlayer = targetColumn;
        }
        else if (card.target == "square")
        {
            if (!inBounds(targetRow, targetColumn) ||
                holes[static_cast<std::size_t>(squareIndex(targetRow, targetColumn))] != 0 ||
                card.effect != "resources")
            {
                setStatusFor(playerNumber, "That square enchantment must add Resources to a board square.");
                return false;
            }
            enchantment.target = static_cast<std::uint8_t>(EnchantmentTarget::Square);
            enchantment.targetRow = targetRow;
            enchantment.targetColumn = targetColumn;
        }
        else if (card.target == "piece")
        {
            Piece* targetPiece = inBounds(targetRow, targetColumn)
                ? pieceAt(targetRow, targetColumn)
                : nullptr;
            if (targetPiece == nullptr || card.effect != "damage")
            {
                setStatusFor(playerNumber, "That piece enchantment must add damage to a piece target.");
                return false;
            }
            enchantment.target = static_cast<std::uint8_t>(EnchantmentTarget::Piece);
            enchantment.targetPieceId = targetPiece->id;
            enchantment.targetRow = targetPiece->row;
            enchantment.targetColumn = targetPiece->column;
        }
        else
        {
            setStatusFor(playerNumber, "That enchantment needs a player, square, or piece target.");
            return false;
        }

        enchantments.push_back(std::move(enchantment));
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
