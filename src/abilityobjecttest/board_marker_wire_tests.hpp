#pragma once
#include "../shared/game_data.hpp"

template <typename Check>
void testBoardMarkerWire(Check check)
{
    using namespace game_data;
    const marker_data::Definition bloom{"Bloom",marker_data::Eligibility::Friendly,marker_data::Effect::Heal,1};
    const marker_data::Definition mine{"Mine",marker_data::Eligibility::Enemy,marker_data::Effect::Damage,3};
    Snapshot source, received;
    source.yourPlayer=1;
    source.boardMarkers={{1,3,3,bloom},{2,3,3,mine}};
    source.traps={{1,3,3,1},{2,3,3,1}};
    Piece piece; piece.passives={"Traces","Rescue"}; piece.tracesMarker=bloom; piece.tracesMarkerName="Bloom";
    source.pieces={piece};
    GameCard card; card.passives={"Traces"}; card.tracesMarker=bloom;
    source.hand={card};
    const auto send=[&](sf::Packet& packet,bool markers) {
        writeSnapshot(packet,source,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,markers);
    };
    sf::Packet latest; send(latest,true);
    check(readSnapshot(latest,received) && latest.endOfPacket() && received.boardMarkers.size()==2 &&
        received.boardMarkers[0].definition==bloom && received.boardMarkers[1].definition==mine &&
        received.pieces[0].tracesMarker==bloom && received.hand[0].tracesMarker==bloom &&
        received.traps.size()==1 && received.traps[0].owner==1,
        "BMR1 delivers both public kinds, typed Traces and only the viewer's Blight");
    sf::Packet previous; send(previous,false);
    check(readSnapshot(previous,received) && previous.endOfPacket() && received.boardMarkers.empty() &&
        received.pieces[0].passives==std::vector<std::string>{"Rescue"} &&
        !received.pieces[0].tracesMarker && received.hand[0].passives.empty() && received.traps.size()==1,
        "DRS1 projections hide unsupported Traces and preserve Rescue and Blight");
    source.boardMarkers[0].definition.imagePath="characters/bloomMarker.png";
    source.pieces[0].tracesMarker->imagePath="characters/bloomMarker.png";
    sf::Packet artSnapshot;
    writeSnapshot(artSnapshot,source,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true);
    check(readSnapshot(artSnapshot,received) && artSnapshot.endOfPacket() &&
        received.boardMarkers[0].definition.imagePath=="characters/bloomMarker.png" &&
        received.pieces[0].tracesMarker->imagePath=="characters/bloomMarker.png" && received.traps.size()==1,
        "BMA1 carries object art on public markers and Traces while retaining Blight secrecy");
    sf::Packet oldArtSnapshot; send(oldArtSnapshot,true);
    check(readSnapshot(oldArtSnapshot,received) && oldArtSnapshot.endOfPacket() &&
        received.boardMarkers[0].definition.imagePath.empty(),
        "BMR1 remains byte-compatible and omits artwork for legacy clients");
    sf::Packet savedArt; writePiece(savedArt,source.pieces[0]);
    Piece restoredArt;
    check(readPiece(savedArt,restoredArt) && savedArt.endOfPacket() &&
        restoredArt.tracesMarker==source.pieces[0].tracesMarker,
        "standalone saved piece retains art and its reusable marker selection");
    auto truncatedArt=artSnapshot;
    sf::Packet truncated;
    truncated.append(truncatedArt.getData(),truncatedArt.getDataSize()-1);
    check(!readSnapshot(truncated,received),"truncated marker art snapshot fails closed");
    source.boardMarkers.push_back({2,3,3,{"bLoOm"}});
    sf::Packet duplicate; send(duplicate,true);
    check(!readSnapshot(duplicate,received),"case-insensitive same-kind duplicate on square is refused");
    sf::Packet standalone; writePiece(standalone,piece);
    Piece copy;
    check(readPiece(standalone,copy) && standalone.endOfPacket() && copy.tracesMarker==bloom,
        "standalone piece preserves selected definition");
    sf::Packet response;
    response << CommandRestrictionSnapshotCapability << SlowSnapshotCapability
        << ChrysalisSnapshotCapability << AreaHealSnapshotCapability << SilenceSnapshotCapability
        << AbilityParametersCapability << UndyingSnapshotCapability << AbilityObjectsSnapshotCapability
        << TrapSnapshotCapability << RootedSnapshotCapability << PassivesSnapshotCapability
        << DashRescueSnapshotCapability << BoardMarkerSnapshotCapability;
    bool slow=false,chrysalis=false,heal=false,silence=false,parameters=false,undying=false,
        objects=false,traps=false,rooted=false,passives=false,dash=false,markers=false;
    check(offersCommandRestrictionSnapshots(response,&slow,&chrysalis,&heal,&silence,&parameters,
        &undying,&objects,&traps,&rooted,&passives,&dash,&markers) && markers,
        "BMR1 only advertised after legacy readiness capabilities");
    response.clear();
    response << SlowSnapshotCapability << ChrysalisSnapshotCapability << AreaHealSnapshotCapability
        << SilenceSnapshotCapability << AbilityParametersCapability << UndyingSnapshotCapability
        << AbilityObjectsSnapshotCapability << TrapSnapshotCapability << RootedSnapshotCapability
        << PassivesSnapshotCapability << DashRescueSnapshotCapability << BoardMarkerSnapshotCapability;
    bool art=false;
    check(readSlowSnapshotOptIn(response,slow,&chrysalis,&heal,&silence,&parameters,&undying,
        &objects,&traps,&rooted,&passives,&dash,&markers,&art) && markers && !art,
        "legacy BMR1 opt-in remains accepted without marker art");
    response.clear();
    response << SlowSnapshotCapability << ChrysalisSnapshotCapability << AreaHealSnapshotCapability
        << SilenceSnapshotCapability << AbilityParametersCapability << UndyingSnapshotCapability
        << AbilityObjectsSnapshotCapability << TrapSnapshotCapability << RootedSnapshotCapability
        << PassivesSnapshotCapability << DashRescueSnapshotCapability << BoardMarkerSnapshotCapability
        << BoardMarkerArtCapability;
    check(readSlowSnapshotOptIn(response,slow,&chrysalis,&heal,&silence,&parameters,&undying,
        &objects,&traps,&rooted,&passives,&dash,&markers,&art) && art,
        "BMA1 art requires explicit negotiated opt-in");

}
