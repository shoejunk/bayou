#include "client_story.hpp"

#include <array>
#include <initializer_list>
#include <utility>

namespace bayou::client
{
namespace
{

struct MissionEntry
{
    std::string_view id;
    std::string_view title;
    std::string_view source;
    std::string_view lesson;
    std::string_view objective;
    std::string_view hint;
    std::string_view lead;
    std::string_view allyOne;
    std::string_view allyTwo;
    std::string_view enemyOne;
    std::string_view enemyTwo;
    std::string_view speaker;
    std::string_view story;
    std::string_view result;
    std::string_view art;
};

bool heroTitle(std::string_view title)
{
    return title == "Joni Pumpernickel" || title == "Vanya Bluewater" ||
        title == "Birdie the Wise" || title == "Thaeron Baelstone" ||
        title == "Ashenfang" || title == "Victor Greyshard" ||
        title == "Maggie Mudroot" || title == "Sylvara";
}

StoryPanel panel(std::string_view speaker, std::string_view text, std::string_view art)
{
    return {speaker, text, art};
}

StoryPiecePlacement piece(
    std::string_view role,
    std::string_view title,
    int owner,
    int row,
    int column,
    int health = -1)
{
    StoryPiecePlacement value;
    value.role = role;
    value.cardTitle = title;
    value.owner = owner;
    value.row = row;
    value.column = column;
    value.isHero = heroTitle(title);
    value.initialHealth = health;
    return value;
}

StoryScriptAction scripted(
    StoryActionKind kind,
    int owner,
    std::string_view actor,
    int row,
    int column,
    std::string_view heading,
    std::string_view instruction,
    std::string_view correction,
    std::string_view target = {},
    std::string_view effect = {},
    std::vector<StoryPanel> panels = {})
{
    StoryScriptAction action;
    action.kind = kind;
    action.owner = owner;
    action.actorRole = actor;
    action.targetRole = target;
    action.effectRole = effect;
    action.targetRow = row;
    action.targetColumn = column;
    action.heading = heading;
    action.instruction = instruction;
    action.correction = correction;
    action.panelsBefore = std::move(panels);
    return action;
}

StoryScriptAction scriptedCard(
    StoryActionKind kind,
    int owner,
    std::string_view cardTitle,
    int row,
    int column,
    std::string_view heading,
    std::string_view instruction,
    std::string_view correction,
    std::string_view resultRole = {})
{
    StoryScriptAction action = scripted(
        kind, owner, {}, row, column, heading, instruction, correction);
    action.cardTitle = cardTitle;
    action.effectRole = resultRole;
    return action;
}

StoryMission storyNode(
    std::string_view id,
    std::string_view title,
    std::string_view source,
    std::initializer_list<StoryPanel> panels)
{
    StoryMission mission;
    mission.id = id;
    mission.title = title;
    mission.sourceChapter = source;
    mission.lesson = "CHRONICLE";
    mission.objective =
        "Follow the evidence, choices, and consequences connecting the tactical chapters.";
    mission.hint =
        "Chronicle chapters preserve Book One events without inventing a battle.";
    mission.briefing.assign(panels.begin(), panels.end());
    mission.objectiveSpec.kind = StoryObjectiveKind::StoryOnly;
    return mission;
}

StoryMission chronicleFromEntry(const MissionEntry& entry)
{
    StoryMission mission;
    mission.id = entry.id;
    mission.title = entry.title;
    mission.sourceChapter = entry.source;
    mission.lesson = "STORY BEAT";
    mission.objective =
        "Read this connective chapter, then continue to the next playable lesson.";
    mission.hint =
        "Story beats preserve the novel's chronology without pretending an unrelated skirmish reenacts it.";
    mission.briefing = {
        panel(entry.speaker, entry.story, entry.art),
        panel("Context",
            "This scene advances the Book One chronicle without adding a board action or a game rule. Continue through the story panels when ready.",
            entry.art),
        panel("Chronicle", entry.result, entry.art)};
    mission.objectiveSpec.kind = StoryObjectiveKind::StoryOnly;
    return mission;
}

StoryMission playableFromEntry(const MissionEntry& entry)
{
    StoryMission mission;
    mission.id = entry.id;
    mission.title = entry.title;
    mission.sourceChapter = entry.source;
    mission.lesson = entry.lesson;
    mission.objective = entry.objective;
    mission.hint = entry.hint;
    mission.briefing = {
        panel(entry.speaker, entry.story, entry.art),
        panel("Coach", entry.hint, entry.art)};
    mission.aftermath = {panel("Chronicle", entry.result, entry.art)};
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.firstPlayer = 1;
    return mission;
}

StoryMission sharedS01()
{
    return storyNode(
        "s01_hospitality",
        "Hospitality and the Bluewater Below",
        "Chapters 4-5",
        {
            panel("Maggie Mudroot",
                "A house that cannot refuse is another cage. Mangletooth's no remains an answer.",
                "cards/maggieMudroot.png"),
            panel("Nibsy",
                "Repair can cross the wound. That does not make repair permission.",
                "cards/nibsy.png"),
            panel("Birdie the Wise",
                "Food first. Names after. Work before anyone calls this a headquarters.",
                "cards/birdieTheWise.png"),
            panel("Vanya Bluewater",
                "No route belongs to the person who remembers it. Thirty-seven keys and a laundry chain keep households ahead of evidence.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "Thaeron's puzzle box offers a map, but useful is not the same thing as kind. Verify the bait separately.",
                "cards/reedBaelstone.png")
        });
}

StoryMission sharedS02()
{
    return storyNode(
        "s02_no_one_alone",
        "No One Alone",
        "Chapter 8",
        {
            panel("Hollis",
                "I brought twenty debtor names and routes. I also brought fear. Check both.",
                "cards/mirewatchInformant.png"),
            panel("Erevan the Shadow",
                "I took the pearl with the hand my mentor told me to keep empty.",
                "cards/erevanTheShadow.png"),
            panel("Vanya Bluewater",
                "You do not inherit forgiveness from the person who was forced to give it.",
                "cards/vanyaBluewater.png"),
            panel("Rowan",
                "Elliot drowned. His portfolio and Sylvara's tune survive him; the work belongs to no keeper of bad news.",
                "cards/rowanLeafbound.png")
        });
}

StoryMission sharedS03()
{
    return storyNode(
        "s03_published_mystery",
        "The Mystery Was Published",
        "Chapter 14",
        {
            panel("Narrator",
                "Juniper's work is divided among people. It is not inherited by the loudest mourner.",
                "characters/juniperFlash.png"),
            panel("Vanya Bluewater",
                "A good theory does not grant Erevan a private arrest, runner, or risk. Remy escapes through that mistake.",
                "cards/vanyaBluewater.png"),
            panel("Joni Pumpernickel",
                "Five invitations share pulp, watermark, routing fibre, and one damaged moth wing. The mystery was manufactured.",
                "cards/joniPumpernickel.png"),
            panel("Maggie Mudroot",
                "LeGrim. Baalzepub. Thaeron paid Remy, but the architect beneath the room was Lash.",
                "cards/maggieMudroot.png")
        });
}

StoryMission sharedS04()
{
    return storyNode(
        "s04_wounds_that_vote",
        "Wounds That Vote",
        "Chapters 16-17",
        {
            panel("Birdie the Wise",
                "Minnow gets the whole fever-bark course. We do not turn usefulness into body weight.",
                "cards/birdieTheWise.png"),
            panel("Rowan",
                "I made Reed large and Minnow small. That was my error.",
                "cards/rowanLeafbound.png"),
            panel("Mara",
                "Forty-three people vote. Twenty-two choose public action, and every dissent stays in the minutes.",
                "cards/mirewatchInformant.png"),
            panel("Joni Pumpernickel",
                "Officers hold records, not people. Food is never conditional. Five duties bind the new Society.",
                "cards/joniPumpernickel.png")
        });
}

StoryMission sharedS05()
{
    return storyNode(
        "s05_memory_contradicts",
        "The Memory That Contradicts",
        "Chapter 22",
        {
            panel("Reed Baelstone",
                "I can ask what Thaeron chose. I cannot choose which part of my mother pays.",
                "cards/reedBaelstone.png"),
            panel("Donella of the Marsh",
                "Thaeron arranged the deaths, preserved Reed, and bought a boundary that could be moved.",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "The moonfruit takes Reed's last sensory memory of his mother. Lash's cuff, cane, and third account wait behind the bargain.",
                "cards/thaeronBaelstone.png"),
            panel("Vesper",
                "The Mirror may name the past. The grove is being hurt now. I choose temporary company, not ownership.",
                "cards/erevanTheShadow.png")
        });
}

StoryMission sharedS06()
{
    return storyNode(
        "s06_town_owns_itself",
        "A Town That Owns Itself",
        "Chapter 30 and Epilogue",
        {
            panel("Rowan",
                "Count the bodies that came back, including every permanent injury, not the bodies anyone wishes had returned.",
                "cards/rowanLeafbound.png"),
            panel("Vanya Bluewater",
                "For three weeks the town ran ferries, burials, wages, and shortages without waiting to be rescued.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "Granary only. No lien, sale, ferry claim, appointment, or second act. The narrow authority passes by three votes.",
                "cards/reedBaelstone.png"),
            panel("Donella of the Marsh",
                "Fourteen days to Emberhaven. Then every companion chooses again.",
                "cards/donellaOfTheMarsh.png"),
            panel("Fizzlewick Gearwright",
                "Mirewatch won. The measurement survived. The Root Key, Mirror, Caltheriel, and southern shadow war remain open.",
                "cards/fizzlewickGearwright.png"),
            panel("The old thief's voice",
                "The pearl cracks northward and answers the unfinished maxim: No one alone.",
                "cards/erevanTheShadow.png")
        });
}

StoryMission mirewatchOpening()
{
    StoryMission mission;
    mission.id = "mw01_river_teeth";
    mission.title = "River Teeth";
    mission.sourceChapter = "Chapter 1 - Teeth in the River";
    mission.lesson = "MOVEMENT, TURN CADENCE, ATTACKS, HEALTH, AND DEATH";
    mission.objective =
        "Move Reed one square on each of two turns. Defeat three wounded gators with printed attacks; Telos's Travel must clear Reed's final shot.";
    mission.hint =
        "Only the highlighted story action is accepted. Drag its named character to the marked square or target.";
    mission.briefing = {
        panel("Narrator",
            "Three harnessed gators close around Telos's freight skiff. Leather, burnt pine, and old wounds show that someone trained the attack.",
            "story/mw/mw01/01_surrounded.png"),
        panel("Donella of the Marsh",
            "Reed, two squares away. Move one square now; the next when the water gives you a turn.",
            "story/mw/mw01/02_two_squares.png"),
        panel("Coach",
            "Guided Actions count completed game actions, not mouse motions. To move or attack, drag the ACT piece onto its TARGET. This one-action restriction applies only to this tutorial.",
            "story/mw/mw01/02_two_squares.png"),
        panel("Coach",
            "Reed's injury is story context, not a status effect. The center gator will pursue him after the first End Turn. Number badges show current Health: blue for your pieces, red for enemies. These gators begin wounded at 1, and a 1-damage hit removes a unit at 0.",
            "story/mw/mw01/02_two_squares.png")};
    mission.aftermath = {
        panel("Erevan the Shadow",
            "Someone taught those mouths what Blackthorn cargo smells like.",
            "story/mw/mw01/04_after.png"),
        panel("Chronicle adaptation",
            "Story Mode lets all four travelers help defeat the three trained gators. The novel's exchange is shorter: Reed kills one, and the other two withdraw.",
            "story/mw/mw01/04_after.png"),
        panel("Narrator",
            "Pedros lies dead in the clearing. Beyond him, plants crowd a covered Gilded Hold and an unnamed captive who will free herself.",
            "story/mw/mw01/04_after.png")};
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 5, 3),
        piece("donella", "Donella of the Marsh", 1, 4, 5),
        piece("erevan", "Erevan the Shadow", 1, 3, 2),
        piece("telos", "Telos the Merchant", 1, 3, 1),
        piece("cargo_gator", "Bull Gator", 2, 2, 1, 1),
        piece("knife_gator", "Bull Gator", 2, 3, 3, 1),
        piece("center_gator", "Bull Gator", 2, 4, 4, 1)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.firstPlayer = 1;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"reed", "donella", "erevan", "telos"};
    mission.script = {
        scripted(
            StoryActionKind::Move, 1, "reed", 5, 2,
            "GET REED CLEAR - MOVE 1 OF 2",
            "Drag ACT: Reed from D6 onto TARGET: C6 immediately left of him. While dragging, C6 shows the normal teal move marker. Reed's printed \"Step\" action moves one square in any direction.",
            "Reed must make the highlighted one-square move first."),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "GIVE THE WATER A TURN",
            "End Turn so the nearest gator can lunge.",
            "End Turn now; Reed cannot make both moves in one activation.",
            {}, {},
            {
                panel("Narrator",
                    "Reed reaches C6. The center gator turns to pursue him; his injured leg is story context, not a game status.",
                    "story/mw/mw01/01_surrounded.png")
            }),
        scripted(
            StoryActionKind::Move, 2, "center_gator", 4, 3,
            "THE GATOR LUNGES",
            "The center gator moves from E5 to D5.",
            "Mission data error."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE WATER SHIFTS",
            "The gator ends its turn.",
            "Mission data error."),
        scripted(
            StoryActionKind::Move, 1, "reed", 5, 1,
            "GET REED CLEAR - MOVE 2 OF 2",
            "Move Reed from C6 to B6. He is now two squares from where he began and farther from the threat.",
            "Reed must finish the second highlighted square himself."),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "HOLD THE SAFE LINE", "End Turn so Donella can answer the lunge.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATOR TURNS", "The gator loses Reed's trail and faces Donella.",
            "Mission data error."),
        scripted(
            StoryActionKind::Attack, 1, "donella", 4, 3,
            "DONELLA - SPARK",
            "Drag Donella at F5 onto the wounded gator at D5. Her printed Spark reaches two squares along the row, and its 1 damage reduces the gator's last Health to 0.",
            "Use Donella's printed orthogonal Spark on the wounded center gator.",
            "center_gator"),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "ONE GATOR DOWN",
            "End Turn after Donella's shot.",
            "End Turn to continue the rescue."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE OTHER TWO CIRCLE",
            "The remaining gators hold.",
            "Mission data error."),
        scripted(
            StoryActionKind::Attack, 1, "erevan", 3, 3,
            "EREVAN - SHADOW BLADE",
            "Drag Erevan onto the wounded gator beside him at D4. Shadow Blade is an attacking move: after the target reaches 0 Health, Erevan enters D4.",
            "Erevan must use his printed Shadow Blade on the adjacent gator.",
            "knife_gator"),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "TWO GATORS DOWN",
            "End Turn after Erevan's strike.",
            "End Turn to bring Telos into the fight."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE LAST HARNESS PULLS TIGHT",
            "The final gator holds under the suspended freight.",
            "Mission data error."),
        scripted(
            StoryActionKind::Move, 1, "telos", 4, 0,
            "TELOS - TRAVEL",
            "Move Telos from B4 to A5 with Travel. This ordinary one-square move gets him out of Bite range and clears Reed's shot along column B.",
            "Use Telos's printed Travel action on highlighted A5.",
            {}, {},
            {
                panel("Telos the Merchant",
                    "Reed, take the clear line. I am getting the skiff clear.",
                    "story/mw/mw01/03_drop_cargo.png"),
                panel("Coach",
                    "Travel is Telos's printed one-square move. Going from B4 to A5 clears column B for Reed's Bow.",
                    "story/mw/mw01/03_drop_cargo.png")
            }),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "THE FIRING LINE IS CLEAR",
            "End Turn after Telos Travels.",
            "End Turn so Reed can recover his firing line."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE LAST GATOR HOLDS",
            "The final gator remains in Reed's clear column B.",
            "Mission data error."),
        scripted(
            StoryActionKind::Attack, 1, "reed", 2, 1,
            "REED - BOW",
            "Drag Reed at B6 onto the wounded gator at B3. His Bow reaches three squares along column B; Telos vacated B4, so the normal line-of-sight path is clear.",
            "Finish with Reed's printed Bow attack.",
            "cargo_gator")
    };
    mission.masteryCards = {
        "Reed Baelstone", "Donella of the Marsh", "Erevan the Shadow",
        "Telos the Merchant"};
    mission.masteryRules = {
        "board-coordinates", "normal-activation", "end-turn", "omni-movement",
        "ranged-line-of-sight", "attacking-movement", "health-and-destruction"};
    return mission;
}

