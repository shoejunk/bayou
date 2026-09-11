#include "client_story_cards.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace bayou::client
{
namespace
{

using game_data::ActionKind;
using game_data::ActionProfile;
using game_data::GameCard;
using game_data::MovePattern;

std::uint8_t pattern(MovePattern value)
{
    return static_cast<std::uint8_t>(value);
}

std::uint8_t kind(ActionKind value)
{
    return static_cast<std::uint8_t>(value);
}

ActionProfile action(
    std::string name,
    MovePattern movePattern,
    int minimum,
    int maximum,
    bool canMove,
    bool canAttack,
    int damage = 0,
    ActionKind actionKind = ActionKind::Slide)
{
    ActionProfile result;
    result.name = std::move(name);
    result.kind = kind(actionKind);
    result.pattern = pattern(movePattern);
    result.minRange = minimum;
    result.maxRange = maximum;
    result.damage = damage;
    result.canMove = canMove;
    result.canAttack = canAttack;
    return result;
}

std::string assetStem(std::string_view title)
{
    if (title == "Ashenfang") return "Ashenfang";
    if (title == "Grask") return "Grask";
    if (title == "Mog") return "Mog";
    if (title == "Victor Greyshard") return "victorGreyshard";
    if (title == "Fizzlewick Gearwright") return "fizzlewickGearwright";
    if (title == "Nettle Starbright") return "nettleStarbright";
    if (title == "Swamp Tracker") return "swampTracker_mounted";
    if (title == "Swamp Tracker Unmounted") return "swampTracker";
    if (title == "Maggie Mudroot") return "maggieMudroot_mounted";
    if (title == "Maggie Mudroot Unmounted") return "maggieMudroot";

    std::string stem;
    bool capitalize = false;
    for (char character : title)
    {
        if (character == ' ' || character == '-' || character == '\'' || character == '(' ||
            character == ')')
        {
            capitalize = !stem.empty();
            continue;
        }
        if (stem.empty())
        {
            stem.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }
        else if (capitalize)
        {
            stem.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            capitalize = false;
        }
        else
        {
            stem.push_back(character);
        }
    }
    return stem;
}

GameCard baseCard(
    std::string_view title,
    std::string_view type,
    int cost,
    int heroCost,
    int health,
    std::vector<std::string> traits)
{
    GameCard card;
    card.title = std::string(title);
    card.type = std::string(type);
    // Authoritative Hero rows omit cost, and toGameCard therefore retains its
    // schema default of 1. Rebirth forms that explicitly carry cost keep it.
    card.cost = type == "Hero" && cost == 0 ? 1 : cost;
    card.heroCost = heroCost;
    card.health = health;
    card.traits = std::move(traits);
    for (std::string& trait : card.traits)
    {
        std::transform(trait.begin(), trait.end(), trait.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    }
    const std::string stem = assetStem(title);
    card.imagePath = "cards/" + stem + ".png";
    card.tokenPath = "characters/" + stem + ".png";
    std::string animationStem = stem;
    if (title == "Ashenfang") animationStem = "ashenfang";
    if (title == "Grask") animationStem = "grask";
    if (title == "Mog") animationStem = "mog";
    if (title == "Victor Greyshard") animationStem = "victorGreyshard";
    card.walkAnimPath = "animations/" + animationStem + "-walk.png";
    card.attackAnimPath = "animations/" + animationStem + "-attack.png";
    card.damagedAnimPath = "animations/" + animationStem + "-damaged.png";
    card.killedAnimPath = "animations/" + animationStem + "-killed.png";
    card.fidgetAnimPath = "animations/" + animationStem + "-fidget.png";
    const bool hasFidget =
        title == "Ashenfang" || title == "Blackthorn Alchemist" ||
        title == "Blackthorn Debt Collector" || title == "Blackthorn Foreman" ||
        title == "Blackthorn Lumberjack" || title == "Braun Stonefist" ||
        title == "Fizzlewick Gearwright" || title == "Grask" ||
        title == "Pavo Quickstep";
    if (!hasFidget)
    {
        card.fidgetAnimPath.clear();
    }
    if (title == "Blackthorn Foreman")
    {
        card.attackAnimPath = "animations/blackthornForeman-attack01.png";
    }
    if (title == "Juniper Flash")
    {
        card.imagePath = "characters/juniperFlash.png";
    }
    if (title == "Victor Greyshard")
    {
        card.tokenPath = "characters/VictorGreyshard.png";
    }
    card.walkAnimFrames = 24;
    card.idleAnimFrames = 24;
    card.attackAnimFrames = 12;
    card.damagedAnimFrames = 12;
    card.killedAnimFrames = 12;
    card.fidgetAnimFrames = hasFidget ? 12 : 1;
    card.pieceBaseBluePath = "characters/bases/piece-base-blue.png";
    card.pieceBaseRedPath = "characters/bases/piece-base-red.png";
    card.width = 1;
    card.height = 1;
    card.canControl = true;
    return card;
}

void setSummary(GameCard& card)
{
    card.attack = 0;
    card.attackRange = 0;
    card.movePattern = pattern(MovePattern::None);
    card.moveRange = 0;
    bool foundMoveAction = false;
    bool foundAttackAction = false;
    for (const ActionProfile& profile : card.actions)
    {
        if (!foundMoveAction && profile.canMove)
        {
            card.movePattern = profile.pattern;
            card.moveRange = profile.maxRange;
            foundMoveAction = true;
        }
        if (profile.canAttack && (!foundAttackAction || profile.damage > card.attack))
        {
            card.attack = profile.damage;
            card.attackRange = profile.maxRange;
            foundAttackAction = true;
        }
        card.attackingMove = card.attackingMove ||
            (profile.state == 0 && profile.canMove && profile.canAttack);
    }
}

// Offline mirrors of the authoritative production card catalog. Keep every
// gameplay field and player-facing action label synchronized with the card
// service; Story Mode scenarios may change placement and current health only.
GameCard mirewatchCard(std::string_view title)
{
    if (title == "Joni Pumpernickel")
    {
        GameCard card = baseCard(title, "Hero", 0, 20, 1, {"Arcane", "Civilized"});
        card.healingAura = 1;
        card.actions = {action("Walking Stick", MovePattern::Omni, 1, 1, true, true, 1)};
        return card;
    }
    if (title == "Vanya Bluewater")
    {
        GameCard card = baseCard(title, "Hero", 0, 35, 2, {"Civilized", "Honorable"});
        card.actions = {action("Sidestep", MovePattern::Ortho, 1, 1, true, false)};
        ActionProfile dance = action("Blade Dance", MovePattern::Diag, 1, 1, true, true, 1);
        dance.repeat = 1;
        card.actions.push_back(dance);
        return card;
    }
    if (title == "Birdie the Wise")
    {
        GameCard card = baseCard(title, "Hero", 0, 35, 1, {"Honorable", "Wild"});
        card.keywords = {"foresight"};
        card.actions = {action("Scout", MovePattern::Diag, 1, 7, true, false)};
        ActionProfile shot = action(
            "Longbow Shot", MovePattern::Ortho, 1, 3, false, true, 1, ActionKind::Ranged);
        shot.cooldownTurns = 1;
        card.actions.push_back(shot);
        return card;
    }
    if (title == "Donella of the Marsh")
    {
        GameCard card = baseCard(title, "Unit", 40, 0, 3, {"Arcane", "Wild"});
        card.actions = {action("Sprint", MovePattern::Diag, 1, 2, true, false)};
        ActionProfile heal = action(
            "Marshlight Mend", MovePattern::Omni, 1, 1, false, true, 0, ActionKind::Ranged);
        heal.heal = 3;
        card.actions.push_back(heal);
        ActionProfile shot = action(
            "Spark", MovePattern::Ortho, 1, 2, false, true, 1, ActionKind::Ranged);
        card.actions.push_back(shot);
        return card;
    }
    if (title == "Juniper Flash")
    {
        GameCard card = baseCard(title, "Unit", 20, 0, 1, {"Arcane", "Honorable"});
        card.keywords = {"intercept"};
        card.actions = {action(
            "Spark", MovePattern::Ortho, 1, 2, false, true, 1, ActionKind::Ranged)};
        card.actions.push_back(action("Sprint", MovePattern::Ortho, 1, 2, true, false));
        return card;
    }
    if (title == "Scooter")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 2, {"Wild"});
        card.actions = {action("River Rush", MovePattern::Ortho, 1, 2, true, true, 1)};
        ActionProfile dash = action("River Dash", MovePattern::Diag, 1, 4, true, false);
        dash.repeat = 1;
        card.actions.push_back(dash);
        return card;
    }
    if (title == "Erevan the Shadow")
    {
        GameCard card = baseCard(title, "Unit", 20, 0, 2, {"Arcane", "Civilized"});
        card.ability = "dematerialize";
        card.actions = {action("Shadow Blade", MovePattern::Omni, 1, 1, true, true, 1)};
        ActionProfile hiddenMove = action(
            "Fade Through Shadow", MovePattern::Omni, 1, 7, true, false);
        hiddenMove.state = 1;
        hiddenMove.passThrough = true;
        card.actions.push_back(hiddenMove);
        ActionProfile push = action("Hidden Shove", MovePattern::Omni, 1, 1, false, true);
        push.state = 1;
        push.push = 1;
        card.actions.push_back(push);
        return card;
    }
    if (title == "Reed Baelstone")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 4, {"Civilized", "Honorable"});
        card.actions = {action("Step", MovePattern::Omni, 1, 1, true, false)};
        ActionProfile shot = action(
            "Bow", MovePattern::Ortho, 1, 3, false, true, 1, ActionKind::Ranged);
        shot.cooldownTurns = 1;
        card.actions.push_back(shot);
        return card;
    }
    if (title == "Bog Spearman")
    {
        GameCard card = baseCard(title, "Unit", 25, 0, 2, {"Wild"});
        ActionProfile hook = action(
            "Hooked Spear", MovePattern::Diag, 2, 3, false, true, 1, ActionKind::Ranged);
        hook.pull = true;
        card.actions = {hook};
        card.actions.push_back(
            action("Spear Thrust", MovePattern::Diag, 1, 1, true, true, 2));
        return card;
    }
    if (title == "Marshland Veteran")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 3, {"Honorable"});
        card.actions = {action("Advance", MovePattern::Ortho, 1, 3, true, true, 1)};
        return card;
    }
    if (title == "Resistance Smuggler")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 1, {"Civilized"});
        card.actions = {action("Step", MovePattern::Ortho, 1, 1, true, false)};
        ActionProfile route = action(
            "Swashbuckle Blade", MovePattern::Diag, 1, 2, true, true, 1);
        route.repeat = 1;
        card.actions.push_back(route);
        return card;
    }
    if (title == "Mirewatch Informant")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 1, {"Civilized"});
        card.tax = 10;
        card.actions = {action("Lunge", MovePattern::Diag, 1, 2, true, true, 1)};
        return card;
    }
    if (title == "Swamp Tracker" || title == "Swamp Tracker Unmounted")
    {
        const bool mounted = title == "Swamp Tracker";
        GameCard card = baseCard(
            title, "Unit", mounted ? 45 : 20, 0, mounted ? 3 : 1, {"Honorable", "Wild"});
        card.keywords = {"Reveal"};
        if (mounted)
        {
            card.actions = {action(
                "Frogback Leap", MovePattern::Jump, 2, 2, true, true, 1)};
            card.actions.front().passThrough = true;
            card.rebirthTitle = "Swamp Tracker Unmounted";
        }
        else
        {
            card.actions = {action(
                "Tracker's Spear", MovePattern::Omni, 1, 1, true, true, 1)};
        }
        return card;
    }
    return {};
}

