#pragma once

#include "../client/client_story.hpp"
#include "../gameserver/game_engine.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bayou::storytest
{

using RoleIds = std::unordered_map<std::string_view, int>;

struct EngineState
{
    std::vector<game_data::Piece> pieces;
    std::array<GameEngine::EnginePlayer, 2> players;
    std::array<std::uint8_t, game_data::BoardSquares> control{};
    std::array<std::uint8_t, game_data::BoardSquares> holes{};
    std::vector<game_data::Enchantment> enchantments;
    int currentPlayer = 0;
    int commandingPiece = 0;
    int relentlessPiece = 0;
};

inline EngineState captureEngineState(const GameEngine& engine)
{
    return {
        engine.boardPieces(),
        {engine.playerState(1), engine.playerState(2)},
        engine.boardControl(),
        engine.boardHoles(),
        engine.boardEnchantments(),
        engine.currentPlayer(),
        engine.commandingPiece(),
        engine.relentlessPiece()};
}

enum class ClaimKind : std::uint8_t
{
    BasicMove,
    BasicAttack,
    Deployment,
    HealingAction,
    StatusApplied,
    StatusAtTurnBoundary,
    DamageAndAdvance,
    DamageAndPull,
    HealingAuraTurn,
    StartTurnEconomy,
    HiddenNoControl,
    PassThroughAction,
    PushWithoutDamage,
    CooldownApplied,
    CooldownSkippedActivation,
    RepeatPending,
    RepeatFinished,
    SurvivingTargetStagesAttacker,
    LethalTargetCannotBePulled,
    RebirthReplacement,
    RebirthReplacementReady,
    InterceptRedirect,
    PassiveReveal,
    CommandPending,
    CommandedSummon,
    TrailSpawn,
    ForesightDraw,
    ForesightChoose,
    DiscardToBottom,
    InfestMarked,
    InfestPersists,
    InfestReplacement,
    TemporaryControl,
    ControlRestored,
    RelentlessPending,
    RelentlessCleared,
    HiddenCollision
};

struct StepClaimSpec
{
    std::string_view missionId;
    std::size_t stepIndex = 0;
    std::string_view heading;
    ClaimKind kind = ClaimKind::Deployment;
    std::string_view role;
    int expectedValue = 0;
    std::array<std::string_view, 6> masteryRules{};
};

// Each row is anchored to all three authored identifiers. A changed mission,
// reordered step, or renamed heading is therefore a release-gate failure until
// the claimed mechanic and its assertion are reviewed together.
inline constexpr StepClaimSpec StepClaims[] = {
    {"mw01_river_teeth", 0, "GET REED CLEAR - MOVE 1 OF 2",
        ClaimKind::BasicMove, {}, 0, {"normal-activation", "omni-movement"}},
    {"mw01_river_teeth", 2, "THE GATOR LUNGES", ClaimKind::BasicMove},
    {"mw01_river_teeth", 4, "GET REED CLEAR - MOVE 2 OF 2", ClaimKind::BasicMove},
    {"mw01_river_teeth", 7, "DONELLA - SPARK", ClaimKind::BasicAttack,
        "center_gator", 0, {"health-and-destruction"}},
    {"mw01_river_teeth", 10, "EREVAN - SHADOW BLADE", ClaimKind::BasicAttack,
        "knife_gator", 0, {"attacking-movement"}},
    {"mw01_river_teeth", 13, "TELOS - TRAVEL", ClaimKind::BasicMove},
    {"mw01_river_teeth", 16, "REED - BOW", ClaimKind::BasicAttack,
        "cargo_gator", 0, {"ranged-line-of-sight"}},

    {"bt01_harness_hunger", 3, "HEAL THE WORKER", ClaimKind::HealingAction,
        "worker", 0, {"friendly-heal", "maximum-health"}},
    {"bt01_harness_hunger", 7, "DISABLE WITHOUT DAMAGE", ClaimKind::StatusApplied,
        "north_gator", 2, {"zero-damage-disable"}},
    {"bt01_harness_hunger", 8, "MEASURE THE FIRST SKIPPED TURN",
        ClaimKind::StatusAtTurnBoundary, "north_gator", 1},
    {"bt01_harness_hunger", 10, "ONE MEASURED HIT", ClaimKind::StatusApplied,
        "center_gator", 1, {"positive-damage-disable"}},

    {"mw03_town_under_company", 0, "DEPLOY ON CONTROLLED GROUND",
        ClaimKind::Deployment, "informant", 0,
        {"deployment-cost", "arrival-exhaustion", "card-play-plus-activation"}},
    {"mw03_town_under_company", 1, "SPEARMAN - HOOKED SPEAR",
        ClaimKind::DamageAndPull, "notice_guard", 0, {"pull-surviving-target"}},
    {"mw03_town_under_company", 2, "JONI'S AURA",
        ClaimKind::HealingAuraTurn, {}, 0, {"healing-aura", "maximum-health"}},
    {"mw03_town_under_company", 3, "START-TURN ECONOMY",
        ClaimKind::StartTurnEconomy, {}, 1, {"tax"}},
    {"mw03_town_under_company", 4, "INFORMANT - LUNGE",
        ClaimKind::DamageAndAdvance, "tax_guard"},
    {"mw03_town_under_company", 7, "SPEARMAN - SPEAR THRUST",
        ClaimKind::DamageAndAdvance, "notice_guard"},

    {"mw04_watcher_protects", 0, "JUNIPER'S RANGE", ClaimKind::BasicAttack,
        "far_guard", 0, {"orthogonal-ranged-attack", "range-two"}},
    {"mw04_watcher_protects", 3, "EREVAN AT CLOSE RANGE", ClaimKind::BasicAttack,
        "near_guard", 0, {"one-activation-per-turn"}},

    {"bt03_sanctuary_debt", 0, "DEPLOY THE ALCHEMIST", ClaimKind::Deployment,
        "alchemist", 0, {"deployment-cost", "arrival-exhaustion"}},
    {"bt03_sanctuary_debt", 1, "FOREMAN - SLIDE", ClaimKind::BasicMove,
        {}, 0, {"orthogonal-movement"}},
    {"bt03_sanctuary_debt", 4, "THAERON - COMMAND", ClaimKind::CommandPending,
        {}, 0, {"command"}},
    {"bt03_sanctuary_debt", 5, "COMMAND THE FOREMAN", ClaimKind::CommandedSummon,
        {}, 0, {"summon", "summon-front-square",
            "summoned-unit-arrives-exhausted",
            "command-preserves-normal-activation"}},
    {"bt03_sanctuary_debt", 6, "GROVE SISTER - GLIDE AND TRAIL",
        ClaimKind::TrailSpawn, {}, 0, {"trail"}},
    {"bt03_sanctuary_debt", 8, "GATHER AND TAX", ClaimKind::StartTurnEconomy,
        {}, 1, {"gather", "tax", "controlled-income"}},
    {"bt03_sanctuary_debt", 9, "ALCHEMIST - HEALING ELIXER",
        ClaimKind::HealingAction, "lumberjack", 0, {"friendly-heal"}},
    {"bt03_sanctuary_debt", 12, "SAPLING - HEAL",
        ClaimKind::HealingAction, "sapling_patient", 0,
        {"friendly-heal", "maximum-health"}},
    {"bt03_sanctuary_debt", 15, "SAPLING - ENTANGLE",
        ClaimKind::StatusApplied, "sapling_target", 2,
        {"zero-damage-disable", "disable-two-turns"}},
    {"bt03_sanctuary_debt", 15, "SAPLING - ENTANGLE",
        ClaimKind::CooldownApplied, "sapling_target", 0, {"cooldown"}},
    {"bt03_sanctuary_debt", 17, "SAPLING COOLDOWN CONSUMED",
        ClaimKind::CooldownSkippedActivation, "sapling"},
    {"bt03_sanctuary_debt", 17, "SAPLING COOLDOWN CONSUMED",
        ClaimKind::StatusAtTurnBoundary, "sapling_target", 1},
    {"bt03_sanctuary_debt", 19, "DEPENDENT PROFILE PROOF COMPLETE",
        ClaimKind::StatusAtTurnBoundary, "sapling_target", 0,
        {"status-duration"}},
    {"bt03_sanctuary_debt", 20, "THAERON - KNIFE STAB",
        ClaimKind::BasicAttack, "thaeron_target", 0, {"basic-attack"}},

    {"mw05_watched_office", 0, "DEMATERIALIZE EREVAN",
        ClaimKind::HiddenNoControl, {}, 0, {"hidden-adds-no-control"}},
    {"mw05_watched_office", 3, "PASS THROUGH THE SCREEN",
        ClaimKind::PassThroughAction, {}, 0, {"pass-through"}},
    {"mw05_watched_office", 6, "HIDDEN SHOVE",
        ClaimKind::PushWithoutDamage, "ledger_guard", 0, {"push"}},

    {"mw06_cost_seen", 0, "BIRDIE - FORESIGHT", ClaimKind::ForesightDraw,
        {}, 0, {"foresight", "draw-cost"}},
    {"mw06_cost_seen", 1, "CHOOSE WITH FORESIGHT", ClaimKind::ForesightChoose},
    {"mw06_cost_seen", 2, "BIRDIE - LONGBOW SHOT", ClaimKind::CooldownApplied,
        {}, 0, {"cooldown"}},
    {"mw06_cost_seen", 4, "COMPANY TURN ENDS",
        ClaimKind::CooldownSkippedActivation, "birdie"},
    {"mw06_cost_seen", 5, "SCOOTER - RIVER DASH", ClaimKind::RepeatPending,
        {}, 0, {"repeat-lock"}},
    {"mw06_cost_seen", 6, "SCOOTER - REQUIRED REPEAT", ClaimKind::RepeatFinished},
    {"mw06_cost_seen", 9, "VETERAN - ADVANCE",
        ClaimKind::SurvivingTargetStagesAttacker, "durable_guard", 0,
        {"attacking-move-survivor-staging"}},
    {"mw06_cost_seen", 12, "SMUGGLER - FIRST BLADE", ClaimKind::RepeatPending},
    {"mw06_cost_seen", 13, "SMUGGLER - SECOND BLADE", ClaimKind::RepeatFinished},
    {"mw06_cost_seen", 16, "SPEARMAN - HOOKED SPEAR",
        ClaimKind::LethalTargetCannotBePulled, "spear_guard", 0,
        {"pull-requires-surviving-target"}},
    {"mw06_cost_seen", 19, "TRACKER - FROGBACK LEAP",
        ClaimKind::DamageAndAdvance, "durable_guard"},
    {"mw06_cost_seen", 19, "TRACKER - FROGBACK LEAP",
        ClaimKind::PassThroughAction, {}, 0, {"pass-through"}},
    {"mw06_cost_seen", 21, "BITE TRIGGERS REBIRTH",
        ClaimKind::RebirthReplacement, "tracker", 0,
        {"rebirth", "normal-enemy-attack"}},
    {"mw06_cost_seen", 22, "UNMOUNTED TRACKER READIES",
        ClaimKind::RebirthReplacementReady, "tracker"},
    {"mw06_cost_seen", 23, "UNMOUNTED TRACKER - SPEAR",
        ClaimKind::BasicAttack, "rebirth_gator", 0, {"dependent-rebirth-action"}},
    {"mw06_cost_seen", 26, "SCOOTER - RIVER RUSH",
        ClaimKind::DamageAndAdvance, "rush_target"},
    {"mw06_cost_seen", 29, "SMUGGLER - STEP", ClaimKind::BasicMove},
    {"mw06_cost_seen", 32, "BIRDIE - SCOUT", ClaimKind::BasicMove},

    {"mw10_beautiful_plan", 0, "VANYA - BLADE DANCE", ClaimKind::RepeatPending,
        {}, 0, {"repeat-lock"}},
    {"mw10_beautiful_plan", 1, "FINISH THE DANCE", ClaimKind::RepeatFinished},
    {"mw10_beautiful_plan", 4, "VANYA - SIDESTEP", ClaimKind::BasicMove,
        {}, 0, {"orthogonal-movement"}},

    {"mw11_no_plan_saves_all", 8, "INTERCEPT TRIGGERS AUTOMATICALLY",
        ClaimKind::InterceptRedirect, "juniper", 0,
        {"intercept-automatic", "intercept-adjacent-ally",
         "intercept-position-swap", "intercept-receives-damage"}},

    {"mw13_making_credit", 0, "AMBUSHER - DEMATERIALIZE",
        ClaimKind::HiddenNoControl},
    {"mw13_making_credit", 3, "END TURN TO TRIGGER REVEAL",
        ClaimKind::PassiveReveal, "hidden_ambusher", 0,
        {"passive-reveal-at-owner-end-turn", "reveal-adjacent-enemy",
         "reveal-materializes-without-damage-or-disable"}},

    {"mw15_title_follows_burden", 0, "DEPLOY ON CONTROLLED GROUND",
        ClaimKind::Deployment, "reinforcement", 0,
        {"deployment-cost", "arrival-exhaustion", "card-play-plus-activation"}},
    {"mw15_title_follows_burden", 1, "VANYA - BLADE DANCE",
        ClaimKind::RepeatPending, {}, 0, {"repeat-lock"}},
    {"mw15_title_follows_burden", 2, "VANYA - COMPLETE THE REPEAT",
        ClaimKind::RepeatFinished},
    {"mw15_title_follows_burden", 5, "BIRDIE - LONGBOW SHOT",
        ClaimKind::CooldownApplied},
    {"mw15_title_follows_burden", 8, "SPEARMAN - HOOKED SPEAR",
        ClaimKind::LethalTargetCannotBePulled, "spear_guard"},
    {"mw15_title_follows_burden", 17, "DONELLA - SPRINT",
        ClaimKind::BasicMove, {}, 0, {"diagonal-movement"}},
    {"mw15_title_follows_burden", 20, "DONELLA - MARSHLIGHT MEND",
        ClaimKind::HealingAction, "mend_target", 0, {"friendly-heal", "maximum-health"}},

    {"bt02_customs_bell", 0, "VICTOR - FIRST AXE DANCE",
        ClaimKind::RelentlessPending, {}, 0,
        {"relentless-optional-extra-action", "relentless-action-lock"}},
    {"bt02_customs_bell", 1, "VICTOR - OPTIONAL RELENTLESS ACTION",
        ClaimKind::RelentlessPending},
    {"bt02_customs_bell", 2, "DECLINE THE NEXT RELENTLESS ACTION",
        ClaimKind::RelentlessCleared},
    {"bt02_customs_bell", 12, "TAX TRANSFERS, NEVER CREATES DEBT",
        ClaimKind::StartTurnEconomy, {}, 1, {"tax"}},
    {"bt02_customs_bell", 19, "SHARPSHOOTER - ADVANCE",
        ClaimKind::BasicMove, {}, 0, {"state-specific-actions"}},
    {"bt04_terms_conditions", 0, "DEMATERIALIZE THE FIRST AMBUSHER",
        ClaimKind::HiddenNoControl, {}, 0, {"hidden-adds-no-control"}},
    {"bt04_terms_conditions", 4, "SNEAK AROUND VISIBLE UNITS",
        ClaimKind::PassThroughAction, {}, 0, {"pass-through"}},
    {"bt04_terms_conditions", 7, "DEMATERIALIZE THE SECOND AMBUSHER",
        ClaimKind::HiddenNoControl},
    {"bt04_terms_conditions", 10, "CHOOSE AMBUSH BEFORE THE HIDDEN COLLISION",
        ClaimKind::HiddenCollision, "erevan"},
    {"bt04_terms_conditions", 13, "AMBUSHER - VISIBLE STAB",
        ClaimKind::BasicAttack, "stab_target", 0, {"visible-state-action"}},

    {"bt05_freight_office", 0, "GRASK - SWING AXES",
        ClaimKind::SurvivingTargetStagesAttacker, "capture_target", 0,
        {"capture", "capture-staging"}},
    {"bt05_freight_office", 3, "MOG - AXE SWING", ClaimKind::BasicAttack,
        "mog_target", 0, {"health-and-destruction"}},
    {"bt05_freight_office", 6, "GRASK - CHARGE", ClaimKind::BasicAttack,
        "charge_target", 0, {"long-orthogonal-attacking-movement"}},

    {"bt15_monster_rules", 0, "ASHENFANG - ENTANGLING LUNGE",
        ClaimKind::StatusApplied, "veteran", 2, {"disable-two-turns"}},
    {"bt15_monster_rules", 1, "DISABLE: FIRST OWNER TURN",
        ClaimKind::StatusAtTurnBoundary, "veteran", 1},
    {"bt15_monster_rules", 2, "FIRST ACTIVATION MISSED",
        ClaimKind::StatusAtTurnBoundary, "veteran", 1},
    {"bt15_monster_rules", 3, "DISABLE: SECOND OWNER TURN",
        ClaimKind::StatusAtTurnBoundary, "veteran", 0},
    {"bt15_monster_rules", 4, "SECOND ACTIVATION MISSED",
        ClaimKind::StatusAtTurnBoundary, "veteran", 0, {"status-duration"}},

    {"bt17_natural_order", 0, "VICTOR - FIRST AXE DANCE",
        ClaimKind::RelentlessPending, {}, 0,
        {"relentless-optional-extra-action", "relentless-action-lock"}},
    {"bt17_natural_order", 1, "VICTOR - OPTIONAL RELENTLESS ACTION",
        ClaimKind::RelentlessPending},
    {"bt17_natural_order", 2, "RELENTLESS LOCKS", ClaimKind::RelentlessCleared},
    {"bt17_natural_order", 4, "COMMAND", ClaimKind::CommandPending,
        {}, 0, {"command", "command-preserves-normal-activation"}},
    {"bt17_natural_order", 5, "COMMAND THE SUMMON", ClaimKind::CommandedSummon,
        {}, 0, {"summon-front-square", "summoned-unit-arrives-exhausted"}},
    {"bt17_natural_order", 6, "GRASK - SWING AXES",
        ClaimKind::SurvivingTargetStagesAttacker, "cage_guard", 0,
        {"capture", "capture-staging"}},
    {"bt17_natural_order", 9, "GROVE SISTER - GLIDE AND TRAIL",
        ClaimKind::TrailSpawn, {}, 0, {"trail"}},
    {"bt17_natural_order", 12, "ALCHEMIST - HEALING ELIXER",
        ClaimKind::HealingAction, "mog", 0, {"friendly-heal"}},
    {"bt17_natural_order", 14, "LUMBERJACK GATHERS",
        ClaimKind::StartTurnEconomy, {}, 1, {"gather"}},
    {"bt17_natural_order", 15, "THAERON - KNIFE STAB",
        ClaimKind::BasicAttack, "knife_target"},
    {"bt17_natural_order", 18, "FOREMAN - SLIDE", ClaimKind::BasicMove},
    {"bt17_natural_order", 21, "GRASK - CHARGE",
        ClaimKind::SurvivingTargetStagesAttacker, "charge_target", 0,
        {"long-orthogonal-attacking-movement"}},
    {"bt17_natural_order", 24, "ALCHEMIST - PARALYSIS POTION",
        ClaimKind::StatusApplied, "paralysis_target", 2,
        {"zero-damage-disable", "disable-two-turns"}},
    {"bt17_natural_order", 25, "PARALYSIS: FIRST OWNER TURN",
        ClaimKind::StatusAtTurnBoundary, "paralysis_target", 1},
    {"bt17_natural_order", 26, "FIRST PARALYZED ACTIVATION MISSED",
        ClaimKind::StatusAtTurnBoundary, "paralysis_target", 1},
    {"bt17_natural_order", 27, "LUMBERJACK - AXE SWING",
        ClaimKind::BasicAttack, "worker_target"},
    {"bt17_natural_order", 28, "PARALYSIS: SECOND OWNER TURN",
        ClaimKind::StatusAtTurnBoundary, "paralysis_target", 0},
    {"bt17_natural_order", 29, "SECOND PARALYZED ACTIVATION MISSED",
        ClaimKind::StatusAtTurnBoundary, "paralysis_target", 0,
        {"status-duration"}},
    {"bt17_natural_order", 29, "SECOND PARALYZED ACTIVATION MISSED",
        ClaimKind::StartTurnEconomy, {}, 1},
    {"bt17_natural_order", 30, "SAPLING - HEAL",
        ClaimKind::HealingAction, "sapling_heal_target", 0,
        {"friendly-heal", "maximum-health"}},
    {"bt17_natural_order", 33, "SAPLING - ENTANGLE",
        ClaimKind::StatusApplied, "sapling_disable_target", 2,
        {"zero-damage-disable", "disable-two-turns"}},
    {"bt17_natural_order", 33, "SAPLING - ENTANGLE",
        ClaimKind::CooldownApplied, "sapling_disable_target", 0, {"cooldown"}},
    {"bt17_natural_order", 34, "ENTANGLE: FIRST OWNER TURN",
        ClaimKind::StatusAtTurnBoundary, "sapling_disable_target", 1},
    {"bt17_natural_order", 35, "SAPLING COOLDOWN CONSUMED",
        ClaimKind::CooldownSkippedActivation, "sapling"},
    {"bt17_natural_order", 35, "SAPLING COOLDOWN CONSUMED",
        ClaimKind::StatusAtTurnBoundary, "sapling_disable_target", 1},
    {"bt17_natural_order", 36, "ENTANGLE: SECOND OWNER TURN",
        ClaimKind::StatusAtTurnBoundary, "sapling_disable_target", 0},
    {"bt17_natural_order", 37, "DEPENDENT PROFILE PROOF COMPLETE",
        ClaimKind::StatusAtTurnBoundary, "sapling_disable_target", 0,
        {"status-duration"}},
    {"bt17_natural_order", 38, "ASHENFANG - ENTANGLING LUNGE",
        ClaimKind::StatusApplied, "ashenfang_target", 2,
        {"positive-damage-disable", "disable-two-turns"}},
    {"bt17_natural_order", 39, "ASHENFANG DISABLE BEGINS",
        ClaimKind::StatusAtTurnBoundary, "ashenfang_target", 1},
    {"bt17_natural_order", 40, "ASHENFANG TARGET MISSES ACTIVATION",
        ClaimKind::StatusAtTurnBoundary, "ashenfang_target", 1},
    {"bt17_natural_order", 41, "DEBT COLLECTOR - KNIFE STAB",
        ClaimKind::BasicAttack, "collector_target"},
    {"bt17_natural_order", 42, "ASHENFANG DISABLE COMPLETES",
        ClaimKind::StatusAtTurnBoundary, "ashenfang_target", 0},
    {"bt17_natural_order", 43, "DEBT COLLECTOR TAXES",
        ClaimKind::StatusAtTurnBoundary, "ashenfang_target", 0,
        {"status-duration"}},
    {"bt17_natural_order", 43, "DEBT COLLECTOR TAXES",
        ClaimKind::StartTurnEconomy, {}, 1, {"tax"}},
    {"bt17_natural_order", 44, "MOG - AXE SWING",
        ClaimKind::BasicAttack, "mog_target"},
    {"bt17_natural_order", 47, "BRAUN - CHARGE",
        ClaimKind::SurvivingTargetStagesAttacker, "braun_target"},
    {"bt17_natural_order", 50, "SHARPSHOOTER - ADVANCE", ClaimKind::BasicMove},
    {"bt17_natural_order", 56, "SHARPSHOOTER - FIRE",
        ClaimKind::BasicAttack, "sharpshooter_target", 0, {"line-of-sight"}},
    {"bt17_natural_order", 62, "AMBUSHER - VISIBLE STAB",
        ClaimKind::BasicAttack, "ambusher_stab_target", 0, {"visible-state-action"}},
    {"bt17_natural_order", 65, "AMBUSHER - DEMATERIALIZE",
        ClaimKind::HiddenNoControl, {}, 0, {"dematerialize", "hidden-adds-no-control"}},
    {"bt17_natural_order", 68, "AMBUSHER - SNEAK AROUND",
        ClaimKind::PassThroughAction, {}, 0, {"pass-through"}},
    {"bt17_natural_order", 71, "AMBUSHER - HIDDEN AMBUSH",
        ClaimKind::BasicAttack, "collector_target", 0, {"state-specific-actions"}},

    {"se01_cathedral_last_lights", 0, "MESSENGER - COURIER FLIGHT",
        ClaimKind::PassThroughAction, {}, 0, {"pass-through"}},
    {"se01_cathedral_last_lights", 3, "MESSENGER - STAR DART",
        ClaimKind::BasicAttack, "fairy_dart", 0, {"line-of-sight"}},
    {"se02_broken_bridge", 1, "HEALING AURA", ClaimKind::HealingAuraTurn,
        {}, 0, {"healing-aura", "maximum-health"}},
    {"se02_broken_bridge", 2, "BRISTLEJACK STRIKES",
        ClaimKind::InterceptRedirect, "knight", 1, {"bodyguard", "damage-redirection"}},
    {"se02_broken_bridge", 3, "BODYGUARD REDIRECTS DAMAGE",
        ClaimKind::StatusAtTurnBoundary, "knight", 0},
    {"se02_broken_bridge", 5, "THE FORMATION HOLDS",
        ClaimKind::RebirthReplacementReady, "knight", 1},

    {"se03_road_small_lights", 0, "NETTLE - FLY", ClaimKind::PassThroughAction,
        {}, 0, {"flying-pass-through"}},
    {"se03_road_small_lights", 5, "MOUNTED REBIRTH",
        ClaimKind::RebirthReplacement, "nettle", 0, {"mounted", "rebirth"}},
    {"se03_road_small_lights", 6, "THE REPLACEMENT WAITS",
        ClaimKind::RebirthReplacementReady, "nettle", 0,
        {"replacement-acts-next-turn"}},
    {"se03_road_small_lights", 10, "GRIFFIN - FLYING ATTACK",
        ClaimKind::SurvivingTargetStagesAttacker, "widow_griffin"},
    {"se03_road_small_lights", 13, "VESPER - ROYAL FLIGHT",
        ClaimKind::PassThroughAction},

    {"se04_reading_room", 0, "PLAY MOSSWAKE", ClaimKind::Deployment,
        "mosswake", 0, {"deployment-cost", "arrival-exhaustion"}},
    {"se04_reading_room", 1, "DRAW A CARD", ClaimKind::ForesightDraw,
        {}, 0, {"foresight", "draw-cost"}},
    {"se04_reading_room", 2, "CHOOSE WITH FORESIGHT", ClaimKind::ForesightChoose},
    {"se04_reading_room", 3, "DISCARD ONE CARD", ClaimKind::DiscardToBottom,
        {}, 0, {"discard-to-bottom"}},
    {"se04_reading_room", 6, "MOSSWAKE - ROOTWALK",
        ClaimKind::DamageAndAdvance, "root_target"},

    {"se07a_queens_price_scene", 0, "NYXARA - NYXARAN INFEST",
        ClaimKind::InfestMarked, "infest_target", 0,
        {"infest-marks-a-surviving-nonhero"}},
    {"se07a_queens_price_scene", 1, "INFEST WAITS FOR DEATH",
        ClaimKind::InfestPersists, "infest_target", 0,
        {"infest-persists-until-target-death"}},
    {"se07a_queens_price_scene", 2, "THE MARKED WARDEN HOLDS",
        ClaimKind::InfestPersists, "infest_target"},
    {"se07a_queens_price_scene", 3, "FAIRY - SHOOT SPARK",
        ClaimKind::InfestReplacement, "infest_target", 0,
        {"infest-spawns-allied-unit-on-death", "infestation-replacement-arrives-acted"}},
    {"se07a_queens_price_scene", 6, "NYXARA - ROYAL ADVANCE",
        ClaimKind::BasicMove, {}, 0, {"orthogonal-movement"}},

    {"se07_queens_price", 0, "PAVO - ENCHANTING PIPES",
        ClaimKind::TemporaryControl, "pipes_target", 0,
        {"zero-damage-control", "control-two-turns"}},
    {"se07_queens_price", 3, "LARK - DEMATERIALIZE",
        ClaimKind::HiddenNoControl, {}, 0, {"hidden-adds-no-control"}},
    {"se07_queens_price", 6, "LARK - SNEAK AROUND",
        ClaimKind::PassThroughAction, {}, 0, {"pass-through"}},
    {"se07_queens_price", 7, "CONTROL RETURNS AT TURN START",
        ClaimKind::ControlRestored, "pipes_target", 0,
        {"control-restores-at-original-owner-turn-start"}},

    {"se10_no_crown_holds_light", 0, "CALTHERIEL - HEALING AURA",
        ClaimKind::HealingAuraTurn, {}, 0, {"healing-aura", "maximum-health"}},
    {"se10_no_crown_holds_light", 5, "CALTHERIEL - CHARM",
        ClaimKind::TemporaryControl, "charm_target", 0,
        {"zero-damage-control", "control-two-turns"}},

    {"se13_pearl_and_root", 0, "SYLVARA - SOVEREIGN THORNS",
        ClaimKind::StatusApplied, "thorns_target", 2,
        {"positive-damage-disable", "disable-two-turns"}},
    {"se13_pearl_and_root", 1, "DISABLE: FIRST ACTIVATION",
        ClaimKind::StatusAtTurnBoundary, "thorns_target", 1},
    {"se13_pearl_and_root", 2, "FIRST ACTIVATION MISSED",
        ClaimKind::StatusAtTurnBoundary, "thorns_target", 1},
    {"se13_pearl_and_root", 3, "DISABLE: SECOND ACTIVATION",
        ClaimKind::StatusAtTurnBoundary, "thorns_target", 0},
    {"se13_pearl_and_root", 4, "SECOND ACTIVATION MISSED",
        ClaimKind::StatusAtTurnBoundary, "thorns_target", 0, {"status-duration"}},
    {"se13_pearl_and_root", 5, "DEWBELL - DECREE",
        ClaimKind::StatusApplied, "decree_target", 2,
        {"range-two", "attacking-movement"}},
};

inline constexpr std::size_t ExpectedOutcomeCheckCount = 160;
// Counts are an explicit release gate: additions require a reviewed, executable
// before/after assertion and an intentional mastery-claim budget update here.
inline constexpr std::size_t ExpectedAuthoredMasteryClaimCount = 146;
static_assert(
    std::size(StepClaims) == ExpectedOutcomeCheckCount,
    "A Story outcome check was added or removed; review and update the fail-closed coverage budget.");

struct ClaimCoverage
{
    std::array<unsigned int, std::size(StepClaims)> hits{};
    std::size_t evaluated = 0;
};

struct ClaimResult
{
    bool ok = true;
    std::string error;
};

inline const game_data::Piece* pieceById(const EngineState& state, int id)
{
    const auto found = std::find_if(
        state.pieces.begin(), state.pieces.end(),
        [id](const game_data::Piece& piece) { return piece.id == id; });
    return found == state.pieces.end() ? nullptr : &*found;
}

inline const game_data::Piece* pieceForRole(
    const EngineState& state,
    const RoleIds& roles,
    std::string_view role)
{
    const auto found = roles.find(role);
    return found == roles.end() ? nullptr : pieceById(state, found->second);
}

inline const game_data::ActionProfile* exactAction(
    const game_data::Piece& actor,
    std::string_view name,
    int* actionIndex = nullptr)
{
    const game_data::ActionProfile* match = nullptr;
    for (std::size_t index = 0; index < actor.actions.size(); ++index)
    {
        const game_data::ActionProfile& action = actor.actions[index];
        if (action.state != actor.actionState || action.name != name)
        {
            continue;
        }
        if (match != nullptr)
        {
            return nullptr;
        }
        match = &action;
        if (actionIndex != nullptr)
        {
            *actionIndex = static_cast<int>(index);
        }
    }
    return match;
}

inline int cardCount(
    const std::vector<game_data::GameCard>& cards,
    std::string_view title)
{
    return static_cast<int>(std::count_if(
        cards.begin(), cards.end(),
        [title](const game_data::GameCard& card) { return card.title == title; }));
}

inline ClaimResult failure(const StepClaimSpec& spec, std::string_view reason)
{
    return {
        false,
        std::string(spec.missionId) + " step " +
            std::to_string(spec.stepIndex + 1) + " (" +
            std::string(spec.heading) + "): " + std::string(reason)};
}

inline ClaimResult evaluateClaim(
    const StepClaimSpec& spec,
    const client::StoryScriptAction& step,
    const EngineState& before,
    const EngineState& after,
    const RoleIds& beforeRoles,
    const RoleIds& afterRoles)
{
    const game_data::Piece* actorBefore = step.actorRole.empty()
        ? nullptr
        : pieceForRole(before, beforeRoles, step.actorRole);
    const game_data::Piece* actorAfter = step.actorRole.empty()
        ? nullptr
        : pieceForRole(after, afterRoles, step.actorRole);
    const std::string_view subjectRole = spec.role.empty()
        ? step.targetRole
        : spec.role;
    const game_data::Piece* subjectBefore = subjectRole.empty()
        ? nullptr
        : pieceForRole(before, beforeRoles, subjectRole);
    const game_data::Piece* subjectAfter = subjectRole.empty()
        ? nullptr
        : pieceForRole(after, afterRoles, subjectRole);
    const GameEngine::EnginePlayer& playerBefore =
        before.players[static_cast<std::size_t>(step.owner - 1)];
    const GameEngine::EnginePlayer& playerAfter =
        after.players[static_cast<std::size_t>(step.owner - 1)];

    int actionIndex = -1;
    const game_data::ActionProfile* action = actorBefore == nullptr
        ? nullptr
        : exactAction(*actorBefore, step.expectedActionProfile, &actionIndex);
    const auto requireActorAndAction = [&]() -> ClaimResult {
        if (actorBefore == nullptr || actorAfter == nullptr || action == nullptr)
        {
            return failure(spec, "the exact live actor/action state was not available");
        }
        return {};
    };

    switch (spec.kind)
    {
    case ClaimKind::BasicMove:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        const game_data::ActionResolution resolution = game_data::resolvePieceAction(
            before.pieces,
            before.holes,
            *actorBefore,
            step.targetRow,
            step.targetColumn,
            false,
            actionIndex);
        if (!resolution.legal || !resolution.moves || resolution.attacks ||
            actorAfter->row != step.targetRow || actorAfter->column != step.targetColumn ||
            !actorAfter->hasActed || actorAfter->health != actorBefore->health ||
            after.pieces.size() != before.pieces.size())
        {
            return failure(spec, "ordinary printed movement did not reach the exact empty destination and exhaust its actor");
        }
        return {};
    }
    case ClaimKind::BasicAttack:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || action->heal != 0 || action->control != 0 ||
            action->push != 0 || action->pull || !action->infest.empty() ||
            action->repeat != 0 ||
            game_data::hasKeyword(actorBefore->keywords, "relentless") ||
            !subjectBefore->rebirthTitle.empty() ||
            !subjectBefore->infestationTitle.empty())
        {
            return failure(spec, "basic attack assertion was attached to a special-effect action or target");
        }
        const game_data::ActionResolution resolution = game_data::resolvePieceAction(
            before.pieces,
            before.holes,
            *actorBefore,
            subjectBefore->row,
            subjectBefore->column,
            false,
            actionIndex);
        const int damage = action->damage +
            game_data::pieceEnchantmentDamageBonus(before.enchantments, actorBefore->id);
        const bool lethal = subjectBefore->health <= damage;
        if (!resolution.legal || !resolution.attacks || damage <= 0 || !actorAfter->hasActed)
        {
            return failure(spec, "ordinary printed attack no longer resolves as an exhausting positive-damage action");
        }
        const int expectedActorRow = resolution.moves
            ? (lethal ? subjectBefore->row : resolution.stagingRow)
            : actorBefore->row;
        const int expectedActorColumn = resolution.moves
            ? (lethal ? subjectBefore->column : resolution.stagingColumn)
            : actorBefore->column;
        if (actorAfter->row != expectedActorRow || actorAfter->column != expectedActorColumn)
        {
            return failure(spec, "ordinary attack did not leave its actor on the exact ranged/destination/staging square");
        }
        if (lethal)
        {
            if (subjectAfter != nullptr)
            {
                return failure(spec, "lethal ordinary damage did not remove its target");
            }
        }
        else
        {
            const int expectedDisabled = std::max(
                subjectBefore->disabledTurns,
                game_data::disabledTurnsForDamage(damage, action->statusTurns));
            if (subjectAfter == nullptr ||
                subjectAfter->health != subjectBefore->health - damage ||
                subjectAfter->disabledTurns != expectedDisabled ||
                subjectAfter->sleepTurnsRemaining !=
                    std::max(subjectBefore->sleepTurnsRemaining, 1))
            {
                return failure(spec, "surviving ordinary target did not receive exact Health/Disable/Sleep results");
            }
        }
        return {};
    }
    case ClaimKind::Deployment:
    {
        const auto card = std::find_if(
            playerBefore.hand.begin(), playerBefore.hand.end(),
            [&](const game_data::GameCard& value) { return value.title == step.cardTitle; });
        const game_data::Piece* deployed = pieceForRole(after, afterRoles, spec.role);
        if (card == playerBefore.hand.end() || deployed == nullptr ||
            deployed->owner != step.owner || deployed->name != step.cardTitle ||
            deployed->row != step.targetRow || deployed->column != step.targetColumn ||
            !deployed->hasActed ||
            cardCount(playerAfter.hand, step.cardTitle) !=
                cardCount(playerBefore.hand, step.cardTitle) - 1 ||
            playerAfter.resources != playerBefore.resources - card->cost ||
            playerAfter.pieceActionUsedThisTurn != playerBefore.pieceActionUsedThisTurn ||
            after.pieces.size() != before.pieces.size() + 1)
        {
            return failure(
                spec,
                "deployment did not spend the exact card/cost, create the authored exhausted unit, and preserve the normal piece action");
        }
        return {};
    }
    case ClaimKind::HealingAction:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr || action->heal <= 0 ||
            subjectAfter->health !=
                std::min(subjectBefore->maxHealth, subjectBefore->health + action->heal) ||
            subjectAfter->health > subjectAfter->maxHealth)
        {
            return failure(spec, "printed healing did not restore exactly up to maximum Health");
        }
        return {};
    }
    case ClaimKind::StatusApplied:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr)
        {
            return failure(spec, "status target did not survive for its authored timing check");
        }
        const int expectedHealth = subjectBefore->health - std::max(0, action->damage);
        const int expectedDisabled = std::max(
            subjectBefore->disabledTurns,
            game_data::disabledTurnsForDamage(action->damage, action->statusTurns));
        const int expectedSleep = action->damage > 0
            ? std::max(subjectBefore->sleepTurnsRemaining, 1)
            : subjectBefore->sleepTurnsRemaining;
        if (expectedDisabled != spec.expectedValue ||
            subjectAfter->health != expectedHealth ||
            subjectAfter->disabledTurns != expectedDisabled ||
            subjectAfter->sleepTurnsRemaining != expectedSleep)
        {
            return failure(spec, "damage/Disable/Sleep did not match the selected printed action");
        }
        return {};
    }
    case ClaimKind::StatusAtTurnBoundary:
        if (subjectBefore == nullptr || subjectAfter == nullptr ||
            subjectAfter->disabledTurns != spec.expectedValue || !subjectAfter->hasActed)
        {
            return failure(spec, "the Disabled piece did not skip the authored owner-turn boundary");
        }
        return {};

    case ClaimKind::DamageAndAdvance:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter != nullptr ||
            subjectBefore->health > action->damage || !action->canMove || !action->canAttack ||
            actorAfter->row != subjectBefore->row ||
            actorAfter->column != subjectBefore->column)
        {
            return failure(spec, "lethal attacking movement did not remove the target and occupy its square");
        }
        return {};
    }
    case ClaimKind::DamageAndPull:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr || !action->pull ||
            subjectBefore->health <= action->damage)
        {
            return failure(spec, "Pull claim no longer has a surviving target and printed Pull action");
        }
        std::vector<game_data::Piece> expectedPieces = before.pieces;
        auto expectedTarget = std::find_if(
            expectedPieces.begin(), expectedPieces.end(),
            [&](const game_data::Piece& piece) { return piece.id == subjectBefore->id; });
        game_data::applyActionDamage(*expectedTarget, action->damage, action->statusTurns);
        const game_data::PullResult pulled = game_data::applyActionPull(
            expectedPieces, subjectBefore->id, actorBefore->id);
        expectedTarget = std::find_if(
            expectedPieces.begin(), expectedPieces.end(),
            [&](const game_data::Piece& piece) { return piece.id == subjectBefore->id; });
        if (pulled.movedSquares <= 0 || subjectAfter->row != expectedTarget->row ||
            subjectAfter->column != expectedTarget->column ||
            subjectAfter->health != expectedTarget->health ||
            subjectAfter->disabledTurns != expectedTarget->disabledTurns ||
            actorAfter->row != actorBefore->row || actorAfter->column != actorBefore->column ||
            !game_data::piecesAreAdjacent(*actorAfter, *subjectAfter))
        {
            return failure(spec, "the surviving target was not damaged and pulled to the exact legal square");
        }
        return {};
    }
    case ClaimKind::HealingAuraTurn:
    {
        std::vector<game_data::Piece> expected = before.pieces;
        game_data::applyHealingAuras(expected, step.owner);
        bool healedAny = false;
        for (const game_data::Piece& expectedPiece : expected)
        {
            const game_data::Piece* prior = pieceById(before, expectedPiece.id);
            const game_data::Piece* actual = pieceById(after, expectedPiece.id);
            if (prior == nullptr || actual == nullptr || actual->health != expectedPiece.health ||
                actual->health > actual->maxHealth)
            {
                return failure(spec, "end-turn Healing Aura health totals drifted from the authoritative rule");
            }
            healedAny = healedAny || expectedPiece.health > prior->health;
        }
        if (!healedAny)
        {
            return failure(spec, "the authored Healing Aura beat no longer heals any adjacent piece");
        }
        return {};
    }
    case ClaimKind::StartTurnEconomy:
    {
        const int owner = spec.expectedValue;
        const int opponent = owner == 1 ? 2 : 1;
        if (after.currentPlayer != owner || !before.enchantments.empty() ||
            !after.enchantments.empty())
        {
            return failure(spec, "economy assertion requires the authored unenchanted owner-turn start");
        }
        const int controlledIncome = static_cast<int>(std::count(
            after.control.begin(), after.control.end(), static_cast<std::uint8_t>(owner)));
        int gather = 0;
        int tax = 0;
        for (const game_data::Piece& piece : after.pieces)
        {
            if (piece.owner == owner)
            {
                gather += piece.gatherResources;
                tax += piece.tax;
            }
        }
        const int collectedTax = std::min(
            std::max(0, tax),
            before.players[static_cast<std::size_t>(opponent - 1)].resources);
        const int expectedResources =
            before.players[static_cast<std::size_t>(owner - 1)].resources +
            controlledIncome + gather + collectedTax;
        if (after.players[static_cast<std::size_t>(owner - 1)].resources != expectedResources ||
            after.players[static_cast<std::size_t>(opponent - 1)].resources !=
                before.players[static_cast<std::size_t>(opponent - 1)].resources - collectedTax)
        {
            return failure(spec, "controlled-square income, Gather, or capped Tax changed the wrong resource total");
        }
        return {};
    }
    case ClaimKind::HiddenNoControl:
    {
        if (actorAfter == nullptr || !actorAfter->hidden || actorAfter->actionState == 0 ||
            game_data::pieceExertsControl(*actorAfter) ||
            after.control != game_data::recomputeBoardControl(before.control, after.pieces))
        {
            return failure(spec, "Dematerialize did not hide the piece and remove its control influence");
        }
        return {};
    }
    case ClaimKind::PassThroughAction:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (!action->passThrough || !action->canMove ||
            actorAfter->row != step.targetRow || actorAfter->column != step.targetColumn)
        {
            return failure(spec, "the exact pass-through profile did not reach the authored destination");
        }
        return {};
    }
    case ClaimKind::PushWithoutDamage:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr || action->push <= 0 ||
            action->damage != 0 || subjectAfter->health != subjectBefore->health ||
            subjectAfter->disabledTurns != subjectBefore->disabledTurns ||
            (subjectAfter->row == subjectBefore->row &&
             subjectAfter->column == subjectBefore->column))
        {
            return failure(spec, "zero-damage Push did not move the survivor without changing Health/Disable");
        }
        return {};
    }
    case ClaimKind::CooldownApplied:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (action->cooldownTurns <= 0 ||
            actorAfter->disabledTurns !=
                std::max(actorBefore->disabledTurns, action->cooldownTurns) ||
            !actorAfter->hasActed)
        {
            return failure(spec, "the selected action did not apply its exact cooldown");
        }
        return {};
    }
    case ClaimKind::CooldownSkippedActivation:
        if (subjectBefore == nullptr || subjectAfter == nullptr ||
            subjectBefore->disabledTurns <= 0 || subjectAfter->disabledTurns != 0 ||
            !subjectAfter->hasActed)
        {
            return failure(spec, "cooldown did not consume the piece's next activation");
        }
        return {};
    case ClaimKind::RepeatPending:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (action->repeat <= 0 || actorAfter->repeatActionIndex != actionIndex ||
            actorAfter->repeatActionUses != 0 || actorAfter->hasActed ||
            !playerAfter.pieceActionUsedThisTurn)
        {
            return failure(spec, "the first use did not lock the same printed Repeat action");
        }
        return {};
    }
    case ClaimKind::RepeatFinished:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (actorBefore->repeatActionIndex != actionIndex ||
            actorAfter->repeatActionIndex != -1 || actorAfter->repeatActionUses != 0 ||
            !actorAfter->hasActed)
        {
            return failure(spec, "the required repeated use did not clear the Repeat lock");
        }
        return {};
    }
    case ClaimKind::SurvivingTargetStagesAttacker:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr ||
            subjectBefore->health <= action->damage)
        {
            return failure(spec, "staging claim no longer targets a survivor");
        }
        const game_data::ActionResolution resolution = game_data::resolvePieceAction(
            before.pieces,
            before.holes,
            *actorBefore,
            subjectBefore->row,
            subjectBefore->column,
            false,
            actionIndex);
        if (!resolution.legal || !resolution.moves || !resolution.attacks ||
            actorAfter->row != resolution.stagingRow ||
            actorAfter->column != resolution.stagingColumn ||
            (actorAfter->row == subjectBefore->row &&
             actorAfter->column == subjectBefore->column) ||
            subjectAfter->health != subjectBefore->health - action->damage)
        {
            return failure(spec, "attacking movement did not stop on its exact staging square before a survivor");
        }
        return {};
    }
    case ClaimKind::LethalTargetCannotBePulled:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter != nullptr || !action->pull ||
            subjectBefore->health > action->damage ||
            actorAfter->row != actorBefore->row || actorAfter->column != actorBefore->column)
        {
            return failure(spec, "lethal Hooked Spear did not remove the target without moving it or its ranged attacker");
        }
        return {};
    }
    case ClaimKind::RebirthReplacement:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter != nullptr ||
            subjectBefore->rebirthTitle.empty() || subjectBefore->health > action->damage)
        {
            return failure(spec, "Rebirth claim no longer begins with a lethal hit on a mounted form");
        }
        const auto replacement = std::find_if(
            after.pieces.begin(), after.pieces.end(), [&](const game_data::Piece& piece) {
                return piece.id != subjectBefore->id &&
                    piece.owner == subjectBefore->owner &&
                    piece.name == subjectBefore->rebirthTitle &&
                    piece.row == subjectBefore->row &&
                    piece.column == subjectBefore->column;
            });
        if (replacement == after.pieces.end() || !replacement->hasActed ||
            replacement->health != replacement->maxHealth)
        {
            return failure(spec, "lethal damage did not create the exact exhausted Rebirth form on the same square");
        }
        return {};
    }
    case ClaimKind::RebirthReplacementReady:
        if (subjectAfter == nullptr || after.currentPlayer != subjectAfter->owner ||
            subjectAfter->disabledTurns != 0 || subjectAfter->hasActed)
        {
            return failure(spec, "the surviving/reborn piece did not ready at its next ordinary owner turn");
        }
        return {};

    case ClaimKind::InterceptRedirect:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        const game_data::Piece* protectedBefore = step.targetRole.empty()
            ? nullptr
            : pieceForRole(before, beforeRoles, step.targetRole);
        const game_data::Piece* protectedAfter = step.targetRole.empty()
            ? nullptr
            : pieceForRole(after, afterRoles, step.targetRole);
        if (subjectBefore == nullptr || protectedBefore == nullptr ||
            protectedAfter == nullptr || action->damage <= 0 ||
            protectedAfter->health != protectedBefore->health)
        {
            return failure(spec, "the protected target did not remain undamaged through redirection");
        }
        if (spec.expectedValue == 1)
        {
            if (subjectAfter == nullptr ||
                subjectAfter->health != subjectBefore->health - action->damage ||
                subjectAfter->disabledTurns != game_data::disabledTurnsForDamage(
                    action->damage, action->statusTurns) ||
                subjectAfter->row != subjectBefore->row ||
                subjectAfter->column != subjectBefore->column ||
                protectedAfter->row != protectedBefore->row ||
                protectedAfter->column != protectedBefore->column)
            {
                return failure(spec, "the sole adjacent Bodyguard did not receive the exact damage in place");
            }
            return {};
        }

        if (subjectBefore->health > action->damage || subjectAfter != nullptr ||
            protectedAfter->row != subjectBefore->row ||
            protectedAfter->column != subjectBefore->column ||
            actorAfter->row != protectedBefore->row ||
            actorAfter->column != protectedBefore->column)
        {
            return failure(spec, "Intercept did not swap the ally, receive lethal damage, and leave the cleared target square to the attacker");
        }
        return {};
    }
    case ClaimKind::PassiveReveal:
        if (subjectBefore == nullptr || subjectAfter == nullptr || !subjectBefore->hidden ||
            subjectBefore->actionState == 0 || subjectAfter->hidden ||
            subjectAfter->actionState != 0 || subjectAfter->health != subjectBefore->health ||
            subjectAfter->disabledTurns != subjectBefore->disabledTurns ||
            subjectAfter->sleepTurnsRemaining != subjectBefore->sleepTurnsRemaining)
        {
            return failure(spec, "passive Reveal did not materialize the adjacent enemy without damage or Disable");
        }
        return {};

    case ClaimKind::CommandPending:
        if (actorBefore == nullptr || actorAfter == nullptr ||
            after.commandingPiece != actorBefore->id || !actorAfter->hasActed ||
            playerAfter.pieceActionUsedThisTurn != playerBefore.pieceActionUsedThisTurn)
        {
            return failure(spec, "Command did not nominate its commander while preserving the normal piece action");
        }
        return {};
    case ClaimKind::CommandedSummon:
    {
        if (actorBefore == nullptr || actorAfter == nullptr ||
            before.commandingPiece == 0 || after.commandingPiece != 0 ||
            actorBefore->summonTitle.empty() || !actorAfter->hasActed ||
            playerAfter.pieceActionUsedThisTurn != playerBefore.pieceActionUsedThisTurn)
        {
            return failure(spec, "commanded Summon did not consume only the commanded activation");
        }
        const auto [row, column] = game_data::summonDestination(*actorBefore);
        const auto spawned = std::find_if(
            after.pieces.begin(), after.pieces.end(), [&](const game_data::Piece& piece) {
                return piece.owner == actorBefore->owner &&
                    piece.name == actorBefore->summonTitle && piece.row == row &&
                    piece.column == column && piece.hasActed &&
                    pieceById(before, piece.id) == nullptr;
            });
        if (spawned == after.pieces.end() || after.pieces.size() != before.pieces.size() + 1)
        {
            return failure(spec, "Summon did not create the exact exhausted unit in the printed front square");
        }
        return {};
    }
    case ClaimKind::TrailSpawn:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (!game_data::pieceHasTrailAbility(*actorBefore) ||
            actorAfter->row != step.targetRow || actorAfter->column != step.targetColumn)
        {
            return failure(spec, "Trail actor did not make its authored printed movement");
        }
        const auto trail = std::find_if(
            after.pieces.begin(), after.pieces.end(), [&](const game_data::Piece& piece) {
                return pieceById(before, piece.id) == nullptr &&
                    piece.owner == actorBefore->owner &&
                    piece.name == actorBefore->summonTitle &&
                    piece.row == actorBefore->row && piece.column == actorBefore->column &&
                    piece.hasActed;
            });
        if (trail == after.pieces.end())
        {
            return failure(spec, "Trail did not leave the exact exhausted summoned unit on the origin square");
        }
        return {};
    }
    case ClaimKind::ForesightDraw:
    {
        int foresightPieces = 0;
        for (const game_data::Piece& piece : before.pieces)
        {
            if (piece.owner == step.owner &&
                game_data::hasKeyword(piece.keywords, "foresight"))
            {
                ++foresightPieces;
            }
        }
        const std::size_t expectedChoices = std::min(
            playerBefore.drawPile.size(),
            static_cast<std::size_t>(foresightPieces + 1));
        bool choicesMatch = playerAfter.foresightChoices.size() == expectedChoices;
        for (std::size_t index = 0; choicesMatch && index < expectedChoices; ++index)
        {
            choicesMatch = playerAfter.foresightChoices[index].title ==
                playerBefore.drawPile[playerBefore.drawPile.size() - 1 - index].title;
        }
        if (foresightPieces <= 0 || !choicesMatch ||
            playerAfter.drawPile.size() + expectedChoices != playerBefore.drawPile.size() ||
            playerAfter.hand.size() != playerBefore.hand.size() ||
            playerAfter.resources !=
                playerBefore.resources - game_data::DrawCardResourceCost)
        {
            return failure(spec, "paid Foresight draw did not reveal the exact capped top-card choices");
        }
        return {};
    }
    case ClaimKind::ForesightChoose:
    {
        const auto chosen = std::find_if(
            playerBefore.foresightChoices.begin(), playerBefore.foresightChoices.end(),
            [&](const game_data::GameCard& card) { return card.title == step.cardTitle; });
        if (chosen == playerBefore.foresightChoices.end() ||
            !playerAfter.foresightChoices.empty() ||
            cardCount(playerAfter.hand, step.cardTitle) !=
                cardCount(playerBefore.hand, step.cardTitle) + 1 ||
            playerAfter.drawPile.size() != playerBefore.drawPile.size() +
                playerBefore.foresightChoices.size() - 1 ||
            playerAfter.resources != playerBefore.resources)
        {
            return failure(spec, "Foresight choice did not add exactly the chosen card and return every other choice");
        }
        std::vector<std::string> expectedBottom;
        for (std::size_t index = playerBefore.foresightChoices.size(); index-- > 0;)
        {
            if (&playerBefore.foresightChoices[index] != &*chosen)
            {
                expectedBottom.insert(
                    expectedBottom.begin(), playerBefore.foresightChoices[index].title);
            }
        }
        for (std::size_t index = 0; index < expectedBottom.size(); ++index)
        {
            if (playerAfter.drawPile[index].title != expectedBottom[index])
            {
                return failure(spec, "unchosen Foresight card did not return to the bottom of the draw pile");
            }
        }
        return {};
    }
    case ClaimKind::DiscardToBottom:
        if (cardCount(playerAfter.hand, step.cardTitle) !=
                cardCount(playerBefore.hand, step.cardTitle) - 1 ||
            playerAfter.drawPile.size() != playerBefore.drawPile.size() + 1 ||
            playerAfter.drawPile.empty() ||
            playerAfter.drawPile.front().title != step.cardTitle ||
            playerAfter.discardsThisTurn != playerBefore.discardsThisTurn + 1 ||
            playerAfter.resources != playerBefore.resources ||
            playerAfter.pieceActionUsedThisTurn != playerBefore.pieceActionUsedThisTurn)
        {
            return failure(spec, "discard did not move exactly one card to deck bottom without consuming a piece action");
        }
        return {};

    case ClaimKind::InfestMarked:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr || action->infest.empty() ||
            subjectAfter->health != subjectBefore->health - action->damage ||
            subjectAfter->health <= 0 || subjectAfter->isHero ||
            subjectAfter->infestationTitle != action->infest ||
            subjectAfter->infestationOwner != actorBefore->owner ||
            std::any_of(after.pieces.begin(), after.pieces.end(), [&](const game_data::Piece& piece) {
                return pieceById(before, piece.id) == nullptr && piece.name == action->infest;
            }))
        {
            return failure(spec, "Infest did not mark the surviving non-Hero without spawning early");
        }
        return {};
    }
    case ClaimKind::InfestPersists:
        if (subjectBefore == nullptr || subjectAfter == nullptr ||
            subjectBefore->infestationTitle.empty() ||
            subjectAfter->infestationTitle != subjectBefore->infestationTitle ||
            subjectAfter->infestationOwner != subjectBefore->infestationOwner ||
            subjectAfter->health != subjectBefore->health)
        {
            return failure(spec, "the infestation mark did not persist while its host remained alive");
        }
        return {};
    case ClaimKind::InfestReplacement:
    {
        if (subjectBefore == nullptr || subjectAfter != nullptr ||
            subjectBefore->infestationTitle.empty() || subjectBefore->infestationOwner == 0)
        {
            return failure(spec, "Infest replacement did not begin from a marked destroyed target");
        }
        const auto replacement = std::find_if(
            after.pieces.begin(), after.pieces.end(), [&](const game_data::Piece& piece) {
                return pieceById(before, piece.id) == nullptr &&
                    piece.owner == subjectBefore->infestationOwner &&
                    piece.name == subjectBefore->infestationTitle &&
                    piece.row == subjectBefore->row &&
                    piece.column == subjectBefore->column && piece.hasActed;
            });
        if (replacement == after.pieces.end() || replacement->health != replacement->maxHealth)
        {
            return failure(spec, "destroyed infested target did not become the exact exhausted allied unit");
        }
        return {};
    }
    case ClaimKind::TemporaryControl:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr || action->damage != 0 ||
            action->control <= 0 || subjectAfter->health != subjectBefore->health ||
            subjectAfter->owner != actorBefore->owner ||
            subjectAfter->originalOwner != subjectBefore->owner ||
            subjectAfter->controlTurnsRemaining != action->control)
        {
            return failure(spec, "zero-damage Control did not preserve Health and record exact temporary ownership/duration");
        }
        return {};
    }
    case ClaimKind::ControlRestored:
        if (subjectBefore == nullptr || subjectAfter == nullptr ||
            subjectBefore->originalOwner == 0 || subjectBefore->controlTurnsRemaining != 0 ||
            subjectAfter->owner != subjectBefore->originalOwner ||
            subjectAfter->originalOwner != 0 || subjectAfter->controlTurnsRemaining != 0)
        {
            return failure(spec, "temporarily Controlled unit did not return at its original owner's turn start");
        }
        return {};
    case ClaimKind::RelentlessPending:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (!game_data::hasKeyword(actorBefore->keywords, "relentless") ||
            after.relentlessPiece != actorBefore->id || actorAfter->hasActed ||
            subjectAfter != nullptr)
        {
            return failure(spec, "a lethal Relentless action did not lock an immediate optional action to its attacker");
        }
        return {};
    }
    case ClaimKind::RelentlessCleared:
        if (before.relentlessPiece == 0 || after.relentlessPiece != 0 ||
            after.commandingPiece != 0 || after.currentPlayer == before.currentPlayer)
        {
            return failure(spec, "End Turn did not explicitly decline and clear the pending Relentless action");
        }
        return {};
    case ClaimKind::HiddenCollision:
    {
        const ClaimResult ready = requireActorAndAction();
        if (!ready.ok) return ready;
        if (subjectBefore == nullptr || subjectAfter == nullptr ||
            !actorBefore->hidden || !subjectBefore->hidden ||
            actorAfter->hidden || subjectAfter->hidden ||
            subjectAfter->health != subjectBefore->health - action->damage ||
            subjectAfter->disabledTurns < game_data::HiddenRevealStunTurns ||
            actorAfter->row != actorBefore->row || actorAfter->column != actorBefore->column)
        {
            return failure(spec, "chosen hidden attacking profile did not reveal/stun the collision target and materialize the mover");
        }
        return {};
    }
    default:
        break;
    }

    return failure(spec, "claim evaluator is missing this claim kind");
}

