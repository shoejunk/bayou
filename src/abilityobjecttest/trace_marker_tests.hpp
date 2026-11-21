#pragma once
#include "../gameserver/game_engine.hpp"

template <typename Check> void testTraceMarkers(Check check)
{
    using namespace game_data;
    marker_data::Definition bloom; bloom.name="Bloom";
    GameCard duchess; duchess.title="Duchess Dewbell"; duchess.type="Unit"; duchess.health=4;
    duchess.passives={"Traces"}; duchess.tracesMarkerName="Bloom"; duchess.tracesMarker=bloom;
    ActionProfile step; step.canMove=true; step.canAttack=false; step.maxRange=1; duchess.actions={step};
    GameEngine e(2901,{});
    check(e.loadScenario({{1,duchess,3,3,false}}, {},{},0,0,1,"Traces",false),"Traces fixture loads");
    const int id=e.boardPieces()[0].id;
    check(e.movePiece(1,id,3,4,0),"Dewbell moves with Traces");
    const auto owner=e.snapshotFor(1), enemy=e.snapshotFor(2);
    check(owner.boardMarkers.size()==1 && enemy.boardMarkers.size()==1 &&
        owner.boardMarkers[0].row==3 && owner.boardMarkers[0].column==3 &&
        owner.boardMarkers[0].owner==1 && owner.boardMarkers[0].definition.name=="Bloom",
        "Bloom is a named visible marker left on exact departure square");
    check(e.endTurn(1)&&e.endTurn(2)&&e.movePiece(1,id,3,3,0)&&e.boardPieces()[0].health==4&&
        e.snapshotFor(1).boardMarkers.size()==1 && e.snapshotFor(1).boardMarkers[0].column==4,
        "Dewbell can step back into her Bloom at full health, consume it and leave a fresh one");

    Piece source; source.owner=1; source.health=4; source.passives={"Traces"}; source.tracesMarker=bloom;
    Piece unit; unit.owner=2; unit.id=42; unit.health=2; unit.maxHealth=5; unit.row=2; unit.column=2;
    std::vector<BoardMarker> markers;
    check(placeTraceMarker(markers,source,2,2),"generic helper places trace");
    source.tracesMarker->name="bLOOM"; source.owner=2;
    check(!placeTraceMarker(markers,source,2,2) && markers[0].owner==1,
        "case-matching kind cannot overwrite an existing marker, even from opponent");
    check(!applyMarkerEntry(markers,{},unit,2,1)&&markers.size()==1&&unit.health==2,
        "enemy entry does not consume or receive Bloom");
    unit.owner=1;
    check(!applyMarkerEntry(markers,{},unit,2,2)&&markers.size()==1,
        "continuing to occupy a marker square is not re-entry");
    check(applyMarkerEntry(markers,{},unit,2,1)&&markers.empty()&&unit.health==3,
        "friendly entry consumes and heals exactly one");
    source.owner=1;
    source.tracesMarker->name="Bloom";
    check(placeTraceMarker(markers,source,2,2),"place Bloom for Rot Aura case");
    Piece rot; rot.owner=2; rot.id=43; rot.health=3; rot.row=2; rot.column=3; rot.passives={"Rot Aura"};
    check(applyMarkerEntry(markers,{rot},unit,-1,-1)&&markers.empty()&&unit.health==3,
        "friendly deployment consumes even when Rot Aura prevents healing");
    check(placeTraceMarker(markers,source,2,2),"place first kind");
    source.tracesMarker->name="Other Bloom";
    check(placeTraceMarker(markers,source,2,2)&&markers.size()==2,
        "different named marker kinds coexist at a square");
    markers.clear();
    source.tracesMarker->name="Blight";
    check(!placeTraceMarker(markers,source,2,2),"reserved Blight cannot be authored as a visible marker");
    source.tracesMarker->name="Snap";
    source.tracesMarker->eligibility=marker_data::Eligibility::Enemy;
    source.tracesMarker->effect=marker_data::Effect::Damage;
    source.tracesMarker->strength=5;
    check(placeTraceMarker(markers,source,2,2)&&!applyMarkerEntry(markers,{},unit,2,1)&&markers.size()==1,
        "Enemy Damage marker ignores its owner");
    unit.owner=2;unit.passives={"Undying"};
    check(applyMarkerEntry(markers,{},unit,2,1)&&unit.health==1&&unit.undyingUsed&&markers.empty(),
        "Enemy Damage consumes and ordinary Undying rules preserve eligible victim once");
    source.tracesMarker->eligibility=marker_data::Eligibility::Any;
    unit.owner=1;
    check(placeTraceMarker(markers,source,2,2)&&applyMarkerEntry(markers,{},unit,2,1)&&unit.health<=0,
        "Any Damage marker can defeat its own owner's piece");
    markers.assign(MaxBoardMarkers,BoardMarker{});
    check(!placeTraceMarker(markers,source,1,1),"runtime placement enforces per-board marker bound");
}
