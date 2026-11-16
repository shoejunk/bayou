#pragma once
#include "../gameserver/game_engine.hpp"
#include "../client/client_sandbox.hpp"

template <typename Check> void testAbilityParameters(Check check)
{
    using namespace game_data;
    card_data::Card definition; definition.title = "Pallid test"; definition.type = "Unit";
    definition.stringValues = {{"ability", "weaken"}};
    auto card = toGameCard(definition);
    check(activatedRange(card)==1 && activatedStrength(card)==1 && activatedDuration(card)==1, "shared ability defaults are one");
    definition.integerValues = {{AbilityRangeField,2},{AbilityStrengthField,3},{AbilityDurationField,4},{HealRangeField,6},{SilenceTurnsField,8}};
    card=toGameCard(definition); card.health=5;
    check(activatedRange(card)==2 && activatedStrength(card)==3 && activatedDuration(card)==4, "canonical parameters override legacy and are independent");
    definition.integerValues = {{AbilityRangeField,0},{AbilityStrengthField,0}};
    auto zero=toGameCard(definition);
    check(activatedRange(zero)==0 && activatedStrength(zero)==0, "explicit zero is not a missing parameter");
    definition.stringValues={{"ability","shriek"}}; definition.integerValues={{ShriekRangeField,3},{ShriekDamageField,0}};
    auto legacy=toGameCard(definition);
    check(activatedRange(legacy)==3 && activatedStrength(legacy)==0, "legacy reveal-only Shriek stays zero damage");
    definition.stringValues={{"ability","silence"}}; definition.integerValues={{SilenceTurnsField,4}};
    legacy=toGameCard(definition);
    check(activatedDuration(legacy)==4 && activatedRange(legacy)==7, "legacy Silence keeps its duration and global radius");
    Piece caster; populatePieceFromCard(caster,card,false); caster.id=1;caster.owner=1;caster.row=caster.column=3;
    caster.abilityStrength=1;
    Piece target=caster; target.id=2;target.owner=2;target.row=target.column=1;target.power=2;
    Piece friendUnit=target; friendUnit.id=3;friendUnit.owner=1;
    Piece far=target;far.id=4;far.row=far.column=0;
    Piece dead=target;dead.id=5;dead.health=0;
    std::vector<Piece> board{caster,target,friendUnit,far,dead};
    applyWeaken(board,board[0]);
    check(board[1].weakenedPower==1 && board[2].weakenedPower==0 && board[3].weakenedPower==0 && board[4].weakenedPower==0 && board[0].weakenedPower==0,"Weaken selects living enemies within diagonal radius only");
    check(damageWithPower(1,effectivePiecePower(board,board[1]))==2,"positive and negative Power sum before final damage floor");
    applyWeaken(board,board[0]);applyWeaken(board,board[0]);
    check(board[1].weakenedPower==3 && damageWithPower(1,effectivePiecePower(board,board[1]))==0,"permanent Weaken stacks to reduce a one-damage attack to zero");
    board[1].power=0;
    check(damageWithPower(1,effectivePiecePower(board,board[1]))==0 && damageWithPower(0,100)==0,"negative total never produces negative damage and non-damage stays zero");
    board[1].power=4;
    check(damageWithPower(1,effectivePiecePower(board,board[1]))==2,"later positive Power offsets existing negative status without intermediate floor");
    board.erase(board.begin());
    check(board[0].weakenedPower==3,"Weaken survives caster removal");
    board[0].row=7; updateSilenceAtTurnEnd(board,2);
    check(board[0].weakenedPower==3,"Weaken does not expire with movement or turn end");
    clearWeaken(board[0]);check(board[0].power==4 && board[0].weakenedPower==0,"clearing negative status preserves positive Power");
    board[0].power=-2; board[0].weakenedPower=2; board[0].powerIncludesModifiers=true;
    check(effectivePiecePower(board,board[0])==-2,"effective snapshot Power is not modified twice");
    board[0].powerIncludesModifiers=false;board[0].power=0;board[0].weakenedPower=std::numeric_limits<int>::max();
    caster.abilityStrength=std::numeric_limits<int>::max();caster.row=7;
    board.push_back(caster);applyWeaken(board,board.back());
    check(board[0].weakenedPower==std::numeric_limits<int>::max(),"stacking penalty saturates without overflow");
    Snapshot snap;snap.pieces=board;snap.hand={card};snap.foresightChoices={card};snap.graveyard={card};snap.exiled={card};snap.raiseUndeadChoices={card};
    sf::Packet packet;writeSnapshot(packet,snap);Snapshot decoded;
    check(readSnapshot(packet,decoded) && decoded.pieces[0].weakenedPower==board[0].weakenedPower && decoded.hand[0].abilityStrength==card.abilityStrength && decoded.exiled[0].abilityDuration==card.abilityDuration,"new snapshot roundtrips canonical fields and negative status");
    sf::Packet old;writeSnapshot(old,snap,true,true,true,true,true,true,true,true,false);Snapshot oldDecoded;
    check(readSnapshot(old,oldDecoded) && !oldDecoded.hand[0].abilityStrength && oldDecoded.pieces[0].weakenedPower==0,"pre-parameter snapshot retains compatibility defaults");
    sf::Packet cardPacket;writeGameCard(cardPacket,card);GameCard decodedCard;
    check(readGameCard(cardPacket,decodedCard) && decodedCard.abilityRange==card.abilityRange,"standalone card parameters roundtrip");
    sf::Packet piecePacket;writePiece(piecePacket,board[0]);Piece decodedPiece;
    check(readPiece(piecePacket,decodedPiece) && decodedPiece.weakenedPower==board[0].weakenedPower,"standalone negative status roundtrip");
    bool slow=false,chrysalis=false,heal=false,silence=false,parameters=false;
    sf::Packet opt;opt<<SlowSnapshotCapability<<ChrysalisSnapshotCapability<<AreaHealSnapshotCapability<<SilenceSnapshotCapability<<AbilityParametersCapability;
    check(readSlowSnapshotOptIn(opt,slow,&chrysalis,&heal,&silence,&parameters)&&parameters,"parameter extension is explicitly negotiated");
    // Authoritative execution and a second complete turn validate persistence/action use.
    card.abilityRange=2;card.abilityStrength=1;card.power=0;
    GameCard victim=card;victim.ability.clear();victim.title="victim";
    GameEngine engine(1701,{});
    check(engine.loadScenario({{1,card,3,3,false},{2,victim,1,1,false}}, {},{},0,0,1,"Weaken",false),"authoritative Weaken fixture loads");
    const int casterId=engine.boardPieces()[0].id;
    check(!engine.useAbility(2,casterId) && engine.useAbility(1,casterId) && !engine.useAbility(1,casterId),"Weaken requires owner/turn and consumes normal action");
    check(engine.boardPieces()[1].weakenedPower==1 && engine.snapshotFor(1).pieces[1].power==-1,"authoritative Weaken publishes signed effective Power");
    check(engine.endTurn(1)&&engine.endTurn(2)&&engine.useAbility(1,casterId)&&engine.boardPieces()[1].weakenedPower==2,"authoritative repeated casts stack across full turns");
    Snapshot sandbox;int nextId=1;
    bayou::client::spawnSandboxPiece(sandbox,nextId,1,card,3,3,false);
    bayou::client::spawnSandboxPiece(sandbox,nextId,2,victim,1,1,false);
    applyWeaken(sandbox.pieces,sandbox.pieces[0]);
    check(sandbox.pieces[1].weakenedPower==1 && sandbox.pieces[0].abilityRange==2,"sandbox spawn and Weaken retain generic config");
    card.chrysalisUsed=true;snap.husks.push_back(PendingHusk{1,2,2,card});sf::Packet nested;writeSnapshot(nested,snap);Snapshot nestedResult;
    check(readSnapshot(nested,nestedResult)&&nestedResult.husks[0].card.abilityRange==2,"nested Chrysalis card preserves canonical fields");
    caster.row=caster.column=3;Piece healer=caster;healer.ability="area heal";healer.abilityRange=2;healer.abilityStrength=2;healer.health=1;healer.maxHealth=5;
    std::vector<Piece> healing{healer};applyAreaHeal(healing,healing[0]);
    check(healing[0].health==3,"area heal uses shared strength and heals self");
    Piece boost=healer;boost.ability="power boost";Piece ally=boost;ally.id=9;ally.row=1;ally.power=0;
    std::vector<Piece> boosting{boost,ally};applyPowerBoost(boosting,boosting[0]);
    check(boosting[1].power==2 && boosting[0].power==boost.power,"Power Boost uses shared range and strength, still excluding self");
    Piece silencer=caster;silencer.ability="silence";silencer.abilityRange=1;silencer.abilityDuration=2;
    Piece close=target;close.row=2;close.column=3;Piece distant=target;distant.row=0;
    std::vector<Piece> silencing{silencer,close,distant};applySilence(silencing,silencing[0]);
    check(silencing[1].silencedTurnsRemaining==2 && silencing[2].silencedTurnsRemaining==0,"explicit Silence range and duration act independently");

    GameCard mirrored=card;mirrored.abilityRange=2;mirrored.abilityStrength=3;mirrored.abilityDuration=4;
    Snapshot mirrorSnapshot;mirrorSnapshot.hand={mirrored};sf::Packet mirrorPacket;
    writeSnapshot(mirrorPacket,mirrorSnapshot,true,true,true,true,true,true,true,true,false);Snapshot mirrorRead;
    check(readSnapshot(mirrorPacket,mirrorRead) && mirrorRead.hand[0].healRange==2 && mirrorRead.hand[0].shriekRange==2 && mirrorRead.hand[0].shriekDamage==3 && mirrorRead.hand[0].silenceTurns==4,"canonical values mirror into legacy wire slots without ABP1");

}
