#pragma once

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace game_data;

class GameEngine
{
public:
    static constexpr std::int64_t RegularClockMs = 15 * 60 * 1000;
    static constexpr std::int64_t IncrementMs = 10 * 1000;
    static constexpr std::size_t IncrementMoveLimit = 30;
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
        std::vector<GameCard> foresightChoices;
        std::vector<GameCard> hand;
        std::vector<GameCard> heroesToPlace;
        int resources = 0;
        int discardsThisTurn = 0;
        bool pieceActionUsedThisTurn = false;
        bool deckSubmitted = false;
    };

    struct ScenarioPiece
    {
        int owner = 1;
        GameCard card;
        int row = 0;
        int column = 0;
        bool isHero = false;
        int initialHealth = -1;
    };

    enum class ScenarioObjectiveKind : std::uint8_t
    {
        None,
        DefeatOriginalOwner,
        DefeatPiece,
        ReachSquare,
        ControlSquares
    };

    enum class ScenarioObjectiveFailure : std::uint8_t
    {
        None,
        RequiredPieceMissing,
        ObjectivePieceMissing,
        ForceEliminated
    };

    // Optional terminal rules for authored or generated scenarios. These are
    // deliberately expressed in engine terms rather than campaign terms so
    // any local scenario can use the same authoritative adjudication.
    struct ScenarioObjective
    {
        ScenarioObjectiveKind kind = ScenarioObjectiveKind::None;
        int successPlayer = 1;
        int opposingOriginalOwner = 2;
        int targetPieceId = 0;
        int targetRow = -1;
        int targetColumn = -1;
        int controlAmount = 0;
        // Zero disables whole-force defeat. Otherwise the scenario is lost
        // when no piece with this original owner remains.
        int requiredForceOriginalOwner = 0;
        std::vector<int> requiredSurvivorPieceIds;
    };

    struct ScenarioObjectiveProgress
    {
        bool configured = false;
        bool valid = false;
        bool complete = false;
        bool failed = false;
        int current = 0;
        int required = 0;
        int remaining = 0;
        int failedPieceId = 0;
        ScenarioObjectiveFailure failure = ScenarioObjectiveFailure::None;
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
    const ScenarioObjectiveProgress& scenarioObjectiveProgress() const
    {
        return scenarioObjectiveProgressValue;
    }
    const ScenarioObjective& scenarioObjective() const
    {
        return scenarioObjectiveValue;
    }
    const std::vector<Piece>& boardPieces() const { return pieces; }
    const std::vector<Enchantment>& boardEnchantments() const { return enchantments; }
    const std::array<std::uint8_t, BoardSquares>& boardControl() const { return control; }
    const std::array<std::uint8_t, BoardSquares>& boardHoles() const { return holes; }
    const EnginePlayer& playerState(int playerNumber) const { return playerRef(playerNumber); }
    bool hasPendingForesightChoice(int playerNumber) const
    {
        return playerNumber >= 1 && playerNumber <= 2 &&
            !playerRef(playerNumber).foresightChoices.empty();
    }

    // Seeds an authored board while preserving the ordinary match rules for
    // every action taken after setup. This is used by Story Mode and by tests;
    // multiplayer setup continues through submitDeck/placeHero/beginPlay.
    bool loadScenario(
        const std::vector<ScenarioPiece>& scenarioPieces,
        std::vector<GameCard> playerOneHand = {},
        std::vector<GameCard> playerTwoHand = {},
        int playerOneResources = 0,
        int playerTwoResources = 0,
        int firstPlayer = 1,
        std::string scenarioStatus = "Story encounter started.",
        bool heroEliminationVictory = true,
        std::vector<GameCard> playerOneDrawPile = {},
        std::vector<GameCard> playerTwoDrawPile = {})
    {
        // Validate the complete authored geometry before mutating any engine
        // state. Silently dropping an off-board or overlapping piece can turn
        // a broken Story setup into a different, falsely playable mission.
        std::array<bool, BoardSquares> occupied{};
        for (const ScenarioPiece& scenarioPiece : scenarioPieces)
        {
            const GameCard& card = scenarioPiece.card;
            if (scenarioPiece.owner < 1 || scenarioPiece.owner > 2 ||
                card.width < 1 || card.height < 1 ||
                card.width > BoardSize || card.height > BoardSize ||
                scenarioPiece.row < 0 || scenarioPiece.column < 0 ||
                scenarioPiece.row > BoardSize - card.height ||
                scenarioPiece.column > BoardSize - card.width)
            {
                return false;
            }
            for (int row = scenarioPiece.row;
                 row < scenarioPiece.row + card.height;
                 ++row)
            {
                for (int column = scenarioPiece.column;
                     column < scenarioPiece.column + card.width;
                     ++column)
                {
                    bool& squareOccupied =
                        occupied[static_cast<std::size_t>(squareIndex(row, column))];
                    if (squareOccupied)
                    {
                        return false;
                    }
                    squareOccupied = true;
                }
            }
        }

        phaseValue = Phase::Playing;
        activePlayer = std::clamp(firstPlayer, 1, 2);
        winnerValue = 0;
        heroEliminationVictoryEnabled = heroEliminationVictory;
        control.fill(0);
        holes.fill(0);
        pieces.clear();
        enchantments.clear();
        nextPieceId = 1;
        nextEnchantmentId = 1;
        commandingPieceId = 0;
        relentlessPieceId = 0;
        relentlessActionKeepsTurn = false;
        scenarioObjectiveValue = {};
        scenarioObjectiveProgressValue = {};

        for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
        {
            EnginePlayer& player = playerRef(playerNumber);
            player.number = playerNumber;
            player.drawPile = playerNumber == 1
                ? std::move(playerOneDrawPile)
                : std::move(playerTwoDrawPile);
            player.foresightChoices.clear();
            player.hand = playerNumber == 1
                ? std::move(playerOneHand)
                : std::move(playerTwoHand);
            player.heroesToPlace.clear();
            player.resources = playerNumber == 1
                ? std::max(0, playerOneResources)
                : std::max(0, playerTwoResources);
            player.discardsThisTurn = 0;
            player.pieceActionUsedThisTurn = false;
            player.deckSubmitted = true;
            for (const GameCard& card : player.hand)
            {
                rememberSummonCard(card);
            }
            for (const GameCard& card : player.drawPile)
            {
                rememberSummonCard(card);
            }
        }

        for (const ScenarioPiece& scenarioPiece : scenarioPieces)
        {
            rememberSummonCard(scenarioPiece.card);
            spawnPiece(
                scenarioPiece.owner,
                scenarioPiece.card,
                scenarioPiece.row,
                scenarioPiece.column,
                scenarioPiece.isHero);
            if (scenarioPiece.initialHealth > 0)
            {
                pieces.back().health = std::clamp(
                    scenarioPiece.initialHealth,
                    1,
                    pieces.back().maxHealth);
            }
        }
        recomputeControl();
        for (Piece& piece : pieces)
        {
            if (piece.owner == activePlayer)
            {
                beginPieceTurn(piece);
            }
        }
        status = std::move(scenarioStatus);
        return true;
    }

    // Story scenarios can depend on cards that are not initially in either
    // hand or on the board (Summon, Trail, and Rebirth destinations).
    void registerScenarioCard(const GameCard& card)
    {
        rememberSummonCard(card);
    }

    bool configureScenarioObjective(ScenarioObjective objective)
    {
        scenarioObjectiveValue = std::move(objective);
        scenarioObjectiveProgressValue = {};
        scenarioObjectiveProgressValue.configured = true;

        const auto validPlayer = [](int player) {
            return player == 1 || player == 2;
        };
        const auto liveIdentity = [&](int pieceId) {
            return pieceId > 0 && std::any_of(
                pieces.begin(), pieces.end(),
                [&](const Piece& piece) { return piece.id == pieceId; });
        };

        bool valid = phaseValue == Phase::Playing &&
            validPlayer(scenarioObjectiveValue.successPlayer) &&
            (scenarioObjectiveValue.requiredForceOriginalOwner == 0 ||
             validPlayer(scenarioObjectiveValue.requiredForceOriginalOwner));
        for (int pieceId : scenarioObjectiveValue.requiredSurvivorPieceIds)
        {
            valid = valid && liveIdentity(pieceId);
        }
        if (scenarioObjectiveValue.requiredForceOriginalOwner != 0)
        {
            valid = valid && std::any_of(
                pieces.begin(), pieces.end(), [&](const Piece& piece) {
                    return pieceOriginalOwner(piece) ==
                        scenarioObjectiveValue.requiredForceOriginalOwner;
                });
        }

        switch (scenarioObjectiveValue.kind)
        {
        case ScenarioObjectiveKind::None:
            break;
        case ScenarioObjectiveKind::DefeatOriginalOwner:
            valid = valid && validPlayer(scenarioObjectiveValue.opposingOriginalOwner) &&
                scenarioObjectiveValue.opposingOriginalOwner !=
                    scenarioObjectiveValue.successPlayer &&
                std::any_of(pieces.begin(), pieces.end(), [&](const Piece& piece) {
                    return pieceOriginalOwner(piece) ==
                        scenarioObjectiveValue.opposingOriginalOwner;
                });
            break;
        case ScenarioObjectiveKind::DefeatPiece:
            valid = valid && liveIdentity(scenarioObjectiveValue.targetPieceId);
            break;
        case ScenarioObjectiveKind::ReachSquare:
            valid = valid && liveIdentity(scenarioObjectiveValue.targetPieceId) &&
                inBounds(
                    scenarioObjectiveValue.targetRow,
                    scenarioObjectiveValue.targetColumn);
            break;
        case ScenarioObjectiveKind::ControlSquares:
            valid = valid && scenarioObjectiveValue.controlAmount > 0 &&
                scenarioObjectiveValue.controlAmount <= BoardSquares;
            break;
        }

        scenarioObjectiveProgressValue.valid = valid;
        if (!valid)
        {
            if (narrationEnabled)
            {
                status = "Scenario objective configuration is invalid.";
            }
            return false;
        }

        evaluateScenarioObjective();
        return true;
    }

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
        movesWithIncrement.fill(0);
        turnTimerLevels.fill(0);
        turnTimerDurations = {
            fullTurnTimerMs,
            reducedTurnTimerMs,
            minimumTurnTimerMs};
        turnRemainingMs = turnTimerDurations[0];
    }

    bool timersAreEnabled() const { return timersEnabled; }

    // Search copies do not show their status line to anyone, and composing it
    // costs more than simulating the action it describes. Turning narration off
    // leaves every rules outcome untouched and only stops the prose.
    void setNarrationEnabled(bool enabled) { narrationEnabled = enabled; }
    bool narrationIsEnabled() const { return narrationEnabled; }

    std::int64_t timeUntilNextTimerEventMs() const
    {
        if (!timersEnabled ||
            (phaseValue != Phase::Playing && phaseValue != Phase::HeroPlacement))
        {
            return 0;
        }
        if (phaseValue == Phase::HeroPlacement)
        {
            return turnRemainingMs;
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
        if (!timersEnabled ||
            (phaseValue != Phase::Playing && phaseValue != Phase::HeroPlacement) ||
            elapsedMs <= 0)
        {
            return false;
        }

        bool transitioned = false;
        while (elapsedMs > 0 &&
               (phaseValue == Phase::Playing || phaseValue == Phase::HeroPlacement))
        {
            const std::size_t activeIndex = static_cast<std::size_t>(activePlayer - 1);
            const bool placingHeroes = phaseValue == Phase::HeroPlacement;
            const std::int64_t untilEvent = placingHeroes
                ? turnRemainingMs
                : std::min(playerClockRemainingMs[activeIndex], turnRemainingMs);
            const std::int64_t consumed = std::min(elapsedMs, untilEvent);
            if (!placingHeroes)
            {
                playerClockRemainingMs[activeIndex] -= consumed;
            }
            turnRemainingMs -= consumed;
            elapsedMs -= consumed;

            // The match clock is decisive when both deadlines land together.
            if (!placingHeroes && playerClockRemainingMs[activeIndex] <= 0)
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
                if (placingHeroes)
                {
                    winnerValue = timedOutPlayer == 1 ? 2 : 1;
                    phaseValue = Phase::GameOver;
                    status = fmt::format(
                        "Player {} ran out of hero placement time. Player {} wins!",
                        timedOutPlayer,
                        winnerValue);
                    return true;
                }

                std::size_t& level = turnTimerLevels[activeIndex];
                level = std::min<std::size_t>(level + 1, 2);
                advanceTurn(fmt::format(
                    "Player {}'s turn timer expired.",
                    timedOutPlayer));
                evaluateScenarioObjective();
                transitioned = true;
            }
        }
        return transitioned;
    }

    // A successful play, piece action, ability, discard, or pass restores that
    // player's next turn to two minutes. The first 30 completed moves also
    // receive a 10-second increment on that player's authoritative clock.
    void recordPlayerMove(int playerNumber)
    {
        if (!timersEnabled || playerNumber < 1 || playerNumber > 2)
        {
            return;
        }
        const std::size_t playerIndex = static_cast<std::size_t>(playerNumber - 1);
        if (movesWithIncrement[playerIndex] < IncrementMoveLimit)
        {
            playerClockRemainingMs[playerIndex] += IncrementMs;
            ++movesWithIncrement[playerIndex];
        }
        turnTimerLevels[playerIndex] = 0;
        if (phaseValue == Phase::Playing && activePlayer == playerNumber)
        {
            turnRemainingMs = turnTimerDurations[0];
        }
    }

    // Splits already-resolved cards into the same shuffled draw pile and hero
    // roster used by ordinary multiplayer setup. Story capture fixtures call
    // this entry point only after their reviewed definitions pass the same deck
    // contract; connected matches continue to submit authoritative Card rows.
    void submitResolvedDeck(int playerNumber, std::vector<GameCard> cards)
    {
        EnginePlayer& player = playerRef(playerNumber);
        player.drawPile.clear();
        player.foresightChoices.clear();
        player.heroesToPlace.clear();

        for (GameCard& resolved : cards)
        {
            rememberSummonCard(resolved);
            if (resolved.type == "Hero")
            {
                if (static_cast<int>(player.heroesToPlace.size()) < MaxHeroes)
                {
                    player.heroesToPlace.push_back(std::move(resolved));
                }
            }
            else
            {
                player.drawPile.push_back(std::move(resolved));
            }
        }

        std::shuffle(player.drawPile.begin(), player.drawPile.end(), rng);
        player.deckSubmitted = true;

        if (bothDecksSubmitted())
        {
            activePlayer = 1;
            if (timersEnabled)
            {
                turnRemainingMs = turnTimerDurations[0];
            }
            status = "Place your heroes on your starting squares.";
        }
    }

    // Splits an authoritative submitted deck into a shuffled draw pile and a
    // hero roster.
    void submitDeck(int playerNumber, const std::vector<card_data::Card>& cards)
    {
        std::vector<GameCard> resolved;
        resolved.reserve(cards.size());
        for (const card_data::Card& card : cards)
        {
            resolved.push_back(toGameCard(card));
        }
        submitResolvedDeck(playerNumber, std::move(resolved));
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
        else if (playerNumber == activePlayer && player.heroesToPlace.empty())
        {
            activePlayer = playerNumber == 1 ? 2 : 1;
            if (timersEnabled)
            {
                turnRemainingMs = turnTimerDurations[0];
            }
        }
        return true;
    }

    bool playCard(int playerNumber, int handIndex, int targetRow, int targetColumn)
    {
        if (hasPendingForesightChoice(playerNumber))
        {
            return false;
        }
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        if (pendingRepeatPiece(playerNumber) != nullptr)
        {
            setStatusFor(playerNumber, "Finish the repeatable action or pass before playing a card.");
            return false;
        }
        if (relentlessPieceId != 0)
        {
            setStatusFor(playerNumber, "The Relentless piece must act again or you must pass.");
            return false;
        }
        if (commandingPieceId != 0)
        {
            setStatusFor(playerNumber, "Resolve Command with an adjacent piece or pass before playing a card.");
            return false;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (handIndex < 0 || handIndex >= static_cast<int>(player.hand.size()))
        {
            return false;
        }

        const GameCard card = player.hand[static_cast<std::size_t>(handIndex)];
        // Treat an incomplete catalog row as unplayable card data before
        // affordability or targeting. This gives the same truthful reason at
        // every resource total and guarantees no cost can ever be charged.
        if (card.type == "Spell" && !isSupportedSpellEffect(card))
        {
            setStatusFor(
                playerNumber,
                "This spell has no defined game effect and cannot be played.");
            return false;
        }
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
                // The client correctly treats opposing hidden pieces as absent.
                // If every otherwise-valid blocker is hidden, accept this as a
                // real collision: the attempted card is spent and every blocker
                // materializes stunned, matching ordinary movement collisions.
                bool validControlledFootprint =
                    targetRow >= 0 && targetColumn >= 0 &&
                    targetRow + card.height <= BoardSize &&
                    targetColumn + card.width <= BoardSize;
                std::vector<int> hiddenBlockerIds;
                for (int row = targetRow;
                     validControlledFootprint && row < targetRow + card.height;
                     ++row)
                {
                    for (int column = targetColumn;
                         column < targetColumn + card.width;
                         ++column)
                    {
                        if (control[static_cast<std::size_t>(squareIndex(row, column))] !=
                            playerNumber)
                        {
                            validControlledFootprint = false;
                            break;
                        }
                        const Piece* blocker = pieceAt(row, column);
                        if (blocker == nullptr)
                        {
                            continue;
                        }
                        if (!blocker->hidden || blocker->owner == playerNumber)
                        {
                            validControlledFootprint = false;
                            break;
                        }
                        if (std::find(
                                hiddenBlockerIds.begin(),
                                hiddenBlockerIds.end(),
                                blocker->id) == hiddenBlockerIds.end())
                        {
                            hiddenBlockerIds.push_back(blocker->id);
                        }
                    }
                }
                if (!validControlledFootprint || hiddenBlockerIds.empty())
                {
                    setStatusFor(playerNumber, "Units deploy onto an empty square you control.");
                    return false;
                }

                std::vector<std::string> hiddenBlockerNames;
                for (int blockerId : hiddenBlockerIds)
                {
                    if (Piece* blocker = pieceById(blockerId))
                    {
                        hiddenBlockerNames.push_back(blocker->name);
                        materializeRevealedPiece(*blocker);
                    }
                }
                player.resources -= card.cost;
                player.hand.erase(player.hand.begin() + handIndex);
                recordPlayerMove(playerNumber);
                recomputeControl();
                if (narrationEnabled)
                {
                    std::string names;
                    for (std::size_t index = 0; index < hiddenBlockerNames.size(); ++index)
                    {
                        if (index > 0)
                        {
                            names += index + 1 == hiddenBlockerNames.size() ? " and " : ", ";
                        }
                        names += hiddenBlockerNames[index];
                    }
                    status = fmt::format(
                        "{} tried to deploy, but collided with hidden {}. The hidden piece{} materialized and became Disabled for {} next owner-turn activation; the card and its Resources were spent.",
                        card.title,
                        names,
                        hiddenBlockerNames.size() == 1 ? "" : "s",
                        hiddenBlockerNames.size() == 1 ? "its" : "their");
                }
                evaluateScenarioObjective();
                return true;
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
        recomputeControl();
        if (narrationEnabled)
        {
            status = fmt::format("Player {} played {}.", playerNumber, card.title);
        }
        evaluateScenarioObjective();
        return true;
    }

    // Discarding sends the card to the bottom of the draw pile without ending the turn.
    bool discardCard(int playerNumber, int handIndex)
    {
        if (hasPendingForesightChoice(playerNumber))
        {
            return false;
        }
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        if (pendingRepeatPiece(playerNumber) != nullptr)
        {
            setStatusFor(playerNumber, "Finish the repeatable action or pass before discarding.");
            return false;
        }
        if (relentlessPieceId != 0)
        {
            setStatusFor(playerNumber, "The Relentless piece must act again or you must pass.");
            return false;
        }
        if (commandingPieceId != 0)
        {
            setStatusFor(playerNumber, "Resolve Command with an adjacent piece or pass before discarding.");
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
        if (narrationEnabled)
        {
            status =
                fmt::format("Player {} discarded {} to the bottom of the deck.", playerNumber, card.title);
        }
        recordPlayerMove(playerNumber);
        return true;
    }

    bool movePiece(
        int playerNumber,
        int pieceId,
        int toRow,
        int toColumn,
        int selectedActionIndex = -1)
    {
        const bool accepted = performPieceAction(
            playerNumber, pieceId, toRow, toColumn, selectedActionIndex);
        if (accepted)
        {
            recordPlayerMove(playerNumber);
            evaluateScenarioObjective();
        }
        return accepted;
    }

    bool attackPiece(
        int playerNumber,
        int attackerId,
        int targetRow,
        int targetColumn,
        int selectedActionIndex = -1)
    {
        const bool accepted = performPieceAction(
            playerNumber, attackerId, targetRow, targetColumn, selectedActionIndex);
        if (accepted)
        {
            recordPlayerMove(playerNumber);
            evaluateScenarioObjective();
        }
        return accepted;
    }

    bool useAbility(int playerNumber, int pieceId)
    {
        const bool accepted = useAbilityWithoutRecordingMove(playerNumber, pieceId);
        if (accepted)
        {
            recordPlayerMove(playerNumber);
            evaluateScenarioObjective();
        }
        return accepted;
    }

private:
    bool useAbilityWithoutRecordingMove(int playerNumber, int pieceId)
    {
        if (hasPendingForesightChoice(playerNumber))
        {
            return false;
        }
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }

        Piece* piece = pieceById(pieceId);
        bool hiddenSummonCollision = false;
        if (piece != nullptr && piece->owner == playerNumber &&
            !piece->hasActed && pieceAbilityAvailable(*piece) &&
            normalizedAbility(piece->ability) == "summon")
        {
            const auto [summonRow, summonColumn] = summonDestination(*piece);
            const Piece* blocker = inBounds(summonRow, summonColumn)
                ? pieceAt(summonRow, summonColumn)
                : nullptr;
            hiddenSummonCollision = blocker != nullptr && blocker->hidden &&
                blocker->owner != playerNumber;
        }
        if (piece == nullptr || piece->owner != playerNumber || piece->hasActed ||
            (!pieceAbilityAvailable(pieces, *piece) && !hiddenSummonCollision))
        {
            return false;
        }
        if (pendingRepeatPiece(playerNumber) != nullptr)
        {
            setStatusFor(playerNumber, "Finish the repeatable action or pass before using an ability.");
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
        if (playerRef(playerNumber).pieceActionUsedThisTurn &&
            !commandedAction && !relentlessAction)
        {
            setStatusFor(playerNumber, "Only one piece may take a normal action each turn.");
            return false;
        }
        if (commandedAction && !pieceCanReceiveCommand(*commander, *piece))
        {
            setStatusFor(playerNumber, "Command must activate a ready adjacent friendly piece.");
            return false;
        }

        const std::string abilityLabel = pieceAbilityLabel(*piece);
        const std::string actingPieceName = piece->name;
        const int actingPieceId = piece->id;
        const std::string commanderName = commandedAction ? commander->name : std::string();
        std::string hiddenAbilityCollisionName;
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
            Piece* summonBlocker = inBounds(row, column) ? pieceAt(row, column) : nullptr;
            if (summonBlocker != nullptr && summonBlocker->hidden &&
                summonBlocker->owner != playerNumber)
            {
                hiddenAbilityCollisionName = summonBlocker->name;
                materializeRevealedPiece(*summonBlocker);
            }
            else if (!pieceSummonDestinationFree(pieces, *piece))
            {
                setStatusFor(playerNumber, "That summon needs an empty space in front.");
                return false;
            }
            if (hiddenAbilityCollisionName.empty())
            {
                spawnPiece(playerNumber, *summonCard, row, column, false);
                pieces.back().hasActed = true;
            }
        }
        else if (piece->ability == "command")
        {
            piece->hasActed = true;
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
            commandingPieceId = piece->id;
            if (narrationEnabled)
            {
                status = fmt::format(
                    "{} used Command. Activate one adjacent friendly piece.",
                    actingPieceName);
            }
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
        if (!commandedAction)
        {
            playerRef(playerNumber).pieceActionUsedThisTurn = true;
        }
        if (relentlessAction)
        {
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
        }
        if (commandedAction)
        {
            commandingPieceId = 0;
            recomputeControl();
            if (narrationEnabled)
            {
                status = hiddenAbilityCollisionName.empty()
                    ? fmt::format(
                          "{} commanded {} to use {}.",
                          commanderName,
                          actingPieceName,
                          abilityLabel)
                    : fmt::format(
                          "{} commanded {} to use {}, but it collided with hidden {}. The hidden piece materialized and became Disabled for its next owner-turn activation; no unit was summoned.",
                          commanderName,
                          actingPieceName,
                          abilityLabel,
                          hiddenAbilityCollisionName);
            }
        }
        else
        {
            recomputeControl();
            if (narrationEnabled)
            {
                status = hiddenAbilityCollisionName.empty()
                    ? fmt::format("{} used {}.", actingPieceName, abilityLabel)
                    : fmt::format(
                          "{} tried to use {}, but collided with hidden {}. The hidden piece materialized and became Disabled for its next owner-turn activation; no unit was summoned.",
                          actingPieceName,
                          abilityLabel,
                          hiddenAbilityCollisionName);
            }
        }
        return true;
    }

public:
    bool chooseForesightCard(int playerNumber, int choiceIndex)
    {
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        EnginePlayer& player = playerRef(playerNumber);
        if (player.foresightChoices.empty() || choiceIndex < 0 ||
            choiceIndex >= static_cast<int>(player.foresightChoices.size()))
        {
            return false;
        }

        const GameCard chosenCard = player.foresightChoices[static_cast<std::size_t>(choiceIndex)];
        for (std::size_t i = player.foresightChoices.size(); i-- > 0;)
        {
            if (static_cast<int>(i) != choiceIndex)
            {
                player.drawPile.insert(player.drawPile.begin(), player.foresightChoices[i]);
            }
        }
        player.foresightChoices.clear();
        player.hand.push_back(chosenCard);
        status = fmt::format("Player {} chose {} with Foresight.", playerNumber, chosenCard.title);
        return true;
    }

    bool drawCard(int playerNumber)
    {
        if (hasPendingForesightChoice(playerNumber))
        {
            return false;
        }
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        if (pendingRepeatPiece(playerNumber) != nullptr)
        {
            setStatusFor(playerNumber, "Finish the repeatable action or pass before drawing.");
            return false;
        }
        if (relentlessPieceId != 0)
        {
            setStatusFor(playerNumber, "The Relentless piece must act again or you must pass before drawing.");
            return false;
        }
        if (commandingPieceId != 0)
        {
            setStatusFor(playerNumber, "Resolve Command with an adjacent piece or pass before drawing.");
            return false;
        }

        EnginePlayer& player = playerRef(playerNumber);
        if (static_cast<int>(player.hand.size()) >= MaxHandSize)
        {
            setStatusFor(playerNumber, "Your hand is full.");
            return false;
        }
        if (player.drawPile.empty())
        {
            setStatusFor(playerNumber, "Your deck is empty.");
            return false;
        }
        if (player.resources < DrawCardResourceCost)
        {
            setStatusFor(
                playerNumber,
                fmt::format("Drawing a card costs {} Resources.", DrawCardResourceCost));
            return false;
        }

        player.resources -= DrawCardResourceCost;
        const int foresightPieces = foresightPieceCount(playerNumber);
        if (foresightPieces > 0)
        {
            prepareForesightDraw(player, foresightPieces);
            if (narrationEnabled)
            {
                status = fmt::format(
                    "Player {} spent {} Resources to draw with Foresight.",
                    playerNumber,
                    DrawCardResourceCost);
            }
        }
        else
        {
            drawTopCard(player);
            if (narrationEnabled)
            {
                status = fmt::format(
                    "Player {} spent {} Resources to draw a card.",
                    playerNumber,
                    DrawCardResourceCost);
            }
        }
        recordPlayerMove(playerNumber);
        return true;
    }

    bool endTurn(int playerNumber)
    {
        if (hasPendingForesightChoice(playerNumber))
        {
            return false;
        }
        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }

        recordPlayerMove(playerNumber);
        advanceTurn(
            narrationEnabled ? fmt::format("Player {} passed.", playerNumber) : std::string());
        evaluateScenarioObjective();
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
        if (phaseValue == Phase::Playing && playerNumber == activePlayer)
        {
            snapshot.foresightChoices = viewingPlayer.foresightChoices;
        }
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
            view.pieceActionUsedThisTurn = player.pieceActionUsedThisTurn;
            view.clockRemainingMs = playerClockRemainingMs[static_cast<std::size_t>(p)];
        }

        return snapshot;
    }

    // A tally a player can read straight off their own snapshot.
    int controlledSquares(int playerNumber) const { return controlledCount(playerNumber); }

    // Reduces this engine to what one player is entitled to reason about, and
    // returns how many opposing heroes were removed by that reduction.
    // Opposing dematerialized pieces are erased outright - they are absent
    // from that player's snapshot, so a copy used to plan a move must not
    // contain them either - and the opponent's hidden cards go with them. A
    // player still sees the opponent's living-hero count in their snapshot, so
    // the returned tally lets a planner keep that number honest without
    // learning where a concealed hero stands. Presentation-only strings are
    // dropped too: a planning copy is duplicated thousands of times during a
    // search and never rendered.
    int redactForPlanning(int playerNumber)
    {
        narrationEnabled = false;
        status.clear();

        const int opponent = playerNumber == 1 ? 2 : 1;
        int concealedOpponentHeroes = 0;
        for (const Piece& piece : pieces)
        {
            if (piece.hidden && piece.owner != playerNumber && piece.isHero)
            {
                ++concealedOpponentHeroes;
            }
        }
        pieces.erase(
            std::remove_if(
                pieces.begin(),
                pieces.end(),
                [&](const Piece& piece) { return piece.hidden && piece.owner != playerNumber; }),
            pieces.end());
        enchantments.erase(
            std::remove_if(
                enchantments.begin(),
                enchantments.end(),
                [&](const Enchantment& enchantment) {
                    return enchantment.target ==
                        static_cast<std::uint8_t>(EnchantmentTarget::Piece) &&
                        std::none_of(
                            pieces.begin(),
                            pieces.end(),
                            [&](const Piece& piece) {
                                return piece.id == enchantment.targetPieceId;
                            });
                }),
            enchantments.end());

        EnginePlayer& hiddenSide = playerRef(opponent);
        hiddenSide.hand.clear();
        hiddenSide.drawPile.clear();
        hiddenSide.foresightChoices.clear();
        hiddenSide.heroesToPlace.clear();

        for (Piece& piece : pieces)
        {
            stripPresentation(piece);
        }
        // This copy no longer owns a complete board, so it must never produce
        // an authoritative scenario result from redacted information.
        scenarioObjectiveValue = {};
        scenarioObjectiveProgressValue = {};
        EnginePlayer& planner = playerRef(playerNumber);
        // A player knows what is in their deck but not what order it is in, so
        // the planner gets one plausible ordering instead of the real one. It
        // may still decide a draw is worth paying for; it just cannot pick the
        // turn that lands the card it wants.
        std::shuffle(planner.drawPile.begin(), planner.drawPile.end(), rng);
        for (GameCard& card : planner.hand)
        {
            stripPresentation(card);
        }
        for (GameCard& card : planner.drawPile)
        {
            stripPresentation(card);
        }
        for (GameCard& card : planner.heroesToPlace)
        {
            stripPresentation(card);
        }
        for (GameCard& card : mutableSummonCatalog())
        {
            stripPresentation(card);
        }
        return concealedOpponentHeroes;
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
    // The summon catalog is the whole card library and stops changing once a
    // match is under way, so copies of the engine (the AI search makes many)
    // share one and only detach if a copy actually records a new card.
    std::shared_ptr<std::vector<GameCard>> summonCatalog =
        std::make_shared<std::vector<GameCard>>();
    int nextPieceId = 1;
    int nextEnchantmentId = 1;
    int commandingPieceId = 0;
    int relentlessPieceId = 0;
    bool relentlessActionKeepsTurn = false;
    bool heroEliminationVictoryEnabled = true;
    ScenarioObjective scenarioObjectiveValue;
    ScenarioObjectiveProgress scenarioObjectiveProgressValue;
    bool timersEnabled = false;
    std::array<std::int64_t, 2> playerClockRemainingMs{RegularClockMs, RegularClockMs};
    std::array<std::size_t, 2> movesWithIncrement{};
    std::array<std::size_t, 2> turnTimerLevels{};
    std::array<std::int64_t, 3> turnTimerDurations{
        FullTurnTimerMs,
        ReducedTurnTimerMs,
        MinimumTurnTimerMs};
    std::int64_t turnRemainingMs = FullTurnTimerMs;
    bool narrationEnabled = true;
    std::string status = "Waiting for both decks...";

    template <typename Renderable>
    static void stripPresentation(Renderable& value)
    {
        value.imagePath.clear();
        value.walkAnimPath.clear();
        value.idleAnimPath.clear();
        value.attackAnimPath.clear();
        value.damagedAnimPath.clear();
        value.killedAnimPath.clear();
        value.fidgetAnimPath.clear();
        value.tokenPath.clear();
        value.state1TokenPath.clear();
        value.pieceBaseBluePath.clear();
        value.pieceBaseRedPath.clear();
        value.abilityLabels.clear();
    }

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

    Piece* pendingRepeatPiece(int playerNumber)
    {
        const auto found = std::find_if(
            pieces.begin(),
            pieces.end(),
            [playerNumber](const Piece& piece) {
                return piece.owner == playerNumber && piece.repeatActionIndex >= 0;
            });
        return found == pieces.end() ? nullptr : &*found;
    }

    struct PieceDestructionResult
    {
        bool replacementSpawned = false;
        bool wasRebirth = false;
        bool wasInfestation = false;
    };

    // A replacement prevents the death from counting as a kill. The result
    // identifies whether that replacement came from Rebirth or infestation.
    PieceDestructionResult destroyPiece(int id)
    {
        PieceDestructionResult result;
        const auto dying = std::find_if(
            pieces.begin(),
            pieces.end(),
            [id](const Piece& piece) { return piece.id == id; });
        if (dying == pieces.end())
        {
            return result;
        }

        const Piece original = *dying;
        const GameCard* infestationDefinition = summonCardByTitle(original.infestationTitle);
        const bool hasValidInfestation = !original.isHero &&
            original.infestationOwner >= 1 && original.infestationOwner <= 2 &&
            infestationDefinition != nullptr && infestationDefinition->type == "Unit";
        const GameCard* rebirthDefinition = summonCardByTitle(original.rebirthTitle);
        const bool hasValidRebirth = rebirthDefinition != nullptr &&
            (rebirthDefinition->type == "Unit" || rebirthDefinition->type == "Hero");
        const bool hasValidReplacement = hasValidInfestation || hasValidRebirth;
        const bool replacementIsInfestation = hasValidInfestation;
        const GameCard replacementCard = hasValidInfestation
            ? *infestationDefinition
            : (hasValidRebirth ? *rebirthDefinition : GameCard{});

        pieces.erase(dying);

        int replacementPieceId = 0;
        if (hasValidReplacement &&
            cardFootprintFree(pieces, replacementCard, original.row, original.column))
        {
            spawnPiece(
                replacementIsInfestation
                    ? original.infestationOwner
                    : pieceOriginalOwner(original),
                replacementCard,
                original.row,
                original.column,
                !replacementIsInfestation && replacementCard.type == "Hero");
            Piece& replacement = pieces.back();
            replacement.hasActed = true;
            replacementPieceId = replacement.id;
        }

        if (replacementPieceId != 0)
        {
            if (!replacementIsInfestation)
            {
                remapScenarioPieceIdentity(id, replacementPieceId);
            }
            for (Enchantment& enchantment : enchantments)
            {
                if (enchantment.target == static_cast<std::uint8_t>(EnchantmentTarget::Piece) &&
                    enchantment.targetPieceId == id)
                {
                    enchantment.targetPieceId = replacementPieceId;
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
        result.replacementSpawned = replacementPieceId != 0;
        result.wasInfestation = replacementIsInfestation && result.replacementSpawned;
        result.wasRebirth = !replacementIsInfestation && result.replacementSpawned;
        return result;
    }

    std::vector<GameCard>& mutableSummonCatalog()
    {
        if (summonCatalog.use_count() > 1)
        {
            summonCatalog = std::make_shared<std::vector<GameCard>>(*summonCatalog);
        }
        return *summonCatalog;
    }

    void rememberSummonCard(const GameCard& card)
    {
        std::vector<GameCard>& catalog = mutableSummonCatalog();
        const auto found = std::find_if(
            catalog.begin(),
            catalog.end(),
            [&](const GameCard& candidate) { return candidate.title == card.title; });
        if (found == catalog.end())
        {
            catalog.push_back(card);
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
            summonCatalog->begin(),
            summonCatalog->end(),
            [&](const GameCard& card) { return card.title == title; });
        return found == summonCatalog->end() ? nullptr : &*found;
    }

    bool performPieceAction(
        int playerNumber,
        int pieceId,
        int toRow,
        int toColumn,
        int selectedActionIndex)
    {
        if (hasPendingForesightChoice(playerNumber))
        {
            return false;
        }

        if (phaseValue != Phase::Playing || playerNumber != activePlayer)
        {
            return false;
        }
        Piece* piece = pieceById(pieceId);
        if (piece == nullptr || piece->owner != playerNumber)
        {
            return false;
        }
        if (selectedActionIndex < -1 ||
            selectedActionIndex >= static_cast<int>(piece->actions.size()))
        {
            setStatusFor(playerNumber, "That printed action does not exist on this piece.");
            return false;
        }
        const Piece* repeatingPiece = pendingRepeatPiece(playerNumber);
        if (repeatingPiece != nullptr && repeatingPiece->id != piece->id)
        {
            setStatusFor(playerNumber, "Finish the repeatable action with that piece or pass.");
            return false;
        }
        const bool continuingRepeat = piece->repeatActionIndex >= 0;
        const int requiredActionIndex = continuingRepeat
            ? piece->repeatActionIndex
            : selectedActionIndex;
        if (piece->hasActed && !continuingRepeat)
        {
            return false;
        }
        if (continuingRepeat)
        {
            if (requiredActionIndex >= static_cast<int>(piece->actions.size()) ||
                piece->repeatActionUses < 0)
            {
                piece->repeatActionIndex = -1;
                piece->repeatActionState = 0;
                piece->repeatActionUses = 0;
                piece->hasActed = true;
                return false;
            }
            if (selectedActionIndex >= 0 && selectedActionIndex != requiredActionIndex)
            {
                setStatusFor(playerNumber, "Finish the same repeatable printed action or pass.");
                return false;
            }
            // A repeat continues the original action even when that action's
            // normal next state would otherwise have changed the unit.
            piece->actionState = piece->repeatActionState;
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
        if (playerRef(playerNumber).pieceActionUsedThisTurn &&
            !continuingRepeat && !commandedAction && !relentlessAction)
        {
            setStatusFor(playerNumber, "Only one piece may take a normal action each turn.");
            return false;
        }
        if (commandedAction && !pieceCanReceiveCommand(*commander, *piece))
        {
            setStatusFor(playerNumber, "Command must activate a ready adjacent friendly piece.");
            return false;
        }

        // Resolve against the acting player's view of the board so hidden
        // enemy pieces do not block or betray their squares; a collision with
        // one is adjusted into a strike or a harmless bump below.
        const PieceActionOutcome outcome =
            resolvePieceActionThroughHidden(
                pieces, holes, *piece, toRow, toColumn, requiredActionIndex);
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
        const int attackerActionState = piece->actionState;
        std::vector<std::string> damagedTargetNames;
        std::vector<std::string> healedTargetNames;
        std::vector<std::string> controlledTargetNames;
        std::vector<int> defeatedOwners;
        bool anyTargetDestroyed = false;
        bool anyTargetReborn = false;
        bool anyTargetInfestationSpawned = false;
        bool anyTargetWasHidden = false;
        int pushedSquares = 0;
        int pushCollisionDamage = 0;
        int pulledSquares = 0;
        std::vector<int> revealedPieceIds = outcome.revealedPieceIds;
        const auto rememberRevealedPieces = [&](const std::vector<int>& ids) {
            for (int id : ids)
            {
                if (std::find(revealedPieceIds.begin(), revealedPieceIds.end(), id) ==
                    revealedPieceIds.end())
                {
                    revealedPieceIds.push_back(id);
                }
            }
        };
        const int attackDamage = action.damage +
            pieceEnchantmentDamageBonus(enchantments, attackerId);
        const std::string infestationTitle = action.actionIndex >= 0 &&
                action.actionIndex < static_cast<int>(piece->actions.size())
            ? piece->actions[static_cast<std::size_t>(action.actionIndex)].infest
            : std::string();
        const GameCard* infestationDefinition = summonCardByTitle(infestationTitle);
        const bool actionHasInfest = !infestationTitle.empty() &&
            infestationDefinition != nullptr && infestationDefinition->type == "Unit";

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
                    const DamageResolution damageResolution =
                        resolveDamageWithBodyguardsAndIntercepts(
                            pieces, targetId, attackDamage, action.statusTurns, rng, true);
                    const int effectiveTargetId = damageResolution.effectiveTargetId;
                    const Piece* effectiveTarget = pieceById(effectiveTargetId);
                    const std::string effectiveTargetName = effectiveTarget == nullptr
                        ? targetName
                        : (effectiveTarget->hidden ? "a hidden " : "") + effectiveTarget->name;
                    damagedTargetNames.push_back(effectiveTargetName);
                    if (actionHasInfest && effectiveTarget != nullptr && !effectiveTarget->isHero)
                    {
                        Piece* infestTarget = pieceById(effectiveTargetId);
                        if (infestTarget != nullptr)
                        {
                            infestTarget->infestationTitle = infestationTitle;
                            infestTarget->infestationOwner = attackerOwner;
                        }
                    }
                    for (const DamageAssignment& assignment : damageResolution.assignments)
                    {
                        Piece* damagedPiece = pieceById(assignment.pieceId);
                        if (damagedPiece != nullptr && damagedPiece->health <= 0)
                        {
                            defeatedOwners.push_back(pieceOriginalOwner(*damagedPiece));
                            const PieceDestructionResult destruction = destroyPiece(damagedPiece->id);
                            anyTargetReborn = anyTargetReborn || destruction.wasRebirth;
                            anyTargetInfestationSpawned =
                                anyTargetInfestationSpawned || destruction.wasInfestation;
                            anyTargetDestroyed = anyTargetDestroyed || !destruction.replacementSpawned;
                        }
                    }
                    const PushResult pushResult = applyActionPush(
                        pieces,
                        effectiveTargetId,
                        action.stagingRow,
                        action.stagingColumn,
                        action.push);
                    pushedSquares += pushResult.movedSquares;
                    pushCollisionDamage += pushResult.preventedSquares;
                    rememberRevealedPieces(pushResult.revealedPieceIds);
                    if (Piece* pushedTarget = pieceById(effectiveTargetId);
                        pushedTarget != nullptr && pushedTarget->health <= 0)
                    {
                        defeatedOwners.push_back(pieceOriginalOwner(*pushedTarget));
                        const PieceDestructionResult destruction = destroyPiece(pushedTarget->id);
                        anyTargetReborn = anyTargetReborn || destruction.wasRebirth;
                        anyTargetInfestationSpawned =
                            anyTargetInfestationSpawned || destruction.wasInfestation;
                        anyTargetDestroyed = anyTargetDestroyed || !destruction.replacementSpawned;
                    }
                    if (action.pull)
                    {
                        const PullResult pullResult = applyActionPull(
                            pieces,
                            effectiveTargetId,
                            attackerId);
                        pulledSquares += pullResult.movedSquares;
                        rememberRevealedPieces(pullResult.revealedPieceIds);
                    }
                    if (action.control > 0)
                    {
                        if (Piece* controllableTarget = pieceById(effectiveTargetId);
                            controllableTarget != nullptr && controllableTarget->health > 0)
                        {
                            applyPieceControl(*controllableTarget, attackerOwner, action.control);
                            if (controllableTarget->owner == attackerOwner)
                            {
                                controlledTargetNames.push_back(targetName);
                            }
                        }
                    }
                }
            }
            if (damagedTargetNames.empty() && healedTargetNames.empty()) return false;
        }

        // Hidden pieces struck or bumped into by the mover's footprint
        // or by a forced-movement footprint materialize stunned.
        std::vector<std::string> revealedNames;
        for (int revealedPieceId : revealedPieceIds)
        {
            if (Piece* revealed = pieceById(revealedPieceId))
            {
                revealedNames.push_back(revealed->name);
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
        const bool gainsRelentlessAction = anyTargetDestroyed &&
            hasKeyword(survivingAttacker->keywords, "relentless");
        bool repeatRemaining = false;
        if (continuingRepeat)
        {
            ++survivingAttacker->repeatActionUses;
            repeatRemaining = survivingAttacker->repeatActionUses < action.repeat;
        }
        else if (action.repeat > 0)
        {
            survivingAttacker->repeatActionIndex = action.actionIndex;
            survivingAttacker->repeatActionState = attackerActionState;
            survivingAttacker->repeatActionUses = 0;
            repeatRemaining = true;
        }
        if (repeatRemaining)
        {
            survivingAttacker->actionState = survivingAttacker->repeatActionState;
        }
        else
        {
            survivingAttacker->repeatActionIndex = -1;
            survivingAttacker->repeatActionState = 0;
            survivingAttacker->repeatActionUses = 0;
            setPieceActionState(*survivingAttacker, action.nextState);
        }
        survivingAttacker->hasActed = !gainsRelentlessAction && !repeatRemaining;
        if (!commandedAction)
        {
            playerRef(playerNumber).pieceActionUsedThisTurn = true;
        }
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
        if (narrationEnabled && action.attacks)
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
            if (pulledSquares > 0)
            {
                result += fmt::format(" and pulled targets {} square(s)", pulledSquares);
            }
            if (effectiveDisabledTurns > 0)
            {
                result += fmt::format(" and disabled surviving targets for {} turn(s)", effectiveDisabledTurns);
            }
            if (!controlledTargetNames.empty())
            {
                result += fmt::format(
                    " and controlled {} for {} turn(s)",
                    joinTargets(controlledTargetNames),
                    action.control);
            }
            result += anyTargetDestroyed ? "; at least one was destroyed!" : ".";
            if (anyTargetReborn)
            {
                result += " Rebirth returned a piece to the board!";
            }
            if (anyTargetInfestationSpawned)
            {
                result += " Infestation spawned a unit!";
            }
            if (!revealedNames.empty())
            {
                result += fmt::format(
                    " Hidden {} materialized. Each is Disabled for its next owner-turn activation.",
                    joinTargets(revealedNames));
            }
            else if (anyTargetWasHidden)
            {
                result += " It materialized!";
            }
        }
        else if (narrationEnabled && !revealedNames.empty())
        {
            std::string revealedList;
            for (std::size_t index = 0; index < revealedNames.size(); ++index)
            {
                if (index > 0)
                    revealedList += index + 1 == revealedNames.size() ? " and " : ", ";
                revealedList += revealedNames[index];
            }
            result = fmt::format(
                "{} bumped into hidden {}! Collision materialized each hidden piece and made it Disabled for its next owner-turn activation.",
                attackerName,
                revealedList);
        }
        else if (narrationEnabled)
        {
            result = fmt::format("{} moved.", attackerName);
        }

        if (narrationEnabled && repeatRemaining)
        {
            result += fmt::format(
                " {} repeat(s) remaining for this action.",
                action.repeat - survivingAttacker->repeatActionUses);
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
            if (narrationEnabled)
            {
                status =
                    (commandedAction ? fmt::format("{} commanded {}", commanderName, result) : result) +
                    " Relentless: it may act again immediately.";
            }
        }
        else if (commandedAction)
        {
            recomputeControl();
            if (narrationEnabled)
            {
                status = fmt::format("{} commanded {}", commanderName, result);
            }
        }
        else
        {
            relentlessPieceId = 0;
            relentlessActionKeepsTurn = false;
            recomputeControl();
            if (narrationEnabled)
            {
                status = result;
            }
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

    int foresightPieceCount(int playerNumber) const
    {
        int count = 0;
        for (const Piece& piece : pieces)
        {
            if (piece.owner == playerNumber && hasKeyword(piece.keywords, "foresight"))
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
            if (pieceOriginalOwner(piece) == playerNumber && piece.isHero)
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
                drawTopCard(player);
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
        player.foresightChoices.clear();
        player.discardsThisTurn = 0;
        player.pieceActionUsedThisTurn = false;
        updatePieceControlAtTurnStart(pieces, playerNumber);
        // A controlled piece can return to its original owner at the start of
        // this turn.  Control-derived income must use that restored ownership,
        // not the board-control snapshot from the preceding end turn.
        recomputeControl();
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
        for (Piece& piece : pieces)
        {
            if (piece.owner == playerNumber)
            {
                beginPieceTurn(piece);
            }
        }

        if (!narrationEnabled)
        {
            return;
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

    void drawTopCard(EnginePlayer& player)
    {
        if (player.drawPile.empty() || static_cast<int>(player.hand.size()) >= MaxHandSize)
        {
            return;
        }
        player.hand.push_back(player.drawPile.back());
        player.drawPile.pop_back();
    }

    void prepareForesightDraw(EnginePlayer& player, int foresightPieces)
    {
        player.foresightChoices.clear();
        if (player.drawPile.empty() || static_cast<int>(player.hand.size()) >= MaxHandSize)
        {
            return;
        }
        const int choiceCount = std::max(0, foresightPieces) + 1;
        for (int i = 0; i < choiceCount && !player.drawPile.empty(); ++i)
        {
            player.foresightChoices.push_back(player.drawPile.back());
            player.drawPile.pop_back();
        }
    }

    void advanceTurn(const std::string& actionStatus)
    {
        for (Piece& piece : pieces)
        {
            piece.repeatActionIndex = -1;
            piece.repeatActionState = 0;
            piece.repeatActionUses = 0;
        }
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
        if (narrationEnabled && !actionStatus.empty())
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
        applyHealingAuras(pieces, playerNumber);
        applyRevealKeywords(pieces, playerNumber);
    }

    bool resolveSpell(int playerNumber, const GameCard& card, int targetRow, int targetColumn)
    {
        if (!isSupportedSpellEffect(card))
        {
            setStatusFor(
                playerNumber,
                "This spell has no defined game effect and cannot be played.");
            return false;
        }

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
                    const int victimOwner = pieceOriginalOwner(*damagedPiece);
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

        // Kept defensive even though isSupportedSpellEffect above makes every
        // currently accepted effect return from one of the branches.
        setStatusFor(playerNumber, "This spell effect is not supported.");
        return false;
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
        if (heroEliminationVictoryEnabled && heroesAlive(victimOwner) == 0)
        {
            winnerValue = victimOwner == 1 ? 2 : 1;
            phaseValue = Phase::GameOver;
            status = fmt::format("All of Player {}'s heroes fell. Player {} wins!", victimOwner, winnerValue);
        }
    }

    bool hasOriginalOwnerPiece(int playerNumber) const
    {
        return std::any_of(
            pieces.begin(), pieces.end(), [&](const Piece& piece) {
                return pieceOriginalOwner(piece) == playerNumber;
            });
    }

    void remapScenarioPieceIdentity(int oldPieceId, int replacementPieceId)
    {
        if (!scenarioObjectiveProgressValue.configured ||
            oldPieceId <= 0 || replacementPieceId <= 0)
        {
            return;
        }
        if (scenarioObjectiveValue.targetPieceId == oldPieceId)
        {
            scenarioObjectiveValue.targetPieceId = replacementPieceId;
        }
        for (int& pieceId : scenarioObjectiveValue.requiredSurvivorPieceIds)
        {
            if (pieceId == oldPieceId)
            {
                pieceId = replacementPieceId;
            }
        }
    }

    bool hasPieceIdentity(int pieceId) const
    {
        return pieceId > 0 && std::any_of(
            pieces.begin(), pieces.end(),
            [&](const Piece& piece) { return piece.id == pieceId; });
    }

    void failScenarioObjective(ScenarioObjectiveFailure failure, int pieceId = 0)
    {
        scenarioObjectiveProgressValue.failed = true;
        scenarioObjectiveProgressValue.failure = failure;
        scenarioObjectiveProgressValue.failedPieceId = pieceId;
        winnerValue = scenarioObjectiveValue.successPlayer == 1 ? 2 : 1;
        phaseValue = Phase::GameOver;
        if (narrationEnabled)
        {
            if (failure == ScenarioObjectiveFailure::RequiredPieceMissing)
            {
                status = fmt::format(
                    "A required scenario piece (#{}) was defeated. Player {} wins!",
                    pieceId,
                    winnerValue);
            }
            else if (failure == ScenarioObjectiveFailure::ObjectivePieceMissing)
            {
                status = fmt::format(
                    "The piece required to reach the objective was defeated. Player {} wins!",
                    winnerValue);
            }
            else
            {
                status = fmt::format(
                    "Player {} has no surviving scenario force. Player {} wins!",
                    scenarioObjectiveValue.successPlayer,
                    winnerValue);
            }
        }
    }

    void completeScenarioObjective()
    {
        scenarioObjectiveProgressValue.complete = true;
        winnerValue = scenarioObjectiveValue.successPlayer;
        phaseValue = Phase::GameOver;
        if (narrationEnabled)
        {
            status = fmt::format(
                "Scenario objective secured. Player {} wins!",
                winnerValue);
        }
    }

    void evaluateScenarioObjective()
    {
        if (!scenarioObjectiveProgressValue.configured ||
            !scenarioObjectiveProgressValue.valid ||
            scenarioObjectiveProgressValue.complete ||
            scenarioObjectiveProgressValue.failed ||
            phaseValue != Phase::Playing)
        {
            return;
        }

        for (int pieceId : scenarioObjectiveValue.requiredSurvivorPieceIds)
        {
            if (!hasPieceIdentity(pieceId))
            {
                failScenarioObjective(
                    ScenarioObjectiveFailure::RequiredPieceMissing,
                    pieceId);
                return;
            }
        }
        if (scenarioObjectiveValue.requiredForceOriginalOwner != 0 &&
            !hasOriginalOwnerPiece(
                scenarioObjectiveValue.requiredForceOriginalOwner))
        {
            failScenarioObjective(ScenarioObjectiveFailure::ForceEliminated);
            return;
        }

        auto& progress = scenarioObjectiveProgressValue;
        switch (scenarioObjectiveValue.kind)
        {
        case ScenarioObjectiveKind::None:
            return;
        case ScenarioObjectiveKind::DefeatOriginalOwner:
            progress.remaining = static_cast<int>(std::count_if(
                pieces.begin(), pieces.end(), [&](const Piece& piece) {
                    return pieceOriginalOwner(piece) ==
                        scenarioObjectiveValue.opposingOriginalOwner;
                }));
            progress.current = 0;
            progress.required = 0;
            if (progress.remaining == 0)
            {
                completeScenarioObjective();
            }
            return;
        case ScenarioObjectiveKind::DefeatPiece:
            progress.required = 1;
            progress.remaining = hasPieceIdentity(
                scenarioObjectiveValue.targetPieceId) ? 1 : 0;
            progress.current = 1 - progress.remaining;
            if (progress.remaining == 0)
            {
                completeScenarioObjective();
            }
            return;
        case ScenarioObjectiveKind::ReachSquare:
        {
            progress.required = 1;
            const auto target = std::find_if(
                pieces.begin(), pieces.end(), [&](const Piece& piece) {
                    return piece.id == scenarioObjectiveValue.targetPieceId;
                });
            if (target == pieces.end())
            {
                failScenarioObjective(ScenarioObjectiveFailure::ObjectivePieceMissing);
                return;
            }
            progress.current =
                target->row == scenarioObjectiveValue.targetRow &&
                target->column == scenarioObjectiveValue.targetColumn ? 1 : 0;
            progress.remaining = 1 - progress.current;
            if (progress.current == 1)
            {
                completeScenarioObjective();
            }
            return;
        }
        case ScenarioObjectiveKind::ControlSquares:
            progress.required = scenarioObjectiveValue.controlAmount;
            progress.current = controlledCount(scenarioObjectiveValue.successPlayer);
            progress.remaining = std::max(0, progress.required - progress.current);
            if (progress.current >= progress.required)
            {
                completeScenarioObjective();
            }
            return;
        }
    }

    void setStatusFor(int playerNumber, const std::string& message)
    {
        if (narrationEnabled)
        {
            status = message;
        }
        (void)playerNumber;
    }

    void recomputeControl()
    {
        control = recomputeBoardControl(control, pieces);
    }
};