StoryMission blackthornOpening()
{
    StoryMission mission;
    mission.id = "bt01_harness_hunger";
    mission.title = "Harness the Hunger";
    mission.sourceChapter = "Chapter 1 - reconstructed Company dossier";
    mission.lesson = "ORTHOGONAL MOVEMENT, HEAL, DISABLE, DAMAGE";
    mission.objective =
        "Move a Lumberjack, heal the worker, disable the north gator, and make one Lumberjack attack. Do not kill a gator.";
    mission.hint =
        "This operation is evidence of cruelty, not a heroic victory. Do not kill a gator.";
    mission.briefing = {
        panel("Narrator",
            "Before Reed's skiff is attacked, a Blackthorn crew starves three gators and trains them to hunt unrecorded freight. Donella later recognizes that cruelty in their harnesses.",
            "cards/blackthornLumberjack.png"),
        panel("Blackthorn Foreman",
            "Mark unrecorded traffic as food. Hunger will finish the instruction.",
            "cards/blackthornForeman.png"),
        panel("Narrator",
            "Before player control begins, the crew has already laid scent timber in all three lanes. That cruelty is story context, not a playable rule.",
            "cards/bullGator.png"),
        panel("Coach",
            "Guided Actions count completed game actions, not mouse motions. To move or attack, drag the ACT piece onto its TARGET. This one-action restriction applies only to this tutorial.",
            "cards/blackthornLumberjack.png"),
        panel("Coach",
            "Red badges show current Health. Healing Elixer restores Health only up to the printed maximum; Paralysis Potion applies Disable without damage; ordinary positive damage also disables a surviving target for one turn.",
            "cards/blackthornLumberjack.png")};
    mission.aftermath = {
        panel("Narrator",
            "All three animals are released toward a freight skiff carrying four people and a covered cage.",
            "cards/bullGator.png"),
        panel("Chronicle",
            "The dossier succeeds operationally and fails morally: Blackthorn industrialized a defense Donella once used to protect people.",
            "cards/blackthornAlchemist.png")};
    mission.pieces = {
        piece("alchemist", "Blackthorn Alchemist", 1, 3, 1),
        piece("lumberjack", "Blackthorn Lumberjack", 1, 4, 1),
        piece("worker", "Blackthorn Lumberjack", 1, 3, 2, 1),
        piece("north_gator", "Bull Gator", 2, 2, 3),
        piece("center_gator", "Bull Gator", 2, 4, 3),
        piece("south_gator", "Bull Gator", 2, 6, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.firstPlayer = 1;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"alchemist", "lumberjack", "worker"};
    mission.script = {
        scripted(StoryActionKind::Move, 1, "lumberjack", 4, 2,
            "OPEN THE WORK LANE", "Move the Lumberjack from B5 to C5. Orthogonal movement follows a row or column, never a diagonal.",
            "Use the Lumberjack's ordinary one-square orthogonal Axe Swing action."),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "OBSERVE THE PEN", "End Turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATORS WAIT", "The penned animals hold.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "alchemist", 3, 2,
            "HEAL THE WORKER", "Target the wounded friendly worker at C4.",
            "Use Healing Elixer on the friendly worker; it heals three without overheal.", "worker"),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "OPEN THE NORTH GATE", "End Turn.", "End Turn to release the north gator."),
        scripted(StoryActionKind::Move, 2, "north_gator", 2, 2,
            "THE NORTH GATOR LUNGES", "The north gator moves from D3 to C3.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE HARNESS HOLDS", "The gator ends its turn.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "alchemist", 2, 2,
            "DISABLE WITHOUT DAMAGE", "Target the enemy gator at C3.",
            "Use Paralysis Potion: zero damage and Disable 2.", "north_gator"),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "MEASURE THE SKIPPED TURN", "End Turn.", "End Turn to observe Disable."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "DISABLED", "The north gator misses its activation.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "lumberjack", 4, 3,
            "ONE MEASURED HIT", "Hit the center gator at D5 once.",
            "Land exactly one ordinary hit; the surviving gator stays in place.", "center_gator")
    };
    mission.masteryCards = {"Blackthorn Alchemist", "Blackthorn Lumberjack"};
    mission.masteryRules = {
        "orthogonal-movement", "friendly-heal", "maximum-health", "disable-two-turns",
        "positive-damage-disable"};
    return mission;
}

