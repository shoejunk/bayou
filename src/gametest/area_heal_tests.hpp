#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testAreaHeal(Check check)
{
    using namespace game_data;
    card_data::Card definition;
    definition.title = "Area healer"; definition.type = "Unit";
    definition.stringValues = {{"ability", " Area Heal "}};
    GameCard card = toGameCard(definition);
    check(card.ability == "area heal" && card.healRange == 1 && !card.healFilter,
          "Area Heal normalizes ability and defaults to range one and unrestricted traits");
    definition.integerValues = {{"heal range", 2}};
    definition.stringLists = {{"heal filter", {"Fey", "Mechanical"}},
                              {"abilityLabels", {"Repair the Pattern"}}};
    card = toGameCard(definition);
    check(card.healRange == 2 && card.healFilter && card.healFilter->size() == 2,
          "Area Heal reads range and optional trait list");
    definition.integerValues = {{"heal range", 0}};
    check(toGameCard(definition).healRange == 0, "Area Heal preserves explicit zero range");
    definition.integerValues = {{"heal range", -3}};
    check(toGameCard(definition).healRange == 0, "Area Heal clamps negative range to zero");
    definition.stringLists = {{"heal filter", {}}};
    check(toGameCard(definition).healFilter && toGameCard(definition).healFilter->empty(),
          "Area Heal distinguishes an explicit empty trait restriction from an absent list");

    Piece source;
    populatePieceFromCard(source, card, false);
    source.id = 1; source.owner = 1; source.row = source.column = 3;
    source.health = 2; source.maxHealth = 4; source.traits = {"fey", "mechanical"};
    check(source.healRange == 2 && source.healFilter &&
          pieceAbilityLabel(source) == "Repair the Pattern" && pieceAbilityAvailable(source),
          "Area Heal parameters and card-specific button label survive spawning");
    Piece fey = source; fey.id = 2; fey.row = fey.column = 1; fey.traits = {"FEY"};
    Piece mechanical = source; mechanical.id = 3; mechanical.row = 5; mechanical.traits = {"Mechanical"};
    Piece both = source; both.id = 4; both.column = 5;
    Piece wrong = source; wrong.id = 5; wrong.column = 4; wrong.traits = {"wild"};
    Piece enemy = fey; enemy.id = 6; enemy.owner = 2;
    Piece distant = fey; distant.id = 7; distant.row = distant.column = 0;
    Piece dead = fey; dead.id = 8; dead.health = 0;
    std::vector<Piece> board{source, fey, mechanical, both, wrong, enemy, distant, dead};
    check(pieceCanReceiveAreaHeal(source, fey) && pieceCanReceiveAreaHeal(source, mechanical) &&
          !pieceCanReceiveAreaHeal(source, enemy) && !pieceCanReceiveAreaHeal(source, wrong),
          "Area Heal uses friendly ownership and case-insensitive OR trait matching");
    applyAreaHeal(board, board[0]);
    check(board[0].health == 3 && board[1].health == 3 && board[2].health == 3 && board[3].health == 3 &&
          board[4].health == 2 && board[5].health == 2 && board[6].health == 2 && board[7].health == 0,
          "Area Heal includes self and diagonal range boundary, excludes enemies and dead, and heals dual traits once");
    applyAreaHeal(board, board[0]); applyAreaHeal(board, board[0]);
    check(board[0].health == 4 && board[1].health == 4 && board[3].health == 4,
          "Area Heal caps each target at maximum Health");
    source.healFilter = std::vector<std::string>{"Chrysalis"};
    Piece keywordOnly = wrong; keywordOnly.id = 9; keywordOnly.keywords = {"CHRYSALIS"};
    board = {source, keywordOnly, fey}; applyAreaHeal(board, board[0]);
    check(board[1].health == 3 && board[2].health == 2,
          "Area Heal matches a keyword-only filter case-insensitively without requiring a trait");
    source.healFilter = std::vector<std::string>{"Fey", "Chrysalis"};
    Piece bothMatches = fey; bothMatches.id = 10; bothMatches.keywords = {"chrysalis"};
    board = {source, keywordOnly, fey, bothMatches, wrong}; applyAreaHeal(board, board[0]);
    check(board[0].health == 3 && board[1].health == 3 && board[2].health == 3 &&
          board[3].health == 3 && board[4].health == 2,
          "Area Heal OR-matches mixed traits and keywords and heals a dual match only once");
    source.healFilter.reset(); source.power = 99;
    board = {source, wrong}; applyAreaHeal(board, board[0]);
    check(board[1].health == 3, "unrestricted Area Heal heals other traits by exactly one despite Power");
    source.healFilter = std::vector<std::string>{};
    board = {source, fey}; applyAreaHeal(board, board[0]);
    check(board[0].health == 2 && board[1].health == 2, "empty Area Heal trait restriction matches nothing");
    source.healFilter.reset(); source.healRange = 0;
    board = {source, wrong}; applyAreaHeal(board, board[0]);
    check(board[0].health == 3 && board[1].health == 2, "range zero heals self but not an adjacent unit");
    source.healRange = 1; source.width = source.height = 2;
    fey.row = fey.column = 5; fey.width = fey.height = 2;
    board = {source, fey}; applyAreaHeal(board, board[0]);
    check(board[1].health == 3, "Area Heal measures nearest footprint squares and heals a large unit once");
    source.width = source.height = 1; source.healRange = 2;
    Piece rot = enemy; rot.row = 3; rot.column = 4; rot.keywords = {"Rot Aura"};
    board = {source, rot}; applyAreaHeal(board, board[0]);
    check(board[0].health == 2, "Area Heal respects enemy Rot Aura");
    check(pieceAbilityAvailable(board, board[0]),
          "healing prevention does not disable Area Heal and leak concealed Rot Aura presence");
    board = {source}; board[0].health = board[0].maxHealth;
    check(pieceAbilityAvailable(board, board[0]), "Area Heal permits a harmless full-health no-op");
    board = {source}; board[0].disabledTurns = 1;
    check(!pieceAbilityAvailable(board, board[0]), "disabled units cannot activate Area Heal");
    board[0].disabledTurns = 0; board[0].growTurnsRemaining = 1;
    check(!pieceAbilityAvailable(board, board[0]), "growing units cannot activate Area Heal");
    board = {source, enemy}; board[1].row = 3; board[1].column = 4;
    activateSilenceField(board[1], 2);
    check(!pieceAbilityAvailable(board, board[0]), "Silence Field blocks Area Heal");

    card.health = 4; card.traits = {"fey", "mechanical"};
    GameCard target = card; target.title = "Area Heal target"; target.ability.clear();
    GameCard opponent = target; opponent.title = "Area Heal enemy";
    for (int owner : {1, 2})
    {
        GameEngine engine(1501, {});
        check(engine.loadScenario({{owner, card, 3, 3, false, 2},
                                   {owner, target, 1, 1, false, 2},
                                   {3-owner, opponent, 6, 6, false}},
                                  {}, {}, 0, 0, owner, "Area Heal", false),
              "authoritative Area Heal fixture loads");
        const int caster = engine.boardPieces()[0].id;
        check(!engine.useAbility(3-owner, caster) && engine.useAbility(owner, caster),
              "Area Heal activation requires correct owner and turn");
        check(engine.boardPieces()[0].health == 3 && engine.boardPieces()[1].health == 3 &&
              engine.boardPieces()[0].hasActed && !engine.useAbility(owner, caster),
              "authoritative Area Heal heals all eligible units and consumes the normal action");
    }

    Snapshot snapshot; snapshot.pieces = {source};
    snapshot.hand = snapshot.foresightChoices = snapshot.graveyard = snapshot.exiled =
        snapshot.raiseUndeadChoices = {card};
    GameCard huskCard = card; huskCard.chrysalisUsed = true;
    snapshot.husks.push_back(PendingHusk{1, 2, 2, huskCard});
    sf::Packet packet; writeSnapshot(packet, snapshot);
    Snapshot decoded;
    check(readSnapshot(packet, decoded) && decoded.pieces[0].healRange == 2 &&
          !decoded.pieces[0].healFilter && decoded.hand[0].healFilter == card.healFilter &&
          decoded.foresightChoices[0].healRange == 2 && decoded.graveyard[0].healFilter == card.healFilter &&
          decoded.exiled[0].healRange == 2 && decoded.raiseUndeadChoices[0].healFilter == card.healFilter,
          "Area Heal snapshot fields survive across pieces and every card zone");
    check(decoded.husks.size() == 1 && decoded.husks[0].card.healRange == 2 &&
          decoded.husks[0].card.healFilter == card.healFilter,
          "modern Chrysalis nested card preserves Area Heal range and filter");
    sf::Packet legacy; writeSnapshot(legacy, snapshot, true, true, true, true, true, true, false);
    Snapshot oldDecoded;
    check(readSnapshot(legacy, oldDecoded) && oldDecoded.pieces[0].healRange == 1 &&
          !oldDecoded.pieces[0].healFilter && oldDecoded.hand[0].healRange == 1 &&
          !oldDecoded.hand[0].healFilter && !oldDecoded.graveyard[0].healFilter,
          "pre-Area-Heal snapshots default to range one and unrestricted traits");
    check(oldDecoded.husks.size() == 1 && oldDecoded.husks[0].card.healRange == 1 &&
          !oldDecoded.husks[0].card.healFilter,
          "legacy Chrysalis nested card omits Area Heal fields and reads safe defaults");
    sf::Packet malformed; writeSnapshot(malformed, snapshot, true, true, true, true, true, true, false);
    malformed << std::uint32_t{0x12345678};
    check(!readSnapshot(malformed, oldDecoded), "unknown trailing Area Heal snapshot extension is rejected");
    sf::Packet missingPayload; writeSnapshot(missingPayload, snapshot, true, true, true, true, true, true, false);
    missingPayload << AreaHealSnapshotCapability;
    check(!readSnapshot(missingPayload, oldDecoded), "Area Heal marker without configuration is rejected");
    bool slow = false, chrysalis = false, heal = false;
    sf::Packet ready;
    ready << CommandRestrictionSnapshotCapability << SlowSnapshotCapability <<
        ChrysalisSnapshotCapability << AreaHealSnapshotCapability;
    check(offersCommandRestrictionSnapshots(ready, &slow, &chrysalis, &heal) && slow && chrysalis && heal,
          "ready response advertises Area Heal after the prior snapshot capabilities");
    sf::Packet oldReady;
    oldReady << CommandRestrictionSnapshotCapability << SlowSnapshotCapability << ChrysalisSnapshotCapability;
    check(offersCommandRestrictionSnapshots(oldReady, &slow, &chrysalis, &heal) && slow && chrysalis && !heal,
          "legacy ready response does not imply Area Heal support");
    sf::Packet optIn; optIn << SlowSnapshotCapability << ChrysalisSnapshotCapability << AreaHealSnapshotCapability;
    check(readSlowSnapshotOptIn(optIn, slow, &chrysalis, &heal) && slow && chrysalis && heal,
          "client can explicitly opt into Area Heal snapshots");
    sf::Packet oldOptIn; oldOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability;
    check(readSlowSnapshotOptIn(oldOptIn, slow, &chrysalis, &heal) && slow && chrysalis && !heal,
          "legacy snapshot opt-in leaves Area Heal disabled");
    sf::Packet emptyOptIn;
    check(readSlowSnapshotOptIn(emptyOptIn, slow, &chrysalis, &heal) && !slow && !chrysalis && !heal,
          "empty legacy snapshot opt-in resets all newer capabilities");
    sf::Packet badOptIn; badOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability << std::uint32_t{0};
    check(!readSlowSnapshotOptIn(badOptIn, slow, &chrysalis, &heal),
          "unknown Area Heal opt-in is rejected");
    sf::Packet extraOptIn; extraOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability <<
        AreaHealSnapshotCapability << std::uint8_t{1};
    check(!readSlowSnapshotOptIn(extraOptIn, slow, &chrysalis, &heal),
          "trailing junk after Area Heal opt-in is rejected");
    sf::Packet cardPacket; writeGameCard(cardPacket, card); GameCard decodedCard;
    check(readGameCard(cardPacket, decodedCard) && cardPacket.endOfPacket() &&
          decodedCard.healRange == 2 && decodedCard.healFilter == card.healFilter,
          "Area Heal standalone card serialization preserves configuration");
    sf::Packet piecePacket; writePiece(piecePacket, source); Piece decodedPiece;
    check(readPiece(piecePacket, decodedPiece) && piecePacket.endOfPacket() &&
          decodedPiece.healRange == 2 && !decodedPiece.healFilter,
          "Area Heal standalone piece serialization preserves absent restriction");
    sf::Packet truncated; truncated.append(packet.getData(), packet.getDataSize()-1);
    check(!readSnapshot(truncated, decoded), "truncated Area Heal snapshot fields are rejected");
    Snapshot sandbox; int nextId = 1;
    bayou::client::spawnSandboxPiece(sandbox, nextId, 1, card, 3, 3, false);
    check(sandbox.pieces[0].healRange == 2 && sandbox.pieces[0].healFilter == card.healFilter,
          "sandbox spawning retains Area Heal parameters");
    bayou::client::destroyPieceInSnapshot(sandbox, nextId, sandbox.pieces[0].id, nullptr, nullptr);
    check(sandbox.graveyard.size() == 1 && sandbox.graveyard[0].healRange == 2 &&
          sandbox.graveyard[0].healFilter == card.healFilter,
          "sandbox fallback card reconstruction preserves Area Heal parameters");
}