inline ClaimResult verifyStepOutcome(
    const client::StoryMission& mission,
    const client::StoryScriptAction& step,
    std::size_t stepIndex,
    const EngineState& before,
    const EngineState& after,
    const RoleIds& beforeRoles,
    const RoleIds& afterRoles,
    ClaimCoverage& coverage)
{
    for (std::size_t index = 0; index < std::size(StepClaims); ++index)
    {
        const StepClaimSpec& spec = StepClaims[index];
        if (spec.missionId != mission.id || spec.stepIndex != stepIndex)
        {
            continue;
        }
        if (spec.heading != step.heading)
        {
            return failure(spec, "authored heading changed without reviewing its mechanic assertion");
        }
        ++coverage.hits[index];
        ++coverage.evaluated;
        const ClaimResult result = evaluateClaim(
            spec, step, before, after, beforeRoles, afterRoles);
        if (!result.ok)
        {
            return result;
        }
    }
    return {};
}

inline ClaimResult verifyMissionClaimCoverage(
    const client::StoryMission& mission,
    const ClaimCoverage& coverage)
{
    for (std::size_t index = 0; index < std::size(StepClaims); ++index)
    {
        const StepClaimSpec& spec = StepClaims[index];
        if (spec.missionId == mission.id && coverage.hits[index] != 1)
        {
            return failure(
                spec,
                coverage.hits[index] == 0
                    ? "authored outcome assertion was never reached"
                    : "authored outcome assertion was reached more than once");
        }
    }
    return {};
}

