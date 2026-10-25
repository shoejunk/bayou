#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testAuras(Check check)
{
    using namespace game_data;
    card_data::Card definition;
    definition.title = "Aura fixture";
    definition.stringValues = {{"ability", " Silence Field "}};
    definition.keywords = {"Rot Aura"};
    GameCard card = toGameCard(definition);
    check(card.silenceRange == 1 && card.silenceTurns == 1 && card.ability == "silence field",
          "Silence Field defaults both integer fields to one and normalizes the ability name");
    definition.integerValues = {{SilenceRangeField, 3}, {SilenceTurnsField, 4}};
    card = toGameCard(definition);
    check(card.silenceRange == 3 && card.silenceTurns == 4,
          "Silence Field loads custom integer range and duration");
    definition.integerValues = {{SilenceRangeField, -5}, {SilenceTurnsField, 0}};
    check(toGameCard(definition).silenceRange == 1 && toGameCard(definition).silenceTurns == 1,
          "nonpositive Silence Field parameters normalize to one");
    definition.integerValues = {{SilenceRangeField, 100}, {SilenceTurnsField, 2147483647}};
    check(toGameCard(definition).silenceRange == 7 && toGameCard(definition).silenceTurns == 2147483647,
          "range caps at the board diameter and positive durations cannot overflow on activation");

    Piece source;
    source.id = 1; source.owner = 2; source.row = 3; source.column = 3;
    source.health = source.maxHealth = 10; source.keywords = {"Rot Aura"};
    source.ability = "silence field";
    Piece target = source;
    target.id = 2; target.owner = 1; target.row = 2; target.column = 2;
    target.keywords.clear(); target.ability = "transform"; target.sacrifice = "dig";
    target.abilityUses = 1; target.health = 2;
    std::vector<Piece> board{source, target};
    applyActionHealing(board, board[1], 5, 0);
    check(board[1].health == 2, "Rot Aura blocks diagonal enemy action healing");
    board[1].owner = 2;
    applyActionHealing(board, board[1], 1, 0);
    applyActionHealing(board, board[0], 5, 0);
    check(board[1].health == 3 && board[0].health == 10, "Rot Aura exempts allies and its bearer");
    board[1].owner = 1; board[0].health = 0;
    applyActionHealing(board, board[1], 1, 0);
    check(board[1].health == 4, "dead Rot Aura bearers stop blocking healing before removal");
    board[0].health = 10; board[0].disabledTurns = 3; board[0].hidden = true;
    board[0].abilitySilenced = true;
    check(pieceHealingBlocked(board, board[1]), "Rot Aura remains passive while hidden, Disabled or Silenced");
    board[0].column = 7;
    applyActionHealing(board, board[1], 1, 0);
    check(board[1].health == 5, "leaving Rot Aura immediately restores healing");
    board[0].column = 3; board[0].width = 2; board[0].height = 2;
    board[1].row = 5; board[1].column = 5;
    check(pieceHealingBlocked(board, board[1]), "Rot Aura uses full-footprint diagonal adjacency");
    board[0] = source; board[1] = target;
    board[0].healingAura = 3;
    applyHealingAuras(board, 2);
    check(board[1].health == 2, "Rot Aura blocks end-turn healing auras, including enemy aura healing");
    applyActionLifedrain(board, 2, 5);
    check(board[1].health == 2, "Rot Aura blocks Lifedrain, including its push-collision path");
    board[0].health = 0;
    applyActionLifedrain(board, 2, 5);
    check(board[1].health == 7, "Lifedrain heals immediately after the Rot Aura bearer is defeated");
    board[1].health = 0;
    applyActionHealing(board, board[1], 5, 0);
    check(board[1].health == 0, "healing never revives a defeated piece");

    board = {source, target};
    activateSilenceField(board[0], 2);
    check(pieceIsSilenced(board, board[1]) && !pieceIsSilenced(board, board[0]),
          "default Silence Field includes diagonal enemies but excludes its own bearer");
    board[1].owner = 2;
    check(!pieceAbilityAvailable(board, board[1]) && !pieceAbilityAvailable(board, board[1], true),
          "Silence Field blocks allied active and Sacrifice buttons equally");
    board[1].column = 0;
    check(pieceAbilityAvailable(board, board[1]), "leaving the dynamic field restores button availability");
    board[0].column = 1;
    check(!pieceAbilityAvailable(board, board[1]), "moving the caster moves its field");
    board[0].health = 0;
    check(pieceAbilityAvailable(board, board[1]), "a defeated field caster immediately stops silencing");
    board[0] = source; board[1] = target; board[0].silenceRange = 2; board[0].silenceTurns = 2;
    activateSilenceField(board[0], 2);
    board[1].row = 1; board[1].column = 1;
    check(pieceIsSilenced(board, board[1]), "custom range two includes the diagonal boundary");
    board[1].column = 0;
    check(!pieceIsSilenced(board, board[1]), "custom range excludes squares beyond its boundary");
    board[0].width = board[0].height = 2; board[1].row = 6; board[1].column = 6;
    check(pieceIsSilenced(board, board[1]), "custom Silence Field range measures full footprints");
    updateSilenceFieldsAtTurnStart(board, 1);
    check(board[0].silenceTurnsRemaining == 2, "opponent turn starts do not shorten the field");
    board[0].owner = 1;
    updateSilenceFieldsAtTurnStart(board, 2);
    check(board[0].silenceTurnsRemaining == 1, "expiry retains the activating player's clock after control changes");
    updateSilenceFieldsAtTurnStart(board, 2);
    check(board[0].silenceTurnsRemaining == 0 && board[0].silenceExpiryPlayer == 0 &&
          !pieceIsSilenced(board, board[1]), "custom duration expires on the specified subsequent turn start");

    GameCard caster;
    caster.title = "Silence fixture"; caster.health = 10; caster.ability = "silence field";
    ActionProfile move;
    move.maxRange = 7; move.canMove = true; move.canAttack = true; move.damage = 1;
    caster.actions = {move};
    GameCard button = caster;
    button.title = "Button fixture"; button.ability = "transform";
    GameEngine engine(991, {});
    check(engine.loadScenario({{1, caster, 3, 3, false}, {2, button, 3, 4, false},
                               {1, button, 2, 3, false}}, {}, {}, 0, 0, 1, "Aura test", false),
          "Silence Field authoritative fixture loads");
    const int casterId = engine.boardPieces()[0].id;
    const int enemyId = engine.boardPieces()[1].id;
    check(engine.useAbility(1, casterId) && engine.boardPieces()[0].silenceTurnsRemaining == 1,
          "existing active ability request starts the default field without a target");
    check(!pieceAbilityAvailable(engine.snapshotFor(1), engine.snapshotFor(1).pieces[2]),
          "authoritative snapshots disable allied ability buttons");
    check(engine.endTurn(1) && !engine.useAbility(2, enemyId),
          "authoritative engine rejects an enemy ability in the field");
    check(engine.movePiece(2, enemyId, 3, 6) &&
          pieceAbilityAvailable(engine.boardPieces(), engine.boardPieces()[1]),
          "a silenced unit can drag out of range and escapes immediately");
    check(engine.endTurn(2) && engine.boardPieces()[0].silenceTurnsRemaining == 0,
          "default field expires at activating player's next turn start");

    for (int owner : {1, 2})
    {
        GameEngine attackEngine(992, {});
        attackEngine.loadScenario({{owner, caster, 3, 3, false}, {3-owner, button, 3, 4, false}},
                                  {}, {}, 0, 0, owner, "Silenced attack", false);
        check(attackEngine.useAbility(owner, attackEngine.boardPieces()[0].id) &&
              attackEngine.endTurn(owner) &&
              attackEngine.attackPiece(3-owner, attackEngine.boardPieces()[1].id, 3, 3),
              "Silence Field preserves normal drag attacks for either player");
    }
    GameCard longCaster = caster; longCaster.silenceTurns = 2; longCaster.silenceRange = 2;
    GameEngine longEngine(993, {});
    longEngine.loadScenario({{1, longCaster, 3, 3, false}, {2, button, 1, 1, false}},
                            {}, {}, 0, 0, 1, "Long field", false);
    check(longEngine.useAbility(1, longEngine.boardPieces()[0].id) && longEngine.endTurn(1) &&
          !longEngine.useAbility(2, longEngine.boardPieces()[1].id), "custom range blocks authoritative enemy button");
    check(longEngine.endTurn(2) && longEngine.boardPieces()[0].silenceTurnsRemaining == 1 &&
          longEngine.endTurn(1) && !longEngine.useAbility(2, longEngine.boardPieces()[1].id) &&
          longEngine.endTurn(2) && longEngine.boardPieces()[0].silenceTurnsRemaining == 0 &&
          longEngine.endTurn(1) && longEngine.useAbility(2, longEngine.boardPieces()[1].id),
          "duration two spans two enemy turns, then restores the enemy button");

    GameCard rot = caster; rot.title = "Rot fixture"; rot.ability.clear(); rot.keywords = {"rot aura"};
    GameCard healer = button; healer.title = "Healer fixture";
    ActionProfile mend; mend.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    mend.maxRange = 7; mend.canMove = false; mend.canAttack = true; mend.heal = 5;
    healer.actions = {mend};
    GameCard spell; spell.title = "Heal fixture"; spell.type = "Spell"; spell.effect = "heal";
    spell.power = 5; spell.cost = 0;
    GameEngine rotEngine(994, {});
    rotEngine.loadScenario({{2, rot, 3, 3, false}, {1, button, 2, 2, false, 2},
                            {1, healer, 2, 1, false}}, {spell}, {}, 0, 0, 1, "Rot healing", false);
    check(rotEngine.playCard(1, 0, 2, 2) && rotEngine.boardPieces()[1].health == 2,
          "authoritative heal spell cannot bypass Rot Aura");
    check(rotEngine.attackPiece(1, rotEngine.boardPieces()[2].id, 2, 2) &&
          rotEngine.boardPieces()[1].health == 2, "authoritative healing action cannot bypass Rot Aura");
    GameCard drain = button; drain.title = "Drain fixture";
    drain.actions[0].kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    drain.actions[0].canMove = false; drain.actions[0].lifedrain = true; drain.actions[0].damage = 2;
    GameEngine drainEngine(995, {});
    drainEngine.loadScenario({{2, rot, 3, 3, false}, {1, drain, 2, 2, false, 2},
                              {2, button, 2, 4, false}}, {}, {}, 0, 0, 1, "Rot drain", false);
    check(drainEngine.attackPiece(1, drainEngine.boardPieces()[1].id, 2, 4) &&
          drainEngine.boardPieces()[1].health == 2 && drainEngine.boardPieces()[2].health == 8,
          "Rot Aura prevents Lifedrain without preventing attack damage");

    // Moving attacks heal at the strike square, before movement is committed.
    drain.actions[0] = move;
    drain.actions[0].lifedrain = true;
    drain.actions[0].damage = 2;
    GameEngine leavingRot(996, {});
    leavingRot.loadScenario({{2, rot, 3, 3, false}, {1, drain, 2, 2, false, 2},
                             {2, button, 0, 4, false}}, {}, {}, 0, 0, 1, "Leave Rot", false);
    check(leavingRot.attackPiece(1, leavingRot.boardPieces()[1].id, 0, 4) &&
          leavingRot.boardPieces()[1].health == 4 && leavingRot.boardPieces()[1].row == 1 &&
          leavingRot.boardPieces()[1].column == 3,
          "moving out of Rot Aura restores Lifedrain at the strike square");
    GameEngine enteringRot(997, {});
    enteringRot.loadScenario({{2, rot, 1, 2, false}, {1, drain, 0, 0, false, 2},
                              {2, button, 2, 2, false}}, {}, {}, 0, 0, 1, "Enter Rot", false);
    check(enteringRot.attackPiece(1, enteringRot.boardPieces()[1].id, 2, 2) &&
          enteringRot.boardPieces()[1].health == 2 && enteringRot.boardPieces()[1].row == 1 &&
          enteringRot.boardPieces()[1].column == 1,
          "moving into Rot Aura blocks Lifedrain at the strike square");

    // Concealed sources supply authoritative view facts without leaking positions.
    board = {source, target}; board[0].hidden = true;
    activateSilenceField(board[0], 2);
    refreshAuraIndicators(board);
    Snapshot snapshot; snapshot.pieces = {board[1]}; snapshot.hand = {longCaster};
    snapshot.foresightChoices = {longCaster}; snapshot.graveyard = {longCaster};
    snapshot.exiled = {longCaster}; snapshot.raiseUndeadChoices = {longCaster}; snapshot.status = "Aura wire test";
    check(!pieceAbilityAvailable(snapshot, snapshot.pieces[0]), "view-only silence facts cover concealed sources");
    sf::Packet packet; writeSnapshot(packet, snapshot);
    Snapshot received;
    check(readSnapshot(packet, received) && packet.endOfPacket() && received.pieces.size() == 1 &&
          received.pieces[0].abilitySilenced && received.pieces[0].healingBlocked &&
          received.hand[0].silenceRange == 2 && received.foresightChoices[0].silenceTurns == 2 &&
          received.graveyard[0].silenceRange == 2 && received.exiled[0].silenceTurns == 2 &&
          received.raiseUndeadChoices[0].silenceTurns == 2,
          "snapshot extension preserves aura facts and all card-zone parameters without revealing hidden sources");
    sf::Packet oldReaderPacket; writeSnapshot(oldReaderPacket, snapshot);
    Snapshot legacy;
    check(readSnapshot(oldReaderPacket, legacy, false) && legacy.status == snapshot.status &&
          legacy.hand[0].title == longCaster.title && !oldReaderPacket.endOfPacket(),
          "legacy snapshot layout still decodes and leaves the optional trailer unread");
    sf::Packet oldWriterPacket; writeSnapshot(oldWriterPacket, snapshot, false);
    check(readSnapshot(oldWriterPacket, received) && oldWriterPacket.endOfPacket() &&
          received.hand[0].silenceRange == 1 && !received.pieces[0].abilitySilenced,
          "new snapshot reader accepts legacy payloads and resets omitted aura fields");
    sf::Packet truncated; writeSnapshot(truncated, snapshot);
    sf::Packet broken; broken.append(truncated.getData(), truncated.getDataSize()-1);
    check(!readSnapshot(broken, received), "truncated aura metadata is rejected");
    sf::Packet cardPacket; writeGameCard(cardPacket, longCaster);
    GameCard receivedCard;
    check(readGameCard(cardPacket, receivedCard) && cardPacket.endOfPacket() &&
          receivedCard.silenceRange == 2 && receivedCard.silenceTurns == 2,
          "standalone GameCard serialization retains custom aura parameters");
    sf::Packet piecePacket; writePiece(piecePacket, board[0]);
    Piece receivedPiece;
    check(readPiece(piecePacket, receivedPiece) && piecePacket.endOfPacket() &&
          receivedPiece.silenceTurnsRemaining == 1 && receivedPiece.silenceExpiryPlayer == 2,
          "standalone Piece serialization retains active field state");
    Snapshot sandbox; int nextId = 1;
    bayou::client::spawnSandboxPiece(sandbox, nextId, 1, longCaster, 3, 3, false);
    check(sandbox.pieces[0].silenceRange == 2 && sandbox.pieces[0].silenceTurns == 2 &&
          sandbox.pieces[0].silenceTurnsRemaining == 0, "sandbox spawn carries configuration but never inherits an active field");
    board = {source, target}; board[0].silenceTurnsRemaining = 1; board[0].silenceExpiryPlayer = 2;
    board[1].healingAura = 1; board[1].keywords = {"rot aura"};
    check(pieceHealingBlocked(board, board[0]), "a silenced unit's Rot Aura passive remains effective");
    board[0].keywords.clear(); board[1].keywords.clear(); board[0].health = 5;
    applyHealingAuras(board, 1);
    check(board[0].health == 6, "a silenced unit's healing aura passive remains effective");
}
