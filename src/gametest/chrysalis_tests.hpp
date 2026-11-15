#pragma once

#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check>
void testChrysalis(Check check)
{
    using namespace game_data;
    GameCard grub;
    grub.title = "Chrysalis test Grub";
    grub.type = "Unit";
    grub.health = 4;
    grub.keywords = {"Chrysalis"};
    ActionProfile step;
    step.canMove = true;
    step.maxRange = 1;
    grub.actions = {step};
    GameCard shooter = grub;
    shooter.title = "Chrysalis test attacker";
    shooter.keywords.clear();
    ActionProfile shot;
    shot.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    shot.canMove = false;
    shot.canAttack = true;
    shot.damage = 4;
    shot.maxRange = 5;
    shooter.actions = {shot};

    GameEngine engine(731, {});
    if (!engine.loadScenario({{2, shooter, 0, 1, false}, {1, grub, 2, 3, false}},
            {}, {}, 0, 0, 2, "Chrysalis", false))
    {
        check(false, "Chrysalis engine fixture loads");
        return;
    }
    const int attackerId = engine.boardPieces().front().id;
    check(engine.attackPiece(2, attackerId, 2, 3), "Chrysalis victim can be defeated by ranged attack");
    auto pending = engine.snapshotFor(1);
    check(pending.husks.size() == 1 && pending.graveyard.empty() && pending.pieces.size() == 1 &&
          pending.husks.front().owner == 1 && pending.husks.front().card.chrysalisUsed,
          "defeat leaves a spent non-unit Husk without duplicating a graveyard card");
    check(engine.endTurn(2), "Chrysalis advances to the original owner's next turn");
    auto returned = engine.snapshotFor(1);
    const auto reborn = std::find_if(returned.pieces.begin(), returned.pieces.end(),
        [&](const Piece& p) { return p.name == grub.title; });
    check(returned.husks.empty() && returned.graveyard.empty() && reborn != returned.pieces.end() &&
          reborn->health == 1 && reborn->chrysalisUsed && reborn->row == 2 && reborn->column == 3,
          "unoccupied Husk returns at one health on the next owner turn");
    check(engine.endTurn(1) && engine.attackPiece(2, attackerId, 2, 3),
          "returned Chrysalis unit can be defeated again");
    auto secondDeath = engine.snapshotFor(1);
    check(secondDeath.husks.empty() && secondDeath.graveyard.size() == 1 &&
          secondDeath.graveyard.front().chrysalisUsed,
          "second defeat cannot create a second Husk and preserves spent state in graveyard");

    GameCard melee = shooter;
    melee.title = "Chrysalis melee attacker";
    ActionProfile strike = step;
    strike.canAttack = true;
    strike.damage = 4;
    strike.repeat = 1;
    melee.actions = {strike};
    GameEngine landing(732, {});
    check(landing.loadScenario({{2, melee, 3, 2, false}, {1, grub, 3, 3, false}},
          {}, {}, 0, 0, 2, "Chrysalis lethal landing", false), "Chrysalis melee fixture loads");
    const int meleeId = landing.boardPieces().front().id;
    check(landing.attackPiece(2, meleeId, 3, 3), "lethal melee attack lands on Chrysalis victim's square");
    const auto landed = landing.snapshotFor(1);
    check(landed.husks.empty() && landed.graveyard.size() == 1 && landed.graveyard.front().chrysalisUsed &&
          landed.pieces.size() == 1 && landed.pieces.front().row == 3 && landed.pieces.front().column == 3,
          "authoritative melee landing immediately crushes the newly created Husk");

    GameCard doomed = grub;
    doomed.title = "Chrysalis retaliation victim";
    ActionProfile poke = shot;
    poke.damage = 1;
    doomed.actions = {poke};
    GameCard repeater = melee;
    repeater.title = "Chrysalis repeating walker";
    repeater.health = 10;
    repeater.counterstrike = 4;
    GameCard guard = repeater;
    guard.title = "Chrysalis counterstrike guard";
    repeater.counterstrike = 0;
    GameEngine repeated(733, {});
    check(repeated.loadScenario({{1, doomed, 3, 3, false}, {2, guard, 3, 4, false}, {2, repeater, 2, 4, false}},
          {}, {}, 0, 0, 1, "Chrysalis repeated movement", false), "Chrysalis repeat fixture loads");
    const int doomedId = repeated.boardPieces()[0].id, repeaterId = repeated.boardPieces()[2].id;
    check(repeated.attackPiece(1, doomedId, 3, 4) && repeated.snapshotFor(1).husks.size() == 1 && repeated.endTurn(1),
          "counterstrike leaves a Husk for the opponent's next move");
    check(repeated.movePiece(2, repeaterId, 3, 3, 0), "repeating enemy steps onto Husk");
    check(repeated.snapshotFor(1).husks.empty(), "first repeat step crushes Husk");
    check(repeated.movePiece(2, repeaterId, 2, 3, 0), "repeating enemy then steps away from Husk");
    check(repeated.endTurn(2) && repeated.snapshotFor(1).husks.empty() && repeated.boardPieces().size() == 2 &&
          repeated.snapshotFor(1).graveyard.size() == 1,
          "moving away during repeat cannot undo authoritative crushing or restore the unit");

    const auto deadSnapshot = [&](bool used = false, bool raised = false) {
        Snapshot s;
        s.yourPlayer = 1;
        Piece p;
        populatePieceFromCard(p, grub, false);
        p.id = 10;
        p.owner = 1;
        p.row = 3;
        p.column = 3;
        p.health = 0;
        p.chrysalisUsed = used;
        p.raisedFromGraveyard = raised;
        s.pieces.push_back(p);
        return s;
    };
    int nextId = 20;
    auto sandbox = deadSnapshot();
    const auto death = bayou::client::destroyPieceInSnapshot(sandbox, nextId, 10, nullptr, nullptr, &grub);
    check(!death.replacementSpawned && sandbox.pieces.empty() && sandbox.husks.size() == 1 &&
          sandbox.graveyard.empty(), "sandbox Chrysalis death matches server and still counts as a kill");
    resolveHusksAtTurnStart(sandbox.pieces, sandbox.husks, sandbox.graveyard, sandbox.exiled, nextId, 2);
    check(sandbox.husks.size() == 1 && sandbox.pieces.empty(), "opponent turn cannot hatch the Husk");
    resolveHusksAtTurnStart(sandbox.pieces, sandbox.husks, sandbox.graveyard, sandbox.exiled, nextId, 1);
    check(sandbox.husks.empty() && sandbox.pieces.size() == 1 && sandbox.pieces.front().health == 1 &&
          sandbox.pieces.front().chrysalisUsed, "sandbox resolves the same one-health spent return");

    auto makePending = [&]() {
        auto s = deadSnapshot();
        bayou::client::destroyPieceInSnapshot(s, nextId, 10, nullptr, nullptr, &grub);
        return s;
    };
    auto crushed = makePending();
    Piece occupant;
    populatePieceFromCard(occupant, shooter, false);
    occupant.id = 30;
    occupant.owner = 2;
    occupant.row = occupant.column = 3;
    crushed.pieces.push_back(occupant);
    crushOccupiedHusks(crushed.pieces, crushed.husks, crushed.graveyard, crushed.exiled);
    check(crushed.husks.empty() && crushed.graveyard.size() == 1 &&
          crushed.graveyard.front().chrysalisUsed && crushed.pieces.size() == 1,
          "enemy occupation crushes Husk without harming occupant and consumes rebirth");
    crushed.pieces.front().column = 4;
    resolveHusksAtTurnStart(crushed.pieces, crushed.husks, crushed.graveyard, crushed.exiled, nextId, 1);
    check(crushed.pieces.size() == 1, "moving off a crushed Husk cannot restore it");

    occupant.owner = 1; // Friendly forced movement must crush just as enemy movement does.
    auto pushed = makePending();
    occupant.owner = 2;
    occupant.row = 3;
    occupant.column = 2;
    pushed.pieces.push_back(occupant);
    const auto pushedResult = applyActionPush(pushed.pieces, occupant.id, 3, 1, 2, 0, [&]() {
        crushOccupiedHusks(pushed.pieces, pushed.husks, pushed.graveyard, pushed.exiled);
    });
    check(pushedResult.movedSquares == 2 && pushed.pieces.front().column == 4 &&
          pushed.husks.empty() && pushed.graveyard.size() == 1,
          "each push step crushes a crossed Husk even when friendly unit finishes beyond it");
    auto pulled = makePending();
    occupant.column = 5;
    pulled.pieces.push_back(occupant);
    Piece puller = occupant;
    puller.id = 32;
    puller.owner = 1;
    puller.column = 1;
    pulled.pieces.push_back(puller);
    const auto pulledResult = applyActionPull(pulled.pieces, occupant.id, puller.id, [&]() {
        crushOccupiedHusks(pulled.pieces, pulled.husks, pulled.graveyard, pulled.exiled);
    });
    check(pulledResult.movedSquares == 3 && pulled.pieces.front().column == 2 &&
          pulled.husks.empty() && pulled.graveyard.size() == 1,
          "each pull step crushes a crossed Husk even when friendly unit finishes beyond it");
    occupant.row = occupant.column = 3;

    auto blocked = makePending();
    occupant.owner = 1;
    blocked.pieces.push_back(occupant);
    crushOccupiedHusks(blocked.pieces, blocked.husks, blocked.graveyard, blocked.exiled);
    check(blocked.husks.empty() && blocked.graveyard.size() == 1, "friendly occupation immediately crushes Husk");
    blocked.pieces.front().column = 4;
    resolveHusksAtTurnStart(blocked.pieces, blocked.husks, blocked.graveyard, blocked.exiled, nextId, 1);
    check(blocked.husks.empty() && blocked.graveyard.size() == 1 && blocked.pieces.size() == 1,
          "friendly unit moving away cannot restore crushed Husk at next owner turn");

    auto controlled = deadSnapshot();
    controlled.pieces.front().owner = 2;
    controlled.pieces.front().originalOwner = 1;
    bayou::client::destroyPieceInSnapshot(controlled, nextId, 10, nullptr, nullptr, &grub);
    check(controlled.husks.size() == 1 && controlled.husks.front().owner == 1,
          "controlled Chrysalis returns to its original owner, not temporary controller");
    auto spent = deadSnapshot(true);
    bayou::client::destroyPieceInSnapshot(spent, nextId, 10, nullptr, nullptr, &grub);
    check(spent.husks.empty() && spent.graveyard.size() == 1 && spent.graveyard.front().chrysalisUsed,
          "sandbox catalog reconstruction does not reset an already-used Chrysalis");
    auto raised = deadSnapshot(false, true);
    bayou::client::destroyPieceInSnapshot(raised, nextId, 10, nullptr, nullptr, &grub);
    check(raised.husks.empty() && raised.exiled.size() == 1,
          "raised-card exile retains precedence over Chrysalis");

    GameCard replacement = shooter;
    replacement.title = "Chrysalis replacement";
    auto rebirth = deadSnapshot();
    const auto immediate = bayou::client::destroyPieceInSnapshot(
        rebirth, nextId, 10, &replacement, nullptr, &grub);
    check(immediate.wasRebirth && rebirth.husks.empty() && rebirth.pieces.size() == 1 &&
          rebirth.pieces.front().name == replacement.title,
          "existing immediate Rebirth takes precedence over delayed Chrysalis");
    auto infested = deadSnapshot();
    infested.pieces.front().infestationOwner = 2;
    infested.pieces.front().infestationTitle = replacement.title;
    const auto infestation = bayou::client::destroyPieceInSnapshot(
        infested, nextId, 10, nullptr, &replacement, &grub);
    check(infestation.wasInfestation && infested.husks.empty() && infested.pieces.size() == 1 &&
          infested.pieces.front().owner == 2,
          "existing Infestation takes precedence over delayed Chrysalis");
    auto evacuated = deadSnapshot();
    Piece escort;
    populatePieceFromCard(escort, shooter, false);
    escort.id = 31;
    escort.owner = 1;
    escort.row = 2;
    escort.column = 3;
    escort.keywords = {"Evacuate"};
    evacuated.pieces.push_back(escort);
    bayou::client::destroyPieceInSnapshot(evacuated, nextId, 10, nullptr, nullptr, &grub);
    check(evacuated.husks.empty() && evacuated.hand.size() == 1 && evacuated.graveyard.empty(),
          "successful Evacuate preserves its precedence without a duplicate pending Husk");

    sf::Packet offered;
    offered << CommandRestrictionSnapshotCapability << SlowSnapshotCapability << ChrysalisSnapshotCapability;
    bool supportsSlow = false, supportsChrysalis = false;
    check(offersCommandRestrictionSnapshots(offered, &supportsSlow, &supportsChrysalis) &&
          offered.endOfPacket() && supportsSlow && supportsChrysalis,
          "new ready packet advertises CMD1, SLW1 and CHR1 capabilities");
    sf::Packet emptyOptIn;
    supportsSlow = supportsChrysalis = true;
    check(readSlowSnapshotOptIn(emptyOptIn, supportsSlow, &supportsChrysalis) &&
          !supportsSlow && !supportsChrysalis,
          "legacy empty snapshot opt-in keeps optional capabilities disabled");
    sf::Packet slowOptIn;
    slowOptIn << SlowSnapshotCapability;
    supportsChrysalis = true;
    check(readSlowSnapshotOptIn(slowOptIn, supportsSlow, &supportsChrysalis) &&
          supportsSlow && !supportsChrysalis && slowOptIn.endOfPacket(),
          "legacy SLW1-only opt-in does not enable Chrysalis snapshots");
    sf::Packet newOptIn;
    newOptIn << SlowSnapshotCapability << ChrysalisSnapshotCapability;
    check(readSlowSnapshotOptIn(newOptIn, supportsSlow, &supportsChrysalis) &&
          supportsSlow && supportsChrysalis && newOptIn.endOfPacket(),
          "new SLW1 plus CHR1 opt-in enables Chrysalis snapshots");
    sf::Packet malformedOptIn;
    malformedOptIn << SlowSnapshotCapability << std::uint32_t{0x42414431};
    check(!readSlowSnapshotOptIn(malformedOptIn, supportsSlow, &supportsChrysalis) && !supportsChrysalis,
          "malformed Chrysalis opt-in marker is rejected");
    sf::Packet malformedFirst;
    malformedFirst << ChrysalisSnapshotCapability;
    check(!readSlowSnapshotOptIn(malformedFirst, supportsSlow, &supportsChrysalis),
          "CHR1 without required SLW1 predecessor is rejected");
    Snapshot ordinary;
    ordinary.hand = {grub};
    Piece ordinaryPiece;
    populatePieceFromCard(ordinaryPiece, grub, false);
    ordinary.pieces = {ordinaryPiece};
    sf::Packet ordinaryPacket;
    writeSnapshot(ordinaryPacket, ordinary);
    Snapshot ordinaryDecoded;
    check(readSnapshot(ordinaryPacket, ordinaryDecoded) && ordinaryPacket.endOfPacket() &&
          ordinaryDecoded.husks.empty() && !ordinaryDecoded.hand.front().chrysalisUsed &&
          !ordinaryDecoded.pieces.front().chrysalisUsed,
          "ordinary new-format snapshot preserves false default Chrysalis flags");

    auto wire = makePending();
    GameCard usedCard = grub;
    usedCard.chrysalisUsed = true;
    wire.hand = wire.foresightChoices = wire.graveyard = wire.exiled = wire.raiseUndeadChoices = {usedCard};
    Piece living;
    populatePieceFromCard(living, usedCard, false);
    living.id = 40;
    living.owner = 1;
    wire.pieces.push_back(living);
    sf::Packet packet;
    writeSnapshot(packet, wire);
    Snapshot decoded;
    check(readSnapshot(packet, decoded) && packet.endOfPacket() && decoded.husks.size() == 1 &&
          decoded.husks.front().card.chrysalisUsed && decoded.pieces.front().chrysalisUsed &&
          decoded.hand.front().chrysalisUsed && decoded.foresightChoices.front().chrysalisUsed &&
          decoded.graveyard.front().chrysalisUsed && decoded.exiled.front().chrysalisUsed &&
          decoded.raiseUndeadChoices.front().chrysalisUsed,
          "Chrysalis snapshots preserve Husk and spent flags in every card zone");
    sf::Packet legacy;
    writeSnapshot(legacy, wire, true, true, true, true, true, false);
    check(readSnapshot(legacy, decoded) && legacy.endOfPacket() && decoded.husks.empty() &&
          !decoded.pieces.front().chrysalisUsed && !decoded.hand.front().chrysalisUsed,
          "legacy snapshot read clears stale Husk state and defaults spent flags");
}
