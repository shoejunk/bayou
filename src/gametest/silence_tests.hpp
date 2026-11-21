#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testSilence(Check check)
{
    using namespace game_data;
    card_data::Card definition;
    definition.title = "Silence test caster"; definition.type = "Unit";
    definition.stringValues = {{"ability", " Silence "}};
    GameCard card = toGameCard(definition);
    check(card.ability == "silence" && card.silenceTurns == 1,
          "Silence normalizes its ability name and defaults to one enemy turn");
    definition.integerValues = {{SilenceTurnsField, 4}};
    definition.stringLists = {{"abilityLabels", {"Dissonant Chorus"}}};
    card = toGameCard(definition); card.health = 10;
    check(card.silenceTurns == 4, "Silence loads a configurable four-enemy-turn duration");
    definition.integerValues = {{SilenceTurnsField, 0}};
    check(toGameCard(definition).silenceTurns == 1, "Silence clamps zero duration to one");
    definition.integerValues = {{SilenceTurnsField, -4}};
    check(toGameCard(definition).silenceTurns == 1, "Silence clamps negative duration to one");
    Piece source; populatePieceFromCard(source, card, false);
    source.id = 1; source.owner = 1; source.row = source.column = 0;
    Piece enemy = source; enemy.id = 2; enemy.owner = 2; enemy.row = enemy.column = 7;
    enemy.ability = "transform"; enemy.sacrifice = "dig"; enemy.abilityUses = 1;
    enemy.passives = {"Chrysalis", "Rot Aura"};
    Piece ally = enemy; ally.id = 3; ally.owner = 1; ally.row = 1;
    Piece dead = enemy; dead.id = 4; dead.health = 0;
    std::vector<Piece> board{source, enemy, ally, dead};
    check(pieceAbilityAvailable(board, board[0]) && pieceAbilityLabel(board[0]) == "Dissonant Chorus",
          "Silence uses the ordinary ability button and optional card-specific label");
    applySilence(board, board[0]);
    check(board[1].silencedTurnsRemaining == 4 && board[1].silencedExpiryPlayer == 2 &&
          board[0].silencedTurnsRemaining == 0 && board[2].silencedTurnsRemaining == 0 &&
          board[3].silencedTurnsRemaining == 0,
          "Silence attaches a negative modifier to all living enemies regardless of distance, not allies or dead units");
    check(pieceActivatedAbilitiesSilenced(board, board[1]) &&
          !pieceAbilityAvailable(board, board[1]) && !pieceAbilityAvailable(board, board[1], true),
          "negative Silence blocks activated and Sacrifice buttons without affecting passive keywords");
    check(board[1].passives == enemy.passives && board[1].disabledTurns == 0,
          "Silence does not remove passive keywords or apply movement-blocking Disable");
    Piece newcomer = enemy; newcomer.id = 5;
    board.push_back(newcomer);
    check(!pieceActivatedAbilitiesSilenced(board, board.back()), "later enemy entrants do not inherit snapshot Silence");
    board.erase(board.begin());
    check(board[0].silencedTurnsRemaining == 4 && !pieceAbilityAvailable(board, board[0]),
          "Silence survives removal of its caster");
    updateSilenceAtTurnEnd(board, 1);
    check(board[0].silencedTurnsRemaining == 4, "caster turns do not decrement enemy Silence");
    board[0].owner = 1;
    for (int turn = 1; turn <= 4; ++turn)
    {
        updateSilenceAtTurnEnd(board, 2);
        check(board[0].silencedTurnsRemaining == 4-turn,
              "Silence decrements once per originally affected enemy turn end despite control changes");
    }
    check(board[0].silencedExpiryPlayer == 0 && pieceAbilityAvailable(board, board[0]),
          "Silence restores the button after exactly four affected turns");
    board = {source, enemy}; applySilence(board, board[0]);
    board[0].silenceTurns = 1; applySilence(board, board[0]);
    check(board[1].silencedTurnsRemaining == 4, "shorter Silence refresh cannot shorten an existing modifier");
    board[0].silenceTurns = 6; applySilence(board, board[0]);
    check(board[1].silencedTurnsRemaining == 6, "longer Silence refresh uses maximum duration, not additive stacking");
    clearSilence(board[1]);
    check(board[1].silencedTurnsRemaining == 0 && board[1].silencedExpiryPlayer == 0 &&
          pieceAbilityAvailable(board, board[1]), "negative Silence can be cleared independently for future cleanse effects");
    ActionProfile move; move.maxRange = 7; move.canMove = true; move.canAttack = true; move.damage = 1;
    card.actions = {move};
    GameCard button = card; button.title = "Silence test enemy"; button.ability = "transform";
    for (int owner : {1, 2})
    {
        GameEngine engine(1601, {});
        check(engine.loadScenario({{owner, card, 0, 0, false}, {3-owner, button, 7, 7, false}},
                                  {}, {}, 0, 0, owner, "Silence", false), "Silence authoritative fixture loads");
        const int casterId = engine.boardPieces()[0].id, enemyId = engine.boardPieces()[1].id;
        check(!engine.useAbility(3-owner, casterId) && engine.useAbility(owner, casterId) &&
              !engine.useAbility(owner, casterId), "Silence requires ownership and consumes the normal piece action");
        check(engine.boardPieces()[1].silencedTurnsRemaining == 4,
              "authoritative Silence marks the remote enemy for four turns");
        for (int turn = 0; turn < 4; ++turn)
        {
            check(engine.endTurn(owner) && !engine.useAbility(3-owner, enemyId),
                  "Silence blocks the enemy ability throughout each affected turn");
            check(engine.endTurn(3-owner) && engine.boardPieces()[1].silencedTurnsRemaining == 3-turn,
                  "authoritative Silence expires on enemy turn ends only");
        }
        check(engine.endTurn(owner) && engine.useAbility(3-owner, enemyId),
              "authoritative enemy activated ability returns after four turns");
    }
    GameEngine movement(1602, {});
    movement.loadScenario({{1, card, 0, 0, false}, {2, button, 7, 7, false}},
                          {}, {}, 0, 0, 1, "Silence movement", false);
    check(movement.useAbility(1, movement.boardPieces()[0].id) && movement.endTurn(1) &&
          movement.movePiece(2, movement.boardPieces()[1].id, 7, 6),
          "Silenced units retain drag movement");
    GameEngine attack(1603, {});
    attack.loadScenario({{1, card, 3, 3, false}, {2, button, 3, 4, false}},
                        {}, {}, 0, 0, 1, "Silence attack", false);
    check(attack.useAbility(1, attack.boardPieces()[0].id) && attack.endTurn(1) &&
          attack.attackPiece(2, attack.boardPieces()[1].id, 3, 3),
          "Silenced units retain drag attacks");

    GameCard grub = button; grub.title = "Silenced Chrysalis"; grub.passives = {"Chrysalis"};
    Snapshot sandbox; int nextId = 1;
    bayou::client::spawnSandboxPiece(sandbox, nextId, 1, card, 0, 0, false);
    bayou::client::spawnSandboxPiece(sandbox, nextId, 2, grub, 7, 7, false);
    applySilence(sandbox.pieces, sandbox.pieces[0]);
    const int victimId = sandbox.pieces[1].id;
    sandbox.pieces[1].health = 0;
    bayou::client::destroyPieceInSnapshot(sandbox, nextId, victimId, nullptr, nullptr, &grub);
    check(sandbox.husks.size() == 1 && sandbox.graveyard.empty(),
          "negative Silence leaves Chrysalis passive resurrection intact on death");

    Snapshot snapshot; board = {source, enemy}; applySilence(board, board[0]); snapshot.pieces = board;
    sf::Packet packet; writeSnapshot(packet, snapshot); Snapshot decoded;
    check(readSnapshot(packet, decoded) && decoded.pieces.size() == 2 &&
          decoded.pieces[1].silencedTurnsRemaining == 4 && decoded.pieces[1].silencedExpiryPlayer == 2,
          "snapshot serialization preserves negative Silence and its fixed enemy-turn clock");
    sf::Packet piecePacket; writePiece(piecePacket, board[1]); Piece decodedPiece;
    check(readPiece(piecePacket, decodedPiece) && piecePacket.endOfPacket() &&
          decodedPiece.silencedTurnsRemaining == 4 && decodedPiece.silencedExpiryPlayer == 2,
          "standalone piece serialization preserves negative Silence");
    sf::Packet legacy; writeSnapshot(legacy, snapshot, false); Snapshot oldDecoded;
    check(readSnapshot(legacy, oldDecoded) && oldDecoded.pieces[1].silencedTurnsRemaining == 0,
          "legacy snapshots default to no negative Silence");
    sf::Packet truncated; truncated.append(packet.getData(), packet.getDataSize()-1);
    check(!readSnapshot(truncated, decoded), "truncated negative Silence snapshot is rejected");
    bool slow = false, chrysalis = false, heal = false, silence = false;
    sf::Packet ready;
    ready << CommandRestrictionSnapshotCapability << SlowSnapshotCapability <<
        ChrysalisSnapshotCapability << AreaHealSnapshotCapability << SilenceSnapshotCapability;
    check(offersCommandRestrictionSnapshots(ready, &slow, &chrysalis, &heal, &silence) &&
          slow && chrysalis && heal && silence,
          "ready negotiation advertises SIL1 after HEA1");
    sf::Packet oldReady;
    oldReady << CommandRestrictionSnapshotCapability << SlowSnapshotCapability <<
        ChrysalisSnapshotCapability << AreaHealSnapshotCapability;
    check(offersCommandRestrictionSnapshots(oldReady, &slow, &chrysalis, &heal, &silence) &&
          slow && chrysalis && heal && !silence,
          "HEA1-only ready negotiation does not imply Silence status support");
    sf::Packet optIn;
    optIn << SlowSnapshotCapability << ChrysalisSnapshotCapability <<
        AreaHealSnapshotCapability << SilenceSnapshotCapability;
    check(readSlowSnapshotOptIn(optIn, slow, &chrysalis, &heal, &silence) &&
          slow && chrysalis && heal && silence,
          "client explicitly opts into SIL1 negative status snapshots");
    sf::Packet oldOptIn;
    oldOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability << AreaHealSnapshotCapability;
    check(readSlowSnapshotOptIn(oldOptIn, slow, &chrysalis, &heal, &silence) &&
          slow && chrysalis && heal && !silence,
          "HEA1-only client opt-in leaves Silence status disabled");
    sf::Packet emptyOptIn;
    check(readSlowSnapshotOptIn(emptyOptIn, slow, &chrysalis, &heal, &silence) &&
          !slow && !chrysalis && !heal && !silence,
          "empty legacy opt-in resets all extension negotiation flags");
    sf::Packet badOptIn;
    badOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability <<
        AreaHealSnapshotCapability << std::uint32_t{0};
    check(!readSlowSnapshotOptIn(badOptIn, slow, &chrysalis, &heal, &silence),
          "unknown Silence opt-in marker is rejected");
    sf::Packet trailingOptIn;
    trailingOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability <<
        AreaHealSnapshotCapability << SilenceSnapshotCapability << std::uint8_t{1};
    check(!readSlowSnapshotOptIn(trailingOptIn, slow, &chrysalis, &heal, &silence),
          "trailing junk after SIL1 opt-in is rejected");
    sf::Packet healOnly;
    writeSnapshot(healOnly, snapshot, true, true, true, true, true, true, true, false);
    Snapshot healDecoded;
    check(readSnapshot(healOnly, healDecoded) && healDecoded.pieces[1].silencedTurnsRemaining == 0 &&
          healDecoded.pieces[1].silencedExpiryPlayer == 0,
          "HEA1-only snapshots load with no negative Silence status");
    const auto validSilenceFields = [](int turns, int player) {
        sf::Packet fields; fields << turns << player; Piece target;
        return readSilenceFields(fields, target);
    };
    check(validSilenceFields(0, 0) && validSilenceFields(1, 1) && validSilenceFields(4, 2),
          "Silence field decoder accepts absent or positive status with a valid enemy clock");
    check(!validSilenceFields(-1, 2), "negative Silence duration is rejected");
    check(!validSilenceFields(1, -1) && !validSilenceFields(1, 3),
          "Silence expiry players outside the two-player domain are rejected");
    check(!validSilenceFields(1, 0), "active Silence without an expiry player is rejected");
    check(!validSilenceFields(0, 1) && !validSilenceFields(0, 2),
          "expired Silence cannot retain a stale expiry player");
    sf::Packet missingSilence;
    writeSnapshot(missingSilence, snapshot, true, true, true, true, true, true, true, false);
    missingSilence << SilenceSnapshotCapability;
    check(!readSnapshot(missingSilence, healDecoded), "SIL1 snapshot marker without status payload is rejected");
    Piece transforming = enemy;
    transforming.transformTurnsRemaining = 1; transforming.transformInto = "Silence transformed form";
    transforming.silencedTurnsRemaining = 4; transforming.silencedExpiryPlayer = 2;
    GameCard newForm = button; newForm.title = transforming.transformInto;
    newForm.type = "Unit";
    std::vector<Piece> transformingBoard{transforming};
    advanceTimedTransformations(transformingBoard, 2,
        [&](const std::string& name) -> std::optional<GameCard> {
            return name == newForm.title ? std::optional<GameCard>{newForm} : std::nullopt;
        });
    check(transformingBoard[0].name == newForm.title &&
          transformingBoard[0].silencedTurnsRemaining == 4 && transformingBoard[0].silencedExpiryPlayer == 2 &&
          !pieceAbilityAvailable(transformingBoard, transformingBoard[0]),
          "timed transformation preserves the negative Silence modifier and original enemy-turn clock");

}