StoryMission mirewatchDeployment()
{
    StoryMission mission;
    mission.id = "mw03_town_under_company";
    mission.title = "The Town Under the Company";
    mission.sourceChapter = "Chapter 2";
    mission.lesson = "CONTROL, RESOURCES, DEPLOYMENT, TAX, AURA";
    mission.objective =
        "Deploy the Informant, open a diagonal lane with the Spearman, then observe Joni's aura, income, and Tax.";
    mission.hint =
        "Joni controls adjacent ground and supplies Arcane and Civilized. A deployed unit arrives exhausted.";
    mission.briefing = {
        panel("Reed Baelstone",
            "A number beside a workboat is not proof of inherited debt. The torn wrap reveals my Baelstone crest, and the official records it.",
            "cards/reedBaelstone.png"),
        panel("Joni Pumpernickel",
            "Blackthorn suspended the Infamous Mouse's license. It did not inherit my rooms. Move the guests.",
            "cards/joniPumpernickel.png"),
        panel("Coach",
            "Controlled squares generate Resources. Units deploy on empty controlled ground, meet living-Hero traits, and cannot act on arrival.",
            "cards/mirewatchInformant.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The workboat stays with the Mudfens. Joni moves the Hold while the Informant's Tax makes the next Company turn more expensive.",
            "cards/joniPumpernickel.png")};
    mission.pieces = {
        piece("joni", "Joni Pumpernickel", 1, 3, 1),
        piece("spearman", "Bog Spearman", 1, 4, 1),
        piece("wounded_spearman", "Bog Spearman", 1, 2, 2, 1),
        piece("notice_guard", "Blackthorn Lumberjack", 2, 6, 3, 1),
        piece("tax_guard", "Blackthorn Debt Collector", 2, 4, 3),
        piece("collector", "Blackthorn Debt Collector", 2, 3, 5),
        piece("lumberjack", "Blackthorn Lumberjack", 2, 5, 5)};
    mission.playerHand = {"Mirewatch Informant"};
    mission.playerResources = 35;
    mission.enemyResources = 35;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"joni", "spearman", "wounded_spearman"};
    mission.script = {
        scriptedCard(StoryActionKind::PlayCard, 1, "Mirewatch Informant", 3, 2,
            "DEPLOY ON CONTROLLED GROUND", "Drag the Informant card from your hand onto highlighted C4.",
            "Deploy the Mirewatch Informant on C4. Joni supplies Civilized, and three nearby friendly pieces control that empty square.", "informant"),
        scripted(StoryActionKind::Attack, 1, "spearman", 6, 3,
            "SPEARMAN - SPEAR THRUST", "Use Spear Thrust from B5 through empty C6 to the wounded Lumberjack at D7. Its red badge shows 1 Health, so one damage destroys it.",
            "Card plays do not consume the turn's normal piece action; use the Spearman now.", "notice_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "JONI'S AURA", "End Turn. Joni heals adjacent pieces, but never above their maximum health.",
            "End Turn to resolve Joni's healing aura."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "START-TURN ECONOMY", "The opponent passes; your next turn adds controlled-square income and collects Tax.",
            "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "informant", 4, 3,
            "INFORMANT - LUNGE", "Use Lunge diagonally from C4 into the Debt Collector at D5.",
            "Use the newly readied Informant's printed Lunge.", "tax_guard")
    };
    mission.masteryCards = {
        "Joni Pumpernickel", "Mirewatch Informant", "Bog Spearman"};
    mission.masteryRules = {
        "control", "resources", "deployment-cost", "trait-gate", "arrival-exhaustion",
        "card-play-plus-activation", "healing-aura", "maximum-health", "tax",
        "diagonal-attacking-movement"};
    return mission;
}

StoryMission blackthornDeployment()
{
    StoryMission mission;
    mission.id = "bt03_sanctuary_debt";
    mission.title = "Sanctuary Debt";
    mission.sourceChapter = "Chapter 2";
    mission.lesson = "INCOME, GATHER, TAX, SUMMON, TRAIL, COMMAND";
    mission.objective =
        "Deploy the Alchemist, use Command and Summon, leave a Sapling Trail, then audit Gather and Tax.";
    mission.hint =
        "Thaeron supplies Arcane, Civilized, and Corrupt. Later lessons exercise Summon, Trail, Gather, Tax, and Command.";
    mission.briefing = {
        panel("Thaeron Baelstone",
            "Ownership is the right to make the next square expensive.",
            "cards/thaeronBaelstone.png"),
        panel("Joni Pumpernickel",
            "You suspended a license. You did not inherit my rooms.",
            "cards/joniPumpernickel.png"),
        panel("Coach",
            "Place the Alchemist on C4. It arrives exhausted; card plays do not consume the one normal piece action.",
            "cards/blackthornAlchemist.png")};
    mission.aftermath = {
        panel("Chronicle",
            "Hospitality is entered as a liability, but the Mouse remains Joni's. The seal records pressure, not lawful ownership.",
            "cards/blackthornDebtCollector.png")};
    mission.pieces = {
        piece("thaeron", "Thaeron Baelstone", 1, 3, 1),
        piece("foreman", "Blackthorn Foreman", 1, 4, 1),
        piece("lumberjack", "Blackthorn Lumberjack", 1, 2, 1, 1),
        piece("sister", "Grove Sister", 1, 6, 1),
        piece("collector", "Blackthorn Debt Collector", 1, 7, 1)};
    mission.playerHand = {"Blackthorn Alchemist"};
    mission.playerResources = 20;
    mission.enemyResources = 35;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {
        "thaeron", "foreman", "lumberjack", "sister", "collector"};
    mission.script = {
        scriptedCard(StoryActionKind::PlayCard, 1, "Blackthorn Alchemist", 3, 2,
            "DEPLOY THE ALCHEMIST", "Drag the Alchemist card from your hand onto highlighted C4.",
            "Deploy on C4; Thaeron supplies Arcane and Civilized.", "alchemist"),
        scripted(StoryActionKind::UseAbility, 1, "thaeron", -1, -1,
            "THAERON - COMMAND", "Select Thaeron and use Command.",
            "Command must begin with Thaeron and then activate one adjacent ready unit."),
        scripted(StoryActionKind::UseAbility, 1, "foreman", -1, -1,
            "COMMAND THE FOREMAN", "Use the adjacent Foreman's Summon while Command is active.",
            "Choose the adjacent ready Foreman; its front square at C5 is open."),
        scripted(StoryActionKind::Move, 1, "sister", 5, 2,
            "GROVE SISTER - GLIDE AND TRAIL", "Use Glide diagonally from B7 to C6. Trail leaves a Sapling on the origin.",
            "Command preserved the normal activation; use the Grove Sister's printed Glide."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END THE WORK SHIFT", "End Turn. The deployed and summoned units will ready next turn.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "GATHER AND TAX", "The opponent passes. Lumberjacks Gather; the Collector transfers Tax; controlled ground pays income.",
            "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "alchemist", 2, 1,
            "ALCHEMIST - HEALING ELIXER", "Target the wounded friendly Lumberjack at B3.",
            "Use Healing Elixer on the adjacent friendly Lumberjack, not Paralysis Potion.", "lumberjack")
    };
    mission.masteryCards = {
        "Thaeron Baelstone", "Blackthorn Foreman", "Grove Sister",
        "Blackthorn Debt Collector", "Blackthorn Alchemist"};
    mission.masteryRules = {
        "deployment-cost", "trait-gate", "arrival-exhaustion", "command", "summon",
        "summon-front-square", "trail", "gather", "tax", "controlled-income", "friendly-heal"};
    return mission;
}

StoryMission mirewatchCapstone()
{
    const MissionEntry entry = {
        "mw15_title_follows_burden", "Title Follows Burden", "Chapter 19",
        "DEPLOYMENT, REPEAT, RANGED AND DIRECTIONAL ATTACKS",
        "Use the starting deck's real cards to open every lane into the public hearing.",
        "Deploy a real card, then combine the party's printed actions.",
        "Joni Pumpernickel", "Vanya Bluewater", "Birdie the Wise",
        "Victor Greyshard", "Grask", "Mara",
        "The charter can burn. Performed work, burial silver, ferry records, and forty-three witnesses remain.",
        "Braun refuses an order against children and dies by Grask's hand. Mog offers terms; the town's provisional Society survives.",
        "cards/joniPumpernickel.png"};
    StoryMission mission = playableFromEntry(entry);
    mission.lesson =
        "DEPLOYMENT, REPEAT, RANGED AND DIRECTIONAL ATTACKS";
    mission.objective =
        "Deploy a Bog Spearman and clear six Company blockers using only printed actions.";
    mission.hint =
        "Card play does not spend the turn's normal piece action. Vanya must complete both Blade Dance uses before anyone else acts.";
    mission.briefing = {
        panel("Joni Pumpernickel",
            "The charter can burn. Performed work, burial silver, ferry records, and forty-three witnesses remain.",
            "cards/joniPumpernickel.png"),
        panel("Coach",
            "Every required input in this final exam is a normal action from an authoritative card: deploy, Blade Dance, Longbow Shot, Spear Thrust, Lunge, and Walking Stick.",
            "cards/birdieTheWise.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The Company line breaks. Braun refuses an order against children and dies by Grask's hand; Mog offers terms, and the town's provisional Society survives.",
            "cards/joniPumpernickel.png")};
    mission.pieces = {
        piece("joni", "Joni Pumpernickel", 1, 7, 0),
        piece("vanya", "Vanya Bluewater", 1, 6, 0),
        piece("birdie", "Birdie the Wise", 1, 5, 0),
        piece("informant", "Mirewatch Informant", 1, 4, 1),
        piece("vanya_guard_one", "Blackthorn Debt Collector", 2, 5, 1, 1),
        piece("vanya_guard_two", "Blackthorn Debt Collector", 2, 4, 2, 1),
        piece("birdie_guard", "Blackthorn Alchemist", 2, 5, 3, 1),
        piece("spear_guard", "Blackthorn Alchemist", 2, 4, 3, 1),
        piece("informant_guard", "Blackthorn Debt Collector", 2, 3, 2, 1),
        piece("joni_guard", "Blackthorn Debt Collector", 2, 7, 1, 1)};
    mission.playerHand = {"Bog Spearman"};
    mission.playerResources = 50;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"joni", "vanya", "birdie", "informant"};
    mission.script = {
        scriptedCard(StoryActionKind::PlayCard, 1, "Bog Spearman", 6, 1,
            "DEPLOY ON CONTROLLED GROUND",
            "Deploy the Bog Spearman on B7. It arrives exhausted.",
            "Place the Spearman on highlighted B7.", "reinforcement"),
        scripted(StoryActionKind::Attack, 1, "vanya", 5, 1,
            "VANYA - BLADE DANCE",
            "Attack the Debt Collector on B6 with Blade Dance.",
            "Use Vanya's printed diagonal Blade Dance.", "vanya_guard_one"),
        scripted(StoryActionKind::Attack, 1, "vanya", 4, 2,
            "VANYA - COMPLETE THE REPEAT",
            "Use Blade Dance again on the Debt Collector at C5.",
            "The repeat lock requires the same action or Pass.", "vanya_guard_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE REINFORCEMENT", "End Turn after Vanya's required repeat.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HEARING HOLDS", "The remaining blockers hold their lanes.",
            "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "birdie", 5, 3,
            "BIRDIE - LONGBOW SHOT",
            "Fire three squares along the row at the Alchemist on D6.",
            "Use Birdie's printed Longbow Shot.", "birdie_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE LINE", "End Turn after Birdie's shot.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SPEARMAN READIES", "The remaining blockers hold.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "reinforcement", 4, 3,
            "SPEARMAN - DIAGONAL THRUST",
            "Use Spear Thrust from B7 to the Alchemist on D5.",
            "Choose the highlighted diagonal target.", "spear_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "OPEN THE WITNESS LANE", "End Turn after the Spearman acts.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE RECORD STAYS PUBLIC", "The last two blockers hold.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "informant", 3, 2,
            "INFORMANT - LUNGE",
            "Use Lunge on the adjacent Debt Collector at C4.",
            "Use the Informant's printed Lunge.", "informant_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE FINAL LANE", "End Turn so Joni can act next turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST BLOCKER WAITS", "The final Debt Collector holds.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "joni", 7, 1,
            "JONI - WALKING STICK",
            "Use Walking Stick on the Debt Collector at B8.",
            "Finish with Joni's printed adjacent action.", "joni_guard")};
    mission.masteryCards = {
        "Joni Pumpernickel", "Vanya Bluewater", "Birdie the Wise",
        "Mirewatch Informant", "Bog Spearman"};
    mission.masteryRules = {
        "deployment-cost", "controlled-square-deployment",
        "arrival-exhaustion", "card-play-plus-activation", "repeat-lock",
        "ranged-attack", "diagonal-attacking-movement"};
    return mission;
}

