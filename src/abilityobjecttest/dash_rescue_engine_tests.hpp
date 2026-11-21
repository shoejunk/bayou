#pragma once
#include "../gameserver/game_engine.hpp"

template <typename Check> void testDashRescueEngine(Check check)
{
    using namespace game_data;
    card_data::Card previous; previous.keywords={"Rescue"};
    previous.passives={"Dash's Rescue","Bodyguard","rescue"};
    sf::Packet payload; card_data::writeCard(payload,previous,true,true,true);
    card_data::Card migrated;
    check(card_data::readCard(payload,migrated,true,true,true,true,true,true,true,true,true) &&
        migrated.passives==std::vector<std::string>{"Rescue","Bodyguard"} &&
        migrated.keywords==previous.keywords, "older serialized Rescue spellings survive as one canonical passive; Keywords stay inert");
    GameCard archer; archer.title="Archer"; archer.type="Hero"; archer.health=8;
    ActionProfile shot; shot.kind=static_cast<std::uint8_t>(ActionKind::Ranged);
    shot.canMove=false; shot.canAttack=true; shot.damage=2; shot.statusTurns=1; shot.maxRange=6;
    archer.actions={shot};
    GameCard rider; rider.title="Nettle Starbright"; rider.type="Unit"; rider.health=8;
    rider.passives={"Rescue"};
    GameCard ally; ally.title="Ally"; ally.type="Unit"; ally.health=8;
    GameEngine e(315,{});
    check(e.loadScenario({{1,archer,3,1,true},{2,ally,3,4,false},{2,rider,4,4,false}},
        {},{},0,0,1,"Dash rescue",false),"Dash rescue fixture loads");
    const auto archerId=e.boardPieces()[0].id;
    check(e.attackPiece(1,archerId,3,4,0),"a legal attack evaded by Dash still succeeds as an action");
    check(e.boardPieces()[1].health==8 && e.boardPieces()[1].disabledTurns==0 && e.boardPieces()[1].row==3 && e.boardPieces()[1].column==5 &&
          e.boardPieces()[2].dashRescueUsedThisRound && e.boardPieces()[0].hasActed,
          "Dash moves ally clockwise, prevents shot, marks rescue and spends attack activation");
    check(!e.attackPiece(1,archerId,3,5,0),"evaded attack cannot be retried in the same activation");
    check(e.endTurn(1) && !e.boardPieces()[2].dashRescueUsedThisRound,
          "Dash rescue refreshes on rescuer's owner's next turn");

    std::vector<GameEngine::ScenarioPiece> jammed{{1,archer,0,4,true},{2,ally,3,4,false},{2,rider,4,4,false}};
    for (const auto& [r,c] : std::vector<std::pair<int,int>>{{3,3},{3,5},{4,3},{4,5},{5,3},{5,4},{5,5}})
        jammed.push_back({2,ally,r,c,false});
    GameEngine full(316,{});
    check(full.loadScenario(jammed,{}, {},0,0,1,"Dash blocked",false) &&
        full.attackPiece(1,full.boardPieces()[0].id,3,4,0) &&
        full.boardPieces()[1].health==6 && !full.boardPieces()[2].dashRescueUsedThisRound,
        "Dash cannot find a landing on a full ring, takes no charge, attack resolves");

    GameCard trapper=ally;
    trapper.title="Eyeblight";
    ability_data::Definition blight; blight.kind="corrupt the ground"; blight.strength=1;
    trapper.activatedAbility=blight;
    for (const bool flying : {false,true})
    {
        GameCard defender=ally;
        if (flying) defender.passives={"Flying"};
        GameEngine rescueOntoTrap(317,{});
        check(rescueOntoTrap.loadScenario({{1,archer,1,6,true},{2,defender,3,6,false},
            {2,rider,3,5,false},{1,trapper,4,5,false}}, {},{},0,0,1,"Dash over Blight",false),
            "Dash/Blight fixture loads");
        const auto shooter=rescueOntoTrap.boardPieces()[0].id;
        const auto planter=rescueOntoTrap.boardPieces()[3].id;
        check(rescueOntoTrap.useAbility(1,planter) && rescueOntoTrap.snapshotFor(2).traps.empty() &&
              rescueOntoTrap.endTurn(1) && rescueOntoTrap.endTurn(2) &&
              rescueOntoTrap.attackPiece(1,shooter,3,6,0),
              "Dash/Blight landing action accepted without revealing armed trap");
        check(rescueOntoTrap.boardPieces()[1].row==4 && rescueOntoTrap.boardPieces()[1].column==6 &&
              rescueOntoTrap.boardPieces()[1].health==(flying?8:7) &&
              rescueOntoTrap.snapshotFor(1).traps.size()==(flying?1u:0u),
              flying ? "flying rescue landing ignores Blight" : "normal rescue landing triggers and consumes Blight");
    }
}
