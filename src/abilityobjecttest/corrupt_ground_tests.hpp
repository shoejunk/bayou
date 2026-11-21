#pragma once
#include "../shared/corrupt_ground_rules.hpp"

template <typename Check>
void testCorruptGroundPrimitives(Check check)
{
    using namespace game_data;
    Piece source; source.id=1; source.owner=1; source.health=3; source.row=3; source.column=3;
    std::vector<Piece> pieces{source};
    check(!inspectCorruptGroundPlacement(pieces,source,std::nullopt).spendsAction(),"trap requires explicit Strength");
    check(!inspectCorruptGroundPlacement(pieces,source,-1).spendsAction(),"negative Strength is rejected");
    const auto placed=activateCorruptGround(pieces,source,3);
    check(placed.trap && placed.trap->strength==3 && placed.trap->row==3 && placed.trap->column==4 && placed.placement.spendsAction(),"trap uses configurable Strength and player-one forward");
    Piece reverse=source; reverse.owner=2;
    check(inspectCorruptGroundPlacement(pieces,reverse,0).column==2,"player-two forward reverses direction");
    reverse.column=0;
    check(!inspectCorruptGroundPlacement(pieces,reverse,1).spendsAction(),"off-board placement spends no action");
    Piece occupant=source; occupant.id=2; occupant.owner=2; occupant.column=4; occupant.health=7;
    pieces.push_back(occupant);
    check(!activateCorruptGround(pieces,source,3).placement.spendsAction(),"visible enemy blocks trap placement");
    pieces.back().owner=1; pieces.back().hidden=true;
    check(!activateCorruptGround(pieces,source,3).placement.spendsAction(),"friendly hidden occupant blocks placement");
    pieces.back().owner=2;
    const auto revealed=activateCorruptGround(pieces,source,3);
    check(revealed.placement.spendsAction() && !revealed.trap && !pieces.back().hidden && pieces.back().health==7,"hidden enemy is revealed without trap or damage and spends action");
    const auto enemy=trapContact(*placed.trap,occupant);
    check(enemy && enemy->pieceId==2 && enemy->damage==3,"enemy landing receives configured damage intent");
    occupant.owner=1;
    check(!trapContact(*placed.trap,occupant) && placed.trap->strength==3,"friendly landing leaves trap unchanged");
    occupant.owner=2; occupant.column=5;
    check(!trapContact(*placed.trap,occupant),"non-overlapping landing does not trigger");
    occupant.column=3; occupant.width=2;
    check(trapContact(*placed.trap,occupant).has_value(),"large footprint landing triggers overlapped trap");
    occupant.health=0;
    check(!trapContact(*placed.trap,occupant),"dead unit does not trigger");
    occupant.health=7;
    auto zero=*placed.trap; zero.strength=0;
    check(trapContact(zero,occupant)->damage==0,"explicit zero Strength remains zero");
    // Feed only actual visited anchors; a jump/teleport feeds its landing only.
    occupant.width=1; occupant.column=3;
    check(!trapContact(*placed.trap,occupant),"before physical traversal there is no contact");
    occupant.column=4;
    check(trapContact(*placed.trap,occupant).has_value(),"physically passing through trap square triggers before final landing");
    occupant.column=5;
    check(!trapContact(*placed.trap,occupant),"teleport landing beyond trap does not fabricate a passed-through contact");    check(std::string(trapObjectName(TrapEffect::Damage))=="Blight","board object name is Blight, independent of ability button label");
    pieces.resize(1);
    const auto eyeblight=activateCorruptGround(pieces,source,1);
    check(eyeblight.trap && eyeblight.trap->strength==1,"Eyeblight explicit Strength one is supported");
    const auto ground=trapContactAnchors(3,3,3,6,TrapTravel::Ground);
    check(ground && *ground==std::vector<std::pair<int,int>>{{3,4},{3,5},{3,6}},"ground contacts traverse intermediate squares then landing, excluding origin");
    const auto jump=trapContactAnchors(3,3,3,6,TrapTravel::Jump);
    check(jump && *jump==std::vector<std::pair<int,int>>{{3,6}},"jump contacts only landing");
    const auto diagonal=trapContactAnchors(4,4,2,2,TrapTravel::Ground);
    check(diagonal && *diagonal==std::vector<std::pair<int,int>>{{3,3},{2,2}},"reverse diagonal ground traversal");
    check(!trapContactAnchors(3,3,4,5,TrapTravel::Ground),"non-straight ground path is not guessed");
    check(!trapContactAnchors(3,3,3,6,TrapTravel::Unresolved),"pending forced-motion policy stays unresolved");
    check(trapContactAnchors(3,3,3,3,TrapTravel::Ground)->empty(),"stationary unit does not trigger again");
    check(!trapContactAnchors(3,3,-1,6,TrapTravel::Ground),"invalid traversal is rejected");    const auto flyingOver=trapContactAnchors(3,3,3,6,TrapTravel::Flying);
    check(flyingOver && flyingOver->empty(),"flyer passing over Blight is immune");
    const auto flyingLanding=trapContactAnchors(3,3,3,4,TrapTravel::Flying);
    check(flyingLanding && flyingLanding->empty(),"flyer landing on Blight is immune");}
