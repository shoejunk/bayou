#pragma once
#include "../shared/card_database.hpp"

template <typename Check>
void testBoardMarkerCatalog(Check check)
{
    using namespace card_data;
    const marker_data::Definition marker{"Fox's Trace",marker_data::Eligibility::Friendly,marker_data::Effect::Heal,1};
    Card source;
    source.title="Traces fixture";
    source.type="Unit";
    source.traits={"Fey"};
    source.keywords={"Plant"};
    source.passives={"Traces","Undying"};
    source.tracesMarkerName=marker.name;
    source.tracesMarker=marker;
    sf::Packet native;
    writeCardListHeader(native,1,true,true,true,true);
    writeCard(native,source,true,true,true,true);
    std::uint32_t count=0; bool old=false, objects=false, passives=false, markers=false;
    check(readCardListHeader(native,count,old,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&objects,&passives,&markers)
        && count==1 && !old && objects && passives && markers, "catalog v16 advertises marker definitions");
    Card decoded;
    check(readCard(native,decoded,true,true,true,true,true,true,true,true,true,true) && native.endOfPacket()
        && decoded.tracesMarkerName==source.tracesMarkerName && decoded.tracesMarker==source.tracesMarker
        && decoded.passives==source.passives, "v16 catalog preserves Traces and resolved marker");
    sf::Packet v15;
    writeCardListHeader(v15,1,true,true,true);
    writeCard(v15,source,true,true,true);
    check(readCardListHeader(v15,count,old,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&objects,&passives,&markers)
        && count==1 && objects && passives && !markers, "legacy request still advertises schema 15");
    check(readCard(v15,decoded,true,true,true,true,true,true,true,true,true) && v15.endOfPacket()
        && decoded.tracesMarkerName.empty() && !decoded.tracesMarker && decoded.passives==std::vector<std::string>{"Undying"},
        "v15 catalog safely omits unsupported Traces only");
    sf::Packet bad; auto invalid=source; invalid.tracesMarkerName.clear(); invalid.tracesMarker.reset();
    writeCard(bad,invalid,false,true,true,true);
    check(!readReferencedCard(bad,decoded,true,true,true), "authoring Traces without a marker is invalid");
    check(!marker_data::valid({"BLIGHT",marker_data::Eligibility::Enemy,marker_data::Effect::Damage,1}),
        "generic marker cannot use reserved Blight name");
    check(!marker_data::valid({"No effect",marker_data::Eligibility::Any,marker_data::Effect::Heal,0}),
        "generic marker requires positive strength");

    SQLite::Database db(":memory:",SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    db.exec("PRAGMA foreign_keys=ON; CREATE TABLE cards(title TEXT PRIMARY KEY); INSERT INTO cards VALUES('Nettle');");
    card_database::initializeBoardMarkers(db);
    card_database::saveBoardMarker(db,marker);
    card_database::setCardBoardMarker(db,"Nettle",marker.name);
    check(card_database::loadBoardMarker(db,marker.name)==marker &&
          card_database::loadBoardMarkers(db).size()==1, "marker library stores a reusable definition");
    bool blocked=false;
    try { card_database::deleteBoardMarker(db,marker.name); } catch(const std::exception&) { blocked=true; }
    check(blocked && card_database::loadBoardMarker(db,marker.name).has_value(), "referenced marker cannot be deleted");
    auto rename=marker; rename.name="Friendly Step";
    card_database::saveBoardMarker(db,rename,marker.name);
    check(db.execAndGet("SELECT marker_name FROM card_board_markers").getString()==rename.name &&
        card_database::loadBoardMarker(db,"FRIENDLY STEP")==rename, "rename cascades and lookup is case-insensitive");
    bool duplicate=false;
    try { auto same=rename; same.name="Friendly step"; card_database::saveBoardMarker(db,same); }
    catch(const std::exception&) { duplicate=true; }
    check(duplicate && card_database::loadBoardMarkers(db).size()==1, "case-only duplicate is refused");
    auto illustrated=rename;
    illustrated.imagePath="characters/bloomMarker.png";
    card_database::saveBoardMarker(db,illustrated,rename.name);
    check(card_database::loadBoardMarker(db,rename.name)==illustrated,
        "optional marker object art survives isolated catalog save/readback");
    auto renamedArt=illustrated; renamedArt.name="Renamed art object";
    card_database::saveBoardMarker(db,renamedArt,illustrated.name);
    check(card_database::loadBoardMarker(db,renamedArt.name)==renamedArt,
        "art belongs to the reusable object across renaming");
    rename=renamedArt;
    source.tracesMarker=renamedArt; source.tracesMarkerName=renamedArt.name;
    sf::Packet artCatalog;
    writeCardListHeader(artCatalog,1,true,true,true,true,true);
    writeCard(artCatalog,source,true,true,true,true,true);
    bool art=false;
    check(readCardListHeader(artCatalog,count,old,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
        &objects,&passives,&markers,&art) && art &&
        readCard(artCatalog,decoded,true,true,true,true,true,true,true,true,true,true,true) &&
        artCatalog.endOfPacket() && decoded.tracesMarker==source.tracesMarker,
        "schema 17 carries object artwork without coupling it to a card name");
    sf::Packet legacyArt;
    writeCard(legacyArt,source,true,true,true,true);
    check(readCard(legacyArt,decoded,true,true,true,true,true,true,true,true,true,true) &&
        legacyArt.endOfPacket() && decoded.tracesMarker && decoded.tracesMarker->imagePath.empty(),
        "schema 16 keeps its existing layout when object has art");
    check(!marker_data::validImagePath("../outside.png") && !marker_data::validImagePath("https://example.com/x.png") &&
        marker_data::validImagePath(illustrated.imagePath), "marker artwork accepts relative assets only");
    card_database::setCardBoardMarker(db,"Nettle","");
    card_database::deleteBoardMarker(db,rename.name);
    check(card_database::loadBoardMarkers(db).empty(), "unassigned marker can be deleted");
    SQLite::Database oldDb(":memory:",SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    oldDb.exec("CREATE TABLE cards(title TEXT PRIMARY KEY); CREATE TABLE board_markers (name TEXT PRIMARY KEY, eligibility INTEGER,effect INTEGER,strength INTEGER); INSERT INTO board_markers VALUES('Old Bloom',0,0,1)");
    check(card_database::loadBoardMarker(oldDb,"Old Bloom")->imagePath.empty(),
        "old read-only catalog loads without an art column");
    card_database::initializeBoardMarkers(oldDb);
    card_database::initializeBoardMarkers(oldDb);
    check(card_database::loadBoardMarker(oldDb,"Old Bloom")->imagePath.empty(),
        "optional art migration is repeatable and preserves existing marker definitions");

}