inline constexpr std::string_view HighRiskMasteryRules[] = {
    "deployment-cost",
    "arrival-exhaustion",
    "card-play-plus-activation",
    "friendly-heal",
    "maximum-health",
    "healing-aura",
    "tax",
    "gather",
    "controlled-income",
    "pull-surviving-target",
    "push",
    "cooldown",
    "repeat-lock",
    "attacking-move-survivor-staging",
    "pull-requires-surviving-target",
    "pass-through",
    "hidden-adds-no-control",
    "rebirth",
    "dependent-rebirth-action",
    "normal-enemy-attack",
    "mounted",
    "replacement-acts-next-turn",
    "flying-pass-through",
    "intercept-automatic",
    "intercept-adjacent-ally",
    "intercept-position-swap",
    "intercept-receives-damage",
    "passive-reveal-at-owner-end-turn",
    "reveal-adjacent-enemy",
    "reveal-materializes-without-damage-or-disable",
    "command",
    "command-preserves-normal-activation",
    "summon",
    "summon-front-square",
    "summoned-unit-arrives-exhausted",
    "trail",
    "bodyguard",
    "damage-redirection",
    "foresight",
    "draw-cost",
    "discard-to-bottom",
    "infest-marks-a-surviving-nonhero",
    "infest-persists-until-target-death",
    "infest-spawns-allied-unit-on-death",
    "infestation-replacement-arrives-acted",
    "zero-damage-control",
    "control-two-turns",
    "control-restores-at-original-owner-turn-start",
    "positive-damage-disable",
    "zero-damage-disable",
    "disable-two-turns",
    "status-duration",
    "relentless-optional-extra-action",
    "relentless-action-lock",
    "visible-state-action",
};