GameCard blackthornCard(std::string_view title)
{
    if (title == "Thaeron Baelstone")
    {
        GameCard card = baseCard(title, "Hero", 0, 70, 2, {"Arcane", "Civilized", "Corrupt"});
        card.ability = "command";
        card.actions = {action("Knife Stab", MovePattern::Omni, 1, 1, true, true, 1)};
        return card;
    }
    if (title == "Ashenfang")
    {
        GameCard card = baseCard(title, "Hero", 0, 30, 2, {"Corrupt", "Wild"});
        ActionProfile strike = action(
            "Entangling Lunge", MovePattern::Diag, 1, 2, true, true, 1);
        strike.statusTurns = 2;
        card.actions = {strike};
        return card;
    }
    if (title == "Blackthorn Debt Collector")
    {
        GameCard card = baseCard(title, "Unit", 25, 0, 1, {"Civilized", "Corrupt"});
        card.tax = 5;
        card.actions = {action("Knife Stab", MovePattern::Diag, 1, 1, true, true, 1)};
        return card;
    }
    if (title == "Blackthorn Alchemist")
    {
        GameCard card = baseCard(title, "Unit", 20, 0, 1, {"Arcane", "Civilized"});
        ActionProfile heal = action(
            "Healing Elixer", MovePattern::Omni, 1, 1, true, true);
        heal.heal = 3;
        ActionProfile disable = action(
            "Paralysis Potion", MovePattern::Omni, 1, 1, false, true);
        disable.statusTurns = 2;
        card.actions = {heal, disable};
        return card;
    }
    if (title == "Blackthorn Foreman")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 3, {"Civilized", "Corrupt"});
        card.ability = "summon";
        card.summonTitle = "Blackthorn Lumberjack";
        card.actions = {action("Slide", MovePattern::Ortho, 1, 3, true, true, 1)};
        return card;
    }
    if (title == "Blackthorn Lumberjack")
    {
        GameCard card = baseCard(title, "Unit", 20, 0, 3, {"Civilized", "Corrupt"});
        card.gatherResources = 5;
        card.actions = {action("Axe Swing", MovePattern::Ortho, 1, 1, true, true, 1)};
        return card;
    }
    if (title == "Grove Sister")
    {
        GameCard card = baseCard(title, "Unit", 55, 0, 3, {"Corrupt", "Wild"});
        card.keywords = {"trail"};
        card.summonTitle = "Sapling";
        card.actions = {action("Glide", MovePattern::Diag, 1, 7, true, true, 1)};
        return card;
    }
    if (title == "Sapling")
    {
        GameCard card = baseCard(title, "Unit", 5, 0, 1, {"Wild"});
        card.imagePath = "cards/heartwood.png";
        card.tokenPath = "characters/heartwoodTree.png";
        card.walkAnimPath = "characters/heartwoodTree.png";
        card.walkAnimFrames = 1;
        card.idleAnimPath.clear();
        card.idleAnimFrames = 1;
        card.attackAnimPath.clear();
        card.damagedAnimPath.clear();
        card.killedAnimPath.clear();
        card.fidgetAnimPath.clear();
        card.attackAnimFrames = 1;
        card.damagedAnimFrames = 1;
        card.killedAnimFrames = 1;
        card.fidgetAnimFrames = 1;
        card.pieceBaseBluePath = "characters/bases/piece-base-blue.png";
        card.pieceBaseRedPath = "characters/bases/piece-base-red.png";
        card.keywords = {"plant"};
        ActionProfile heal = action(
            "Heal", MovePattern::Omni, 1, 1, false, true, 0, ActionKind::Ranged);
        heal.heal = 2;
        ActionProfile entangle = action(
            "Entangle", MovePattern::Omni, 1, 1, false, true, 0, ActionKind::Ranged);
        entangle.statusTurns = 2;
        entangle.cooldownTurns = 1;
        card.actions = {heal, entangle};
        return card;
    }
    if (title == "Mog")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 4, {"Corrupt", "Wild"});
        card.actions = {action("Axe Swing", MovePattern::Omni, 1, 1, true, true, 2)};
        return card;
    }
    if (title == "Grask")
    {
        GameCard card = baseCard(title, "Unit", 80, 0, 5, {"Corrupt", "Wild"});
        card.actions = {action("Charge", MovePattern::Ortho, 1, 7, true, true, 2)};
        card.actions.push_back(action(
            "Swing Axes", MovePattern::Diag, 1, 1, true, true, 2, ActionKind::Capture));
        return card;
    }
    if (title == "Goblin Ambusher")
    {
        GameCard card = baseCard(title, "Unit", 70, 0, 2, {"Corrupt"});
        card.ability = "dematerialize";
        card.idleAnimPath = "characters/goblinAmbusher.png";
        card.idleAnimFrames = 1;
        card.actions = {action("Stab", MovePattern::Omni, 1, 1, true, true, 1)};
        ActionProfile hidden = action(
            "Sneak Around", MovePattern::Omni, 1, 7, true, false);
        hidden.state = 1;
        hidden.passThrough = true;
        card.actions.push_back(hidden);
        ActionProfile ambush = action("Ambush", MovePattern::Omni, 1, 1, true, true, 1);
        ambush.state = 1;
        ambush.nextState = 0;
        ambush.passThrough = true;
        card.actions.push_back(ambush);
        return card;
    }
    if (title == "Braun Stonefist")
    {
        GameCard card = baseCard(title, "Unit", 50, 0, 5, {"Civilized", "Corrupt"});
        card.actions = {action("Charge", MovePattern::Ortho, 1, 7, true, true, 2)};
        return card;
    }
    if (title == "Goblin Sharpshooter")
    {
        GameCard card = baseCard(title, "Unit", 45, 0, 1, {"Corrupt"});
        card.ability = "transform";
        card.abilityLabels = {"Raise Gun", "Lower Gun"};
        card.state1TokenPath = "characters/goblinSharpshooter_aim.png";
        ActionProfile move = action("Advance", MovePattern::Ortho, 1, 2, true, false);
        move.state = 0;
        ActionProfile fire = action(
            "Fire", MovePattern::Ortho, 1, 7, false, true, 3, ActionKind::Ranged);
        fire.state = 1;
        fire.nextState = 1;
        fire.lineOfSight = true;
        card.actions = {move, fire};
        return card;
    }
    return {};
}

