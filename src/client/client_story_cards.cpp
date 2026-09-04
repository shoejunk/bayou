#include "client_story_cards.hpp"

#include <algorithm>
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
    card.cost = cost;
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
    card.fidgetAnimFrames = 12;
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
    for (const ActionProfile& profile : card.actions)
    {
        if (profile.canMove)
        {
            card.movePattern = profile.pattern;
            card.moveRange = std::max(card.moveRange, profile.maxRange);
        }
        if (profile.canAttack)
        {
            card.attack = std::max(card.attack, profile.damage);
            card.attackRange = std::max(card.attackRange, profile.maxRange);
            card.attackingMove = card.attackingMove || profile.canMove;
        }
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
        card.actions = {action(
            "Spark", MovePattern::Ortho, 1, 2, false, true, 1, ActionKind::Ranged)};
        card.actions.push_back(action("Sprint", MovePattern::Ortho, 1, 2, false, false));
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
        card.actions = {action("Spear Thrust", MovePattern::Diag, 1, 3, true, true, 1)};
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

GameCard supportingCard(std::string_view title)
{
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
    if (title == "Gearjaw")
    {
        GameCard card = baseCard(title, "Unit", 30, 0, 3, {"Mechanical"});
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
    if (card.title.empty()) card = supportingCard(title);
    if (card.title.empty()) return std::nullopt;
    setSummary(card);
    return card;
}

} // namespace bayou::client
