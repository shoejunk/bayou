#pragma once
#include "../shared/game_data.hpp"

template <typename Check>
void testTrapSnapshotWire(Check check)
{
    using namespace game_data;
    const auto optIn = [](sf::Packet& packet) {
        packet << SlowSnapshotCapability << ChrysalisSnapshotCapability
               << AreaHealSnapshotCapability << SilenceSnapshotCapability
               << AbilityParametersCapability << UndyingSnapshotCapability
               << AbilityObjectsSnapshotCapability;
    };
    sf::Packet ready;
    ready << CommandRestrictionSnapshotCapability;
    optIn(ready);
    ready << TrapSnapshotCapability;
    bool slow, chrysalis, heal, silence, parameters, undying, objects, traps;
    check(offersCommandRestrictionSnapshots(ready,&slow,&chrysalis,&heal,&silence,&parameters,&undying,&objects,&traps)
        && traps, "new server advertises TRP1 after ABO1");
    sf::Packet legacyOptIn; optIn(legacyOptIn);
    check(readSlowSnapshotOptIn(legacyOptIn,slow,&chrysalis,&heal,&silence,&parameters,&undying,&objects,&traps)
        && objects && !traps, "legacy ABO1 client opts in without TRP1");
    sf::Packet newOptIn; optIn(newOptIn); newOptIn << TrapSnapshotCapability;
    check(readSlowSnapshotOptIn(newOptIn,slow,&chrysalis,&heal,&silence,&parameters,&undying,&objects,&traps)
        && traps, "new client opts in to TRP1");
    Snapshot source, decoded;
    source.yourPlayer = 1;
    source.traps = {{1,3,4,1}, {2,5,6,9}};
    GameCard eyeblight;
    eyeblight.title = "Eyeblight";
    eyeblight.type = "Unit";
    eyeblight.activatedAbility.emplace();
    eyeblight.activatedAbility->kind = "corrupt the ground";
    eyeblight.activatedAbility->strength = 1;
    source.hand.push_back(eyeblight);
    const auto writeTraps = [&](sf::Packet& packet) {
        writeSnapshot(packet,source,true,true,true,true,true,true,true,true,true,true,true,true);
    };
    sf::Packet packet; writeTraps(packet);
    check(readSnapshot(packet,decoded) && decoded.traps.size()==1 && decoded.traps[0].owner==1 &&
          decoded.traps[0].row==3 && decoded.traps[0].column==4 && decoded.traps[0].strength==1,
          "owner receives only own trap even if snapshot mistakenly includes enemy trap");
    check(decoded.hand.size()==1 && decoded.hand[0].activatedAbility &&
          decoded.hand[0].activatedAbility->kind=="corrupt the ground",
          "negotiated wire retains Corrupt the Ground definition");
    sf::Packet legacy; writeSnapshot(legacy,source);
    check(readSnapshot(legacy,decoded) && decoded.traps.empty(), "unnegotiated wire omits TRP1 and clears old trap state");
    check(decoded.hand.size()==1 && !decoded.hand[0].activatedAbility,
          "legacy ABO1 wire omits unsupported Corrupt the Ground definition");
    source.traps.push_back({1,3,4,1});
    sf::Packet duplicate; writeTraps(duplicate);
    check(!readSnapshot(duplicate,decoded) && decoded.traps.empty(), "duplicate owner square fails validation atomically");
    source.traps = {{1,-1,4,1}};
    sf::Packet outside; writeTraps(outside);
    check(!readSnapshot(outside,decoded), "out-of-bounds trap is rejected");
    source.traps = {{1,3,4,-1}};
    sf::Packet negative; writeTraps(negative);
    check(!readSnapshot(negative,decoded), "negative Strength is rejected");
}