GameCard seelieStoryCard(std::string_view title)
{
    const auto noIdle = [](GameCard& card) {
        card.idleAnimPath.clear();
        card.idleAnimFrames = card.walkAnimFrames;
    };
    const auto noCombatAnimation = [&](GameCard& card) {
        noIdle(card);
        card.attackAnimPath.clear();
        card.damagedAnimPath.clear();
        card.killedAnimPath.clear();
        card.attackAnimFrames = 1;
        card.damagedAnimFrames = 1;
        card.killedAnimFrames = 1;
    };

    if (title == "Archivist Mosswake")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 1, {"Ancient", "Wild"});
        card.keywords = {"foresight"};
        card.actions = {action("Rootwalk", MovePattern::Ortho, 1, 2, true, true, 1)};
        noIdle(card);
        return card;
    }
    if (title == "Caltheriel")
    {
        GameCard card = baseCard(title, "Hero", 0, 15, 1, {"Fey", "Honorable"});
        card.imagePath = "characters/caltheriel.png";
        card.idleAnimPath = "animations/caltheriel-idle.png";
        card.healingAura = 1;
        card.actions = {action("Fey Flight", MovePattern::Diag, 1, 2, true, false)};
        ActionProfile charm = action(
            "Charm", MovePattern::Ortho, 1, 2, false, true, 0, ActionKind::Ranged);
        charm.control = 2;
        card.actions.push_back(charm);
        return card;
    }
    if (title == "Crystal Unicorn")
    {
        GameCard card = baseCard(title, "Unit", 50, 0, 2, {"Honorable", "Wild"});
        card.tokenPath = "characters/crystallineUnicorn.png";
        card.actions = {action(
            "Prismatic Charge", MovePattern::Ortho, 1, 4, true, true, 2)};
        noIdle(card);
        return card;
    }
    if (title == "Duchess Dewbell")
    {
        GameCard card = baseCard(title, "Unit", 45, 0, 3, {"Ancient"});
        ActionProfile decree = action(
            "Dewbell's Decree", MovePattern::Omni, 1, 2, true, true, 1);
        decree.statusTurns = 2;
        card.actions = {decree};
        noIdle(card);
        return card;
    }
    if (title == "Fey Messenger")
    {
        GameCard card = baseCard(title, "Unit", 25, 0, 1, {"Fey"});
        ActionProfile flight = action(
            "Courier Flight", MovePattern::Omni, 1, 7, true, false);
        flight.passThrough = true;
        ActionProfile dart = action(
            "Star Dart", MovePattern::Omni, 1, 1, false, true, 1, ActionKind::Ranged);
        dart.lineOfSight = true;
        card.actions = {flight, dart};
        noIdle(card);
        return card;
    }
    if (title == "Heartwood Sister")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 1, {"Ancient", "Fey"});
        card.healingAura = 1;
        card.actions = {action(
            "Grovewalk", MovePattern::Diag, 1, 7, true, false)};
        noIdle(card);
        return card;
    }
    if (title == "Nettle Starbright")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 2, {"Wild"});
        card.imagePath = "cards/nettleStarbright_mounted.png";
        card.tokenPath = "characters/nettleStarbright_mounted.png";
        card.walkAnimPath = "animations/nettleStarbright_mounted-walk.png";
        card.idleAnimPath = "animations/nettleStarbright_mounted-idle.png";
        card.attackAnimPath = "animations/nettleStarbright_mounted-attack.png";
        card.damagedAnimPath = "animations/nettleStarbright_mounted-damaged.png";
        card.killedAnimPath = "animations/nettleStarbright_mounted-killed.png";
        card.keywords = {"mounted"};
        card.rebirthTitle = "Nettle Starbright Unmounted";
        ActionProfile flight = action("Fly", MovePattern::Omni, 1, 7, true, false);
        flight.passThrough = true;
        ActionProfile shot = action(
            "Slingshot", MovePattern::Omni, 1, 1, false, true, 1, ActionKind::Ranged);
        shot.lineOfSight = true;
        card.actions = {flight, shot};
        return card;
    }
    if (title == "Nettle Starbright Unmounted")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 1, {"Honorable", "Wild"});
        card.imagePath = "cards/nettleStarbright.png";
        card.tokenPath = "characters/nettleStarbright_unmounted.png";
        card.walkAnimPath = "animations/nettleStarbright_unmounted-walk.png";
        card.idleAnimPath = "characters/nettleStarbright_unmounted.png";
        card.attackAnimPath = "animations/nettleStarbright_unmounted-attack.png";
        card.damagedAnimPath = "animations/nettleStarbright_unmounted-damaged.png";
        card.killedAnimPath = "animations/nettleStarbright_unmounted-killed.png";
        card.idleAnimFrames = 1;
        card.actions = {action("Punch", MovePattern::Omni, 1, 1, true, true, 1)};
        return card;
    }
    if (title == "Pavo Quickstep")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 2, {"Arcane", "Fey"});
        ActionProfile quickstep = action(
            "Quickstep", MovePattern::Jump, 2, 2, true, true, 1);
        quickstep.passThrough = true;
        ActionProfile pipes = action(
            "Enchanting Pipes", MovePattern::Ortho, 1, 2, false, true, 0,
            ActionKind::Ranged);
        pipes.control = 2;
        card.actions = {quickstep, pipes};
        noIdle(card);
        return card;
    }
    if (title == "Prince Vesper")
    {
        GameCard card = baseCard(title, "Hero", 0, 30, 1, {"Arcane", "Honorable"});
        card.idleAnimPath = "animations/princeVesper-idle.png";
        ActionProfile flight = action(
            "Royal Flight", MovePattern::Omni, 1, 3, true, false);
        flight.passThrough = true;
        card.actions = {flight, action(
            "Vesper Spark", MovePattern::Ortho, 1, 2, false, true, 1,
            ActionKind::Ranged)};
        return card;
    }
    if (title == "Quinberry Lark")
    {
        GameCard card = baseCard(title, "Unit", 50, 0, 2, {"Arcane"});
        card.ability = "dematerialize";
        card.idleAnimPath = "characters/quinberryLark.png";
        card.idleAnimFrames = 1;
        card.actions = {action("Swoop", MovePattern::Diag, 1, 7, true, true, 1)};
        ActionProfile sneak = action(
            "Sneak Around", MovePattern::Ortho, 1, 7, true, false);
        sneak.state = 1;
        sneak.nextState = 1;
        sneak.passThrough = true;
        card.actions.push_back(sneak);
        return card;
    }
    if (title == "Queen Nyxara")
    {
        GameCard card = baseCard(
            title, "Hero", 0, 60, 3, {"Ancient", "Corrupt", "Fey"});
        card.actions = {action(
            "Royal Advance", MovePattern::Ortho, 1, 2, true, false)};
        ActionProfile infest = action(
            "Nyxaran Infest", MovePattern::Diag, 1, 2, false, true, 1,
            ActionKind::Ranged);
        infest.infest = "Bristlejack";
        card.actions.push_back(std::move(infest));
        return card;
    }
    if (title == "Starbloom Knight")
    {
        GameCard card = baseCard(title, "Unit", 40, 0, 4, {"Fey", "Honorable"});
        card.keywords = {"bodyguard"};
        card.actions = {action(
            "Starbloom Charge", MovePattern::Ortho, 1, 3, true, true, 1)};
        noIdle(card);
        return card;
    }
    if (title == "Sylvara")
    {
        GameCard card = baseCard(title, "Hero", 0, 55, 2, {"Ancient", "Fey", "Wild"});
        card.imagePath = "cards/Sylvara.png";
        card.summonTitle = "Sapling";
        ActionProfile thorns = action(
            "Sovereign Thorns", MovePattern::Diag, 1, 4, true, true, 1);
        thorns.statusTurns = 2;
        card.actions = {thorns};
        noIdle(card);
        return card;
    }
    if (title == "Thorn Griffin")
    {
        GameCard card = baseCard(title, "Unit", 60, 0, 4, {"Wild"});
        ActionProfile flight = action(
            "Griffin Flight", MovePattern::Ortho, 1, 7, true, true, 1);
        flight.passThrough = true;
        card.actions = {flight};
        noIdle(card);
        return card;
    }
    if (title == "Gloom Fairy")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 1, {"Fey", "Undead"});
        card.walkAnimPath = "animations/GT_animation_gloomFairy_idle.png";
        card.walkAnimFrames = 12;
        card.idleAnimPath = "animations/gloomFairy-idle.png";
        ActionProfile flight = action("Fly", MovePattern::Omni, 1, 7, true, false);
        flight.passThrough = true;
        ActionProfile shot = action(
            "Shoot Spark", MovePattern::Omni, 1, 1, false, true, 1,
            ActionKind::Ranged);
        shot.lineOfSight = true;
        card.actions = {flight, shot};
        return card;
    }
    if (title == "Dusk Harvester")
    {
        GameCard card = baseCard(title, "Unit", 45, 0, 5, {"Corrupt", "Fey", "Undead"});
        card.actions = {
            action("Scythe Advance", MovePattern::Ortho, 1, 3, true, true, 1),
            action("Harvest", MovePattern::Omni, 1, 1, true, true, 1)};
        noIdle(card);
        return card;
    }
    if (title == "Bristlejack")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 1, {"Corrupt"});
        card.actions = {action(
            "Bristle Charge", MovePattern::Omni, 1, 1, true, true, 2)};
        noIdle(card);
        return card;
    }
    if (title == "Widowroot")
    {
        GameCard card = baseCard(title, "Unit", 40, 0, 4, {"Ancient"});
        ActionProfile bind = action(
            "Widow's Bind", MovePattern::Omni, 1, 1, false, true, 0,
            ActionKind::Ranged);
        bind.statusTurns = 2;
        bind.cooldownTurns = 1;
        card.actions = {
            action("Rooted Strike", MovePattern::Ortho, 1, 3, true, true, 1),
            bind};
        noIdle(card);
        return card;
    }
    if (title == "Warden")
    {
        GameCard card = baseCard(title, "Unit", 40, 0, 4, {"Civilized", "Honorable"});
        card.imagePath = "cards/Warden.png";
        card.tokenPath = "characters/Warden.png";
        card.actions = {
            action("RookMoveD1R3", MovePattern::Ortho, 1, 3, true, true, 1),
            action("KingAttack1", MovePattern::Omni, 1, 1, true, true, 1)};
        noIdle(card);
        return card;
    }
    if (title == "Eyeblight")
    {
        GameCard card = baseCard(title, "Unit", 50, 0, 4, {"Corrupt", "Wild"});
        card.keywords = {"plant"};
        card.walkAnimPath = "characters/eyeblight.png";
        card.walkAnimFrames = 1;
        ActionProfile gaze = action(
            "Blighting Gaze", MovePattern::Omni, 1, 1, false, true, 0,
            ActionKind::Ranged);
        gaze.statusTurns = 2;
        gaze.cooldownTurns = 1;
        card.actions = {
            action("Creeping Roots", MovePattern::Diag, 1, 1, true, false), gaze};
        noCombatAnimation(card);
        card.attackAnimPath = "animations/eyeblight-attack.png";
        card.attackAnimFrames = 24;
        return card;
    }
    if (title == "Marrowind")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 2, {"Ancient", "Honorable"});
        card.walkAnimPath = "characters/marrowind.png";
        card.walkAnimFrames = 1;
        card.actions = {
            action("Rooted Advance", MovePattern::Ortho, 1, 3, true, true, 1),
            action("Old Growth", MovePattern::Omni, 1, 1, true, true, 1)};
        noCombatAnimation(card);
        return card;
    }
    if (title == "Sun Sprite")
    {
        GameCard card = baseCard(title, "Unit", 25, 0, 1, {"Arcane", "Fey"});
        card.walkAnimPath = "characters/sunSprite.png";
        card.walkAnimFrames = 1;
        ActionProfile sunrise = action(
            "Sunrise", MovePattern::Omni, 1, 7, true, false);
        sunrise.passThrough = true;
        ActionProfile warmth = action(
            "Warmth", MovePattern::Omni, 1, 1, false, true, 0, ActionKind::Ranged);
        warmth.heal = 2;
        card.actions = {sunrise, warmth};
        noCombatAnimation(card);
        return card;
    }

    return {};
}

