#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testShriek(Check check)
{
    using namespace game_data;
    card_data::Card definition;
    definition.title = "Shrieker";
    definition.type = "Unit";
    definition.stringValues = {{"ability", " Shriek "}};
    GameCard card = toGameCard(definition);
    check(card.ability == "shriek" && card.shriekRange == 1 && card.shriekDamage == 0,
          "Shriek normalizes its name and defaults range to one and damage to zero");
    definition.integerValues = {{ShriekRangeField, 3}, {ShriekDamageField, 2}};
    card = toGameCard(definition);
    check(card.shriekRange == 3 && card.shriekDamage == 2, "Shriek reads spaced integer field names");
    definition.integerValues = {{"shriekRange", 2}, {"shriekDamage", 4}};
    check(toGameCard(definition).shriekRange == 2 && toGameCard(definition).shriekDamage == 4,
          "Shriek accepts camel-case integer field aliases");
    definition.integerValues = {{ShriekRangeField, -2}, {ShriekDamageField, -4}};
    check(toGameCard(definition).shriekRange == 0 && toGameCard(definition).shriekDamage == 0,
          "negative Shriek parameters clamp to zero");
    definition.integerValues = {{ShriekRangeField, 0}};
    check(toGameCard(definition).shriekRange == 0 && toGameCard(definition).shriekDamage == 0,
          "explicit zero range is preserved and omitted damage remains zero");

    Piece source;
    populatePieceFromCard(source, card, false);
    source.id = 1; source.owner = 1; source.row = source.column = 3;
    source.health = source.maxHealth = 10;
    check(source.shriekRange == 3 && source.shriekDamage == 2 &&
          pieceAbilityAvailable(source) && pieceAbilityLabel(source) == "Shriek",
          "spawned Shriek pieces retain parameters and expose the ability button");
    Piece hidden = source;
    hidden.id = 2; hidden.owner = 2; hidden.row = hidden.column = 2;
    hidden.ability = "dematerialize";
    setPieceActionState(hidden, 2);
    Piece ally = hidden; ally.id = 3; ally.owner = 1; ally.row = ally.column = 4;
    Piece visible = hidden; visible.id = 4; visible.row = 3; visible.column = 4;
    setPieceActionState(visible, 0);
    Piece distant = hidden; distant.id = 5; distant.row = distant.column = 1;
    source.shriekRange = 1; source.shriekDamage = 0;
    std::vector<Piece> board{source, hidden, ally, visible, distant};
    std::mt19937 random(1301);
    check(pieceAbilityAvailable(std::vector<Piece>{source}, source),
          "Shriek remains usable without visible targets and does not leak hidden presence");
    const auto revealOnly = applyShriek(board, board[0], random);
    check(revealOnly.empty() && !board[1].hidden && !board[2].hidden && board[4].hidden &&
          board[1].actionState == 0 && board[2].actionState == 0 &&
          board[1].health == 10 && board[2].health == 10 && board[1].disabledTurns == 0,
          "default Shriek reveals diagonal friends and enemies in any hidden state without damage or stun");
    source.shriekDamage = 3; source.power = 9;
    board = {source, hidden, ally, visible, distant};
    applyShriek(board, board[0], random);
    check(board[1].health == 7 && board[2].health == 7 && board[3].health == 10 &&
          board[4].health == 10 && board[4].hidden,
          "positive Shriek damage affects revealed targets only and uses the configured damage");
    source.shriekRange = 2;
    board = {source, distant};
    applyShriek(board, board[0], random);
    check(!board[1].hidden && board[1].health == 7, "custom Shriek range includes its diagonal boundary");
    source.row = source.column = 4; source.width = source.height = 2; source.shriekRange = 1;
    hidden.row = hidden.column = 2; hidden.width = hidden.height = 2;
    board = {source, hidden};
    applyShriek(board, board[0], random);
    check(!board[1].hidden && board[1].health == 7,
          "Shriek measures nearest squares of both large footprints and damages each unit once");
    source.keywords = {"dematerialize"}; source.shriekRange = 0;
    setPieceActionState(source, 1);
    board = {source, hidden};
    applyShriek(board, board[0], random);
    check(!board[0].hidden && board[0].health == 7 && board[1].hidden,
          "a hidden caster is included in all hidden units, including at range zero");

    source.width = source.height = 1; source.row = source.column = 3;
    source.keywords.clear(); setPieceActionState(source, 0); source.shriekRange = 1;
    hidden.width = hidden.height = 1; hidden.row = hidden.column = 2;
    Piece guard = visible; guard.id = 6; guard.row = 1; guard.column = 2;
    guard.keywords = {"bodyguard"};
    board = {source, hidden, guard};
    applyShriek(board, board[0], random);
    check(!board[1].hidden && board[1].health == 10 && board[2].health == 7,
          "Shriek retains ordinary Bodyguard protection for its damage");
    board = {source, hidden, guard}; board[2].keywords = {"intercept"}; board[1].counterstrike = 9;
    applyShriek(board, board[0], random);
    check(board[0].health == 10 && board[1].health == 7 && board[2].health == 10 &&
          !board[2].interceptUsedThisTurn,
          "Shriek damage is not a piece attack and triggers neither Intercept nor Counterstrike");
    board[0].disabledTurns = 1;
    check(!pieceAbilityAvailable(board, board[0]), "Disabled pieces cannot Shriek");
    board[0].disabledTurns = 0; board[0].growTurnsRemaining = 1;
    check(!pieceAbilityAvailable(board, board[0]), "growing pieces cannot Shriek");
    board[0].growTurnsRemaining = 0; board[1].silenceRange = 1;
    activateSilenceField(board[1], 2);
    check(!pieceAbilityAvailable(board, board[0]), "Silence Field blocks Shriek");

    bool supportsShriek = true;
    sf::Packet legacyJoin;
    check(readShriekSnapshotCapability(legacyJoin, supportsShriek) && !supportsShriek,
          "legacy JoinGame requests keep the pre-Shriek snapshot format");
    sf::Packet modernJoin; modernJoin << ShriekSnapshotCapability;
    check(readShriekSnapshotCapability(modernJoin, supportsShriek) && supportsShriek,
          "new JoinGame requests opt into Shriek snapshot metadata");
    sf::Packet brokenJoin; brokenJoin << std::uint8_t{1};
    check(!readShriekSnapshotCapability(brokenJoin, supportsShriek) && !supportsShriek,
          "truncated snapshot capability is rejected");
    sf::Packet unknownJoin; unknownJoin << std::uint32_t{0};
    check(!readShriekSnapshotCapability(unknownJoin, supportsShriek),
          "unknown snapshot capability is rejected");

    Snapshot snapshot;
    snapshot.pieces = {source}; snapshot.hand = snapshot.foresightChoices = {card};
    snapshot.graveyard = snapshot.exiled = snapshot.raiseUndeadChoices = {card};
    sf::Packet packet; writeSnapshot(packet, snapshot);
    Snapshot decoded;
    check(readSnapshot(packet, decoded) && decoded.pieces[0].shriekRange == 1 &&
          decoded.pieces[0].shriekDamage == 3 && decoded.hand[0].shriekRange == 3 &&
          decoded.foresightChoices[0].shriekDamage == 2 && decoded.graveyard[0].shriekRange == 3 &&
          decoded.exiled[0].shriekDamage == 2 && decoded.raiseUndeadChoices[0].shriekDamage == 2,
          "snapshot wire format preserves Shriek configuration across every card zone and board pieces");
    sf::Packet oldAura; writeSnapshot(oldAura, snapshot, true, false);
    check(readSnapshot(oldAura, decoded) && decoded.pieces[0].shriekDamage == 0 &&
          decoded.hand[0].shriekRange == 1, "pre-Shriek aura snapshots receive Shriek defaults");
    sf::Packet legacy; writeSnapshot(legacy, snapshot, false);
    check(readSnapshot(legacy, decoded) && decoded.pieces[0].shriekRange == 1 &&
          decoded.hand[0].shriekDamage == 0, "legacy snapshots receive Shriek defaults");
    sf::Packet truncated; truncated.append(packet.getData(), packet.getDataSize() - 1);
    check(!readSnapshot(truncated, decoded), "truncated Shriek snapshot metadata is rejected");
    sf::Packet cardPacket; writeGameCard(cardPacket, card);
    GameCard decodedCard;
    check(readGameCard(cardPacket, decodedCard) && cardPacket.endOfPacket() &&
          decodedCard.shriekRange == 3 && decodedCard.shriekDamage == 2,
          "standalone card serialization preserves Shriek configuration");
    sf::Packet piecePacket; writePiece(piecePacket, source);
    Piece decodedPiece;
    check(readPiece(piecePacket, decodedPiece) && piecePacket.endOfPacket() &&
          decodedPiece.shriekRange == 1 && decodedPiece.shriekDamage == 3,
          "standalone piece serialization preserves Shriek configuration");
    Snapshot sandbox; int nextId = 1;
    bayou::client::spawnSandboxPiece(sandbox, nextId, 1, card, 3, 3, false);
    check(sandbox.pieces[0].shriekRange == 3 && sandbox.pieces[0].shriekDamage == 2,
          "sandbox spawning preserves Shriek configuration");
    bayou::client::destroyPieceInSnapshot(sandbox, nextId, sandbox.pieces[0].id, nullptr, nullptr);
    check(sandbox.graveyard.size() == 1 && sandbox.graveyard[0].shriekRange == 3 &&
          sandbox.graveyard[0].shriekDamage == 2,
          "sandbox death preserves Shriek fields when reconstructing a card without a catalog definition");

    GameCard ambusher;
    ambusher.title = "Shriek hidden fixture"; ambusher.type = "Unit";
    ambusher.health = 3; ambusher.ability = "dematerialize";
    ActionProfile action; action.state = 1; ambusher.actions = {action};
    card.health = 10; card.shriekRange = 1;
    for (int damage : {0, 2, 3})
    {
        for (int owner : {1, 2})
        {
            card.shriekDamage = damage;
            GameEngine engine(1302, {});
            check(engine.loadScenario({{owner, card, 3, 3, false}, {3-owner, ambusher, 2, 2, false},
                                       {owner, card, 7, 7, false}}, {}, {}, 0, 0, 3-owner, "Shriek test", false),
                  "authoritative Shriek scenario loads");
            const int casterId = engine.boardPieces()[0].id;
            const int hiddenId = engine.boardPieces()[1].id;
            const int secondCasterId = engine.boardPieces()[2].id;
            check(engine.useAbility(3-owner, hiddenId) && engine.endTurn(3-owner) &&
                  engine.snapshotFor(owner).pieces.size() == 2,
                  "authoritative Shriek fixture conceals the enemy from the caster");
            check(engine.useAbility(owner, casterId) && !engine.useAbility(owner, secondCasterId),
                  "Shriek activates without a target and consumes the turn's normal piece action");
            const Snapshot after = engine.snapshotFor(owner);
            if (damage == 3)
            {
                check(after.pieces.size() == 2 && after.graveyard.size() == 1 &&
                      after.graveyard[0].title == ambusher.title,
                      "lethal Shriek damage removes the hidden target through normal graveyard handling");
            }
            else
            {
                const auto found = std::find_if(after.pieces.begin(), after.pieces.end(),
                    [hiddenId](const Piece& piece) { return piece.id == hiddenId; });
                check(found != after.pieces.end() && !found->hidden && found->actionState == 0 &&
                      found->health == 3-damage && found->disabledTurns == (damage > 0 ? DamageDisabledTurns : 0),
                      "authoritative Shriek publishes the revealed target and applies Disable only for positive damage");
            }
        }
    }

    card.shriekDamage = 3;
    GameCard hero = ambusher; hero.type = "Hero"; hero.title = "Shriek hero fixture";
    GameEngine victory(1304, {});
    victory.loadScenario({{1, card, 3, 3, true}, {2, hero, 2, 2, true}},
                         {}, {}, 0, 0, 2, "Shriek victory");
    const int victoryCaster = victory.boardPieces()[0].id;
    check(victory.useAbility(2, victory.boardPieces()[1].id) && victory.endTurn(2) &&
          victory.useAbility(1, victoryCaster) && victory.snapshotFor(1).winner == 1 &&
          victory.snapshotFor(1).phase == static_cast<std::uint8_t>(Phase::GameOver),
          "Shriek killing the last hidden hero resolves match victory");

    GameCard sacrifice = card; sacrifice.ability.clear(); sacrifice.sacrifice = "shriek";
    GameEngine sacrificeEngine(1305, {});
    sacrificeEngine.loadScenario({{1, sacrifice, 3, 3, false}, {2, ambusher, 2, 2, false}},
                                 {}, {}, 0, 0, 2, "Sacrifice Shriek", false);
    const int sacrificeId = sacrificeEngine.boardPieces()[0].id;
    check(sacrificeEngine.useAbility(2, sacrificeEngine.boardPieces()[1].id) &&
          sacrificeEngine.endTurn(2) && sacrificeEngine.useAbility(1, sacrificeId, true) &&
          sacrificeEngine.boardPieces().empty() && sacrificeEngine.snapshotFor(1).graveyard.size() == 2,
          "Shriek also resolves through the existing Sacrifice ability path");

    // A hidden Shriek caster can kill itself. The action must still be spent.
    card.health = 1; card.shriekDamage = 1; card.keywords = {"dematerialize"};
    action.state = 0; action.nextState = 1; action.canMove = true;
    action.pattern = static_cast<std::uint8_t>(MovePattern::Omni); action.maxRange = 1;
    card.actions = {action};
    GameEngine self(1303, {});
    self.loadScenario({{1, card, 3, 3, false}, {1, card, 7, 7, false},
                       {2, ambusher, 0, 0, false}}, {}, {}, 0, 0, 1, "Self Shriek", false);
    const int selfId = self.boardPieces()[0].id;
    const int otherId = self.boardPieces()[1].id;
    check(self.movePiece(1, selfId, 3, 4) && self.endTurn(1) && self.endTurn(2) &&
          self.boardPieces()[0].hidden && self.useAbility(1, selfId) &&
          self.boardPieces().size() == 2 && !self.useAbility(1, otherId),
          "self-lethal Shriek still completes turn bookkeeping after removing its caster");
}