StoryMission blackthornCapstone()
{
    const MissionEntry entry = {
        "bt17_natural_order", "Natural Order", "Chapter 29",
        "BLACKTHORN FINAL EXAM, RELENTLESS",
        "Break the Company line with real card actions while Fizzlewick records the collapse off-board.",
        "A Relentless defeat offers Victor an optional extra action. Demonstrate it, then combine Command, Summon, Capture, Trail, and healing.",
        "Thaeron Baelstone", "Ashenfang", "Grask",
        "Reed Baelstone", "Donella of the Marsh", "Fizzlewick Gearwright",
        "A failed machine can still leave an accurate measurement.",
        "Grask dies beneath the cages. The collective gate opens; Victor breaks surrender and Pavo kills him. Fizzlewick escapes with partial measurements.",
        "cards/fizzlewickGearwright.png"};
    StoryMission mission = playableFromEntry(entry);
    mission.pieces = {
        piece("victor", "Victor Greyshard", 1, 7, 7),
        piece("thaeron", "Thaeron Baelstone", 1, 7, 0),
        piece("foreman", "Blackthorn Foreman", 1, 6, 0),
        piece("grask", "Grask", 1, 5, 3),
        piece("sister", "Grove Sister", 1, 7, 3),
        piece("alchemist", "Blackthorn Alchemist", 1, 2, 0),
        piece("mog", "Mog", 1, 2, 1, 1),
        piece("first_guard", "Blackthorn Debt Collector", 2, 6, 6),
        piece("second_guard", "Blackthorn Debt Collector", 2, 5, 5),
        piece("cage_guard", "Bog Spearman", 2, 4, 2)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"thaeron", "foreman", "grask", "sister", "alchemist", "mog"};
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "victor", 6, 6,
            "VICTOR - FIRST AXE DANCE", "Use Axe Dance on the Debt Collector at G7.",
            "Use Victor's printed Axe Dance on the adjacent enemy.", "first_guard"),
        scripted(StoryActionKind::Attack, 1, "victor", 5, 5,
            "VICTOR - OPTIONAL RELENTLESS ACTION", "Relentless allows Victor to act again after his defeat. This lesson asks you to take that optional Axe Dance on the Debt Collector at F6; ordinary play also allows End Turn.",
            "Demonstrate the optional Relentless action with Victor. In ordinary play, End Turn may decline it.", "second_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE TERMS HAVE BROKEN", "End Turn after resolving both Relentless actions.",
            "Finish Victor's second action first, then End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE CAGES SHIFT", "The defenders pass while the Company line measures the collapse.",
            "Mission data error."),
        scripted(StoryActionKind::UseAbility, 1, "thaeron", -1, -1,
            "COMMAND", "Use Thaeron's Command beside the Foreman.",
            "Select Thaeron and use Command."),
        scripted(StoryActionKind::UseAbility, 1, "foreman", -1, -1,
            "COMMAND THE SUMMON", "Use the adjacent Foreman's Summon while Command is active.",
            "Choose the Foreman; for player 1 its front square at B7 is open."),
        scripted(StoryActionKind::Attack, 1, "grask", 4, 2,
            "GRASK - SWING AXES", "Use Grask's diagonal Capture action, Swing Axes, on the Bog Spearman at C5.",
            "Choose the adjacent diagonal enemy, not a long orthogonal target.", "cage_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "MEASUREMENT HOLDS", "End Turn after the commanded action and normal activation.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "A ROOTWAY OPENS", "The opposing line passes.", "Mission data error."),
        scripted(StoryActionKind::Move, 1, "sister", 5, 1,
            "GROVE SISTER - GLIDE AND TRAIL", "Use Glide diagonally to B6; Trail leaves a Sapling at D8.",
            "Use the Grove Sister's printed diagonal Glide."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE PARTIAL RECORD LEAVES", "End Turn before treating the wounded ally.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "NO CLEAN VICTORY", "The broken surrender is recorded, not rewritten.",
            "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "alchemist", 2, 1,
            "ALCHEMIST - HEALING ELIXER", "Use Healing Elixer on the wounded friendly Mog at B3.",
            "Target Mog with the Alchemist's printed friendly heal.", "mog")};
    mission.masteryCards = {
        "Victor Greyshard", "Thaeron Baelstone", "Blackthorn Foreman", "Grask",
        "Grove Sister", "Blackthorn Alchemist"};
    mission.masteryRules = {
        "relentless-optional-extra-action", "relentless-action-lock", "command",
        "command-preserves-normal-activation", "summon-front-square",
        "summoned-unit-arrives-exhausted", "capture", "trail", "friendly-heal"};
    return mission;
}