GameCard supportingCard(std::string_view title)
{
    if (title == "Briar Whisperthorn")
    {
        GameCard card = baseCard(title, "Unit", 35, 0, 2, {"Fey"});
        ActionProfile thornStrike = action(
            "Thorn Strike", MovePattern::Omni, 1, 1, true, true, 1);
        ActionProfile bindingBriars = action(
            "Binding Briars", MovePattern::Omni, 1, 1, false, true);
        bindingBriars.statusTurns = 2;
        card.actions = {thornStrike, bindingBriars};
        return card;
    }
    if (title == "Maggie's Recruit")
    {
        GameCard card = baseCard(title, "Unit", 25, 0, 2, {"Civilized", "Undead"});
        card.imagePath = "cards/maggiesRecruit.png";
        card.tokenPath = "characters/maggiesRecruit.png";
        card.walkAnimPath = "animations/maggiesRecruit-walk.png";
        card.attackAnimPath = "animations/maggiesRecruit-attack.png";
        card.damagedAnimPath = "animations/maggiesRecruit-damaged.png";
        card.killedAnimPath = "animations/maggiesRecruit-killed.png";
        card.idleAnimPath.clear();
        card.fidgetAnimPath.clear();
        card.walkAnimFrames = 24;
        card.idleAnimFrames = 24;
        card.attackAnimFrames = 12;
        card.damagedAnimFrames = 12;
        card.killedAnimFrames = 12;
        card.fidgetAnimFrames = 1;
        card.pieceBaseBluePath = "characters/bases/piece-base-blue.png";
        card.pieceBaseRedPath = "characters/bases/piece-base-red.png";
        card.actions = {action(
            "Axe Swing", MovePattern::Ortho, 1, 1, true, true, 1)};
        return card;
    }
    if (title == "Bull Gator")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 4, {"Wild"});
        card.actions = {action("Bite", MovePattern::Omni, 1, 1, true, true, 2)};
        return card;
    }
    if (title == "Telos the Merchant")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 2, {"Arcane", "Civilized"});
        card.actions = {action("Travel", MovePattern::Omni, 1, 1, true, false)};
        return card;
    }
    if (title == "Victor Greyshard")
    {
        GameCard card = baseCard(title, "Hero", 0, 40, 3, {"Civilized", "Corrupt"});
        card.keywords = {"relentless"};
        card.actions = {action("Axe Dance", MovePattern::Omni, 1, 2, true, true, 1)};
        return card;
    }
    if (title == "Rowan Leafbound")
    {
        GameCard card = baseCard(title, "Unit", 40, 0, 3, {"Civilized", "Honorable"});
        card.keywords = {"bodyguard"};
        card.actions = {action("Shield Bash", MovePattern::Omni, 1, 1, true, true, 1)};
        card.walkAnimPath = "animations/rowan-walk.png";
        card.attackAnimPath = "animations/rowan-attack.png";
        card.damagedAnimPath = "animations/rowan-damaged.png";
        card.killedAnimPath = "animations/rowan-killed.png";
        card.fidgetAnimPath.clear();
        card.fidgetAnimFrames = 1;
        card.pieceBaseBluePath = "characters/bases/piece-base-blue.png";
        card.pieceBaseRedPath = "characters/bases/piece-base-red.png";
        return card;
    }
    if (title == "Maggie Mudroot")
    {
        GameCard card = baseCard(title, "Hero", 0, 80, 3, {"Arcane", "Undead", "Wild"});
        card.width = 2;
        card.height = 2;
        card.rebirthTitle = "Maggie Mudroot Unmounted";
        card.actions = {action(
            "Gator-House Charge", MovePattern::Ortho, 1, 4, true, true, 2)};
        return card;
    }
    if (title == "Maggie Mudroot Unmounted")
    {
        GameCard card = baseCard(
            title, "Hero", 70, 0, 1, {"Arcane", "Undead", "Wild"});
        ActionProfile marshStep = action(
            "Marsh Step", MovePattern::Omni, 1, 1, true, false);
        ActionProfile mudrootHex = action(
            "Mudroot Hex", MovePattern::Omni, 1, 1, false, true, 0,
            ActionKind::Ranged);
        mudrootHex.statusTurns = 2;
        mudrootHex.cooldownTurns = 1;
        card.actions = {marshStep, mudrootHex};
        return card;
    }
    if (title == "Gearjaw")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 3, {"Mechanical"});
        card.walkAnimPath = "animations/gearJaw-walk.png";
        card.attackAnimPath = "animations/gearJaw-attack.png";
        card.damagedAnimPath = "animations/gearJaw-damaged.png";
        card.killedAnimPath = "animations/gearJaw-killed.png";
        card.actions = {action("Bite", MovePattern::Omni, 1, 1, true, true, 1)};
        ActionProfile rebuild = action(
            "Rebuild", MovePattern::Omni, 1, 1, true, true);
        rebuild.heal = 2;
        rebuild.targetFilter = {"construct"};
        card.actions.push_back(rebuild);
        return card;
    }
    if (title == "Fizzlewick Gearwright")
    {
        GameCard card = baseCard(title, "Unit", 20, 0, 1, {"Corrupt", "Mechanical"});
        card.ability = "summon";
        card.actions = {action("Knife Stab", MovePattern::Omni, 1, 1, true, true, 1)};
        ActionProfile repair = action(
            "Repair", MovePattern::Omni, 1, 1, true, true);
        repair.heal = 2;
        repair.targetFilter = {"construct"};
        card.actions.push_back(repair);
        return card;
    }
    if (title == "Pavo Quickstep")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 2, {"Arcane", "Fey"});
        ActionProfile quickstep = action(
            "Quickstep", MovePattern::Jump, 2, 2, true, true, 1);
        quickstep.passThrough = true;
        ActionProfile pipes = action(
            "Enchanting Pipes", MovePattern::Ortho, 1, 2, false, true, 0,
            ActionKind::Ranged);
        pipes.control = 2;
        card.actions = {quickstep, pipes};
        return card;
    }
    if (title == "Nettle Starbright")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 2, {"Wild"});
        card.keywords = {"mounted"};
        card.rebirthTitle = "Nettle Starbright Unmounted";
        ActionProfile fly = action(
            "Fly", MovePattern::Omni, 1, 7, true, false);
        fly.passThrough = true;
        ActionProfile slingshot = action(
            "Slingshot", MovePattern::Omni, 1, 1, false, true, 1,
            ActionKind::Ranged);
        slingshot.lineOfSight = true;
        card.actions = {fly, slingshot};
        return card;
    }
    if (title == "Sylvara")
    {
        GameCard card = baseCard(title, "Hero", 0, 55, 2, {"Ancient", "Fey", "Wild"});
        card.imagePath = "cards/Sylvara.png";
        card.summonTitle = "Sapling";
        ActionProfile thorns = action(
            "Sovereign Thorns", MovePattern::Diag, 1, 4, true, true, 1);
        thorns.statusTurns = 2;
        card.actions = {thorns};
        return card;
    }

    return {};
}

} // namespace

