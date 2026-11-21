#pragma once
#include "../gameserver/game_engine.hpp"

template<typename Check> void testBlightEngine(Check check)
{
    using namespace game_data;
    GameCard eye; eye.title="Eyeblight fixture"; eye.type="Unit"; eye.health=3;
    ability_data::Definition ability; ability.name="Corrupt the Ground"; ability.kind="corrupt the ground"; ability.strength=1;
    eye.activatedAbility=ability;
    GameCard walker; walker.title="Walker"; walker.type="Unit"; walker.health=3;
    ActionProfile walk; walk.canMove=true; walk.canAttack=false; walk.maxRange=5; walker.actions={walk};
    const auto find=[](const GameEngine& e,int id)->const Piece* {for(const auto& p:e.boardPieces())if(p.id==id)return &p;return nullptr;};
    for(int mode=0;mode<5;++mode)
    {
        auto mover=walker;
        if(mode==1)mover.actions[0].kind=static_cast<std::uint8_t>(ActionKind::Teleport);
        if(mode==2)mover.actions[0].passThrough=true;
        if(mode==3)mover.health=1;
        const int owner=mode==4?1:2;
        GameEngine e(2000+mode,{});
        check(e.loadScenario({{1,eye,4,3,false},{owner,mover,2,4,false}}, {},{},0,0,1,"Blight",false),"Blight engine fixture loads");
        const int eyeId=e.boardPieces()[0].id, moverId=e.boardPieces()[1].id;
        check(e.useAbility(1,eyeId),"Eyeblight activates approved ability");
        check(e.snapshotFor(1).traps.size()==1 && e.snapshotFor(2).traps.empty(),"trap exists only in owner's projected snapshot");
        check(!e.useAbility(1,eyeId),"normal action limit prevents repeated activation");
        check(e.endTurn(1),"end Eyeblight turn");
        if(owner==1)check(e.endTurn(2),"advance to friendly mover turn");
        check(e.movePiece(owner,moverId,6,4),"mover takes traversal action");
        if(mode==0)check(find(e,moverId) && find(e,moverId)->health==2 && e.snapshotFor(1).traps.empty(),"ground crossing takes Strength damage and consumes Blight");
        if(mode==1)check(find(e,moverId) && find(e,moverId)->health==3 && e.snapshotFor(1).traps.size()==1,"nonphysical relocation over trap does not trigger");
        if(mode==2)check(find(e,moverId) && find(e,moverId)->health==3 && e.snapshotFor(1).traps.size()==1,"flyover does not trigger");
        if(mode==3)check(!find(e,moverId) && e.snapshotFor(1).traps.empty() && e.snapshotFor(2).graveyard.size()==1,"lethal transit stops and enters normal graveyard pipeline");
        if(mode==4)check(find(e,moverId) && find(e,moverId)->health==3 && e.snapshotFor(1).traps.size()==1,"friendly traversal leaves Blight armed");
    }
    for(bool flying:{false,true})
    {
        auto mover=walker;
        if(flying)mover.actions[0].passThrough=true;
        else mover.actions[0].pattern=static_cast<std::uint8_t>(MovePattern::Jump);
        GameEngine e(2100+flying,{});e.loadScenario({{1,eye,4,3,false},{2,mover,2,flying?4:3,false}}, {},{},0,0,1,"Blight landing",false);
        int eyeId=e.boardPieces()[0].id, id=e.boardPieces()[1].id;
        check(e.useAbility(1,eyeId)&&e.endTurn(1)&&e.movePiece(2,id,4,4),"Blight landing action accepted");
        check(find(e,id)&&find(e,id)->health==(flying?3:2)&&e.snapshotFor(1).traps.size()==(flying?1:0),"jump landing triggers; flyer landing is immune");
    }
    GameEngine blocked(2200,{});blocked.loadScenario({{1,eye,4,3,false},{2,walker,4,4,false}}, {},{},0,0,1,"Blocked",false);
    check(!blocked.useAbility(1,blocked.boardPieces()[0].id)&&blocked.snapshotFor(1).traps.empty(),"visible occupied placement rejected without trap");
    auto undying=walker;undying.health=1;undying.passives={"Undying"};
    GameEngine u(2300,{});u.loadScenario({{1,eye,4,3,false},{2,undying,2,4,false}}, {},{},0,0,1,"Undying Blight",false);
    const int uid=u.boardPieces()[1].id;
    check(u.useAbility(1,u.boardPieces()[0].id)&&u.endTurn(1)&&u.movePiece(2,uid,6,4),"Undying Blight traversal accepted");
    check(find(u,uid)&&find(u,uid)->health==1&&find(u,uid)->undyingUsed&&find(u,uid)->row==6,"Undying survives Blight and finishes traversal");
    // Forced movement callbacks retain stable references; lethal trap damage
    // interrupts displacement without inventing collision damage.
    Piece pushed; pushed.id=10;pushed.owner=2;pushed.health=1;pushed.row=3;pushed.column=2;
    std::vector<Piece> forced{pushed};std::vector<Trap> traps{{1,3,3,1}};
    const auto push=applyActionPush(forced,10,3,1,3,0,[&](){applyBlightContact(traps,forced.front());});
    check(push.movedSquares==1&&push.collisionDamage==0&&forced.front().health==0&&traps.empty(),"lethal Blight interrupts push without collision damage");
    forced.front()=pushed;forced.front().passives={"Undying"};traps={{1,3,3,1}};
    const auto pushUndying=applyActionPush(forced,10,3,1,3,0,[&](){applyBlightContact(traps,forced.front());});
    check(pushUndying.movedSquares==3&&forced.front().health==1&&forced.front().undyingUsed,"Undying continues forced traversal after trap revival");
    // Hidden blocker follows Summon collision exactly.
    GameCard hidden=walker;hidden.activatedAbility=ability_data::Definition{};
    hidden.activatedAbility->kind="dematerialize";hidden.activatedAbility->states={{0,1,"Hide"},{1,0,"Show"}};
    GameEngine h(2400,{});h.loadScenario({{1,eye,4,3,false},{2,hidden,4,4,false}}, {},{},0,0,2,"Hidden Blight",false);
    check(h.useAbility(2,h.boardPieces()[1].id)&&h.endTurn(2)&&h.useAbility(1,h.boardPieces()[0].id),"hidden enemy collision spends Eyeblight action");
    check(h.snapshotFor(1).traps.empty()&&!h.boardPieces()[1].hidden&&h.boardPieces()[0].hasActed,"hidden occupant revealed and no trap created");    Piece target=pushed;target.id=11;target.health=3;target.row=3;target.column=3;
    Piece interceptor=target;interceptor.id=12;interceptor.column=4;interceptor.health=1;interceptor.passives={"Intercept","Bristle"};interceptor.counterstrike=3;
    Piece attacker=target;attacker.id=13;attacker.owner=1;attacker.column=2;
    std::vector<Piece> interceptPieces{target,interceptor,attacker};traps={{1,3,3,1}};std::mt19937 random(47);
    const auto intercepted=resolvePieceAttackDamage(interceptPieces,13,11,1,0,3,2,random,false,ActionKind::Slide,0,[&](){for(auto& p:interceptPieces)applyBlightContact(traps,p);});
    check(intercepted.attack.intercepted&&intercepted.attack.assignments.empty()&&intercepted.counterstrike.empty()&&intercepted.bristle.empty(),"interceptor killed by Blight receives no incoming strike or retaliation");
    check(interceptPieces[0].health==3&&interceptPieces[1].health==0&&interceptPieces[2].health==3,"lethal interception Blight preserves protected target and attacker");    auto stronger=eye;stronger.activatedAbility->strength=3;
    auto tougher=walker;tougher.health=5;
    GameEngine magnitude(2500,{});
    magnitude.loadScenario({{1,stronger,4,3,false},{2,tougher,2,4,false}}, {},{},0,0,1,"Configurable Blight",false);
    const int strongEye=magnitude.boardPieces()[0].id, tougherId=magnitude.boardPieces()[1].id;
    check(magnitude.useAbility(1,strongEye) && magnitude.snapshotFor(1).traps.size()==1 &&
        magnitude.snapshotFor(1).traps.front().strength==3,"Blight marker uses configured Strength 3");
    check(magnitude.endTurn(1) && magnitude.movePiece(2,tougherId,4,4) &&
        find(magnitude,tougherId) && find(magnitude,tougherId)->health==2,"Strength 3 Blight landing reduces health 5 to 2");
}