const std::vector<MissionEntry> MirewatchEntries = {
    {
        "mw02_gilded_hold", "The Gilded Hold", "Chapter 1 clearing and customs",
        "REPEAT, DIAGONAL ATTACKING MOVEMENT",
        "Route both threats to secure the Hold and clear the customs route.",
        "Use Swashbuckle Blade twice. Repeat 1 locks the Smuggler to that action or Pass.",
        "Resistance Smuggler", "Donella of the Marsh", "Erevan the Shadow",
        "Bull Gator", "Blackthorn Debt Collector", "Donella of the Marsh",
        "Pedros is dead. That does not make his knife, his grief, or the person in this cage ours.",
        "The unnamed captive reverses the hidden splinter and leaves herself. Asta removes exactly 64 pounds: 26 around the Hold and 38 to baskets, leaving Telos 412.",
        "cards/gildedCage.png"
    },
    {
        "mw04_watcher_protects", "What the Watcher Protects", "Chapter 3",
        "RANGED ATTACKS, RANGE TWO, TURN CADENCE",
        "Break the chains, preserve the watcher, and route the observers.",
        "Juniper uses Spark from range two; after the turn cycle, Erevan uses adjacent Shadow Blade.",
        "Juniper Flash", "Erevan the Shadow", "Donella of the Marsh",
        "Goblin Sharpshooter", "Goblin Ambusher", "Juniper Flash",
        "I caused Lio's arrest by asking the question they prepared.",
        "Eight refugees escape. The watcher shields Pellan and Seli, gives Erevan a black root that pulses beneath two skies, and leaves alive.",
        "characters/juniperFlash.png"
    },
    {
        "mw05_watched_office", "The Office Everyone Is Watching", "Chapter 6",
        "DEMATERIALIZE, PASS-THROUGH, HIDDEN SHOVE",
        "Route both office guards, take the authentic freight chits, and escape the prepared office.",
        "Use Dematerialize, cross occupied squares with Fade Through Shadow, then use Hidden Shove.",
        "Erevan the Shadow", "Mirewatch Informant", "Reed Baelstone",
        "Grask", "Goblin Sharpshooter", "Hara Dole",
        "Warn my mother. I keep the dye here.",
        "The party takes chits showing 920 pounds received and 396 extracted. Hara closes the hatch and stays; the prepared confession is left behind.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw06_cost_seen", "The Cost of Being Seen", "Chapter 7",
        "COOLDOWN, REPEAT, JUMP, REBIRTH",
        "Free the Mudfen wagon and route the seizure crew.",
        "Birdie shoots at range; Scooter and Smuggler repeat; the Tracker jumps blockers and may return unmounted.",
        "Birdie the Wise", "Scooter", "Swamp Tracker",
        "Grask", "Blackthorn Debt Collector", "Birdie the Wise",
        "Open the wagon. People before receipts.",
        "Four Mudfens escape. A recorder learns five rescue motions; Victor retaliates against ferries, medicine, wages, tools, food, Torren, and Missus Vale.",
        "cards/birdieTheWise.png"
    },
    {
        "mw07_twenty_debtors", "Twenty Debtors", "Chapter 9",
        "DAMAGE DISABLE, HEALING, BODYGUARD, FAIL-FORWARD",
        "Route both prison guards to break the line and keep the escape road open.",
        "Damage makes a survivor miss its next activation. Donella heals while the Veterans hold the children first.",
        "Donella of the Marsh", "Marshland Veteran", "Reed Baelstone",
        "Grask", "Goblin Sharpshooter", "Tommy",
        "Tell me the risk. The hand is still mine to offer.",
        "Nineteen prisoners and Garrett's body leave. Garrett dies protecting Little Fen; the prisoners vote 17-3 to flood the mill and burn the ledger.",
        "cards/marshlandVeteran.png"
    },
    {
        "mw08_lesson_night", "The Lesson at Night", "Chapter 10",
        "HAZARD CLOCKS, DIG/TUNNEL, LARGE FOOTPRINTS",
        "Route both lock guards, escape Thaeron's north lock, and reach the Bluewater rescue.",
        "Story holes and tunnels are marked as scenario grammar. Reed refuses every offer without sealed amnesty.",
        "Reed Baelstone", "Vanya Bluewater", "Birdie the Wise",
        "Thaeron Baelstone", "Blackthorn Foreman", "Thaeron Baelstone",
        "I kept the socks dry. I also built the need that brought you here.",
        "Reed and seven vulnerable people escape. Missus Vale holds the door and dies; Bluewater Below is destroyed, and Vanya removes Reed's route authority.",
        "cards/thaeronBaelstone.png"
    },
    {
        "mw09_invitations", "Invitations", "Chapter 11",
        "CONSENT PREDICATES, CAPTURE-NOT-KILL, ROLE CAPS",
        "Route both gate guards so three consent-locked plates can open and Remy can be subdued alive.",
        "A volunteer's yes must exist before a blood plate can open. Remy is a captive, never an ally.",
        "Vanya Bluewater", "Donella of the Marsh", "Reed Baelstone",
        "Grask", "Goblin Ambusher", "Sedge",
        "The token is under the skin. Ask me before you cut.",
        "Three survivors escape and Remy enters custody while records burn. Seven bounded roles are assigned, and Donella may end Reed's part.",
        "cards/vanyaBluewater.png"
    },
    {
        "mw10_beautiful_plan", "A Beautiful Plan", "Chapter 12",
        "REPEAT LOCK, DIAGONAL ATTACKING MOVEMENT",
        "Route both auction handlers to release captives while keeping Mirror and Root Key in declared custody.",
        "Vanya's diagonal Blade Dance repeats the same action. Finish it or Pass before using another piece.",
        "Vanya Bluewater", "Joni Pumpernickel", "Birdie the Wise",
        "Grask", "Blackthorn Foreman", "Nima",
        "I was working this lock before you bid on me.",
        "At least sixteen captives leave. Nima opens her own lock; Reed rejects a buyer's murder schedule; the paired wound leads the group to the reliquary.",
        "cards/vanyaBluewater.png"
    },
    {
        "mw11_no_plan_saves_all", "No Plan Saves Everyone", "Chapter 13",
        "PARALLEL CLOCKS, PRIORITIES, PERMANENT COST",
        "Route both shutter crews to keep four rescue lanes open until closing time.",
        "Switch lanes after every hazard pulse. A right plan can still record irrecoverable losses without calling them player failure.",
        "Reed Baelstone", "Juniper Flash", "Birdie the Wise",
        "Goblin Sharpshooter", "Grask", "Erevan the Shadow",
        "A perfect plan would need five bodies in each of five places.",
        "Sixteen of nineteen escape. Reed's right hand is permanently injured; Juniper dies after giving Mae the flare; Nima, Cal, and Dessa remain missing.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw12_road_reaches", "The Road and What It Reaches", "Chapter 15",
        "REFUSAL, TELEPORT ROUTES, SEPARATE CUSTODY",
        "Route both pursuing scouts, record three custodians, and follow Briar's reflection route.",
        "Mangletooth's no locks the tested route. Hold, Root Key, and Mirror need separate custodians.",
        "Donella of the Marsh", "Erevan the Shadow", "Joni Pumpernickel",
        "Goblin Ambusher", "Goblin Sharpshooter", "Nibsy",
        "You asked every piece of me. Then you forgot to ask the living.",
        "Briar protects Mangletooth's refusal and still steals Mirror and Root Key. The group ratifies no solo tail, no uninformed runner, and checked fear.",
        "cards/donellaOfTheMarsh.png"
    },
    {
        "mw13_making_credit", "Making Credit", "Chapter 17",
        "JUMP, REVEAL, RHYTHM EVIDENCE, CORROBORATION",
        "Route both hunters and confirm the Feyward road without converting witnesses into allies.",
        "Use Scooter's lines and the Tracker's knight jump. Mounted Tracker reveals hidden enemies and returns unmounted once.",
        "Scooter", "Swamp Tracker", "Erevan the Shadow",
        "Goblin Ambusher", "Goblin Sharpshooter", "Mog",
        "Ask what we chose before you ask what we saw.",
        "Mog and Braun provide separate motive and fact. Gearjaw's eight-tooth rhythm and unnatural blue trace point to Fizzlewick's shipment.",
        "cards/scooter.png"
    },
    {
        "mw14_public_lie", "The Public Lie", "Chapter 18",
        "SPLIT OBJECTIVES, CROWD PRESSURE, PUBLIC EVIDENCE",
        "Route both pressure agents, keep the hearing open, and move the charter into public custody.",
        "Classify claims as true, omitted, or claim. Vanya surrenders herself and a dead route to make room.",
        "Joni Pumpernickel", "Mirewatch Informant", "Erevan the Shadow",
        "Thaeron Baelstone", "Blackthorn Debt Collector", "Mara",
        "You should have told us your name before we chose your voice.",
        "The charter reaches Joni and Thaeron's offer becomes public. Vanya enters custody; Reed loses advocate authority but not his witness voice.",
        "cards/joniPumpernickel.png"
    },
    {
        "mw16_gossiping_trees", "The Stair Between Gossiping Trees", "Chapter 20",
        "HAZARDOUS ROUTES, TEMPO COST, EXACT PROMISES",
        "Route both pipe guards, crimp the poison line, and cross on Briar's narrow promise.",
        "Stopping the pipe costs a pursuit turn. Promise only to carry the Key to the living World Tree - never to use or obey it.",
        "Scooter", "Birdie the Wise", "Donella of the Marsh",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Birdie the Wise",
        "Leave the poison running and we become the reason Half-Ear's brood dies.",
        "The pipe is crimped, seven travelers cross, and Mangletooth stays with the Hold. Four relationship voices open the closure box.",
        "cards/birdieTheWise.png"
    },
    {
        "mw17_factory_heaven", "A Factory in Heaven", "Chapter 21",
        "PIVOT, TELEPORT, PUSH, RELATIONSHIP RECOVERY",
        "Route both factory guards, free the moth sacks, and approach Vesper without force.",
        "No attack breaks the false kitchen; Erevan must call Donella and admit he wants another minute.",
        "Erevan the Shadow", "Donella of the Marsh", "Scooter",
        "Blackthorn Foreman", "Goblin Sharpshooter", "Erevan the Shadow",
        "I know this life is false. I still want another minute inside it.",
        "The party frees moth cargo, destroys the harvester together, and plays Elliot's tune. Vesper chooses temporary company after 103 years.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw18_allies_dishonestly", "Allies Acquired Dishonestly", "Chapter 23",
        "NONLETHAL GOALS, TEMPORARY TERMS, ESCORT",
        "Route both Company toll guards and cross without defeating the Seelie captain.",
        "Pavo, Nettle, and Zippy each choose bounded terms. A borrowed badge opens a gate; it buys no person.",
        "Reed Baelstone", "Pavo Quickstep", "Nettle Starbright",
        "Blackthorn Debt Collector", "Goblin Ambusher", "Nettle Starbright",
        "A royal badge can make an unlawful gate look patient.",
        "Four detained fey and the party cross. Pavo, Nettle, and Zippy agree only to first grove sight and one attempt - no obedience.",
        "cards/nettleStarbright.png"
    },
    {
        "mw19_road_keeps_one", "The Road That Keeps One", "Chapter 24",
        "MOVING BOUNDARY, AUTHORITY TRANSFER, LOST SUPPORT",
        "Route both boundary crews and move the refugees to safety before the gray line closes.",
        "Refugees lift one another; they are not cargo. Transfer road, bodies, food, and retreat authority before Rowan stays.",
        "Reed Baelstone", "Birdie the Wise", "Scooter",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Heartwood Gate",
        "WHAT WILL THE ROAD NOT RETURN?",
        "Reed answers that revenge will not return his name. All refugees reach an arch; Rowan stays because the road has people.",
        "cards/reedBaelstone.png"
    },
    {
        "mw20_monster_rules", "The Monster Has Rules", "Chapter 25",
        "TARGET-FILTER INFERENCE, OBSERVATION, RESTRAINT",
        "Route both tack handlers, infer Ashenfang's imposed rule, and free the unmarked grass-woman.",
        "Observe three targets before acting. Remove a silver-tree mark and play Elliot's tune; Ashenfang is Sylvara wounded into a rule.",
        "Reed Baelstone", "Donella of the Marsh", "Birdie the Wise",
        "Grask", "Blackthorn Foreman", "Donella of the Marsh",
        "The monster is not hiding Sylvara. This is Sylvara, wounded into a rule.",
        "Fen knowingly tests one crossing and dies when a hidden Company tack changes the case. Ashenfang crushes the tack; the incomplete result is recorded.",
        "cards/donellaOfTheMarsh.png"
    },
    {
        "mw21_four_losses", "Four Ways to Lose", "Chapter 26",
        "SIMULTANEOUS METERS, ADAPTIVE COUNTERPLAY, AUTONOMY",
        "Route both cage wardens while keeping cages, root, marks, and learning below failure.",
        "Every action leaves another track moving. Gearjaw may choose to help, but cannot be commanded; Mog remains a prisoner.",
        "Erevan the Shadow", "Scooter", "Donella of the Marsh",
        "Grask", "Blackthorn Foreman", "Fizzlewick Gearwright",
        "Cages, root, marks, learning. You have enough hands to lose four different ways.",
        "Six cage groups escape, Gearjaw exposes the learning core, and Mog survives Grask's knife. Donella gives Sylvara a place to answer.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw22_clean_shot", "The Clean Shot", "Chapter 27",
        "LINE OF SIGHT, COOLDOWN, PATIENCE, SYSTEM TARGETS",
        "Route both cable guards to expose the line, then take the harder system shot.",
        "Birdie's easy target is Ashenfang's throat. The correct target appears one beat later: the six-strand cable.",
        "Birdie the Wise", "Scooter", "Reed Baelstone",
        "Grask", "Goblin Sharpshooter", "Birdie the Wise",
        "The monster is not the mechanism.",
        "Birdie breaks the cable and permanently loses her right shoulder. Gearjaw destroys its core to save Scooter; Ashenfang lives.",
        "cards/birdieTheWise.png"
    },
    {
        "mw23_choice_not_cure", "A Choice, Not a Cure", "Chapter 28",
        "TRANSFORM, OPT-IN NETWORK, RESPECTED REFUSAL",
        "Route both Company interferers, build Sylvara's revocable network, and leave every no outside it.",
        "Ask once. Nettle, Scooter, and prisoners say no; never reprompt. Being unbound lets Nettle cut Lash's wire.",
        "Donella of the Marsh", "Erevan the Shadow", "Reed Baelstone",
        "Blackthorn Alchemist", "Blackthorn Foreman", "Sylvara",
        "Unequal. Revocable. Enough roads that no one body becomes the bridge.",
        "Every refusal holds. Sylvara inserts the Root Key, Nettle grounds the wire, and Ashenfang returns to Sylvara - restored, not cured.",
        "cards/donellaOfTheMarsh.png"
    },
    {
        "mw24_agent_not_heir", "Agent, Not Heir", "Chapter 29",
        "TERRAIN COMBO, SURRENDER, HIDDEN CONTINGENCY",
        "Route both commanders, collapse the cages, close the Baelstone gate collectively, and preserve the north fragment.",
        "Reed's blood identifies the door but owns no work. Offer Victor breath and a mortal venue - never immunity.",
        "Reed Baelstone", "Pavo Quickstep", "Nettle Starbright",
        "Victor Greyshard", "Grask", "Reed Baelstone",
        "My blood identifies the door. It does not own their work. Agent, not heir.",
        "Prisoners collapse cages on Grask. Victor surrenders, burns most of the register, then attacks Nettle; Pavo kills him after terms break.",
        "cards/reedBaelstone.png"
    }
};

