#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testAuras(Check check)
{
    using namespace game_data;
    Piece source;
    source.id = 1; source.owner = 2; source.row = 3; source.column = 3;
    source.health = source.maxHealth = 10; source.keywords = {"Rot Aura"};
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

    GameCard caster; caster.title = "Aura fixture"; caster.health = 10;
    ActionProfile move; move.maxRange = 7; move.canMove = true; move.canAttack = true; move.damage = 1;
    caster.actions = {move};
    GameCard button = caster; button.title = "Button fixture"; button.ability = "transform";
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

    // The retained compatibility trailer transports Rot Aura view facts and
    // generic Silence configuration, but cannot recreate the removed mechanic.
    board = {source, target}; board[0].hidden = true;
    refreshAuraIndicators(board);
    GameCard configured = caster; configured.silenceTurns = 3;
    Snapshot snapshot; snapshot.pieces = {board[1]}; snapshot.hand = {configured};
    snapshot.foresightChoices = {configured}; snapshot.graveyard = {configured};
    snapshot.exiled = {configured}; snapshot.raiseUndeadChoices = {configured}; snapshot.status = "Aura wire test";
    sf::Packet packet; writeSnapshot(packet, snapshot); Snapshot received;
    check(readSnapshot(packet, received) && packet.endOfPacket() && received.pieces[0].healingBlocked &&
          !received.pieces[0].abilitySilenced && received.hand[0].silenceTurns == 3 &&
          received.foresightChoices[0].silenceTurns == 3 && received.graveyard[0].silenceTurns == 3 &&
          received.exiled[0].silenceTurns == 3 && received.raiseUndeadChoices[0].silenceTurns == 3,
          "compatibility trailer preserves healing facts and Silence duration across all card zones");
    sf::Packet oldReaderPacket; writeSnapshot(oldReaderPacket, snapshot); Snapshot legacy;
    check(readSnapshot(oldReaderPacket, legacy, false) && legacy.status == snapshot.status &&
          legacy.hand[0].title == configured.title && !oldReaderPacket.endOfPacket(), "legacy snapshot layout still decodes");
    sf::Packet oldWriterPacket; writeSnapshot(oldWriterPacket, snapshot, false);
    check(readSnapshot(oldWriterPacket, received) && oldWriterPacket.endOfPacket() &&
          received.hand[0].silenceTurns == 1 && !received.pieces[0].healingBlocked,
          "new snapshot reader accepts legacy payload and resets omitted fields");
    sf::Packet truncated; writeSnapshot(truncated, snapshot); sf::Packet broken;
    broken.append(truncated.getData(), truncated.getDataSize()-1);
    check(!readSnapshot(broken, received), "truncated optional metadata is rejected");
    sf::Packet reserved; reserved << std::int32_t{7} << std::int32_t{3} << std::int32_t{9}
                                 << std::int32_t{2} << false << true;
    Piece decoded;
    check(readPieceAuraFields(reserved, decoded) && reserved.endOfPacket() && decoded.silenceTurns == 3 &&
          decoded.healingBlocked && !decoded.abilitySilenced,
          "obsolete wire slots are consumed without restoring removed runtime state");
    sf::Packet emitted; writePieceAuraFields(emitted, decoded);
    std::int32_t range=0, turns=0, active=9, player=9; bool suppressed=false, blocked=false;
    emitted >> range >> turns >> active >> player >> suppressed >> blocked;
    check(emitted.endOfPacket() && range == 1 && turns == 3 && active == 0 && player == 0 && blocked,
          "obsolete outgoing wire slots stay inert for compatible older readers");
    Piece obsolete = target; obsolete.ability = "silence field";
    check(!pieceAbilityAvailable(obsolete), "removed ability name is no longer executable");
}