inline bool specCoversRule(
    const StepClaimSpec& spec,
    std::string_view rule)
{
    return std::find(
        spec.masteryRules.begin(), spec.masteryRules.end(), rule) !=
        spec.masteryRules.end();
}

inline constexpr std::size_t authoredMasteryClaimCount()
{
    std::size_t result = 0;
    for (const StepClaimSpec& spec : StepClaims)
    {
        for (std::string_view rule : spec.masteryRules)
        {
            if (!rule.empty())
            {
                ++result;
            }
        }
    }
    return result;
}

static_assert(
    authoredMasteryClaimCount() == ExpectedAuthoredMasteryClaimCount,
    "A Story mastery-claim mapping was added or removed; review and update the fail-closed coverage budget.");

struct ClaimCatalogResult
{
    bool ok = true;
    std::size_t outcomeChecks = std::size(StepClaims);
    std::size_t masteryClaims = authoredMasteryClaimCount();
    std::string error;
};

inline ClaimCatalogResult validateClaimCatalog()
{
    ClaimCatalogResult result;
    std::vector<const client::StoryMission*> missions;
    for (client::StoryCampaign campaign : {
             client::StoryCampaign::Mirewatch,
             client::StoryCampaign::Blackthorn,
             client::StoryCampaign::Seelie})
    {
        for (const client::StoryMission& mission : client::storyMissions(campaign))
        {
            missions.push_back(&mission);
        }
    }

    for (const client::StoryMission* mission : missions)
    {
        if (!mission->script.empty() &&
            std::none_of(
                std::begin(StepClaims), std::end(StepClaims),
                [&](const StepClaimSpec& spec) { return spec.missionId == mission->id; }))
        {
            result.ok = false;
            result.error = std::string(mission->id) +
                ": scripted mission has no before/after outcome assertion";
            return result;
        }
    }

    for (std::size_t index = 0; index < std::size(StepClaims); ++index)
    {
        const StepClaimSpec& spec = StepClaims[index];
        const auto mission = std::find_if(
            missions.begin(), missions.end(), [&](const client::StoryMission* candidate) {
                return candidate->id == spec.missionId;
            });
        if (mission == missions.end())
        {
            result.ok = false;
            result.error = std::string(spec.missionId) +
                ": mechanic assertion references a missing mission";
            return result;
        }
        if (std::count_if(
                missions.begin(), missions.end(), [&](const client::StoryMission* candidate) {
                    return candidate->id == spec.missionId;
                }) != 1)
        {
            result.ok = false;
            result.error = std::string(spec.missionId) +
                ": mechanic assertion mission id is not globally unique";
            return result;
        }
        if (spec.stepIndex >= (*mission)->script.size() ||
            (*mission)->script[spec.stepIndex].heading != spec.heading)
        {
            result.ok = false;
            result.error = std::string(spec.missionId) + " step " +
                std::to_string(spec.stepIndex + 1) +
                ": assertion index/heading no longer matches the authored script";
            return result;
        }
        for (std::string_view rule : spec.masteryRules)
        {
            if (!rule.empty() &&
                std::find(
                    (*mission)->masteryRules.begin(),
                    (*mission)->masteryRules.end(),
                    rule) == (*mission)->masteryRules.end())
            {
                result.ok = false;
                result.error = std::string(spec.missionId) + ": assertion for " +
                    std::string(rule) + " is not backed by that mission's mastery claims";
                return result;
            }
        }
    }

    for (const client::StoryMission* mission : missions)
    {
        for (std::string_view rule : mission->masteryRules)
        {
            if (std::find(
                    std::begin(HighRiskMasteryRules),
                    std::end(HighRiskMasteryRules),
                    rule) == std::end(HighRiskMasteryRules))
            {
                continue;
            }
            const bool covered = std::any_of(
                std::begin(StepClaims), std::end(StepClaims),
                [&](const StepClaimSpec& spec) {
                    return spec.missionId == mission->id && specCoversRule(spec, rule);
                });
            if (!covered)
            {
                result.ok = false;
                result.error = std::string(mission->id) + ": high-risk mastery claim '" +
                    std::string(rule) + "' has no before/after outcome assertion";
                return result;
            }
        }
    }
    return result;
}

} // namespace bayou::storytest