const std::vector<MissionEntry> BlackthornEntries = {
    {
        "bt02_customs_bell", "The Customs Bell", "Chapter 1 customs",
        "TRANSFORM, LINE OF SIGHT, TAX, OBSERVATION",
        "Control 14 squares to hold observation posts until four descriptions and Reed's crest are recorded.",
        "Use Raise Gun, Fire through a clear line, and let the Collector's real Tax trigger at turn start.",
        "Braun Stonefist", "Goblin Sharpshooter", "Blackthorn Debt Collector",
        "Reed Baelstone", "Donella of the Marsh", "Blackthorn Debt Collector",
        "A description becomes a route when the second bell agrees.",
        "Reed's Baelstone moon and nine stars supply the fourth clue. The convoy escapes; this dossier records rather than invents a massacre.",
        "cards/blackthornDebtCollector.png"
    },
    {
        "bt04_terms_conditions", "Terms and Conditions", "Chapter 3",
        "DEMATERIALIZE, PASS-THROUGH, COLLISION REVEAL",
        "Use two Ambushers' printed actions to cross real blockers and reveal a hidden destination collision.",
        "Use Dematerialize and Sneak Around through real Veterans; landing on hidden Erevan reveals and stuns him.",
        "Goblin Ambusher", "Blackthorn Alchemist", "Blackthorn Debt Collector",
        "Erevan the Shadow", "Juniper Flash", "Blackthorn Observer",
        "The blank question drew Lio. The marked timber will draw what protects him.",
        "The watcher chooses chains and children. Erevan destroys one lantern, but the surviving record captures Company curiosity's cost.",
        "cards/goblinAmbusher.png"
    },
    {
        "bt05_freight_office", "The Freight Office", "Chapter 6",
        "CAPTURE STAGING, DAMAGE, LONG ATTACK",
        "Control 14 squares to mark Hara's route and force the intruders through the predicted hatch.",
        "Swing Axes demonstrates Capture staging; Mog attacks a survivor; Charge defeats a distant real unit.",
        "Grask", "Mog", "Goblin Sharpshooter",
        "Reed Baelstone", "Donella of the Marsh", "Victor Greyshard",
        "They expect an office. Give them a performance of an office.",
        "Authentic chits leave while the planted confession stays. Hara is marked and closes the hatch; she remains alive.",
        "cards/Grask.png"
    },
    {
        "bt06_receipt_book", "The Receipt Book", "Chapter 7",
        "CONTROL DENIAL, HIDDEN OBJECTIVE, WITHDRAWAL",
        "Control 14 squares while recording five rescue motions, then extract the recorder.",
        "The wagon is bait, not a kill objective. Use Mog and Collectors to narrow the road while helpers reveal their methods.",
        "Grask", "Mog", "Blackthorn Debt Collector",
        "Birdie the Wise", "Scooter", "Grask",
        "The wagon is bait. Let every helper show the hand they use.",
        "The wagon escapes as expected. Five motions reach Victor, who turns one rescue into a town-sized invoice and makes Torren disappear.",
        "cards/Grask.png"
    },
    {
        "bt07_debtor_prison", "The Debtor Prison", "Chapter 9",
        "LOS SCREEN, HEAL, DISABLE, ADAPTIVE TARGETING",
        "Control 14 squares to complete the Guardian's pattern, then withdraw before the flood.",
        "Raise and lower the Sharpshooter around blockers. Alchemist heals friends or disables enemies for two turns; Braun holds the lane.",
        "Braun Stonefist", "Goblin Sharpshooter", "Blackthorn Alchemist",
        "Reed Baelstone", "Marshland Veteran", "Fizzlewick Gearwright",
        "The Guardian predicts which body courage moves first.",
        "Garrett dies protecting Little Fen. Prisoners vote to flood the mill; Company units withdraw and lose the prison.",
        "cards/blackthornAlchemist.png"
    },
    {
        "bt08_north_lock", "The North Lock", "Chapter 10",
        "COMMAND, ADJACENT READINESS, FLOOD CLOCK, ESCAPE",
        "Control 14 squares to expose the Bluewater route and extract Thaeron without killing Reed.",
        "Command grants one adjacent ready unit an activation. Reed refuses amnesty, inheritance, and authority without a seal.",
        "Thaeron Baelstone", "Blackthorn Foreman", "Blackthorn Debt Collector",
        "Reed Baelstone", "Vanya Bluewater", "Thaeron Baelstone",
        "The care was real. So is the necessity I built around it.",
        "Reed's flare writes the rescue road he would not sell. Thaeron leaves after exposing it; Missus Vale dies and Bluewater is destroyed.",
        "cards/thaeronBaelstone.png"
    },
    {
        "bt09_published_mystery", "The Published Mystery", "Chapters 11-13",
        "CAPTURE, MULTI-ROUTE SETUP, PREDICTED APPETITES",
        "Control 14 squares to steer rescuers toward the Root Key while the buyer register burns.",
        "Grask Captures a gate piece, not a protagonist. Lash's mechanism needs the Key carried by choice and witnesses alive.",
        "Grask", "Blackthorn Foreman", "Goblin Ambusher",
        "Donella of the Marsh", "Reed Baelstone", "Remy",
        "Four roads look safer than one invitation.",
        "The Root Key leaves with rescuers, the register burns, and sixteen survive. Three names remain missing; Remy's later escape is Erevan's choice.",
        "cards/Grask.png"
    },
    {
        "bt10_stolen_road", "The Stolen Road", "Chapters 15-17",
        "HIDDEN COUNTERPLAY, EVIDENCE VERSUS POSSESSION",
        "Control 14 squares long enough to tag Briar's route and carry one blue-trace vial home.",
        "The Ambusher may shadow but never control Briar. Use pass-through without collision; Braun and Mog corroborate separately.",
        "Goblin Ambusher", "Braun Stonefist", "Mog",
        "Reed Baelstone", "Swamp Tracker", "Briar",
        "You mistook carrying a thing for owning its purpose.",
        "Briar keeps Root Key and Mirror. Gearjaw's trace plus witness statements identify Fizzlewick's Feyward shipment.",
        "cards/goblinAmbusher.png"
    },
    {
        "bt11_public_lie", "The Public Lie", "Chapter 18",
        "HERO PROTECTION, COMMAND, RESOURCES, NONCOMBAT PRESSURE",
        "Control 14 squares to keep the hearing open, log the charter, and withdraw Thaeron.",
        "Spend on food and medicine, choose one selective truth, reject outright lies, and keep violence from destroying trust.",
        "Thaeron Baelstone", "Blackthorn Debt Collector", "Braun Stonefist",
        "Reed Baelstone", "Vanya Bluewater", "Thaeron Baelstone",
        "A public truth needs a bowl, a witness, and one omitted name.",
        "The charter is logged and Vanya survives in custody. Reed publishes the offer and refuses to abandon the town; Thaeron withdraws.",
        "cards/thaeronBaelstone.png"
    },
    {
        "bt12_break_charter", "Break the Charter", "Chapter 19",
        "SUMMON, TRANSFORM, CAPTURE, CHANGING ALLEGIANCE",
        "Control 14 squares to reach the register and extract one measurement without harming the shelter.",
        "Use Foreman and Lumberjack lanes, transform Sharpshooter, and Capture a barricade. Children are inviolable.",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Goblin Sharpshooter",
        "Reed Baelstone", "Birdie the Wise", "Braun Stonefist",
        "That is not a target order. It is an excuse written as one.",
        "Braun refuses Victor and is killed outside player control. Mog offers terms; the register burns as the Company withdraws.",
        "cards/braunStonefist.png"
    },
    {
        "bt13_feyward_transit", "Feyward Transit", "Chapters 20-22",
        "GATHER, SUMMON, TRAIL, HEAL/DISABLE, TRADE-OFFS",
        "Control 14 squares to move one measurement cylinder through the factory route.",
        "Gather adds five; Foreman summons forward; Grove Sister leaves Sapling control; Alchemist heals or disables.",
        "Grove Sister", "Blackthorn Foreman", "Blackthorn Lumberjack",
        "Reed Baelstone", "Donella of the Marsh", "Foreman",
        "The spill is not waste. It is the price moved somewhere without a ledger.",
        "One cylinder escapes. Vespara's contract did not ask which grove could be spared; it asked which grove would be spent.",
        "cards/groveSister.png"
    },
    {
        "bt14_move_boundary", "Move the Boundary", "Chapters 23-24",
        "LARGE FOOTPRINTS, BLOCKERS, PUSH COLLISION, EXTRACTION",
        "Control 14 squares, record three boundary corrections, and withdraw the measurement team.",
        "A large machine needs its whole footprint clear. Blocked pushes damage it; Seelie terms and Rowan's decision stay autonomous.",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Grove Sister",
        "Pavo Quickstep", "Nettle Starbright", "Nettle Starbright",
        "A border that walks is still being pushed by someone.",
        "Three measurements leave. Pavo rejects ownership, Zippy finds the bridge, and Rowan stays by choice to hold the refugee gate.",
        "cards/nettleStarbright.png"
    },
    {
        "bt15_monster_rules", "The Monster Has Rules", "Chapters 25-26",
        "ASHENFANG, DIAGONAL RANGE, DISABLE",
        "Use Entangling Lunge on a real opposing unit and observe the status duration.",
        "Ashenfang moves and attacks diagonally one or two squares; Entangling Lunge Disables for two owner turns.",
        "Ashenfang", "Blackthorn Alchemist", "Grask",
        "Blackthorn Lumberjack", "Blackthorn Debt Collector", "Narrator",
        "The monster follows the hurt hidden under her tack.",
        "The target rule is exposed. Fen dies in the disclosed crossing; the grass-woman and Sylvara survive.",
        "cards/Ashenfang.png"
    },
    {
        "bt16_clean_shot", "The Clean Shot", "Chapters 27-28",
        "CAPTURE, ANCHORS, CONSENT, LOSS WITH INFORMATION",
        "Control 14 squares to hold measurement anchors and extract Fizzlewick's partial record.",
        "Grask attacks long and Captures braces diagonally. Every refusal remains outside the network without penalty.",
        "Grask", "Goblin Sharpshooter", "Blackthorn Alchemist",
        "Birdie the Wise", "Nettle Starbright", "Gearjaw",
        "CORE ROUTE ACCEPTED. SMALL BODY ROUTE DENIED.",
        "Birdie's shot costs her shoulder; Gearjaw destroys its core for Scooter. Sylvara is restored, not cured; Fizzlewick escapes.",
        "cards/Grask.png"
    }
};

StoryMission mirewatchGildedHold()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[0]);
    mission.objective =
        "Defeat both customs guards with the Smuggler's repeated printed action and preserve every traveler.";
    mission.pieces = {
        piece("smuggler", "Resistance Smuggler", 1, 5, 1),
        piece("donella", "Donella of the Marsh", 1, 4, 1),
        piece("erevan", "Erevan the Shadow", 1, 6, 1),
        piece("telos", "Telos the Merchant", 1, 2, 1),
        piece("hold_guard", "Blackthorn Debt Collector", 2, 3, 3, 1),
        piece("customs_guard", "Blackthorn Debt Collector", 2, 2, 4, 1)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"smuggler", "donella", "erevan", "telos"};
    mission.aftermath.push_back(panel("Narrator",
        "The winged captive reverses the splinter from inside the cage. The party protects her exit; it does not recruit or rename her.",
        "cards/gildedCage.png"));
    mission.aftermath.push_back(panel("Asta",
        "Twenty-six pounds around the Hold, thirty-eight into baskets, four hundred twelve left to declare.",
        "cards/gildedCage.png"));
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "smuggler", 3, 3,
            "SWASHBUCKLE BLADE - FIRST GUARD", "Use the Smuggler's diagonal Swashbuckle Blade from B6 to the Debt Collector at D4.",
            "Drag the Resistance Smuggler onto the highlighted guard.", "hold_guard"),
        scripted(StoryActionKind::Attack, 1, "smuggler", 2, 4,
            "REPEAT THE SAME ACTION", "The repeat lock is active. Continue diagonally from D4 to the Debt Collector at E3.",
            "A repeated action must use Swashbuckle Blade again, or you must pass.", "customs_guard")
    };
    mission.masteryCards = {"Resistance Smuggler"};
    mission.masteryRules = {
        "diagonal-movement", "attacking-movement", "repeat-lock", "pass-or-repeat"};
    return mission;
}