std::optional<GameCard> packagedStoryCard(std::string_view title)
{
    GameCard card = mirewatchCard(title);
    if (card.title.empty()) card = blackthornCard(title);
    if (card.title.empty()) card = seelieStoryCard(title);
    if (card.title.empty()) card = supportingCard(title);
    if (card.title.empty()) return std::nullopt;
    setSummary(card);
    return card;
}

std::span<const std::string_view> packagedStoryCardTitles()
{
    static constexpr std::array<std::string_view, 60> Titles = {
        "Archivist Mosswake",
        "Ashenfang",
        "Birdie the Wise",
        "Blackthorn Alchemist",
        "Blackthorn Debt Collector",
        "Blackthorn Foreman",
        "Blackthorn Lumberjack",
        "Bog Spearman",
        "Braun Stonefist",
        "Briar Whisperthorn",
        "Bristlejack",
        "Bull Gator",
        "Caltheriel",
        "Crystal Unicorn",
        "Donella of the Marsh",
        "Duchess Dewbell",
        "Dusk Harvester",
        "Erevan the Shadow",
        "Eyeblight",
        "Fey Messenger",
        "Fizzlewick Gearwright",
        "Gearjaw",
        "Gloom Fairy",
        "Goblin Ambusher",
        "Goblin Sharpshooter",
        "Grask",
        "Grove Sister",
        "Heartwood Sister",
        "Joni Pumpernickel",
        "Juniper Flash",
        "Maggie Mudroot",
        "Maggie Mudroot Unmounted",
        "Maggie's Recruit",
        "Marrowind",
        "Marshland Veteran",
        "Mirewatch Informant",
        "Mog",
        "Nettle Starbright",
        "Nettle Starbright Unmounted",
        "Pavo Quickstep",
        "Prince Vesper",
        "Queen Nyxara",
        "Quinberry Lark",
        "Reed Baelstone",
        "Resistance Smuggler",
        "Rowan Leafbound",
        "Sapling",
        "Scooter",
        "Starbloom Knight",
        "Sun Sprite",
        "Swamp Tracker",
        "Swamp Tracker Unmounted",
        "Sylvara",
        "Telos the Merchant",
        "Thaeron Baelstone",
        "Thorn Griffin",
        "Vanya Bluewater",
        "Victor Greyshard",
        "Warden",
        "Widowroot"
    };
    return Titles;
}

} // namespace bayou::client
