#pragma once

#include "../gameserver/game_engine.hpp"

template <typename Check>
void testBristle(Check check)
{
    using namespace game_data;
    Piece attacker;
    attacker.id = 1;
    attacker.owner = 1;
    attacker.row = 3;
    attacker.column = 2;
    attacker.health = attacker.maxHealth = 10;
    attacker.keywords = {"bristle"};
    Piece defender = attacker;
    defender.id = 2;
    defender.owner = 2;
    defender.column = 3;
    defender.power = 4;
    defender.keywords = {"BrIsTlE", "bristle"};
    std::mt19937 rng(97);
    for (const auto kind : {ActionKind::Slide, ActionKind::Capture, ActionKind::Ranged})
    {
        for (const int damage : {0, 2, 10})
        {
            std::vector<Piece> pieces{attacker, defender};
            const auto result = resolvePieceAttackDamage(
                pieces, 1, 2, damage, 0, 3, 2, rng, false, kind);
            const bool ranged = kind == ActionKind::Ranged;
            check(pieces[0].health == (ranged ? 10 : 9) &&
                      pieces[1].health == 10 - damage && result.bristle.empty() == ranged,
                "Bristle returns exactly one damage for non-ranged attacks, including lethal and zero-damage hits, without chaining");
        }
    }
    std::vector<Piece> pieces{attacker, defender};
    pieces[1].keywords.clear();
    resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng);
    check(pieces[0].health == 10, "units without Bristle do not return damage");
    pieces = {attacker, defender};
    pieces[1].owner = 1;
    resolvePieceAttackDamage(pieces, 1, 2, 0, 0, 3, 2, rng);
    check(pieces[0].health == 10, "friendly actions do not trigger Bristle");
    pieces = {attacker, defender};
    applyDamageWithBodyguards(pieces, 2, 2, 0, rng);
    check(pieces[0].health == 10, "spell damage does not trigger Bristle");
    pieces = {attacker, defender};
    pieces[0].health = 5;
    resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng, true);
    check(pieces[0].health == 6, "Lifedrain heals attack damage before Bristle returns damage");
    pieces = {attacker, defender};
    pieces[1].counterstrike = 2;
    resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng);
    check(pieces[0].health == 3, "Bristle stacks with powered Counterstrike but stays fixed at one damage");

    Piece protector = defender;
    protector.id = 3;
    protector.row = 2;
    protector.keywords = {"Intercept"};
    pieces = {attacker, defender, protector};
    const auto intercepted = resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng);
    check(intercepted.attack.effectiveTargetId == 3 && pieces[0].health == 10,
        "Intercept replaces the original Bristle defender");
    pieces = {attacker, defender, protector};
    pieces[1].keywords.clear();
    pieces[2].keywords.push_back("bristle");
    resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng);
    check(pieces[0].health == 9, "a Bristle interceptor returns damage");
    protector.keywords = {"Bodyguard"};
    pieces = {attacker, defender, protector};
    resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng);
    check(pieces[0].health == 9 && pieces[1].health == 10 && pieces[2].health == 8,
        "Bodyguard protects the defender without suppressing its Bristle");
    protector.owner = 1;
    pieces = {attacker, defender, protector};
    pieces[0].column = 0;
    resolvePieceAttackDamage(pieces, 1, 2, 2, 0, 3, 2, rng);
    check(pieces[0].health == 10 && pieces[0].column == 0 && pieces[2].health == 9,
        "Bodyguard protects against Bristle at the moving attack's strike position");

    card_data::Card definition;
    definition.title = "Bristle defender";
    definition.type = "Unit";
    definition.keywords = {"BRISTLE"};
    definition.integerValues = {{"health", 2}};
    const GameCard targetCard = toGameCard(definition);
    for (const auto kind : {ActionKind::Slide, ActionKind::Capture, ActionKind::Ranged})
    {
        for (int health : {1, 3})
        {
            GameCard actorCard = targetCard;
            actorCard.title = "Bristle attacker";
            actorCard.health = health;
            actorCard.keywords = {"Relentless"};
            ActionProfile action;
            action.kind = static_cast<std::uint8_t>(kind);
            action.pattern = static_cast<std::uint8_t>(MovePattern::Omni);
            action.minRange = 1;
            action.maxRange = 7;
            action.canMove = kind != ActionKind::Ranged;
            action.canAttack = true;
            action.damage = 2;
            actorCard.actions = {action};
            GameEngine engine(97, {});
            // Include adjacent ranged attacks: distance must not decide Bristle.
            const int origin = kind == ActionKind::Ranged ? 2 : 0;
            check(engine.loadScenario({{1, actorCard, 3, origin, false},
                      {2, targetCard, 3, 3, false}}, {}, {}, 0, 0, 1, "Bristle", false),
                "Bristle authoritative scenario loads");
            const int id = engine.boardPieces().front().id;
            check(engine.attackPiece(1, id, 3, 3), "authoritative attack against Bristle succeeds");
            const auto& survivors = engine.boardPieces();
            const bool ranged = kind == ActionKind::Ranged;
            if (health == 1 && !ranged)
            {
                const auto snapshot = engine.snapshotFor(1);
                check(survivors.empty() && snapshot.graveyard.size() == 2 &&
                          snapshot.players[0].pieceActionUsedThisTurn && snapshot.relentlessPieceId == 0 &&
                          snapshot.status.find("Bristle") != std::string::npos,
                    "lethal Bristle removes both units, records casualties, consumes the action, and prevents Relentless");
            }
            else
            {
                check(survivors.size() == 1 && survivors[0].id == id &&
                          survivors[0].health == health - (ranged ? 0 : 1) &&
                          survivors[0].column == (ranged ? origin : 3),
                    "Bristle preserves capture movement and excludes even adjacent ranged attacks");
            }
        }
    }
}