StoryMission mirewatchWatcher()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[1]);
    mission.objective =
        "Defeat both Company guards with Juniper's ranged Spark and Erevan's close Shadow Blade.";
    mission.pieces = {
        piece("juniper", "Juniper Flash", 1, 3, 1),
        piece("erevan", "Erevan the Shadow", 1, 5, 1),
        piece("donella", "Donella of the Marsh", 1, 6, 1),
        piece("far_guard", "Blackthorn Lumberjack", 2, 3, 3, 1),
        piece("near_guard", "Blackthorn Debt Collector", 2, 5, 2, 1)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"juniper", "erevan", "donella"};
    mission.aftermath.push_back(panel("Erevan the Shadow",
        "It could have protected itself. It protected the children. That answer changes the job.",
        "cards/erevanTheShadow.png"));
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "juniper", 3, 3,
            "JUNIPER'S RANGE", "Fire Spark two squares along the row into the wounded Lumberjack at D4. Its red badge shows 1 Health, so Spark's one damage destroys it.",
            "Juniper is stationary while attacking; choose the highlighted guard.", "far_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE ACTION, THEN PASS", "End Turn so the watcher can choose.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The Company guards hold; play returns to Mirewatch.",
            "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "erevan", 5, 2,
            "EREVAN AT CLOSE RANGE", "Use Shadow Blade on the Debt Collector beside Erevan at C6.",
            "Drag Erevan onto the adjacent guard.", "near_guard", {},
            {panel("Narrator",
                "Beyond the fight, the watcher turns its body toward Pellan and Seli instead of protecting itself. This choice is story, not a board action.",
                "cards/erevanTheShadow.png")})
    };
    mission.masteryCards = {"Juniper Flash"};
    mission.masteryRules = {
        "orthogonal-ranged-attack", "range-two", "one-activation-per-turn"};
    return mission;
}

StoryMission mirewatchOffice()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[2]);
    mission.objective =
        "Cross two real Lumberjack blockers while hidden, then use Hidden Shove on the Debt Collector; Hara's choice remains in the story.";
    mission.pieces = {
        piece("erevan", "Erevan the Shadow", 1, 3, 1),
        piece("informant", "Mirewatch Informant", 1, 5, 1),
        piece("reed", "Reed Baelstone", 1, 6, 1),
        piece("screen_one", "Blackthorn Lumberjack", 2, 3, 2),
        piece("screen_two", "Blackthorn Lumberjack", 2, 3, 3),
        piece("ledger_guard", "Blackthorn Debt Collector", 2, 2, 4)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"erevan", "informant", "reed"};
    mission.aftermath.push_back(panel("Hara Dole",
        "Warn my mother. I keep the dye here. I close the hatch when I choose.",
        "cards/mirewatchInformant.png"));
    mission.script = {
        scripted(StoryActionKind::UseAbility, 1, "erevan", -1, -1,
            "DEMATERIALIZE EREVAN", "Select Erevan and use the printed Dematerialize ability.",
            "Erevan must dematerialize before crossing the occupied lane."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDDEN PIECES ADD NO CONTROL", "End Turn. While hidden, Erevan adds no adjacent control influence; existing square ownership remains when influence is tied.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OFFICE WATCHES THE WRONG DOOR", "The guards hold their rehearsed sight lines.",
            "Mission data error."),
        scripted(StoryActionKind::Move, 1, "erevan", 3, 4,
            "PASS THROUGH THE SCREEN", "Move Erevan from B4 to E4, through both occupied squares.",
            "Only Erevan's hidden Fade Through Shadow can pass through those blockers."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "STAY UNSEEN", "End Turn while Erevan remains hidden beside the Debt Collector.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "erevan", 2, 4,
            "HIDDEN SHOVE", "Use Hidden Shove on the Debt Collector at E3.",
            "Use Erevan's printed hidden adjacent push.", "ledger_guard")
    };
    mission.masteryCards = {"Erevan the Shadow"};
    mission.masteryRules = {
        "dematerialize", "hidden-adds-no-control", "pass-through", "push"};
    return mission;
}

StoryMission mirewatchWagonRescue()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[3]);
    mission.objective =
        "Clear the guarded route with six characters' printed actions, then survive a real Bull Gator counterattack. The wagon opens only in the aftermath panel.";
    mission.pieces = {
        piece("birdie", "Birdie the Wise", 1, 3, 1),
        piece("scooter", "Scooter", 1, 5, 1),
        piece("veteran", "Marshland Veteran", 1, 6, 1),
        piece("smuggler", "Resistance Smuggler", 1, 1, 1),
        piece("tracker", "Swamp Tracker", 1, 7, 1, 2),
        piece("spearman", "Bog Spearman", 1, 4, 1),
        piece("notice_guard", "Blackthorn Debt Collector", 2, 3, 4, 1),
        piece("durable_guard", "Blackthorn Lumberjack", 2, 6, 3, 2),
        piece("route_guard_one", "Blackthorn Debt Collector", 2, 2, 2, 1),
        piece("route_guard_two", "Blackthorn Debt Collector", 2, 1, 3, 1),
        piece("spear_guard", "Blackthorn Debt Collector", 2, 2, 3, 1),
        piece("rebirth_gator", "Bull Gator", 2, 5, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {
        "birdie", "scooter", "veteran", "smuggler", "spearman"};
    mission.aftermath.push_back(panel("Narrator",
        "With the route clear, the Mudfen wagon opens. The trained gator tore the mount away, but the scout rose on foot and kept the road.",
        "cards/swampTracker.png"));
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "birdie", 3, 4,
            "BIRDIE - LONGBOW SHOT", "Shoot the Debt Collector three squares along the row.",
            "Drag Birdie onto the highlighted target at E4.", "notice_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COOLDOWN BEGINS", "End Turn. Birdie's bow will miss her next activation.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scripted(StoryActionKind::Move, 1, "scooter", 7, 3,
            "SCOOTER - RIVER DASH", "Use River Dash diagonally from B6 to empty D8.",
            "River Dash is move-only; choose highlighted D8."),
        scripted(StoryActionKind::Move, 1, "scooter", 6, 4,
            "SCOOTER - REQUIRED REPEAT", "Continue the same River Dash from D8 to empty E7.",
            "Finish Scooter's printed repeat with the same action."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE BRIDGE PIN MOVES", "End Turn after both Dash actions.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "veteran", 6, 3,
            "VETERAN - ADVANCE", "Strike the wounded Lumberjack two squares along the row.",
            "Use the Veteran's printed Advance. The target survives, so the Veteran stops short.",
            "durable_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SURVIVING DEFENDER", "End Turn with the Veteran short of the surviving screen.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "smuggler", 2, 2,
            "SMUGGLER - FIRST BLADE", "Use Swashbuckle Blade diagonally from B2 into the Debt Collector at C3.",
            "Use Swashbuckle Blade on the first guard.", "route_guard_one"),
        scripted(StoryActionKind::Attack, 1, "smuggler", 1, 3,
            "SMUGGLER - SECOND BLADE", "Repeat diagonally from C3 into the Debt Collector at D2.",
            "The Smuggler must finish the repeated action.", "route_guard_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE REAR OPENING CLEARS", "End Turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The remaining guards hold; play returns to Mirewatch.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "spearman", 2, 3,
            "SPEARMAN - SPEAR THRUST", "Use Spear Thrust diagonally from B5 into the Debt Collector at D3.",
            "Use the Bog Spearman's printed diagonal attack.", "spear_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE LOCK LEFT", "End Turn for the mounted scout's jump.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The final guard and gator hold; play returns to Mirewatch.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "tracker", 6, 3,
            "TRACKER - FROGBACK LEAP", "Jump from B8 into the weakened Lumberjack at D7.",
            "The Tracker uses its printed Frogback Leap and ignores intervening squares.", "durable_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "BRACE FOR THE BITE", "End Turn so the surviving gator can counterattack.",
            "End Turn after the Tracker's Leap."),
        scripted(StoryActionKind::Attack, 2, "rebirth_gator", 6, 3,
            "BITE TRIGGERS REBIRTH", "The Bull Gator uses its ordinary Bite on the wounded mounted Tracker.",
            "Mission data error.", "tracker")
    };
    mission.masteryCards = {
        "Birdie the Wise", "Scooter", "Marshland Veteran", "Resistance Smuggler",
        "Bog Spearman", "Swamp Tracker"};
    mission.masteryRules = {
        "cooldown", "repeat-lock", "orthogonal-movement", "diagonal-movement",
        "attacking-move-survivor-staging", "knight-jump", "pass-through", "rebirth",
        "normal-enemy-attack"};
    return mission;
}

StoryMission mirewatchAuction()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[7]);
    mission.objective =
        "Use Vanya's printed Blade Dance twice to defeat the two auction guards; Nima's self-release remains a story event.";
    mission.pieces = {
        piece("vanya", "Vanya Bluewater", 1, 5, 1),
        piece("joni", "Joni Pumpernickel", 1, 6, 1),
        piece("birdie", "Birdie the Wise", 1, 7, 1),
        piece("first_guard", "Blackthorn Debt Collector", 2, 4, 2),
        piece("second_guard", "Blackthorn Debt Collector", 2, 3, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"vanya", "joni", "birdie"};
    mission.aftermath.push_back(panel(
        "Nima", "I was working this lock before you bid on me.", "cards/vanyaBluewater.png"));
    mission.aftermath.push_back(panel(
        "Chronicle",
        "Nima opens her own lock. Joni records the Mirror and Root Key under separate custodians.",
        "cards/joniPumpernickel.png"));
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "vanya", 4, 2,
            "VANYA - BLADE DANCE", "Drag ACT-labeled Vanya onto the Debt Collector at C5. Her printed Blade Dance has Repeat 1: after attacking and moving diagonally, use it again or Pass before another piece can act. This clears the auction route toward Nima.",
            "Use Vanya's diagonal Blade Dance on the highlighted enemy.", "first_guard"),
        scripted(StoryActionKind::Attack, 1, "vanya", 3, 3,
            "FINISH THE DANCE", "Blade Dance is repeat-locked: finish the same action on the second Debt Collector at D4 before choosing anyone else. Nima opens her own lock in the following story panel.",
            "Finish Blade Dance before choosing another piece.", "second_guard")
    };
    mission.masteryCards = {"Vanya Bluewater"};
    mission.masteryRules = {"repeat-lock", "attacking-movement"};
    return mission;
}

