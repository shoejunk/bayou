#pragma once

#include "game_engine.hpp"

#include "../shared/conquest_event_data.hpp"
#include "../shared/network.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>

// Shared decoding and dispatch for the tactical protocol. Ranked GameProcess
// and the durable Conquest battle runner must interpret an action identically;
// keeping the switch here also prevents the Conquest action log from accepting
// client-supplied decks or lifecycle messages.
namespace game_action
{

inline bool supported(network::MessageType type)
{
    switch (type)
    {
        case network::MessageType::PlaceHero:
        case network::MessageType::PlayCard:
        case network::MessageType::MovePiece:
        case network::MessageType::AttackPiece:
        case network::MessageType::UseAbility:
        case network::MessageType::DiscardCard:
        case network::MessageType::EndTurn:
            return true;
        default:
            return false;
    }
}

inline bool decodePayload(
    network::MessageType type,
    sf::Packet& packet,
    int playerNumber,
    std::uint32_t sequence,
    conquest_data::BattleAction& action,
    std::string& error)
{
    action = {};
    if (playerNumber != 1 && playerNumber != 2)
    {
        error = "Invalid tactical player";
        return false;
    }
    if (!supported(type))
    {
        error = "Unsupported tactical action";
        return false;
    }

    action.sequence = sequence;
    action.playerNumber = playerNumber;
    action.actionType = static_cast<std::uint8_t>(type);

    switch (type)
    {
        case network::MessageType::PlaceHero:
        case network::MessageType::PlayCard:
        case network::MessageType::MovePiece:
        case network::MessageType::AttackPiece:
            packet >> action.argumentOne >> action.argumentTwo >> action.argumentThree;
            break;
        case network::MessageType::UseAbility:
        case network::MessageType::DiscardCard:
            packet >> action.argumentOne;
            break;
        case network::MessageType::EndTurn:
            break;
        default:
            // The supported() check above makes this unreachable, but retaining
            // a closed default keeps future MessageType additions fail-closed.
            error = "Unsupported tactical action";
            return false;
    }

    if (!packet)
    {
        error = "Malformed tactical action";
        return false;
    }
    error.clear();
    return true;
}

inline bool decode(
    sf::Packet& packet,
    int playerNumber,
    std::uint32_t sequence,
    conquest_data::BattleAction& action,
    std::string& error)
{
    std::uint8_t rawType = 0;
    packet >> rawType;
    if (!packet)
    {
        error = "Missing tactical action type";
        return false;
    }
    return decodePayload(
        static_cast<network::MessageType>(rawType),
        packet,
        playerNumber,
        sequence,
        action,
        error);
}

inline bool apply(
    GameEngine& engine,
    const conquest_data::BattleAction& action,
    std::string* error = nullptr)
{
    if (action.playerNumber != 1 && action.playerNumber != 2)
    {
        if (error != nullptr)
        {
            *error = "Invalid tactical player";
        }
        return false;
    }

    const auto type = static_cast<network::MessageType>(action.actionType);
    bool accepted = false;
    switch (type)
    {
        case network::MessageType::PlaceHero:
            accepted = engine.placeHero(
                action.playerNumber,
                action.argumentOne,
                action.argumentTwo,
                action.argumentThree);
            break;
        case network::MessageType::PlayCard:
            accepted = engine.playCard(
                action.playerNumber,
                action.argumentOne,
                action.argumentTwo,
                action.argumentThree);
            break;
        case network::MessageType::MovePiece:
            accepted = engine.movePiece(
                action.playerNumber,
                action.argumentOne,
                action.argumentTwo,
                action.argumentThree);
            break;
        case network::MessageType::AttackPiece:
            accepted = engine.attackPiece(
                action.playerNumber,
                action.argumentOne,
                action.argumentTwo,
                action.argumentThree);
            break;
        case network::MessageType::UseAbility:
            accepted = engine.useAbility(action.playerNumber, action.argumentOne);
            break;
        case network::MessageType::DiscardCard:
            accepted = engine.discardCard(action.playerNumber, action.argumentOne);
            break;
        case network::MessageType::EndTurn:
            accepted = engine.endTurn(action.playerNumber);
            break;
        default:
            if (error != nullptr)
            {
                *error = "Unsupported tactical action";
            }
            return false;
    }

    if (!accepted)
    {
        if (error != nullptr)
        {
            *error = "Tactical action was not accepted in the current state";
        }
        return false;
    }

    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// A pathological battle must not pin its strategic event forever by filling
// the durable action log. At the cap, compare deterministic board state and
// forfeit the trailing player; the frozen battle seed breaks an exact tie.
// Calling this after replay makes a crash between the final action append and
// result persistence reconstruct the same winner.
inline bool adjudicateConquestActionLimit(
    GameEngine& engine,
    std::size_t actionCount,
    std::uint32_t battleSeed)
{
    if (actionCount < conquest_data::MaxConquestBattleActions ||
        engine.phase() == game_data::Phase::GameOver)
    {
        return false;
    }

    const game_data::Snapshot snapshot = engine.snapshotFor(1);
    int healthOne = 0;
    int healthTwo = 0;
    for (const game_data::Piece& piece : engine.boardPieces())
    {
        if (piece.owner == 1)
        {
            healthOne += std::max(0, piece.health);
        }
        else if (piece.owner == 2)
        {
            healthTwo += std::max(0, piece.health);
        }
    }

    const auto scoreOne = std::tuple{
        snapshot.players[0].heroesAlive,
        snapshot.players[0].controlledSquares,
        healthOne};
    const auto scoreTwo = std::tuple{
        snapshot.players[1].heroesAlive,
        snapshot.players[1].controlledSquares,
        healthTwo};
    int loser = 0;
    if (scoreOne < scoreTwo)
    {
        loser = 1;
    }
    else if (scoreTwo < scoreOne)
    {
        loser = 2;
    }
    else
    {
        loser = 1 + static_cast<int>(battleSeed & 1U);
    }
    engine.resign(loser);
    return true;
}

} // namespace game_action
