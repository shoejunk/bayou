#pragma once

#include "../gameserver/game_engine.hpp"

template <typename Check>
void testTimedTransformation(Check check)
{
    using namespace game_data;
    card_data::Card definition;
    definition.title = "Chrysalis";
    definition.type = "Unit";
    check(toGameCard(definition).transformTurns == 0 &&
          toGameCard(definition).transformInto.empty(), "old cards have no transformation");
    definition.integerValues = {{"transformTurns", 2}};
    definition.stringValues = {{"transformInto", "Mature Unit"}};
    GameCard young = toGameCard(definition);
    young.cost = 0;
    young.health = 2;
    check(young.transformTurns == 2 && young.transformInto == "Mature Unit",
          "integer and string fields configure timed transformation");
    definition.integerValues[0].value = -2;
    check(toGameCard(definition).transformTurns == 0, "negative transformation times disable the timer");

    GameCard mature;
    mature.title = "Mature Unit";
    mature.type = "Unit";
    mature.health = 7;
    mature.power = 3;
    mature.tokenPath = "mature.png";
    ActionProfile movement;
    movement.canMove = true;
    movement.maxRange = 1;
    mature.actions = {movement};
    const auto lookup = [&](const std::string& title) -> const GameCard* {
        return title == mature.title ? &mature : nullptr;
    };
    Piece source;
    populatePieceFromCard(source, young, false);
    source.id = 9;
    source.owner = 1;
    source.row = source.column = 3;
    source.hasActed = true;
    std::vector<Piece> board{source};
    advanceTimedTransformations(board, 2, lookup);
    check(board[0].transformTurnsRemaining == 2, "opponent turn does not tick the deployment timer");
    beginPieceTurn(board[0]);
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == young.title && board[0].transformTurnsRemaining == 1,
          "first controlling-player turn counts exactly once");
    board[0].health = 1;
    board[0].hasActed = true;
    board[0].disabledTurns = 1;
    board[0].sleepTurnsRemaining = 1;
    board[0].infestationTitle = "Parasite";
    board[0].infestationOwner = 2;
    board[0].raisedFromGraveyard = true;
    beginPieceTurn(board[0]);
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].id == 9 && board[0].row == 3 && board[0].column == 3 &&
          board[0].name == mature.title && board[0].health == 7 && board[0].power == 3 &&
          board[0].tokenPath == "mature.png" && board[0].transformTurnsRemaining == -1,
          "expiry applies the new form once with full printed health and stable identity");
    check(board[0].hasActed && board[0].sleepTurnsRemaining == 1 &&
          board[0].infestationTitle == "Parasite" && board[0].raisedFromGraveyard,
          "acting and damage do not reset the timer or erase disable, sleep, infestation or exile status");

    board = {source};
    board[0].owner = 2;
    board[0].originalOwner = 1;
    board[0].controlTurnsRemaining = 2;
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].transformTurnsRemaining == 2, "original owner's turn does not tick a stolen unit");
    advanceTimedTransformations(board, 2, lookup);
    advanceTimedTransformations(board, 2, lookup);
    check(board[0].name == mature.title && board[0].owner == 2 && board[0].originalOwner == 1 &&
          board[0].controlTurnsRemaining == 2, "transformation retains temporary control and original ownership");

    mature.width = 2;
    board = {source, source};
    board[0].transformTurnsRemaining = 1;
    board[1].id = 10;
    board[1].owner = 2;
    board[1].column = 4;
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == young.title && board[0].transformTurnsRemaining == 0 && board.size() == 2,
          "blocked growth waits at zero without overlapping or destroying a neighbor");
    board.pop_back();
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == mature.title && board[0].width == 2, "blocked transformation retries when space is free");
    board = {source};
    board[0].column = 7;
    board[0].transformTurnsRemaining = 1;
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == young.title && board[0].column == 7,
          "larger transformation cannot extend outside the board");
    mature.width = 1;
    mature.type = "Hero";
    board = {source};
    board[0].transformTurnsRemaining = 1;
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == young.title && !board[0].isHero, "timer targets must be Units");
    mature.type = "Unit";
    board[0].transformInto = "Missing Unit";
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == young.title && board[0].health == 2, "missing target leaves the original piece intact");
    mature.transformTurns = 1;
    mature.transformInto = mature.title;
    board = {source};
    board[0].transformTurnsRemaining = 1;
    advanceTimedTransformations(board, 1, lookup);
    check(board[0].name == mature.title && board[0].transformTurnsRemaining == 1,
          "a chained or self transformation starts its own timer without ticking twice");
    mature.transformTurns = 0;
    mature.transformInto.clear();

    GameCard hero;
    hero.title = "Hero";
    hero.type = "Hero";
    hero.health = 20;
    GameCard enchantment;
    enchantment.title = "Attached Damage";
    enchantment.type = "Enchantment";
    enchantment.cost = 0;
    enchantment.target = "piece";
    enchantment.effect = "damage";
    enchantment.power = 2;
    GameEngine engine(803, {});
    engine.registerScenarioCard(mature);
    check(engine.loadScenario({{1, hero, 3, 3, true}, {2, hero, 7, 7, true}},
          {young, enchantment}, {}, 0, 0, 1, "Timer test", false),
          "timer scenario loads through ordinary engine setup");
    check(engine.playCard(1, 0, 3, 4), "timed unit deploys through ordinary card play");
    const int deployedId = engine.boardPieces().back().id;
    check(engine.boardPieces().back().transformTurnsRemaining == 2 &&
          engine.playCard(1, 0, 3, 4), "deployment initializes the full timer and accepts a piece enchantment");
    engine.endTurn(1);
    check(engine.boardPieces().back().transformTurnsRemaining == 2, "deployment turn end does not tick the timer");
    engine.endTurn(2);
    check(engine.boardPieces().back().transformTurnsRemaining == 1, "engine starts the first owner turn at one remaining");
    engine.endTurn(1);
    engine.endTurn(2);
    const auto transformed = engine.snapshotFor(1);
    check(transformed.pieces.back().name == mature.title &&
          transformed.pieces.back().id == deployedId && !transformed.pieces.back().hasActed &&
          transformed.graveyard.empty() && transformed.enchantments.size() == 1 &&
          transformed.enchantments[0].targetPieceId == deployedId,
          "engine transforms at the second owner-turn start, retaining attachments without a death event");

    Snapshot snapshot;
    snapshot.pieces = {source};
    snapshot.hand = snapshot.foresightChoices = snapshot.graveyard =
        snapshot.exiled = snapshot.raiseUndeadChoices = {young};
    sf::Packet packet;
    writeSnapshot(packet, snapshot);
    Snapshot received;
    check(readSnapshot(packet, received) && packet.endOfPacket() &&
          received.pieces[0].transformTurnsRemaining == 2 && received.pieces[0].transformInto == mature.title &&
          received.hand[0].transformTurns == 2 && received.foresightChoices[0].transformTurns == 2 &&
          received.graveyard[0].transformTurns == 2 && received.exiled[0].transformTurns == 2 &&
          received.raiseUndeadChoices[0].transformInto == mature.title,
          "snapshot round trip retains timers and destinations in all card zones");
    sf::Packet old;
    writeSnapshot(old, snapshot, true, true, false);
    check(readSnapshot(old, received) && old.endOfPacket() &&
          received.pieces[0].transformTurnsRemaining == -1 && received.hand[0].transformTurns == 0 &&
          received.hand[0].transformInto.empty(), "older snapshots reset reused objects to disabled timers");
    sf::Packet truncated;
    truncated.append(packet.getData(), packet.getDataSize() - 1);
    check(!readSnapshot(truncated, received), "truncated transformation metadata is rejected");
    sf::Packet standaloneCard;
    writeGameCard(standaloneCard, young);
    GameCard receivedCard;
    sf::Packet standalonePiece;
    writePiece(standalonePiece, source);
    Piece receivedPiece;
    check(readGameCard(standaloneCard, receivedCard) && standaloneCard.endOfPacket() &&
          readPiece(standalonePiece, receivedPiece) && standalonePiece.endOfPacket() &&
          receivedCard.transformTurns == 2 && receivedPiece.transformInto == mature.title,
          "standalone cards and pieces retain transformation fields");
    bool shriek = false, transforms = false;
    sf::Packet join;
    join << ShriekSnapshotCapability << TransformSnapshotCapability;
    check(readShriekSnapshotCapability(join, shriek, &transforms) && shriek && transforms,
          "updated clients negotiate transformation snapshots");
    sf::Packet oldJoin;
    oldJoin << ShriekSnapshotCapability;
    check(readShriekSnapshotCapability(oldJoin, shriek, &transforms) && shriek && !transforms,
          "old clients negotiate their existing snapshot format");
    sf::Packet deck;
    deck << std::uint32_t{1};
    card_data::writeCard(deck, definition);
    deck << ShriekSnapshotCapability << TransformSnapshotCapability;
    std::uint32_t count = 0;
    card_data::Card submitted;
    deck >> count;
    check(count == 1 && card_data::readCard(deck, submitted) && submitted.title == definition.title &&
          readShriekSnapshotCapability(deck, shriek, &transforms) && shriek && transforms,
          "deck capability trailer follows the unchanged legacy card payload");
}