StoryMission blackthornCustoms()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[0]);
    mission.objective =
        "Open Braun's lane, raise the Sharpshooter's gun, fire through a clear line, and observe Tax repeat at normal owner-turn starts.";
    mission.pieces = {
        piece("braun", "Braun Stonefist", 1, 3, 1),
        piece("sharpshooter", "Goblin Sharpshooter", 1, 5, 1),
        piece("collector", "Blackthorn Debt Collector", 1, 6, 1),
        piece("signal_guard", "Bull Gator", 2, 5, 7),
        piece("crest_guard", "Blackthorn Debt Collector", 2, 7, 2)};
    mission.playerResources = 0;
    mission.enemyResources = 12;
    mission.requiredSurvivorRoles = {"braun", "sharpshooter", "collector"};
    mission.script = {
        scripted(StoryActionKind::Move, 1, "braun", 3, 4,
            "BRAUN - OPEN THE ROW", "Move Braun three squares along the row from B4 to E4.",
            "Use Braun's printed orthogonal Charge as movement into the empty square."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE NORMAL ACTION", "End Turn after Braun opens the observation lane.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing side holds; play returns to Blackthorn.", "Mission data error."),
        scripted(StoryActionKind::UseAbility, 1, "sharpshooter", -1, -1,
            "RAISE THE GUN", "Select the Goblin Sharpshooter and use Raise Gun.",
            "The lowered gun can move; the raised gun can fire."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "STATE PERSISTS", "End Turn with the gun raised.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing side holds; play returns to Blackthorn.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "sharpshooter", 5, 7,
            "SHARPSHOOTER - FIRE", "Use Fire along the clear row at the Bull Gator on H6. Three damage leaves its fourth Health intact.",
            "Use the raised Sharpshooter's printed Fire action.", "signal_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COLLECTOR'S TAX", "End Turn so the Debt Collector can transfer Tax next turn.",
            "End Turn to observe Tax."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "TAX TRANSFERS, NEVER CREATES DEBT", "The Collector takes at most the resources the other side actually has.",
            "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "collector", 7, 2,
            "COLLECTOR - KNIFE STAB", "Use Knife Stab diagonally on the opposing Debt Collector at C8.",
            "Drag your Debt Collector onto the highlighted enemy.", "crest_guard")
    };
    mission.masteryCards = {
        "Braun Stonefist", "Goblin Sharpshooter", "Blackthorn Debt Collector"};
    mission.masteryRules = {
        "orthogonal-long-movement", "transform", "state-specific-actions",
        "line-of-sight", "tax", "health-and-destruction"};
    return mission;
}

StoryMission blackthornTerms()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[1]);
    mission.objective =
        "Use two Ambushers' real Dematerialize and Sneak Around actions to cross occupied squares and expose a hidden destination collision.";
    mission.pieces = {
        piece("ambusher_path", "Goblin Ambusher", 1, 3, 1),
        piece("ambusher_collision", "Goblin Ambusher", 1, 5, 1),
        piece("lane_blocker_one", "Marshland Veteran", 2, 3, 2),
        piece("lane_blocker_two", "Marshland Veteran", 2, 3, 3),
        piece("erevan", "Erevan the Shadow", 2, 5, 2)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"ambusher_path", "ambusher_collision", "erevan"};
    mission.script = {
        scripted(StoryActionKind::UseAbility, 1, "ambusher_path", -1, -1,
            "DEMATERIALIZE THE FIRST AMBUSHER", "Select the Ambusher at B4 and use the printed Dematerialize ability.",
            "Dematerialize the upper Ambusher first."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDDEN PIECES ADD NO CONTROL", "End Turn; a hidden piece adds no adjacent control influence, while tied squares keep their prior owner.", "End Turn to continue."),
        scripted(StoryActionKind::UseAbility, 2, "erevan", -1, -1,
            "EREVAN DISAPPEARS", "The opposing scout hides at C6.", "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "BOTH SIDES HIDE INFORMATION", "The watched lanes now contain one unseen body.", "Mission data error."),
        scripted(StoryActionKind::Move, 1, "ambusher_path", 3, 4,
            "SNEAK AROUND VISIBLE UNITS", "Use Sneak Around from B4 through the Veterans on C4 and D4 to E4.",
            "Use the hidden Ambusher's printed pass-through move."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "KEEP THE RECORD SEPARATE", "End Turn before testing the second lane.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HIDDEN SCOUT HOLDS", "Erevan stays concealed in the lower lane.", "Mission data error."),
        scripted(StoryActionKind::UseAbility, 1, "ambusher_collision", -1, -1,
            "DEMATERIALIZE THE SECOND AMBUSHER", "Select the Ambusher at B6 and use the printed Dematerialize ability.",
            "Dematerialize the lower Ambusher."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "UNKNOWN OCCUPANCY", "End Turn. The destination preview cannot expose an enemy's hidden square.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE COLLISION WAITS", "Erevan remains unseen.", "Mission data error."),
        scripted(StoryActionKind::Move, 1, "ambusher_collision", 5, 2,
            "HIDDEN COLLISION TRIGGERS AMBUSH", "Drag the hidden Ambusher toward C6 with Sneak Around. The unseen collision re-resolves as its printed Ambush: Erevan takes 1 damage and materializes disabled; the Ambusher also materializes and stays at B6 because Erevan survives.",
            "Use the highlighted Sneak Around route.")
    };
    mission.masteryCards = {"Goblin Ambusher"};
    mission.masteryRules = {
        "dematerialize", "hidden-adds-no-control", "pass-through",
        "hidden-collision-reveal", "hidden-collision-damage", "collision-stun"};
    return mission;
}

StoryMission blackthornOffice()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[2]);
    mission.objective =
        "Use Grask's two printed attack geometries and Mog's Axe Swing against real opposing units.";
    mission.pieces = {
        piece("grask", "Grask", 1, 3, 1),
        piece("mog", "Mog", 1, 5, 1),
        piece("capture_target", "Marshland Veteran", 2, 4, 2),
        piece("mog_target", "Marshland Veteran", 2, 5, 2),
        piece("charge_target", "Bog Spearman", 2, 3, 5)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"grask", "mog"};
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "grask", 4, 2,
            "GRASK - SWING AXES", "Use Swing Axes on the adjacent Veteran at C5. Because three Health exceeds two damage, the target survives and Grask remains staged in front of it.",
            "Choose Grask's printed diagonal Capture action.", "capture_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "CAPTURE IS ITS OWN ACTION", "End Turn after the staged Capture.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing units hold; play returns to Blackthorn.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "mog", 5, 2,
            "MOG - AXE SWING", "Use Axe Swing on the adjacent Veteran at C6. Two damage leaves one Health.",
            "Drag Mog onto the adjacent Veteran.", "mog_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "A SCREEN BUYS TIME", "End Turn; two damage did not remove the three-health screen.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The final opposing unit holds; play returns to Blackthorn.", "Mission data error."),
        scripted(StoryActionKind::Attack, 1, "grask", 3, 5,
            "GRASK - CHARGE", "Use Charge four squares along the row on the Bog Spearman at F4.",
            "Use Grask's printed orthogonal Charge on the highlighted enemy.", "charge_target")
    };
    mission.masteryCards = {"Grask", "Mog"};
    mission.masteryRules = {
        "capture", "capture-staging", "health-and-destruction",
        "long-orthogonal-attacking-movement"};
    return mission;
}

StoryMission blackthornMonsterRules()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[12]);
    mission.objective =
        "Use Ashenfang's printed Entangling Lunge on a real Veteran and observe its two-owner-turn status duration.";
    mission.pieces = {
        piece("ashenfang", "Ashenfang", 1, 3, 1),
        piece("alchemist", "Blackthorn Alchemist", 1, 6, 1),
        piece("grask", "Grask", 1, 7, 1),
        piece("veteran", "Marshland Veteran", 2, 5, 3),
        piece("sylvara", "Sylvara", 2, 2, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"ashenfang", "alchemist", "grask", "sylvara"};
    mission.briefing.push_back(panel("Donella's later account",
        "The monster is not hiding Sylvara. This is Sylvara, wounded into a rule.",
        "cards/Ashenfang.png"));
    mission.aftermath.push_back(panel("Chronicle",
        "The hidden tack is removed and both unmarked women remain alive. Survival is recorded without becoming ownership.",
        "cards/Ashenfang.png"));
    mission.script = {
        scripted(StoryActionKind::Attack, 1, "ashenfang", 5, 3,
            "ASHENFANG - ENTANGLING LUNGE", "Use Entangling Lunge on the Veteran two diagonal squares away at D6.",
            "Use Ashenfang's printed diagonal-range-two action.", "veteran"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: FIRST OWNER TURN", "End Turn so the marked mechanism misses its first activation.",
            "End Turn to measure Disable."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "FIRST ACTIVATION MISSED", "The disabled mark cannot act.", "Mission data error."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: SECOND OWNER TURN", "End Turn to measure the second skipped activation.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "SECOND ACTIVATION MISSED", "Disable reaches zero only after the second owner turn begins.",
            "Mission data error.")
    };
    mission.masteryCards = {"Ashenfang"};
    mission.masteryRules = {
        "diagonal-range-two", "disable-two-turns", "status-duration"};
    return mission;
}

const std::vector<StoryMission> MirewatchMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(30);
    missions.push_back(mirewatchOpening());
    missions.push_back(mirewatchGildedHold());
    missions.push_back(mirewatchDeployment());
    missions.push_back(mirewatchWatcher());
    missions.push_back(sharedS01());
    missions.push_back(mirewatchOffice());
    missions.push_back(mirewatchWagonRescue());
    missions.push_back(sharedS02());
    missions.push_back(chronicleFromEntry(MirewatchEntries[4]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[5]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[6]));
    missions.push_back(mirewatchAuction());
    missions.push_back(chronicleFromEntry(MirewatchEntries[8]));
    missions.push_back(sharedS03());
    missions.push_back(chronicleFromEntry(MirewatchEntries[9]));
    missions.push_back(sharedS04());
    missions.push_back(chronicleFromEntry(MirewatchEntries[10]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[11]));
    missions.push_back(mirewatchCapstone());
    missions.push_back(chronicleFromEntry(MirewatchEntries[12]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[13]));
    missions.push_back(sharedS05());
    for (std::size_t index = 14; index < MirewatchEntries.size(); ++index)
    {
        missions.push_back(chronicleFromEntry(MirewatchEntries[index]));
    }
    missions.push_back(sharedS06());
    return missions;
}();

const std::vector<StoryMission> BlackthornMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(23);
    missions.push_back(blackthornOpening());
    missions.push_back(blackthornCustoms());
    missions.push_back(blackthornDeployment());
    missions.push_back(blackthornTerms());
    missions.push_back(sharedS01());
    missions.push_back(blackthornOffice());
    missions.push_back(chronicleFromEntry(BlackthornEntries[3]));
    missions.push_back(sharedS02());
    missions.push_back(chronicleFromEntry(BlackthornEntries[4]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[5]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[6]));
    missions.push_back(sharedS03());
    missions.push_back(chronicleFromEntry(BlackthornEntries[7]));
    missions.push_back(sharedS04());
    missions.push_back(chronicleFromEntry(BlackthornEntries[8]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[9]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[10]));
    missions.push_back(sharedS05());
    missions.push_back(chronicleFromEntry(BlackthornEntries[11]));
    missions.push_back(blackthornMonsterRules());
    missions.push_back(chronicleFromEntry(BlackthornEntries[13]));
    missions.push_back(blackthornCapstone());
    missions.push_back(sharedS06());
    return missions;
}();

} // namespace

std::span<const StoryMission> storyMissions(StoryCampaign campaign)
{
    return campaign == StoryCampaign::Blackthorn ? BlackthornMissions : MirewatchMissions;
}

} // namespace bayou::client
