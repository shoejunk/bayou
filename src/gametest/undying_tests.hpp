#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testUndying(Check check)
{
    using namespace game_data;
    GameCard knight;
    knight.title = "Undying test knight";
    knight.type = "Unit";
    knight.health = 3;
    knight.passives = {"UnDyInG"};
    ActionProfile strike;
    strike.canMove = strike.canAttack = true;
    strike.maxRange = 1;
    strike.damage = 5;
    knight.actions = {strike};
    GameCard slayer = knight;
    slayer.title = "Undying test slayer";
    slayer.passives = {"Relentless"};
    slayer.health = 10;
    const auto find = [](const auto& pieces, int id) -> const Piece* {
        const auto it = std::find_if(pieces.begin(), pieces.end(), [id](const Piece& p) { return p.id == id; });
        return it == pieces.end() ? nullptr : &*it;
    };
    GameEngine engine(1810, {});
    check(engine.loadScenario({{1, slayer, 3, 2, false}, {2, knight, 3, 3, false}},
        {}, {}, 0, 0, 1, "Undying melee", false), "Undying melee fixture loads");
    const int attackerId = engine.boardPieces()[0].id, knightId = engine.boardPieces()[1].id;
    check(engine.attackPiece(1, attackerId, 3, 3), "lethal melee hits an Undying unit");
    const auto* alive = find(engine.boardPieces(), knightId);
    check(alive && alive->health == 1 && alive->undyingUsed && alive->maxHealth == 3 &&
        alive->row == 3 && alive->column == 3 && alive->owner == 2 &&
        engine.snapshotFor(2).graveyard.empty() && engine.snapshotFor(2).husks.empty() &&
        find(engine.boardPieces(), attackerId)->column == 2 && engine.relentlessPiece() == 0,
        "first defeat keeps the same one-health piece, blocks landing, creates no graveyard card or kill reward");
    check(engine.endTurn(1) && engine.endTurn(2) && engine.attackPiece(1, attackerId, 3, 3),
        "Undying can be hit again after a turn cycle");
    check(!find(engine.boardPieces(), knightId) && engine.snapshotFor(2).graveyard.size() == 1 &&
        find(engine.boardPieces(), attackerId)->column == 3 && engine.relentlessPiece() == attackerId,
        "second defeat is ordinary death with landing and Relentless reward");

    // The shared sandbox handler must keep all state and attachments in place.
    Snapshot sandbox; sandbox.yourPlayer = 2; int nextId = 10;
    bayou::client::spawnSandboxPiece(sandbox, nextId, 2, knight, 4, 4, false);
    auto& victim = sandbox.pieces.front();
    const int id = victim.id;
    victim.health = -8; victim.originalOwner = 1; victim.controlTurnsRemaining = 2;
    victim.disabledTurns = 2; victim.slowTurnsRemaining = 3;
    victim.hasActed = true; victim.sacrificePending = true;
    victim.silencedTurnsRemaining = 2; victim.silencedExpiryPlayer = 1;
    victim.abilitySilenced = true; victim.healingBlocked = true;
    victim.weakenedPower = 2;
    sandbox.commandingPieceId = sandbox.relentlessPieceId = id;
    Enchantment attachment; attachment.target = static_cast<std::uint8_t>(EnchantmentTarget::Piece);
    attachment.targetPieceId = id; attachment.targetRow = attachment.targetColumn = 4;
    sandbox.enchantments.push_back(attachment);
    const auto revival = bayou::client::destroyPieceInSnapshot(sandbox, nextId, id, nullptr, nullptr, &knight);
    check(revival.revived && !revival.replacementSpawned && !revival.wasRebirth &&
        sandbox.pieces.size() == 1 && victim.id == id && victim.health == 1 && victim.undyingUsed &&
        victim.owner == 2 && victim.originalOwner == 1 && victim.controlTurnsRemaining == 2 &&
        victim.hasActed && !victim.sacrificePending && victim.disabledTurns == 2 && victim.slowTurnsRemaining == 3 &&
        victim.silencedTurnsRemaining == 2 && victim.abilitySilenced && victim.healingBlocked && victim.weakenedPower == 2 &&
        sandbox.enchantments.size() == 1 && sandbox.enchantments[0].targetPieceId == id &&
        sandbox.commandingPieceId == id && sandbox.relentlessPieceId == id && nextId == 11,
        "sandbox revival preserves identity, allegiance, attachments, readiness, conditions and pending actions; Silence does not block it");

    sf::Packet piecePacket; writePiece(piecePacket, victim); Piece copy;
    check(readPiece(piecePacket, copy) && copy.undyingUsed && copy.id == id,
        "standalone piece serialization retains spent Undying state");
    sf::Packet packet; writeSnapshot(packet, sandbox); Snapshot received;
    check(readSnapshot(packet, received) && received.pieces.front().undyingUsed,
        "modern snapshot retains spent Undying state");
    sf::Packet legacy; writeSnapshot(legacy, sandbox, true, true, true, true, true, true, true, true, true, false);
    check(readSnapshot(legacy, received) && !received.pieces.front().undyingUsed && legacy.endOfPacket(),
        "pre-Undying snapshot remains readable with a safe default and no trailing data");
    sf::Packet truncated; writeSnapshot(truncated, sandbox);
    sf::Packet missingFlag; missingFlag.append(truncated.getData(), truncated.getDataSize() - 1);
    check(!readSnapshot(missingFlag, received), "truncated Undying snapshot is rejected");

    auto optIn = []() { sf::Packet p; p << SlowSnapshotCapability << ChrysalisSnapshotCapability <<
        AreaHealSnapshotCapability << SilenceSnapshotCapability << AbilityParametersCapability; return p; };
    bool slow=false, chrysalis=false, heal=false, silence=false, parameters=false, undying=false;
    auto modern = optIn(); modern << UndyingSnapshotCapability;
    check(readSlowSnapshotOptIn(modern, slow, &chrysalis, &heal, &silence, &parameters, &undying) && undying,
        "Undying snapshot metadata requires explicit capability opt-in");
    auto old = optIn();
    check(readSlowSnapshotOptIn(old, slow, &chrysalis, &heal, &silence, &parameters, &undying) && parameters && !undying,
        "older clients can still negotiate their previous snapshot format");
    auto malformed = optIn(); malformed << UndyingSnapshotCapability << std::uint8_t{1};
    check(!readSlowSnapshotOptIn(malformed, slow, &chrysalis, &heal, &silence, &parameters, &undying),
        "Undying opt-in rejects trailing data");
    sf::Packet ready; ready << CommandRestrictionSnapshotCapability << SlowSnapshotCapability <<
        ChrysalisSnapshotCapability << AreaHealSnapshotCapability << SilenceSnapshotCapability <<
        AbilityParametersCapability << UndyingSnapshotCapability;
    check(offersCommandRestrictionSnapshots(ready, &slow, &chrysalis, &heal, &silence, &parameters, &undying) && undying,
        "ready packets advertise Undying snapshot support");

    // No card-lifetime flag: a genuinely new piece starts unused.
    victim.health = 0;
    const auto finalDeath = bayou::client::destroyPieceInSnapshot(sandbox, nextId, id, nullptr, nullptr, &knight);
    check(!finalDeath.revived && sandbox.pieces.empty() && sandbox.graveyard.size() == 1 && sandbox.enchantments.empty(),
        "sandbox second defeat uses ordinary card and attachment cleanup");
    bayou::client::spawnSandboxPiece(sandbox, nextId, 1, sandbox.graveyard.front(), 4, 4, false);
    check(!sandbox.pieces.front().undyingUsed, "replaying the base card starts a new board lifetime");

    GameCard spell; spell.title = "Undying damage spell"; spell.type = "Spell";
    spell.effect = "damage"; spell.power = 5;
    GameEngine magic(1811, {});
    check(magic.loadScenario({{2, knight, 3, 3, false}}, {spell}, {}, 99, 0, 1, "Undying spell", false) &&
        magic.playCard(1, 0, 3, 3) && magic.boardPieces().size() == 1 &&
        magic.boardPieces()[0].health == 1 && magic.boardPieces()[0].undyingUsed,
        "spell damage reaches the same authoritative revival handler");

    GameCard guard = slayer; guard.counterstrike = 5;
    GameEngine retaliation(1812, {});
    check(retaliation.loadScenario({{1, knight, 3, 2, false}, {2, guard, 3, 3, false}},
        {}, {}, 0, 0, 1, "Undying counterstrike", false) &&
        retaliation.attackPiece(1, retaliation.boardPieces()[0].id, 3, 3) &&
        retaliation.boardPieces()[0].health == 1 && retaliation.boardPieces()[0].undyingUsed,
        "Counterstrike revives the attacking Undying piece in place");
    GameCard fragile = knight; fragile.health = 1;
    guard.counterstrike = 0; guard.passives = {"Bristle"};
    GameEngine bristle(1813, {});
    check(bristle.loadScenario({{1, fragile, 3, 2, false}, {2, guard, 3, 3, false}},
        {}, {}, 0, 0, 1, "Undying Bristle", false) &&
        bristle.attackPiece(1, bristle.boardPieces()[0].id, 3, 3) &&
        bristle.boardPieces()[0].health == 1 && bristle.boardPieces()[0].undyingUsed,
        "Bristle defeat also revives once");

    GameCard sacrifice = knight; sacrifice.sacrifice = "area heal";
    GameEngine offering(1814, {});
    check(offering.loadScenario({{1, sacrifice, 3, 3, false, 1}}, {}, {}, 0, 0, 1, "Undying sacrifice", false) &&
        offering.useAbility(1, offering.boardPieces()[0].id, true) && offering.boardPieces().size() == 1 &&
        offering.boardPieces()[0].health == 1 && offering.boardPieces()[0].undyingUsed &&
        !offering.boardPieces()[0].sacrificePending && offering.endTurn(1) && offering.boardPieces().size() == 1,
        "Undying survives sacrifice once and clears its pending resolution instead of dying again on pass");

    GameCard shrieker = knight; shrieker.passives.clear(); shrieker.ability = "shriek";
    shrieker.shriekRange = 2; shrieker.shriekDamage = 5;
    GameCard hidden = knight; hidden.ability = "dematerialize";
    hidden.actions[0].state = 1;
    // Hidden is represented by an action state greater than zero.
    GameEngine shriek(1815, {});
    shriek.loadScenario({{1, shrieker, 3, 2, false}, {2, hidden, 3, 3, false}},
        {}, {}, 0, 0, 2, "Undying Shriek", false);
    check(shriek.useAbility(2, shriek.boardPieces()[1].id) && shriek.endTurn(2) &&
        shriek.useAbility(1, shriek.boardPieces()[0].id) && shriek.boardPieces().size() == 2 &&
        shriek.boardPieces()[1].health == 1 && shriek.boardPieces()[1].undyingUsed,
        "Shriek lethal damage uses Undying after revealing its hidden target");

    GameCard pusher = slayer; pusher.actions[0].damage = 0; pusher.actions[0].push = 1;
    GameEngine collision(1816, {});
    check(collision.loadScenario({{1, pusher, 3, 6, false}, {2, fragile, 3, 7, false}},
        {}, {}, 0, 0, 1, "Undying collision", false) &&
        collision.attackPiece(1, collision.boardPieces()[0].id, 3, 7) &&
        collision.boardPieces().size() == 2 && collision.boardPieces()[1].health == 1 &&
        collision.boardPieces()[1].undyingUsed && collision.relentlessPiece() == 0,
        "lethal push collision at a board edge revives once without a Relentless reward");

    GameCard protector = knight; protector.passives.push_back("Bodyguard");
    GameCard protectedUnit = knight; protectedUnit.passives.clear();
    GameCard ranged = slayer; ranged.actions[0].kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    ranged.actions[0].canMove = false; ranged.actions[0].maxRange = 4;
    GameEngine bodyguard(1817, {});
    check(bodyguard.loadScenario({{1, ranged, 3, 1, false}, {2, protectedUnit, 3, 4, false},
        {2, protector, 2, 4, false}}, {}, {}, 0, 0, 1, "Undying Bodyguard", false) &&
        bodyguard.attackPiece(1, bodyguard.boardPieces()[0].id, 3, 4) &&
        bodyguard.boardPieces().size() == 3 && bodyguard.boardPieces()[1].health == 3 &&
        bodyguard.boardPieces()[2].health == 1 && bodyguard.boardPieces()[2].undyingUsed,
        "Bodyguard redirects lethal damage to Undying and preserves the protected unit");

    GameEngine objectiveEngine(1818, {});
    objectiveEngine.loadScenario({{1, slayer, 3, 2, false}, {2, knight, 3, 3, false}},
        {}, {}, 0, 0, 1, "Undying objective", false);
    const int striker = objectiveEngine.boardPieces()[0].id, objectiveId = objectiveEngine.boardPieces()[1].id;
    GameEngine::ScenarioObjective objective;
    objective.kind = GameEngine::ScenarioObjectiveKind::DefeatPiece;
    objective.targetPieceId = objectiveId; objective.successPlayer = 1;
    check(objectiveEngine.configureScenarioObjective(objective) && objectiveEngine.attackPiece(1, striker, 3, 3) &&
        objectiveEngine.phase() == Phase::Playing && objectiveEngine.scenarioObjectiveProgress().remaining == 1 &&
        objectiveEngine.endTurn(1) && objectiveEngine.endTurn(2) && objectiveEngine.attackPiece(1, striker, 3, 3) &&
        objectiveEngine.phase() == Phase::GameOver && objectiveEngine.winner() == 1,
        "defeat objective waits for the same Undying piece's actual second defeat");

    // When competing death rules exist, Undying prevents the first defeat;
    // the next defeat follows the original replacement/off-board rules.
    for (int mode = 0; mode < 4; ++mode)
    {
        Snapshot s; s.yourPlayer = 1; int next = 1;
        GameCard combined = knight;
        if (mode == 0) combined.passives.push_back("Chrysalis");
        bayou::client::spawnSandboxPiece(s, next, 1, combined, 3, 3, false);
        if (mode == 1) s.pieces[0].raisedFromGraveyard = true;
        if (mode == 2) { s.pieces[0].infestationOwner = 2; s.pieces[0].infestationTitle = slayer.title; }
        const int target = s.pieces[0].id;
        const GameCard* rebirth = mode == 3 ? &slayer : nullptr;
        const GameCard* infest = mode == 2 ? &slayer : nullptr;
        auto first = bayou::client::destroyPieceInSnapshot(s, next, target, rebirth, infest, &combined);
        check(first.revived && s.pieces[0].id == target && s.graveyard.empty() && s.exiled.empty() && s.husks.empty(),
            "Undying prevents first defeat before Chrysalis, raised exile, Infest or Rebirth");
        s.pieces[0].health = 0;
        auto second = bayou::client::destroyPieceInSnapshot(s, next, target, rebirth, infest, &combined);
        check(!second.revived && (mode == 0 ? s.husks.size() == 1 : mode == 1 ? s.exiled.size() == 1 :
            mode == 2 ? second.wasInfestation : second.wasRebirth),
            "after Undying is spent the existing death rule still runs");
    }
    Piece transformed; populatePieceFromCard(transformed, knight, false);
    transformed.id=1; transformed.owner=1; transformed.row=transformed.column=3;
    transformed.undyingUsed=true; transformed.transformTurnsRemaining=0; transformed.transformInto=slayer.title;
    std::vector<Piece> forms{transformed};
    advanceTimedTransformations(forms, 1, [&](const std::string&) { return &slayer; });
    check(forms[0].id == 1 && forms[0].undyingUsed, "same-piece timed transformation cannot refresh spent Undying");
}
