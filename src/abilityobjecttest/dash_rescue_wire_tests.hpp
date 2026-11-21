#pragma once
#include "../shared/game_data.hpp"

template <typename Check>
void testDashRescueWire(Check check)
{
    using namespace game_data;
    const auto pas1Prefix = [](sf::Packet& packet) {
        packet << SlowSnapshotCapability << ChrysalisSnapshotCapability
               << AreaHealSnapshotCapability << SilenceSnapshotCapability
               << AbilityParametersCapability << UndyingSnapshotCapability
               << AbilityObjectsSnapshotCapability << TrapSnapshotCapability
               << RootedSnapshotCapability << PassivesSnapshotCapability;
    };
    bool slow=false,chrysalis=false,heal=false,silence=false,parameters=false,
        undying=false,objects=false,traps=false,rooted=false,passives=false,dash=false;
    sf::Packet ready;
    ready << CommandRestrictionSnapshotCapability;
    pas1Prefix(ready);
    ready << DashRescueSnapshotCapability;
    check(offersCommandRestrictionSnapshots(ready,&slow,&chrysalis,&heal,&silence,&parameters,&undying,&objects,&traps,&rooted,&passives,&dash)
        && passives && dash, "new server advertises DRS1 after PAS1");
    const auto optIn = [&](sf::Packet& packet) {
        return readSlowSnapshotOptIn(packet,slow,&chrysalis,&heal,&silence,&parameters,&undying,&objects,&traps,&rooted,&passives,&dash);
    };
    sf::Packet legacyOptIn; pas1Prefix(legacyOptIn);
    check(optIn(legacyOptIn) && passives && !dash, "PAS1-only clients remain compatible");
    sf::Packet modernOptIn; pas1Prefix(modernOptIn); modernOptIn << DashRescueSnapshotCapability;
    check(optIn(modernOptIn) && passives && dash, "client opts into DRS1");
    sf::Packet unknown; pas1Prefix(unknown); unknown << std::uint32_t{0};
    check(!optIn(unknown) && !dash, "invalid DRS1 opt-in fails closed");

    Snapshot source, copy;
    Piece dashPiece, other;
    dashPiece.id=1; dashPiece.owner=1; dashPiece.name="Dash";
    dashPiece.dashRescueUsedThisRound=true;
    other.id=2; other.owner=2;
    source.pieces={dashPiece,other};
    const auto send = [&](sf::Packet& packet, bool includeDash) {
        writeSnapshot(packet,source,true,true,true,true,true,true,true,true,true,true,true,true,true,true,includeDash);
    };
    sf::Packet modern; send(modern,true);
    check(readSnapshot(modern,copy) && modern.endOfPacket() && copy.pieces.size()==2 &&
        copy.pieces[0].dashRescueUsedThisRound && !copy.pieces[1].dashRescueUsedThisRound,
        "negotiated snapshot preserves per-piece Dash Rescue use");
    sf::Packet legacy; send(legacy,false);
    check(readSnapshot(legacy,copy) && legacy.endOfPacket() && copy.pieces.size()==2 &&
        !copy.pieces[0].dashRescueUsedThisRound, "PAS1-only snapshot clears stale Dash Rescue use");
    sf::Packet standalone; writePiece(standalone,dashPiece);
    Piece read;
    check(readPiece(standalone,read) && standalone.endOfPacket() && read.dashRescueUsedThisRound,
        "standalone piece round-trips DRS1");
    sf::Packet legacyPiece; writePiece(legacyPiece,dashPiece,true,false);
    check(readPiece(legacyPiece,read) && legacyPiece.endOfPacket() && !read.dashRescueUsedThisRound,
        "standalone PAS1-only piece resets Dash Rescue use");
    sf::Packet malformed; send(malformed,false);
    malformed << DashRescueSnapshotCapability << std::uint8_t{2} << std::uint8_t{0};
    check(!readSnapshot(malformed,copy), "invalid per-piece DRS1 byte is refused");
}
