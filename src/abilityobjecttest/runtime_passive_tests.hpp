#pragma once
#include "../gameserver/game_engine.hpp"

template <typename Check> void testRuntimePassives(Check check)
{
    using namespace game_data;
    GameCard card; card.title="Passive fixture"; card.type="Unit"; card.health=3;
    card.keywords={"Undying","Tag"}; card.passives={"Bind"};
    Piece piece;
    populatePieceFromCard(piece,card,false);
    check(piece.keywords==card.keywords && piece.passives==card.passives && !reviveUndyingPiece(piece),
          "an inert Keyword Undying never revives");
    check(pieceMatchesActionTargetFilter(piece,"undying") && pieceMatchesActionTargetFilter(piece,"bind"),
          "literal action filters match both keywords and passives");
    auto real=piece; real.passives={"Undying"}; real.health=0;
    check(reviveUndyingPiece(real) && real.undyingUsed && real.health==1 && !reviveUndyingPiece(real),
          "Passive Undying revives once");
    sf::Packet standalone;
    writeGameCard(standalone,card);
    GameCard decoded;
    check(readGameCard(standalone,decoded) && decoded.keywords==card.keywords && decoded.passives==card.passives,
          "standalone GameCard preserves inert behavior-spelled keyword and true passives exactly");
    sf::Packet standalonePiece;
    writePiece(standalonePiece,piece);
    Piece pieceDecoded;
    check(readPiece(standalonePiece,pieceDecoded) && pieceDecoded.keywords==card.keywords && pieceDecoded.passives==card.passives,
          "standalone Piece preserves exact Passives and Keywords");
    Snapshot state; state.yourPlayer=1; state.pieces={piece};
    state.hand=state.foresightChoices=state.graveyard=state.exiled=state.raiseUndeadChoices={card};
    card.chrysalisUsed=true;
    state.husks.push_back({1,0,0,card});
    sf::Packet modern, legacy;
    writeSnapshot(modern,state,true,true,true,true,true,true,true,true,true,true,true,true,true,true);
    writeSnapshot(legacy,state,true,true,true,true,true,true,true,true,true,true,true,true,true,false);
    Snapshot complete, old;
    check(readSnapshot(modern,complete) && complete.pieces[0].keywords==card.keywords &&
        complete.pieces[0].passives==card.passives && complete.hand[0].keywords==card.keywords &&
        complete.foresightChoices[0].passives==card.passives && complete.graveyard[0].keywords==card.keywords &&
        complete.exiled[0].passives==card.passives && complete.raiseUndeadChoices[0].passives==card.passives &&
        complete.husks[0].card.keywords==card.keywords && complete.husks[0].card.passives==card.passives,
        "PAS1 preserves exact piece lists, all five zones, and husks");
    check(readSnapshot(legacy,old) && old.pieces[0].keywords==std::vector<std::string>{"Tag"} &&
          old.pieces[0].passives==std::vector<std::string>{"Bind"} && old.husks[0].card.keywords==std::vector<std::string>{"Tag"},
          "legacy projection carries actual mechanic only without phantom Undying");
}
