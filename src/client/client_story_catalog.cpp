#include "client_story.hpp"

#include <algorithm>
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
        title == "Maggie Mudroot" || title == "Sylvara" ||
        title == "Prince Vesper" || title == "Caltheriel" ||
        title == "Queen Nyxara";
}

StoryPanel panel(std::string_view speaker, std::string_view text, std::string_view art)
{
    return {speaker, text, art};
}

std::string_view generatedScenarioArtPath(
    StoryCampaign campaign,
    std::string_view missionId)
{
    // These scenes now focus on Juniper's recovery and trapped workers.
    // Their old planning-table illustrations depict details the story removed.
    if (missionId == "s03_published_mystery" ||
        (campaign == StoryCampaign::Seelie && missionId == "se18a_complete_is_a_label"))
    {
        return {};
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se00_what_mirror_remembers")
    {
        return "story/se/se00_what_mirror_remembers/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw02_gilded_hold")
    {
        return "story/mw/mw02_gilded_hold/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw03_town_under_company")
    {
        return "story/mw/mw03_town_under_company/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw04_watcher_protects")
    {
        return "story/mw/mw04_watcher_protects/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "s01_hospitality")
    {
        return "story/mw/s01_hospitality/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw05_watched_office")
    {
        return "story/mw/mw05_watched_office/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw06_cost_seen")
    {
        return "story/mw/mw06_cost_seen/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "s02_no_one_alone")
    {
        return "story/mw/s02_no_one_alone/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw07_twenty_debtors")
    {
        return "story/mw/mw07_twenty_debtors/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw08_lesson_night")
    {
        return "story/mw/mw08_lesson_night/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw09_invitations")
    {
        return "story/mw/mw09_invitations/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw10_beautiful_plan")
    {
        return "story/mw/mw10_beautiful_plan/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw11_no_plan_saves_all")
    {
        return "story/mw/mw11_no_plan_saves_all/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "s03_published_mystery")
    {
        return "story/mw/s03_published_mystery/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "s04_wounds_that_vote")
    {
        return "story/mw/s04_wounds_that_vote/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw13_making_credit")
    {
        return "story/mw/mw13_making_credit/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw14_public_lie")
    {
        return "story/mw/mw14_public_lie/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw15_title_follows_burden")
    {
        return "story/mw/mw15_title_follows_burden/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw16_gossiping_trees")
    {
        return "story/mw/mw16_gossiping_trees/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw17_factory_heaven")
    {
        return "story/mw/mw17_factory_heaven/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "s05_memory_contradicts")
    {
        return "story/mw/s05_memory_contradicts/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw18_allies_dishonestly")
    {
        return "story/mw/mw18_allies_dishonestly/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw19_road_keeps_one")
    {
        return "story/mw/mw19_road_keeps_one/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw20_monster_rules")
    {
        return "story/mw/mw20_monster_rules/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw21_four_losses")
    {
        return "story/mw/mw21_four_losses/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw22_clean_shot")
    {
        return "story/mw/mw22_clean_shot/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw23_choice_not_cure")
    {
        return "story/mw/mw23_choice_not_cure/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw24_agent_not_heir")
    {
        return "story/mw/mw24_agent_not_heir/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "mw25_deed_own_hand")
    {
        return "story/mw/mw25_deed_own_hand/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Mirewatch && missionId == "s06_town_owns_itself")
    {
        return "story/mw/s06_town_owns_itself/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt01_harness_hunger")
    {
        return "story/bt/bt01_harness_hunger/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt02_customs_bell")
    {
        return "story/bt/bt02_customs_bell/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt03_sanctuary_debt")
    {
        return "story/bt/bt03_sanctuary_debt/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt04_terms_conditions")
    {
        return "story/bt/bt04_terms_conditions/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "s01_hospitality")
    {
        return "story/bt/s01_hospitality/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt05_freight_office")
    {
        return "story/bt/bt05_freight_office/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt06_receipt_book")
    {
        return "story/bt/bt06_receipt_book/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "s02_no_one_alone")
    {
        return "story/bt/s02_no_one_alone/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt07_debtor_prison")
    {
        return "story/bt/bt07_debtor_prison/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt08_north_lock")
    {
        return "story/bt/bt08_north_lock/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt09_published_mystery")
    {
        return "story/bt/bt09_published_mystery/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "s03_published_mystery")
    {
        return "story/bt/s03_published_mystery/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt10_stolen_road")
    {
        return "story/bt/bt10_stolen_road/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt11_public_lie")
    {
        return "story/bt/bt11_public_lie/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt12_break_charter")
    {
        return "story/bt/bt12_break_charter/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt13_feyward_transit")
    {
        return "story/bt/bt13_feyward_transit/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt13a_poisoned_root_road")
    {
        return "story/bt/bt13a_poisoned_root_road/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "s05_memory_contradicts")
    {
        return "story/bt/s05_memory_contradicts/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt14_move_boundary")
    {
        return "story/bt/bt14_move_boundary/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt15_monster_rules")
    {
        return "story/bt/bt15_monster_rules/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt16_clean_shot")
    {
        return "story/bt/bt16_clean_shot/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt17_natural_order")
    {
        return "story/bt/bt17_natural_order/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt17a_field_judgment")
    {
        return "story/bt/bt17a_field_judgment/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt17b_open_mastery")
    {
        return "story/bt/bt17b_open_mastery/scenario_v3.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt17c_victor_reckoning")
    {
        return "story/bt/bt17c_victor_reckoning/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt18_deed_own_hand")
    {
        return "story/bt/bt18_deed_own_hand/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "s06_town_owns_itself")
    {
        return "story/bt/s06_town_owns_itself/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se01_cathedral_last_lights")
    {
        return "story/se/se01_cathedral_last_lights/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se02_broken_bridge")
    {
        return "story/se/se02_broken_bridge/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se03_road_small_lights")
    {
        return "story/se/se03_road_small_lights/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se04a_lash_reads_the_road")
    {
        return "story/se/se04a_lash_reads_the_road/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se04_reading_room")
    {
        return "story/se/se04_reading_room/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se05_court_behind_curtain")
    {
        return "story/se/se05_court_behind_curtain/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se06_honey_without_hunger")
    {
        return "story/se/se06_honey_without_hunger/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se07a_queens_price_scene")
    {
        return "story/se/se07a_queens_price_scene/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se07_queens_price")
    {
        return "story/se/se07_queens_price/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se08a_sella_receives_herself")
    {
        return "story/se/se08a_sella_receives_herself/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se08_nine_lives_one_choice")
    {
        return "story/se/se08_nine_lives_one_choice/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se09_dusk_crown")
    {
        return "story/se/se09_dusk_crown/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se10a_caltheriel_walks_out")
    {
        return "story/se/se10a_caltheriel_walks_out/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se10_no_crown_holds_light")
    {
        return "story/se/se10_no_crown_holds_light/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se11a_blackthorn_unmade")
    {
        return "story/se/se11a_blackthorn_unmade/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se11b_city_refuses")
    {
        return "story/se/se11b_city_refuses/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se11_price_of_an_army")
    {
        return "story/se/se11_price_of_an_army/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se12_first_boats_burn")
    {
        return "story/se/se12_first_boats_burn/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se13b_cut_sylvara_chooses")
    {
        return "story/se/se13b_cut_sylvara_chooses/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se13_pearl_and_root")
    {
        return "story/se/se13_pearl_and_root/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se14_rules_under_pressure")
    {
        return "story/se/se14_rules_under_pressure/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se15_bell_and_order")
    {
        return "story/se/se15_bell_and_order/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se16_army_that_can_resign")
    {
        return "story/se/se16_army_that_can_resign/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se17_warm_channel")
    {
        return "story/se/se17_warm_channel/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se18a_complete_is_a_label")
    {
        return "story/se/se18a_complete_is_a_label/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se18c_before_next_dawn")
    {
        return "story/se/se18c_before_next_dawn/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se18d_kindness_without_debt")
    {
        return "story/se/se18d_kindness_without_debt/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se18_what_complete_means")
    {
        return "story/se/se18_what_complete_means/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se19_crypt_under_order")
    {
        return "story/se/se19_crypt_under_order/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se20_harness_anyone_can_leave")
    {
        return "story/se/se20_harness_anyone_can_leave/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se21a_cinderworks_choice")
    {
        return "story/se/se21a_cinderworks_choice/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se21_below_last_stair")
    {
        return "story/se/se21_below_last_stair/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se22a_map_eater")
    {
        return "story/se/se22a_map_eater/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se22b_city_spends_at_night")
    {
        return "story/se/se22b_city_spends_at_night/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se22c_orchard_memory")
    {
        return "story/se/se22c_orchard_memory/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se23_controlled_fall")
    {
        return "story/se/se23_controlled_fall/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se24_first_prison_design")
    {
        return "story/se/se24_first_prison_design/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se25_three_turns_fish_rib")
    {
        return "story/se/se25_three_turns_fish_rib/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se25a_abandoned_receiver")
    {
        return "story/se/se25a_abandoned_receiver/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se26a_hunger_has_no_face")
    {
        return "story/se/se26a_hunger_has_no_face/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se26b_burning_root")
    {
        return "story/se/se26b_burning_root/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se26_vacancy_that_holds")
    {
        return "story/se/se26_vacancy_that_holds/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se27_emperor_at_tree")
    {
        return "story/se/se27_emperor_at_tree/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se27a_emperor_at_whitewash")
    {
        return "story/se/se27a_emperor_at_whitewash/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se28a_two_men_one_vacancy")
    {
        return "story/se/se28a_two_men_one_vacancy/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se28b_seven_corrections")
    {
        return "story/se/se28b_seven_corrections/scenario_v3.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se28c_a_vow_can_end")
    {
        return "story/se/se28c_a_vow_can_end/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se28d_an_unfinished_record")
    {
        return "story/se/se28d_an_unfinished_record/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se28_one_vacancy_seven_hands")
    {
        return "story/se/se28_one_vacancy_seven_hands/scenario_v3.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29a_baalzapub_ascendant")
    {
        return "story/se/se29a_baalzapub_ascendant/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29b_fizzlewick_says_no")
    {
        return "story/se/se29b_fizzlewick_says_no/scenario_v2.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29c_all_authority_returns")
    {
        return "story/se/se29c_all_authority_returns/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29c2_separate_withdrawals")
    {
        return "story/se/se29c2_separate_withdrawals/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29e_hold_the_witness_rail")
    {
        return "story/se/se29e_hold_the_witness_rail/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29d_reed_ends_the_house")
    {
        return "story/se/se29_no_complete_bearer/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29_no_complete_bearer")
    {
        return "story/se/se25a_abandoned_receiver/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se30_names_we_keep")
    {
        return "story/se/se30_names_we_keep/scenario_v1.png";
    }
    return {};
}

void attachGeneratedScenarioArt(
    StoryCampaign campaign,
    std::vector<StoryMission>& missions)
{
    for (StoryMission& mission : missions)
    {
        mission.scenarioArtPath = generatedScenarioArtPath(campaign, mission.id);
    }
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

StoryScriptAction scriptedPieceAction(
    StoryActionKind kind,
    int owner,
    std::string_view actor,
    int row,
    int column,
    std::string_view expectedActionProfile,
    std::string_view heading,
    std::string_view instruction,
    std::string_view correction,
    std::string_view target = {},
    std::string_view effect = {},
    std::vector<StoryPanel> panels = {})
{
    StoryScriptAction action = scripted(
        kind,
        owner,
        actor,
        row,
        column,
        heading,
        instruction,
        correction,
        target,
        effect,
        std::move(panels));
    action.expectedActionProfile = expectedActionProfile;
    return action;
}

StoryScriptAction scriptedAbility(
    int owner,
    std::string_view actor,
    std::string_view expectedAbility,
    std::string_view expectedAbilityLabel,
    std::string_view heading,
    std::string_view instruction,
    std::string_view correction,
    std::string_view expectedSummonTitle = {},
    std::string_view expectedNextAbilityLabel = {},
    std::vector<StoryPanel> panels = {})
{
    StoryScriptAction action = scripted(
        StoryActionKind::UseAbility,
        owner,
        actor,
        -1,
        -1,
        heading,
        instruction,
        correction,
        {},
        {},
        std::move(panels));
    action.expectedAbility = expectedAbility;
    action.expectedAbilityLabel = expectedAbilityLabel;
    action.expectedSummonTitle = expectedSummonTitle;
    action.expectedNextAbilityLabel = expectedNextAbilityLabel;
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
    mission.lesson = "STORY SCENE";
    mission.objective =
        "Watch what happens next. Choose Continue to move through the scene.";
    mission.hint =
        "Choose Continue to follow the story. Battles teach you how to play.";
    mission.briefing.assign(panels.begin(), panels.end());
    mission.objectiveSpec.kind = StoryObjectiveKind::StoryOnly;
    return mission;
}

std::string_view chronicleContextFor(std::string_view missionId)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 28> Contexts = {{
        {"mw02_gilded_hold",
            "Donella finds her friend dead beside a gold cage. Someone is still alive inside. Guards may return at any moment."},
        {"mw07_twenty_debtors",
            "Twenty prisoners are trapped in a mill. A machine guards the escape route. Reed and his friends must get them out before more soldiers arrive."},
        {"mw08_lesson_night",
            "Reed leaves the refuge alone to meet his uncle. Thaeron offers to save his friends, but Company boats are already hunting them."},
        {"mw09_invitations",
            "After the refuge burns, the survivors find a way into a hidden prison. The door needs blood from three willing people to open."},
        {"mw11_no_plan_saves_all",
            "The Vault is a trap. Gates slam shut and the escape paths break apart. The rescuers race to get the prisoners out."},
        {"mw13_making_credit",
            "The survivors reach Mirewatch and join forces against Blackthorn. A stolen machine may reveal where the missing people were taken."},
        {"mw14_public_lie",
            "Thaeron faces Reed before the town. Guards push into the crowd as he reveals Reed's link to Blackthorn."},
        {"mw16_gossiping_trees",
            "Victor flees toward the World Tree with his prisoners. Poison pours from a pipe along his route. Stopping it will slow the chase."},
        {"mw17_factory_heaven",
            "A factory traps travelers in dreams of the people they miss. The party must break free and stop the machine."},
        {"mw18_allies_dishonestly",
            "Company guards trap travelers at a toll gate. Reed joins three new fighters to break through and get everyone across."},
        {"mw19_road_keeps_one",
            "A gray wall sweeps toward fleeing families. Rowan must keep the last safe arch open while the others chase Victor."},
        {"mw20_monster_rules",
            "A huge beast guards the World Tree. She is Sylvara, forced into this shape by Blackthorn. A hidden signal may be choosing her targets."},
        {"mw21_four_losses",
            "Cages are falling. The World Tree is dying. Blackthorn's machines close in. The rescuers must split up to save the people inside."},
        {"mw22_clean_shot",
            "Birdie has one arrow left. She spots a cable controlling Sylvara, but reaching the shot could kill her."},
        {"mw23_choice_not_cure",
            "The control cable is broken, but Sylvara is still trapped in the beast. Donella asks for help to free her."},
        {"mw24_agent_not_heir",
            "The freed prisoners turn on Grask. Reed and his friends fight toward Victor before he can escape."},
        {"mw25_deed_own_hand",
            "Mirewatch is free, but food is running out. Thaeron offers grain if Reed agrees to open the family storehouse."},
        {"bt06_receipt_book",
            "A prisoner wagon is a trap. Its hidden machine watches every rescuer, and Victor uses what it learns to strike their homes."},
        {"bt07_debtor_prison",
            "Twenty prisoners are trapped in a mill. A machine predicts each rescue attempt, and rising water may be their only way out."},
        {"bt08_north_lock",
            "Reed meets his uncle Thaeron alone. Thaeron offers to protect him, but soldiers are already closing on his friends."},
        {"bt09_published_mystery",
            "The rescuers enter a prison called the Vault. Its doors slam shut, leaving children trapped above a deadly drop."},
        {"bt10_stolen_road",
            "Mirewatch joins forces after the prison rescue. When a hidden signal sets their shelter ablaze, they must flee together."},
        {"bt11_public_lie",
            "Thaeron turns a hungry crowd against Reed. Vanya steps between the soldiers and the townspeople as Reed reveals his uncle's betrayal."},
        {"bt12_break_charter",
            "Mirewatch drives Blackthorn from power. Victor answers with bombs, and Braun must choose whether to fire on children."},
        {"bt13_feyward_transit",
            "Blackthorn's factory traps people in dreams of the homes they lost. The travelers must break the machine before it keeps them forever."},
        {"bt14_move_boundary",
            "A wall of gray fog swallows the road behind fleeing families. Rowan must choose between chasing Victor and helping them escape."},
        {"bt16_clean_shot",
            "Prisoner cages fall around the World Tree. Gearjaw turns against Victor while Birdie searches for a way to free Sylvara."},
        {"bt18_deed_own_hand",
            "With the town running out of food, Thaeron offers Reed enough grain to feed it. The food comes with a demand."}
    }};
    const auto found = std::find_if(
        Contexts.begin(), Contexts.end(),
        [&](const auto& entry) { return entry.first == missionId; });
    return found == Contexts.end() ? std::string_view{} : found->second;
}

StoryMission chronicleFromEntry(const MissionEntry& entry)
{
    StoryMission mission;
    mission.id = entry.id;
    mission.title = entry.title;
    mission.sourceChapter = entry.source;
    mission.lesson = "STORY SCENE";
    mission.objective =
        "Watch what happens next. Choose Continue to move through the scene.";
    mission.hint =
        "Choose Continue to follow the story. Battles teach you how to play.";

    if (entry.id == "mw08_lesson_night")
    {
        mission.briefing = {
            panel("Narrator",
                "Reed leaves the river camp without telling his friends. His uncle Thaeron waits at a Company dock with a dry coat and warm food. Soldiers block the stairs behind him.",
                "cards/thaeronBaelstone.png"),
            panel("Thaeron",
                "I ordered your family killed. They would never have let me raise you. You lived because I told the soldiers to spare you. Come home, Reed. Blackthorn will be yours one day.",
                "cards/thaeronBaelstone.png"),
            panel("Reed",
                "You had them killed? You held me while I cried for them.",
                "cards/reedBaelstone.png"),
            panel("Thaeron",
                "I kept you safe. Come with me and I can keep your friends safe too.",
                "cards/thaeronBaelstone.png"),
            panel("Reed",
                "Then call off your soldiers. Let the prisoners go. Do it now.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Thaeron stays silent. The soldiers close in. Reed fires his rescue flare. The giant alligator reaches the dock and carries him home, but Company boats follow.",
                "cards/thaeronBaelstone.png"),
            panel("Narrator",
                "The soldiers set the camp's tents on fire. Families flee to the boats. The alligator carries the children away from the flames.",
                "cards/joniPumpernickel.png"),
            panel("Vanya",
                "You went alone. They followed you here. We choose the next route together. You stay with us, Reed.",
                "cards/vanyaBluewater.png")
    };
    }
    else if (entry.id == "mw14_public_lie")
    {
        mission.briefing = {
            panel("Narrator",
                "The people of Mirewatch gather to demand their prisoners back. Thaeron arrives with armed guards. Reed steps between him and the crowd.",
                "cards/joniPumpernickel.png"),
            panel("Thaeron",
                "This is my nephew. He could own Blackthorn one day. Did your leader tell you that?",
                "cards/thaeronBaelstone.png"),
            panel("Reed",
                "He offered it to me. I refused, but I hid the meeting from you. He told me he ordered my family killed. I wanted to believe the man who raised me. I was wrong.",
                "cards/reedBaelstone.png"),
            panel("Joni",
                "You should have told us. Now stay close. The children are behind us, and the soldiers are coming.",
                "cards/joniPumpernickel.png")
    };
    }
    else if (entry.id == "mw20_monster_rules")
    {
        mission.briefing = {
            panel("Narrator",
                "Cages hang above the World Tree's broken roots. Sylvara, the Tree's keeper, has been changed into a huge beast. A cable pulls tight at her neck. She lunges at a prisoner trying to climb down.",
                "cards/Sylvara.png"),
            panel("Donella",
                "Down! Under the roots!",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "Donella pulls the prisoner clear. Sylvara claws at the cable on her own neck. Victor turns a wheel, and she is forced toward them again.",
                "cards/Sylvara.png"),
            panel("Donella",
                "She is fighting it. Birdie, we have to cut that cable. First we need room to get these people down.",
                "cards/donellaOfTheMarsh.png")
    };
    }
    else if (entry.id == "mw21_four_losses")
    {
        mission.briefing = {
            panel("Narrator",
                "With Grask down, Mog helps the prisoners spread a net below the cages. Scooter climbs toward them. A board breaks. Scooter falls toward the stones beyond the net. Gearjaw reaches for him, but Victor's order makes it pull back.",
                "cards/gearjaw.png"),
            panel("Gearjaw",
                "Scooter. My core holds our games and everything we learned together. Victor uses it to make me obey. If I break it, I will forget you. But I will not let you fall.",
                "cards/gearjaw.png"),
            panel("Narrator",
                "Gearjaw crushes its core. Victor's orders go silent. It catches Scooter and lowers him into the net. Scooter taps twice against its hand. This time, no tap comes back.",
                "cards/scooter.png"),
            panel("Birdie",
                "Scooter, stay with him. We will bring you both home.",
                "cards/birdieTheWise.png")
    };
    }
    else if (entry.id == "mw22_clean_shot")
    {
        mission.briefing = {
            panel("Birdie",
                "One arrow left. I can reach the cable, but that wheel keeps crossing in front of it. Donella, keep everyone low.",
                "cards/birdieTheWise.png"),
            panel("Victor",
                "Shoot the beast! She will tear them apart!",
                "cards/victorGreyshard.png"),
            panel("Narrator",
                "Birdie waits for a gap, then shoots. Her arrow cuts the cable. Sylvara strikes her just as it snaps. Birdie falls against the roots.",
                "cards/birdieTheWise.png"),
            panel("Birdie",
                "My shoulder... I cannot move my right arm. Vanya, help me down. Do not pull it.",
                "cards/birdieTheWise.png")
    };
    }
    else if (entry.id == "mw23_choice_not_cure")
    {
        mission.briefing = {
            panel("Narrator",
                "Sylvara stops attacking, but she cannot return to her own shape. Donella fits the Root Key against a broken root. The dying Tree draws strength from Sylvara until her legs buckle.",
                "cards/Sylvara.png"),
            panel("Donella",
                "She cannot heal it alone. Hold a root with me if you want to help. It will draw strength from us too. Let go if you feel weak.",
                "cards/donellaOfTheMarsh.png"),
            panel("Joni",
                "I will stay with Birdie. She should not have to sit alone.",
                "cards/joniPumpernickel.png"),
            panel("Narrator",
                "Reed grips a root. Others join him. Green shoots close the split. Sylvara takes her own shape again. She rests her hand on the Tree and smiles.",
                "cards/Sylvara.png")
    };
    }
    else if (entry.id == "mw25_deed_own_hand")
    {
        mission.briefing = {
            panel("Narrator",
                "Back in Mirewatch, food runs low. Thaeron arrives with boats full of grain. This time, the town's guards stop him at the dock. He sends for Reed.",
                "cards/thaeronBaelstone.png"),
            panel("Thaeron",
                "Let me leave, and the grain is yours. Come with me, Reed. Blackthorn can still be yours.",
                "cards/thaeronBaelstone.png"),
            panel("Reed",
                "You ordered my family killed. You must answer for it. The grain stays here. So do you.",
                "cards/reedBaelstone.png"),
            panel("Joni",
                "Bring the grain ashore. Our neighbors will run the storehouse together. No one family gets to decide who eats.",
                "cards/joniPumpernickel.png"),
            panel("Narrator",
                "Thaeron looks toward his soldiers. They have put down their weapons. Vanya takes him into custody while the boat crews unload the grain.",
                "cards/vanyaBluewater.png")
    };
    }

    else if (entry.id == "bt07_debtor_prison")
    {
        mission.briefing = {
            panel("Narrator",
                "Hara's map leads to a prison in an old mill. The father from the docks is inside. He knows the path to the boats. Mog brings a raft while the rescuers break open the cages.",
                "cards/Mog.png"),
            panel("Young prisoner",
                "Donella, take this Root Key. Victor stole it from the World Tree. Take it back to the Tree's keeper. It can heal the roots.",
                "cards/donellaOfTheMarsh.png"),
            panel("Victor",
                "Open the floodgates. No one leaves my prison.",
                "cards/victorGreyshard.png"),
            panel("Narrator",
                "Water pours into the hall. A guard lunges at the father with a knife. Juniper steps between them and takes the blow. The father catches her. She is badly hurt, but breathing.",
                "characters/juniperFlash.png"),
            panel("Mog",
                "Reed, hold that gate! I have the last two. Get Juniper onto the raft!",
                "cards/Mog.png"),
            panel("Narrator",
                "The father carries Juniper aboard. Mog brings the last captives through the gate, and Reed lets it slam shut. The raft reaches the boats. Hara is not among the people they rescued.",
                "cards/Mog.png")};
    }
    else if (entry.id == "bt08_north_lock")
    {
        mission.briefing = {
            panel("Narrator",
                "Reed goes to his uncle alone. Warm food and a dry coat wait for him. Soldiers block the road outside. Reed leaves the food untouched and asks Thaeron to free the prisoners.",
                entry.art),
            panel("Thaeron Baelstone",
                "Stay, and Blackthorn will be yours. Your parents tried to take you away from me. I ordered Victor to kill them, but I told him to spare you. I have always kept you safe.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "You told me raiders killed them. All these years, you let me believe you. Keep your coat. Keep Blackthorn. I'm going back to my friends.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Soldiers close around Reed. He fires a flare from the dock. Mangletooth swims up, and Reed climbs aboard. Blackthorn boats follow them to the river camp. Guards burn its tents. Vanya, who runs the camp, leads the families away.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "He offered me Blackthorn in his letter. I hid it. I thought I could make him stop Victor. I was wrong. Now he tells me he ordered my family's deaths.",
                "cards/reedBaelstone.png"),
            panel("Vanya Bluewater",
                "Our shelter is gone. You risked our home on a secret bargain. The people who had to run will choose our next step together. You can come, Reed. But you have to listen.",
                "cards/vanyaBluewater.png")};
    }

    else if (entry.id == "bt10_stolen_road")
    {
        mission.briefing = {
            panel("Narrator",
                "Mog and Braun slip into town to meet the rescuers. They bring Gearjaw, a metal guard that once played with a beaver named Scooter. Mog hopes it will still listen to its friend.",
                "cards/gearjaw.png"),
            panel("Scooter",
                "Two taps. Remember? It means your friend is here.",
                "cards/scooter.png"),
            panel("Narrator",
                "Scooter taps Gearjaw's hand. It taps twice back. Then Victor's voice sounds from its chest: 'Bring me Scooter.' Its hand closes around Scooter's arm.",
                "cards/gearjaw.png"),
            panel("Gearjaw",
                "Run. Victor put his orders in my memory core. I can't keep fighting them.",
                "cards/gearjaw.png"),
            panel("Narrator",
                "Mog pulls Scooter free. Gearjaw holds its hand open long enough for Scooter to run. Then Victor's orders draw the machine back to the mill. Mog watches it go. He will not hand Scooter over.",
                "cards/Mog.png")};
    }

    else if (entry.id == "bt12_break_charter")
    {
        mission.briefing = {
            panel("Narrator",
                "Mog pins his badge back on. He and Braun return to their posts to help the remaining prisoners escape. The town has heard what happened in the prison. Families block Victor's wagons. He turns the gun toward them.",
                entry.art),
            panel("Victor Greyshard",
                "Braun, fire. Clear that road.",
                "cards/victorGreyshard.png"),
            panel("Braun Stonefist",
                "There are children down there. Lower that gun. I won't fire on them.",
                "cards/braunStonefist.png"),
            panel("Narrator",
                "Grask kills Braun for refusing. Mog heaves the gun sideways. Its heavy frame crashes between Grask and the families. Mog stands behind it, shaking, and refuses to move.",
                "cards/Mog.png"),
            panel("Mog",
                "No more. These people are leaving. If you want to hurt them, you have to get past me.",
                "cards/Mog.png"),
            panel("Narrator",
                "The rescuers rush to Mog's side and force Grask back. The families escape. Victor takes the remaining cage wagons into the forest. Mog lays down his weapon and offers to guide the rescuers. They follow him, keeping watch.",
                entry.art)};
    }


    else if (entry.id == "bt16_clean_shot")
    {
        mission.briefing = {
            panel("Narrator",
                "Mog drives Grask from the cages. The rescuers spread a net below them. Scooter climbs up to help the prisoners. Birdie climbs toward Sylvara's cable.",
                "cards/Mog.png"),
            panel("Narrator",
                "A board breaks. Scooter falls toward the stones beyond the net. Gearjaw reaches for him, but Victor's orders make it pull back. It can break that control only by breaking the core that holds its memories.",
                "cards/gearjaw.png"),
            panel("Gearjaw",
                "I won't remember our two taps, Scooter. But I won't let you fall.",
                "cards/gearjaw.png"),
            panel("Narrator",
                "Gearjaw crushes its core. Victor's orders go silent. It catches Scooter and lowers him into the net. Scooter taps its hand twice. This time, no answer comes.",
                "cards/gearjaw.png"),
            panel("Scooter",
                "It's me. Your friend. I'll stay with you. We can learn it again.",
                "cards/scooter.png"),
            panel("Narrator",
                "Birdie draws her bow. Sylvara swings toward her, forced by Victor's cable. Birdie's arrow cuts it as the blow breaks her right arm. Sylvara stops. She reaches down to help Birdie.",
                "cards/birdieTheWise.png"),
            panel("Narrator",
                "Donella fits the Root Key against the broken root. She and the others share their strength with the Tree. New leaves open. The cages are emptying. Victor has nowhere left to hide.",
                "cards/Sylvara.png")};
    }
    else if (entry.id == "bt18_deed_own_hand")
    {
        mission.briefing = {
            panel("Narrator",
                "Victor is in custody, but Mirewatch needs food. Thaeron arrives with boats full of grain. The town's guards stop him at the dock. He sends for Reed.",
                entry.art),
            panel("Thaeron Baelstone",
                "Let me leave, and the grain is yours. Come with me, Reed. Blackthorn can still be yours.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "You ordered my parents' deaths. I won't free you or inherit Blackthorn. Food cannot buy my forgiveness. The town will decide what happens to the grain.",
                "cards/reedBaelstone.png"),
            panel("Vanya Bluewater",
                "Bring the grain ashore. Thaeron, you are coming with us. You will answer for the people you hurt.",
                "cards/vanyaBluewater.png"),
            panel("Narrator",
                "Thaeron closes his fist around the keys. Reed waits. At last, his uncle opens his hand. Vanya takes the keys, and the guards lead Thaeron away.",
                entry.art)};
    }
    else
    {
        const std::string_view context = chronicleContextFor(entry.id);
        mission.briefing = {
            panel("Narrator", context.empty() ? entry.objective : context, entry.art),
            panel(entry.speaker, entry.story, entry.art),
            panel("Narrator", entry.result, entry.art)};
    }
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
    mission.aftermath = {panel("Narrator", entry.result, entry.art)};
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.firstPlayer = 1;
    return mission;
}

StoryMission sharedS01()
{
    return storyNode(
        "s01_hospitality",
        "Shelter in the Giant Gator",
        "Mirewatch story",
        {
        panel("Narrator",
            "Vanya runs a camp beside the river. She leads the families into Mangletooth, a friendly giant alligator. Beds fill the space behind his teeth. His jaws close as a patrol passes.",
            "cards/joniPumpernickel.png"),
        panel("Joni",
            "Easy, big friend. They need a home tonight. Shoes off the blankets, all of you.",
            "cards/joniPumpernickel.png"),
        panel("Narrator",
            "Juniper tucks the rescued children into a bed. Reed hears someone laugh for the first time all day. Joni gives him a letter that arrived before them.",
            "characters/juniperFlash.png"),
        panel("Reed",
            "My uncle is alive. He says he can stop Blackthorn. He wants to meet me alone.",
            "cards/reedBaelstone.png"),
        panel("Vanya",
            "Then he can meet all of us. Hara is still out there. We find her first.",
            "cards/vanyaBluewater.png")
    });
}

StoryMission sharedS02()
{
    return storyNode(
        "s02_no_one_alone",
        "Choose Your Own Moves - Optional Practice",
        "Mirewatch story",
        {
        panel("Coach",
            "Try a three-against-three battle using any legal moves. You can skip this practice and return to it from Missions.",
            "cards/reedBaelstone.png")
    });
}

StoryMission mirewatchFirstOpenCheck()
{
    StoryMission mission = sharedS02();
    mission.title = "Choose Your Own Moves - Optional Practice";
    mission.sourceChapter = "After the rescue / optional practice";
    mission.lesson = "OPTIONAL FIGHT - CHOOSE YOUR OWN MOVES";
    mission.objective = "Defeat all three enemies and keep Reed alive. Choose any legal actions.";
    mission.hint = "Inspect a unit to read its actions. Choose your own moves, then use End Turn. There are no marked steps to follow.";
    mission.briefing.push_back(panel("Controls",
        "Mouse: drag a unit to a lit square or target and release the mouse button. Keyboard: focus a unit and press Enter. Choose a target with the arrows, then press Enter again. Press I to inspect a card or A for an available ability.", "cards/donellaOfTheMarsh.png"));
    mission.briefing.push_back(panel("Coach",
        "Choose Start Practice when you are ready, or Skip Practice. You can return to this battle from Missions.", "cards/reedBaelstone.png"));
    mission.aftermath = {
        panel("Coach",
            "You won by choosing your own moves. You can replay this battle from Missions.",
            "cards/donellaOfTheMarsh.png")
    };
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 1, 1),
        piece("donella", "Donella of the Marsh", 1, 3, 1),
        piece("birdie", "Birdie the Wise", 1, 5, 1),
        piece("collector", "Blackthorn Debt Collector", 2, 1, 5),
        piece("lumberjack", "Blackthorn Lumberjack", 2, 3, 5),
        piece("alchemist", "Blackthorn Alchemist", 2, 5, 5)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.firstPlayer = 1;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.requiredSurvivorRoles = {"reed"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission blackthornFirstOpenCheck(const MissionEntry& entry)
{
    StoryMission mission;
    mission.id = entry.id;
    mission.title = "Blackthorn: Optional Battle";
    mission.sourceChapter =
        "Chapter 7 / optional non-canon open tactics check";
    mission.lesson =
        "OPTIONAL OPEN BATTLE - CHOOSE YOUR MOVES";
    mission.objective =
        "Defeat all three enemies and keep Thaeron alive. Choose your own moves in this optional practice battle.";
    mission.hint =
        "Read a unit's card, choose an action, and use End Turn when you are done. The opponent will fight back.";
    mission.briefing = {
        panel("Practice battle",
            "Choose your moves against three training opponents, or continue the story. To win, defeat all three enemies and keep Thaeron alive.",
            "cards/Grask.png"),
        panel("Controls",
            "Mouse: hold the left button on a unit, drag to a glowing target, and release. Keyboard: focus a unit, press Enter, choose a target with arrows, then Enter. Press I to inspect, A for an ability, and End Turn when done.",
            "cards/blackthornAlchemist.png")};
    mission.aftermath = {
        panel("Coach",
            "You won by choosing your own moves. You can replay this battle from Missions.",
            "cards/blackthornAlchemist.png")};
    mission.pieces = {
        piece("thaeron", "Thaeron Baelstone", 1, 1, 1),
        piece("alchemist", "Blackthorn Alchemist", 1, 3, 1),
        piece("grask", "Grask", 1, 5, 1),
        piece("reed", "Reed Baelstone", 2, 1, 5),
        piece("spearman", "Bog Spearman", 2, 3, 5),
        piece("smuggler", "Resistance Smuggler", 2, 5, 5)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.firstPlayer = 1;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.requiredSurvivorRoles = {"thaeron"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission sharedS03()
{
    return storyNode(
        "s03_published_mystery",
        "Soup for Juniper",
        "Mirewatch story",
        {
        panel("Narrator",
            "Juniper wakes with clean bandages around her side. Beside her bed, the boy from the boatyard holds a bowl of soup with both hands.",
            "characters/juniperFlash.png"),
        panel("Juniper",
            "You brought that for me? Then help me sit up. Slowly.",
            "characters/juniperFlash.png"),
        panel("Narrator",
            "He keeps hold of her sleeve while Joni lifts her pillow. Hara's mother takes the bowl so he can stay beside her.",
            "cards/joniPumpernickel.png")
    });
}

StoryMission sharedS04()
{
    return storyNode(
        "s04_wounds_that_vote",
        "Gearjaw Comes Home",
        "Mirewatch story",
        {
        panel("Narrator",
            "As the camp recovers, Mog brings Gearjaw to visit. The metal animal once played with Scooter. Mog points to the forest road on Hara's map.",
            "cards/gearjaw.png"),
        panel("Mog",
            "Victor is taking prisoners to the World Tree. This is the road. I have to return to my post before he notices I am gone.",
            "cards/Mog.png"),
        panel("Narrator",
            "Scooter taps twice on Gearjaw's paw. Gearjaw taps back. Then a red light flashes in its chest. Its paw closes around Scooter.",
            "cards/gearjaw.png"),
        panel("Gearjaw",
            "Victor is calling. I cannot open my hand. Help him, Mog.",
            "cards/gearjaw.png"),
        panel("Narrator",
            "Mog pulls its fingers apart and frees Scooter. Gearjaw turns toward the Company camp. Mog follows it back, leaving Scooter safe with Birdie.",
            "cards/scooter.png")
    });
}



StoryMission sharedS06()
{
    return storyNode(
        "s06_town_owns_itself",
        "Mirewatch Is Free",
        "Mirewatch story",
        {
        panel("Narrator",
            "The World Tree grows new leaves. At home, Birdie learns to tie her coat with one hand. Reed holds it steady for her. Nearby, Scooter teaches Gearjaw their tapping game again, one tap at a time.",
            "cards/birdieTheWise.png"),
        panel("Donella",
            "Hara is still missing. Her map brought so many people home. Now we go back for her.",
            "cards/donellaOfTheMarsh.png"),
        panel("Narrator",
            "Hara's mother packs bread for the search party. Neighbors mend boats and carry meals to the wounded. The first boat waits by the dock, ready to bring Hara home.",
            "cards/joniPumpernickel.png")
    });
}

// Blackthorn follows Mog from uneasy obedience to open refusal.
// Story scenes stay on the route even when the player skips practice.
StoryMission blackthornS01()
{
    return storyNode(
        "s01_hospitality",
        "The House That Swims",
        "Chapters 4-5 - Blackthorn perspective",
        {
            panel("Narrator",
                "Mog follows Reed to a house beside the river. The enormous alligator carrying it rises from the mud. Its name is Mangletooth. Families wave from the windows as it swims away.",
                "cards/Mog.png"),
            panel("Victor Greyshard",
                "Follow it by boat. A house that size cannot hide forever.",
                "cards/victorGreyshard.png"),
            panel("Narrator",
                "Thaeron sends Reed a private letter: come home and inherit Blackthorn. Reed hides the offer from his friends. If his uncle can stop Victor, perhaps he can free the prisoners without a fight.",
                "cards/thaeronBaelstone.png")
        });
}

StoryMission blackthornS03()
{
    return storyNode(
        "s03_published_mystery",
        "Her Name Is Juniper",
        "Chapter 14 - Blackthorn perspective",
        {
            panel("Grask",
                "The prisoners got out. They will be afraid of us now.",
                "cards/Grask.png"),
            panel("Mog",
                "Her name is Juniper. She saved those children at the docks. Today she saved their father. We nearly killed her for it.",
                "cards/Mog.png"),
            panel("Victor Greyshard",
                "She chose to stand in our way. You still work for me, Mog. Remember that.",
                "cards/victorGreyshard.png"),
            panel("Narrator",
                "Mog removes his Blackthorn badge. He keeps it in his pocket and goes to find Braun. He needs help before Victor gives the next order.",
                "cards/Mog.png")
        });
}

StoryMission blackthornPoisonedRootRoad()
{
    return storyNode(
        "bt13a_poisoned_root_road",
        "Poison on the Road",
        "Book One, Chapter 19 - The Poisoned Root Road",
        {
            panel("Narrator",
                "Mog follows Victor's wagon tracks with the rescuers, including a healer named Rowan. A broken pipe pours poison into the forest roots. Stopping it will give Victor more time to escape.",
                "cards/donellaOfTheMarsh.png"),
            panel("Reed Baelstone",
                "I want to catch him. But this poison will reach the river. Donella, what do you need me to do?",
                "cards/reedBaelstone.png"),
            panel("Donella of the Marsh",
                "Hold the pipe steady. Rowan, strike where I cut. We stop the poison, then we follow the wagons.",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "Reed takes his place at the pipe. He holds it while Donella cuts and Rowan strikes. The metal bends shut. The poison stops, but Victor's wagons are gone.",
                "cards/reedBaelstone.png"),
            panel("Mog",
                "I know where he's going. He plans to drain the World Tree for his machines. Donella has its Key. We can still stop him.",
                "cards/Mog.png"),
            panel("Narrator",
                "Farther along the road, Victor has left wounded prisoners behind. Rowan stays to take them home. The others follow the wagon tracks toward the World Tree.",
                "cards/rowanLeafbound.png")
        });
}

StoryMission blackthornS06()
{
    return storyNode(
        "s06_town_owns_itself",
        "Blackthorn Falls",
        "Chapter 30 and Epilogue - Blackthorn perspective",
        {
            panel("Mog",
                "I kept telling myself I only followed orders. Braun is dead. Juniper is still healing. I'll tell the town what I did. Saving people now doesn't hide the people I hurt.",
                "cards/Mog.png"),
            panel("Narrator",
                "Rowan brings the wounded home. At Vanya's new shelter, the children Juniper saved bring meals to her bed. This time, they can help her.",
                "characters/juniperFlash.png"),
            panel("Narrator",
                "Birdie begins learning to use her left arm. Beside her, Scooter taps Gearjaw's hand and waits while it learns.",
                "cards/birdieTheWise.png"),
            panel("Narrator",
                "Hara is still missing. Her map saved the prisoners, and they will not give up on her. Each search party brings food and an empty seat in its boat.",
                "cards/mirewatchInformant.png"),
            panel("Narrator",
                "Vanya keeps the storehouse keys. Reed carries grain with his neighbors. At the docks, Mog helps mend the family's boat. No Blackthorn guard comes to take it away.",
                "cards/Mog.png")
        });
}

StoryMission mirewatchOpening()
{
    StoryMission mission;
    mission.id = "mw01_river_teeth";
    mission.title = "Gators at the Boat";
    mission.sourceChapter = "Chapter 1 - A Cage with a Bill of Sale";
    mission.lesson = "MOVE, END TURN, ATTACK, AND HEALTH";
    mission.objective =
        "Move Reed back one square on each of two turns. Defeat three wounded gators. Move Telos aside to clear Reed's final shot.";
    mission.hint =
        "Mouse: drag the named piece to its target and release the mouse button. Keyboard: use arrows to focus the piece, press Enter, use arrows to choose a target, then press Enter again.";
    mission.briefing = {
        panel("Narrator",
            "Reed crosses toward Mirewatch with Donella, a healer, and Erevan, a scout. Their boatman, Telos, steers for shore. They came to help dock worker Hara free prisoners. Gators in Blackthorn Company harnesses rush the boat.",
            "story/mw/mw01/01_gator_ambush_v2.png"),
        panel("Donella",
            "They're coming for us! Reed, move back. One step now. Take the second when you get another turn.",
            "story/mw/mw01/02_reed_retreat_v2.png"),
        panel("Coach",
            "Keyboard: Tab switches between the board and buttons. On the board, arrows move focus. Press Enter to select a piece, choose its target with arrows, then press Enter again. I inspects a card; A uses a selected piece's available ability. Mouse: drag to the target and release the mouse button.",
            "story/mw/mw01/02_reed_retreat_v2.png"),
        panel("Coach",
            "Normally, one ready piece acts before End Turn unless its card allows more. Badges show Health: blue for your pieces, red for enemies. These gators each have 1 Health. One damage removes them from the battle. The nearest gator follows Reed after End Turn.",
            "story/mw/mw01/02_reed_retreat_v2.png")};
    mission.aftermath = {
        panel("Erevan",
            "Blackthorn trained these gators. The Company takes people's boats and locks their owners in work camps. Victor commands its soldiers.",
            "story/mw/mw01/04_harness_aftermath_v2.png"),
        panel("Narrator",
            "Telos steers for shore. Hara is not waiting at the dock. Beyond it, a Company guard is dragging away a family's boat.",
            "story/mw/mw01/04_harness_aftermath_v2.png")
    };
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
        scriptedPieceAction(
            StoryActionKind::Move, 1, "reed", 5, 2,
            "Step", "MOVE REED BACK - FIRST STEP",
            "Move Reed to C6. Mouse: drag him there and release the mouse button. Keyboard: use arrows to focus Reed and press Enter. Focus jumps to C6; press Enter again. Arrows let you change targets. Step moves one square in any direction.",
            "Move Reed one square to marked C6."),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "END YOUR TURN",
            "End Turn so the nearest gator can lunge.",
            "Use End Turn. Reed must wait until your next turn to move again.",
            {}, {},
            {
                panel("Narrator",
                    "Reed reaches C6. The nearest gator turns toward him, ready to lunge.",
                    "story/mw/mw01/01_gator_ambush_v2.png")
            }),
        scriptedPieceAction(
            StoryActionKind::Move, 2, "center_gator", 4, 3,
            "Bite", "THE GATOR LUNGES",
            "The center gator moves from E5 to D5.",
            "This step could not finish. Restart the battle."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATOR ENDS ITS TURN",
            "The gator ends its turn.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(
            StoryActionKind::Move, 1, "reed", 5, 1,
            "Step", "MOVE REED BACK - SECOND STEP",
            "Move Reed from C6 to B6. This second step takes him farther from the gator.",
            "Move Reed to marked B6."),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "END YOUR TURN", "End Turn. Donella can act on your next turn.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATOR TURNS", "The gator loses Reed's trail and faces Donella.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(
            StoryActionKind::Attack, 1, "donella", 4, 3,
            "Spark", "DONELLA - SPARK",
            "Use Donella's Spark from F5 to hit the gator at D5. Spark reaches two squares along the row. Its 1 damage takes the gator from 1 Health to 0.",
            "Use Spark on the marked gator at D5.",
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
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(
            StoryActionKind::Attack, 1, "erevan", 3, 3,
            "Shadow Blade", "EREVAN - SHADOW BLADE",
            "Use Shadow Blade on the gator beside Erevan at D4. This attack defeats the gator and moves Erevan into its square.",
            "Use Erevan's Shadow Blade on the gator at D4.",
            "knife_gator"),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "TWO GATORS DOWN",
            "End Turn after Erevan's strike.",
            "End Turn to bring Telos into the fight."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "ONE GATOR LEFT",
            "The final gator waits beside the boat.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(
            StoryActionKind::Move, 1, "telos", 4, 0,
            "Travel", "TELOS - TRAVEL",
            "Use Travel to move Telos from B4 to A5. This one-square move takes him out of Bite range. It also clears column B for Reed's shot.",
            "Move Telos to marked A5 with Travel.",
            {}, {},
            {
                panel("Telos",
                    "I'm moving, Reed! Get ready. You'll have a clear shot.",
                    "story/mw/mw01/03_telos_travel_v2.png"),
                panel("Coach",
                    "Travel moves Telos one square. B4 to A5 clears column B so Reed can use Bow.",
                    "story/mw/mw01/03_telos_travel_v2.png")
            }),
        scripted(
            StoryActionKind::EndTurn, 1, "", -1, -1,
            "THE FIRING LINE IS CLEAR",
            "End Turn after Telos moves.",
            "End Turn so Reed can shoot on your next turn."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE LAST GATOR HOLDS",
            "The gator stays at B3. Nothing blocks Reed's shot along column B.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(
            StoryActionKind::Attack, 1, "reed", 2, 1,
            "Bow", "REED - BOW",
            "Use Reed's Bow from B6 to hit the gator at B3. Bow reaches three squares along this column. Telos has moved out of the way, so the shot is clear.",
            "Use Reed's Bow on the gator at B3.",
            "cargo_gator")
    };
    mission.masteryCards = {"Telos the Merchant"};
    mission.masteryRules = {
        "board-coordinates", "normal-activation", "end-turn", "omni-movement",
        "ranged-line-of-sight", "attacking-movement", "health-and-destruction"};
    return mission;
}

StoryMission blackthornOpening()
{
    StoryMission mission;
    mission.id = "bt01_harness_hunger";
    mission.title = "Alligators at the Gate";
    mission.sourceChapter = "Chapter 1 - post-attack rules reconstruction";
    mission.lesson = "MOVE, HEAL, DISABLE, AND ATTACK";
    mission.objective =
        "Practice moving, healing, Disable, and attacking. Keep every gator alive.";
    mission.hint =
        "Follow the glowing moves. Healing restores Health. Disable stops a unit from acting. Keep every gator alive.";
    mission.briefing = {
        panel("Narrator",
            "Mog, a Blackthorn guard, helped strap harnesses onto alligators. Now a gator smashes into a workboat. Mog hauls its owner from the water. The broken harness bears Victor's mark.",
            "cards/Mog.png"),
        panel("Mog",
            "I thought the harnesses would keep people safe. You sent that gator after a workboat.",
            "cards/Mog.png"),
        panel("Victor Greyshard",
            "This town obeys whoever has power. Get the gator back in its pen. Make sure it obeys me.",
            "cards/victorGreyshard.png"),
        panel("Optional practice",
            "Practice moving, healing, Disable, and attacking, or continue Mog's story. Practice never changes the plot. You can replay it from Missions.",
            "cards/blackthornLumberjack.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Controls",
            "Mouse: hold the left button on the Lumberjack at B5. Drag to C5 and release. Keyboard: use arrows to focus B5, press Enter, use arrows to choose C5, then press Enter again. Press I to inspect a focused unit.",
            "cards/blackthornLumberjack.png"),
        panel("Coach",
            "A ready piece can act now. One piece normally acts each turn. Then use End Turn. Some card rules allow extra actions. Guided Actions counts finished actions, not clicks or drags.",
            "cards/blackthornLumberjack.png"),
        panel("Coach",
            "Health badges show how much Health is left. Healing cannot raise it above the card's maximum. Damage makes a survivor miss its next turn. Paralysis Potion deals no damage but makes the target miss its next two turns.",
            "cards/blackthornAlchemist.png")};
    mission.aftermath = {
        panel("Coach",
            "You moved, healed an ally, used Disable, and landed an attack. The north gator missed its first turn. A later lesson shows the full two-turn effect.",
            "cards/bullGator.png")};
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
        scriptedPieceAction(StoryActionKind::Move, 1, "lumberjack", 4, 2,
            "Axe Swing", "MOVE THE LUMBERJACK", "Select the Lumberjack at B5. Choose Axe Swing, then glowing C5. This action can move one square along a row or column.",
            "Hold the left mouse button on the Lumberjack, drag it to C5, then release. Or focus it, press Enter, choose C5, and press Enter again."),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "END YOUR TURN", "Use End Turn after moving the Lumberjack.", "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATORS WAIT", "The gators stay in their pens.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 3, 2,
            "Healing Elixer", "HEAL THE WORKER", "Select the Blackthorn Alchemist at B4. Use Healing Elixer on your wounded worker at C4.",
            "Choose Healing Elixer. It restores 3 Health, up to the card's maximum.", "worker"),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "OPEN THE NORTH GATE", "Use End Turn. The north gator moves next.", "Use End Turn to continue."),
        scriptedPieceAction(StoryActionKind::Move, 2, "north_gator", 2, 2,
            "Bite", "THE NORTH GATOR LUNGES", "The north gator moves from D3 to C3.",
            "This step could not finish. Restart the practice battle."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATOR STOPS", "The north gator ends its turn.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 2, 2,
            "Paralysis Potion", "DISABLE THE GATOR", "Select the Blackthorn Alchemist. Use Paralysis Potion on the gator at C3.",
            "Paralysis Potion deals no damage. Disable 2 makes the gator miss its next two turns.", "north_gator"),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "WATCH DISABLE", "Use End Turn. The north gator must miss its next turn.", "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATOR CANNOT ACT", "The north gator misses its first turn. It still has one Disabled turn left. A later lesson shows both.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "lumberjack", 4, 3,
            "Axe Swing", "ONE AXE SWING", "Use Axe Swing on the center gator at D5. Hit it once.",
            "Attack the gator at D5 once. It survives and keeps its square.", "center_gator")
    };
    // This optional opening introduces the Alchemist, but its two-turn
    // Paralysis Potion is not followed to expiry here. Full card mastery waits
    // for the optional card review, which proves every printed profile.
    mission.masteryCards = {};
    mission.masteryRules = {
        "orthogonal-movement", "friendly-heal", "maximum-health",
        "zero-damage-disable", "positive-damage-disable"};
    mission.optionalRehearsal = true;
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission mirewatchDeployment()
{
    StoryMission mission;
    mission.id = "mw03_town_under_company";
    mission.title = "Hold the Path";
    mission.sourceChapter = "Chapter 2 - rules rehearsal";
    mission.lesson = "DEPLOY, CONTROL GROUND, HEAL, AND PULL";
    mission.objective = "Play the Informant. Pull the guard closer with Hooked Spear, then defeat it with Spear Thrust.";
    mission.hint = "Joni's Civilized Trait lets you play the Informant. C4 is an empty square you control.";
    mission.briefing = {
        panel("Narrator",
            "Reed blocks the guard. Joni, the local innkeeper, opens her back door for the family.",
            "cards/reedBaelstone.png"),
        panel("Joni",
            "Leave that boat! It feeds this family. Come inside, all of you. We will hold the path.",
            "cards/joniPumpernickel.png"),
        panel("How to place a card",
            "Mouse: hold the left mouse button on the card, drag it to C4, then release. Keyboard: press Tab until your hand is focused. Choose the card with arrows and press Enter. Choose C4 with arrows, then press Enter again.",
            "cards/mirewatchInformant.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "The family keeps its boat. Its owner cuts away the guard's rope, then holds out a hand to help Joni aboard.",
            "cards/joniPumpernickel.png")
    };
    mission.pieces = {
        piece("joni", "Joni Pumpernickel", 1, 3, 1),
        piece("spearman", "Bog Spearman", 1, 4, 1),
        piece("wounded_spearman", "Bog Spearman", 1, 2, 2, 1),
        piece("notice_guard", "Blackthorn Lumberjack", 2, 6, 3, 2),
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
            "DEPLOY ON CONTROLLED GROUND", "Play Mirewatch Informant from your hand on C4.",
            "Play Mirewatch Informant on C4. Joni has the required Civilized Trait. Three nearby friendly pieces control this empty square.", "informant"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "spearman", 6, 3,
            "Hooked Spear", "SPEARMAN - HOOKED SPEAR",
            "Use Hooked Spear from B5 to hit the Lumberjack at D7. The guard survives the 1 damage. Pull brings it to empty C6. The Spearman stays at B5.",
            "Playing a card leaves your normal piece action free. Use Hooked Spear on the marked Lumberjack.", "notice_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "JONI'S AURA", "End Turn. Joni heals pieces beside her, up to their maximum Health.",
            "End Turn to let Joni's healing aura work."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "RESOURCES AND TAX", "Your turn begins. Each controlled square earns 1 Resource. The Informant's Tax 10 also takes up to 10 Resources from the enemy and gives them to you. It cannot take more than the enemy has.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "informant", 4, 3,
            "Lunge", "INFORMANT - LUNGE", "Use Lunge diagonally from C4 into the Debt Collector at D5.",
            "The Informant is ready now. Use Lunge on the guard at D5.", "tax_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END YOUR TURN",
            "End Turn. You have used this turn's one normal piece action.",
            "End Turn before using the Spearman."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE COMPANY LINE HOLDS",
            "The enemy ends its turn. The guard remains beside the Spearman.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "spearman", 5, 2,
            "Spear Thrust", "SPEARMAN - SPEAR THRUST",
            "Use Spear Thrust from B5 to hit the nearby Lumberjack at C6. It deals 2 damage. The guard falls, and the Spearman moves into C6.",
            "Use Spear Thrust on the guard at C6.", "notice_guard")
    };
    mission.script[0].instruction =
        "Play Mirewatch Informant on C4 for 35 Resources. C4 is an empty square you control.";
    mission.script[0].panelsBefore = {
        panel("Coach",
            "To play a Unit, each Trait on its card must match a Trait on one of your living Heroes. Joni's Civilized Trait lets you play this Informant.",
            "cards/mirewatchInformant.png")};
    mission.script[1].panelsBefore = {
        panel("Coach",
            "The new Informant waits until your next turn to act. Playing a card leaves your normal piece action free. The Spearman can act now.",
            "cards/mirewatchInformant.png")};
    mission.script[3].panelsBefore = {
        panel("Coach",
            "A visible piece controls its own square. An empty square goes to the side with more pieces touching it, including diagonals. Controlled squares earn Resources when your turn starts.",
            "cards/mirewatchInformant.png")};
    mission.masteryCards = {"Mirewatch Informant", "Bog Spearman"};
    mission.masteryRules = {
        "control", "resources", "deployment-cost", "trait-gate", "arrival-exhaustion",
        "card-play-plus-activation", "healing-aura", "maximum-health", "tax",
        "ranged-diagonal-attack", "pull-surviving-target", "adjacent-diagonal-attacking-movement"};
    return mission;
}

StoryMission blackthornDeployment()
{
    StoryMission mission;
    mission.id = "bt03_sanctuary_debt";
    mission.title = "Call in the Crew";
    mission.sourceChapter = "Chapter 2 - rules simulation";
    mission.lesson = "PLAY CARDS, COMMAND, SUMMON, AND TRAIL";
    mission.objective =
        "Practice playing cards, calling allies, and healing.";
    mission.hint =
        "Thaeron has the Arcane, Civilized, and Corrupt Traits. These let you play matching Unit cards. Turns pass on their own in this drill.";
    mission.briefing = {
        panel("Thaeron Baelstone",
            "My money pays your soldiers, Victor. Reed is my nephew. Bring him home alive. Remember that before you send in your men.",
            "cards/thaeronBaelstone.png"),
        panel("Optional practice",
            "Practice playing cards, calling allies, and healing, or continue the story.",
            "cards/blackthornAlchemist.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Coach",
            "Deploy Units on empty squares you control. C4 is one. Each Trait on the card must match a living friendly Hero. Thaeron has the Alchemist's Traits. It arrives exhausted and must wait until your next turn. Playing it leaves your normal piece action free.",
            "cards/blackthornAlchemist.png"),
        panel("Practice battle",
            "Command gives a ready ally beside Thaeron an extra action. Summon calls a Lumberjack. Trail leaves a Sapling behind the Grove Sister. Each controlled square earns 1 Resource at your turn's start. Then Gather and Tax add more. Turns pass automatically here.",
            "cards/blackthornDebtCollector.png"),
        panel("How to place a card",
            "Mouse: hold the left mouse button on the card, drag it to C4, then release. Keyboard: press Tab until your hand is focused. Choose the card with arrows and press Enter. Choose C4 with arrows, then press Enter again.",
            "cards/blackthornAlchemist.png")};
    mission.aftermath = {
        panel("Coach",
            "You played a card, used Command and Summon, and made a Sapling with Trail. You also used both Sapling actions.",
            "cards/blackthornForeman.png")};
    mission.pieces = {
        piece("thaeron", "Thaeron Baelstone", 1, 3, 1),
        piece("foreman", "Blackthorn Foreman", 1, 4, 1),
        piece("lumberjack", "Blackthorn Lumberjack", 1, 2, 1, 1),
        piece("sister", "Grove Sister", 1, 6, 1),
        piece("collector", "Blackthorn Debt Collector", 1, 7, 1),
        piece("sapling", "Sapling", 1, 6, 4),
        piece("sapling_patient", "Blackthorn Lumberjack", 1, 7, 4, 1),
        piece("thaeron_target", "Blackthorn Debt Collector", 2, 2, 0, 1),
        piece("sapling_target", "Bull Gator", 2, 6, 5)};
    mission.playerHand = {"Blackthorn Alchemist"};
    mission.playerResources = 20;
    mission.enemyResources = 35;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {
        "thaeron", "foreman", "lumberjack", "sister", "collector",
        "sapling", "sapling_patient"};
    mission.autoResolvePlayerEndTurns = true;
    mission.script = {
        scriptedCard(StoryActionKind::PlayCard, 1, "Blackthorn Alchemist", 3, 2,
            "PLAY THE ALCHEMIST", "Play Blackthorn Alchemist from your hand on C4.",
            "Use glowing C4. Thaeron has the Arcane and Civilized Traits the card needs.", "alchemist"),
        scriptedPieceAction(StoryActionKind::Move, 1, "foreman", 4, 2,
            "Slide", "FOREMAN - SLIDE",
            "Use Slide to move the Foreman from B5 to C5. D5 stays empty for Summon.",
            "Choose Slide and move the Foreman to C5."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "thaeron", "command", "Command",
            "THAERON - COMMAND", "Select Thaeron and use Command.",
            "Use Command first. It lets one ready ally next to Thaeron act."),
        scriptedAbility(1, "foreman", "summon", "Summon",
            "FOREMAN - SUMMON", "Use the Foreman's Summon through Command. A Lumberjack appears at D5.",
            "Choose the ready Foreman next to Thaeron. D5, the square in front of it, is empty.",
            "Blackthorn Lumberjack"),
        scriptedPieceAction(StoryActionKind::Move, 1, "sister", 5, 2,
            "Glide", "GROVE SISTER - GLIDE", "Command left your normal action free. Use the Grove Sister's Glide from B7 to C6. Trail leaves a Sapling at B7.",
            "Command left your normal action free. Use the Grove Sister's Glide to C6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY YOUR NEW UNITS", "Your turn ends. The Alchemist and summoned Lumberjack can act on your next turn.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "GAIN RESOURCES", "Your new turn brings income from controlled squares. Lumberjacks Gather Resources, and the Collector uses Tax to take Resources from the other side.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 2, 1,
            "Healing Elixer", "ALCHEMIST - HEALING ELIXER", "Use Healing Elixer on your wounded Lumberjack at B3.",
            "Choose Healing Elixer, then the friendly Lumberjack at B3.", "lumberjack"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 7, 4,
            "Heal", "SAPLING - HEAL",
            "Use the Sapling's Heal on the wounded Lumberjack at E8. Healing stops at the card's maximum Health.",
            "Choose Heal and target your Lumberjack at E8.", "sapling_patient"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN",
            "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SAPLING IS READY",
            "The opponent passes. The Sapling is ready to act again.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 6, 5,
            "Entangle", "SAPLING - ENTANGLE",
            "Use Entangle on the Bull Gator at F7. It deals no damage and makes the gator miss two turns. The Sapling needs one turn to cool down.",
            "Choose Entangle and target F7. The Sapling cannot act during its next turn.", "sapling_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: FIRST TURN",
            "Your turn ends. The gator misses its first turn while Disabled.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SAPLING COOLS DOWN",
            "The gator cannot act. On your next turn, the Sapling cannot act either while it cools down.",
            "This step could not finish. Restart the practice battle."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: SECOND TURN",
            "Your turn ends. The gator must miss one more turn.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "DISABLE ENDS",
            "The gator misses its second turn. Disable ends. You have tried both Sapling actions.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "thaeron", 2, 0,
            "Knife Stab", "THAERON - KNIFE STAB",
            "Use Knife Stab diagonally from B4 to the wounded Debt Collector at A3.",
            "Choose Thaeron's Knife Stab and target A3.", "thaeron_target")
    };
    mission.masteryCards = {
        "Thaeron Baelstone", "Blackthorn Foreman", "Grove Sister",
        "Blackthorn Debt Collector", "Blackthorn Alchemist",
        "Blackthorn Lumberjack"};
    mission.masteryIncludesDependentProfiles = true;
    mission.masteryRules = {
        "deployment-cost", "trait-gate", "arrival-exhaustion", "command", "summon",
        "command-preserves-normal-activation", "summon-front-square",
        "summoned-unit-arrives-exhausted", "trail", "gather", "tax",
        "controlled-income", "friendly-heal", "maximum-health",
        "zero-damage-disable", "disable-two-turns", "status-duration",
        "cooldown", "orthogonal-movement", "basic-attack"};
    mission.optionalRehearsal = true;
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission mirewatchCapstone(bool fullPractice = false)
{
    const MissionEntry entry = {
        "mw15_title_follows_burden", "Escape the Burning Square",
        "Chapter 18 - combined-arms rules dramatization",
        "DEPLOY, REPEAT, SHOOT, AND HEAL",
        "Use the starting deck to clear the guards and help the crowd escape.",
        "Deploy a card, then use the team's actions together.",
        "Joni Pumpernickel", "Vanya Bluewater", "Birdie the Wise",
        "Victor Greyshard", "Grask", "Mara",
        "Get the children to the boats. We hold the path until everyone is clear.",
        "Mog refuses to fire at the children and helps their boat escape.",
        "cards/joniPumpernickel.png"};
    StoryMission mission = playableFromEntry(entry);
    mission.lesson =
        "DEPLOY, REPEAT, SHOOT, AND HEAL";
    mission.objective =
        "Clear the route with Vanya and Birdie. Then move Donella beside the wounded Veteran and heal him.";
    mission.hint =
        "Playing a card leaves your normal piece action available. Finish both uses of Vanya's Blade Dance before choosing another piece.";
    mission.briefing = {
        panel("Narrator",
            "Victor's bombs burst around the square. Joni leads the crowd toward the boats. Guards block the streets. Vanya stays beside the children while the rescuers open a path.",
            "cards/joniPumpernickel.png"),
        panel("Coach",
            "Use Vanya to clear the first guards. Birdie opens a path for Donella. Then move Donella beside the wounded Veteran and heal him.",
            "cards/donellaOfTheMarsh.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "The children reach the boats. Victor orders Mog to fire at them. Mog lowers his weapon.",
            "cards/Mog.png"),
        panel("Mog",
            "No. I helped you lock them up. I will not help you kill them.",
            "cards/Mog.png"),
        panel("Narrator",
            "Mog throws his weapon aside and pushes the last boat free. Victor flees with his prisoners and Gearjaw. Reed's friends follow the forest road Mog showed them.",
            "cards/joniPumpernickel.png")
    };
    mission.pieces = {
        piece("joni", "Joni Pumpernickel", 1, 6, 2),
        piece("vanya", "Vanya Bluewater", 1, 6, 0, 1),
        piece("birdie", "Birdie the Wise", 1, 5, 0),
        piece("informant", "Mirewatch Informant", 1, 4, 1),
        piece("donella", "Donella of the Marsh", 1, 6, 4),
        piece("mend_target", "Marshland Veteran", 1, 4, 4, 1),
        piece("vanya_guard_one", "Blackthorn Debt Collector", 2, 5, 1, 1),
        piece("vanya_guard_two", "Blackthorn Debt Collector", 2, 4, 2, 1),
        piece("birdie_guard", "Blackthorn Alchemist", 2, 5, 3, 1),
        piece("spear_guard", "Blackthorn Alchemist", 2, 4, 3, 1),
        piece("informant_guard", "Blackthorn Debt Collector", 2, 3, 2, 1),
        piece("joni_guard", "Blackthorn Debt Collector", 2, 6, 3, 1)};
    mission.playerHand = {"Bog Spearman"};
    mission.playerResources = 50;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {
        "joni", "vanya", "birdie", "informant", "donella", "mend_target"};
    mission.script = {
        scriptedCard(StoryActionKind::PlayCard, 1, "Bog Spearman", 6, 1,
            "DEPLOY ON CONTROLLED GROUND",
            "Play Bog Spearman on B7. It arrives exhausted and must wait until your next turn to act.",
            "Place the Spearman on highlighted B7.", "reinforcement"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 5, 1,
            "Blade Dance", "VANYA - BLADE DANCE",
            "Attack the Debt Collector on B6 with Blade Dance.",
            "Use Blade Dance on the guard at B6.", "vanya_guard_one"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 4, 2,
            "Blade Dance", "VANYA - COMPLETE THE REPEAT",
            "Use Blade Dance again on the Debt Collector at C5.",
            "Use Blade Dance again on the guard at C5. This lesson requires the second attack.", "vanya_guard_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE REINFORCEMENT", "End Turn after Vanya's second attack.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE ENEMY ENDS ITS TURN", "The enemy turn ends.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "birdie", 5, 3,
            "Longbow Shot", "BIRDIE - LONGBOW SHOT",
            "Fire three squares along the row at the Alchemist on D6.",
            "Use Longbow Shot on the Alchemist at D6.", "birdie_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE LINE", "End Turn after Birdie's shot.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SPEARMAN READIES", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "reinforcement", 4, 3,
            "Hooked Spear", "SPEARMAN - HOOKED SPEAR",
            "Use Hooked Spear from B7 to the Alchemist at D5. The target has 1 Health and is defeated. Pull cannot move a defeated target.",
            "Use Hooked Spear on D5, two diagonal squares away.", "spear_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "CLEAR THE NEXT PATH", "End Turn after the Spearman acts.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "TWO GUARDS LEFT", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "informant", 3, 2,
            "Lunge", "INFORMANT - LUNGE",
            "Use Lunge on the adjacent Debt Collector at C4.",
            "Use Lunge on the guard at C4.", "informant_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE FINAL LANE", "End Turn so Joni can act next turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST BLOCKER WAITS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "joni", 6, 3,
            "Walking Stick", "JONI - WALKING STICK",
            "Use Walking Stick on the Debt Collector at D7.",
            "Use Walking Stick on the guard beside Joni at D7.", "joni_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "OPEN THE HEALING LANE", "End Turn so Donella can move.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE ENEMY ENDS ITS TURN", "The opposing side passes.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "donella", 5, 3,
            "Sprint", "DONELLA - SPRINT",
            "Use Sprint from E7 to D6. This diagonal move puts Donella beside the wounded Veteran at E5.",
            "Use Sprint to reach D6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE MEND", "End Turn with Donella beside the wounded Veteran.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE ENEMY ENDS ITS TURN", "The opposing side passes.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "donella", 4, 4,
            "Marshlight Mend", "DONELLA - MARSHLIGHT MEND",
            "Use Marshlight Mend on the wounded Veteran beside Donella. It heals up to his maximum Health.",
            "Use Marshlight Mend on the Veteran at E5.", "mend_target")};
    mission.masteryCards = {
        "Joni Pumpernickel", "Vanya Bluewater", "Birdie the Wise",
        "Mirewatch Informant", "Bog Spearman", "Donella of the Marsh"};
    mission.masteryRules = {
        "deployment-cost", "controlled-square-deployment",
        "arrival-exhaustion", "card-play-plus-activation", "repeat-lock",
        "ranged-attack", "ranged-diagonal-attack", "diagonal-attacking-movement",
        "diagonal-movement", "friendly-heal", "maximum-health"};
    if (fullPractice)
    {
        mission.id = "mw15a_team_workshop";
        mission.title = "Mirewatch: Team Workshop";
        mission.lesson = "DEPLOY, REPEAT, SHOOT, AND HEAL";
        mission.sourceChapter = "Optional card practice after the story";
        mission.objective = "Play a card, combine attacks, and heal an ally. This practice is separate from the burning square.";
        mission.hint = "Each step names the real card action and target. You can skip this practice and return from Missions.";
        mission.optionalRehearsal = true;
        mission.briefing = {
            panel("Coach", "Play the Spearman, then follow the guide. Playing a card leaves your normal piece action free.", "cards/joniPumpernickel.png"),
            panel("Coach", "Choose Start Practice when you are ready. Skipping this lesson leaves the story complete.", "cards/joniPumpernickel.png")};
        mission.aftermath = {panel("Coach", "Practice complete. You can inspect these cards during any match to check their moves.", "cards/joniPumpernickel.png")};
        for (auto& step : mission.script)
        {
            step.panelsBefore.clear();
            if (step.kind == StoryActionKind::EndTurn && step.owner == 2)
            {
                step.heading = "YOUR NEXT TURN";
                step.instruction = "The opposing side ends its turn.";
            }
        }
    }
    else
    {
        std::vector<StoryScriptAction> rescueSteps;
        for (const std::size_t index : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 17u, 18u, 19u, 20u})
        {
            rescueSteps.push_back(mission.script[index]);
        }
        mission.script = std::move(rescueSteps);
        mission.playerHand.clear();
        mission.playerDrawPile.clear();
        mission.playerResources = 0;
        mission.masteryCards.clear();
        mission.masteryRules = {"repeat-lock", "diagonal-movement", "friendly-heal", "maximum-health"};
        mission.lesson = "CLEAR A PATH AND HELP THE WOUNDED";
        mission.objective = "Clear the route, then move Donella to the wounded Veteran and heal him.";
        mission.hint = "Use both Blade Dances, then Birdie's shot. Donella can move into the cleared square to heal the Veteran.";
        mission.script[2].heading = "THE FAMILIES CAN PASS";
        mission.script[3].heading = "THE FAMILIES PASS";
        mission.script[3].instruction = "Families cross the first cleared street toward the boats.";
        mission.script[6].heading = "REACH THE WOUNDED";
        mission.script[6].instruction = "The wounded Veteran calls for help. Donella can reach him through the cleared square.";
        mission.script[9].instruction = "The last families pass while Donella reaches the wounded man.";
    }
    return mission;
}

std::vector<StoryPanel> victorReckoningPanels()
{
    return {
        panel("Narrator",
            "The prisoners run past the fallen guards. Reed holds the exit while the last child climbs out. Mog blocks Victor's way. There is no one left to send into danger for him.",
            "cards/reedBaelstone.png"),
        panel("Reed",
            "Put your weapon down, Victor. You're coming back with us. The people you locked up are going home.",
            "cards/reedBaelstone.png"),
        panel("Narrator",
            "Victor drops his sword. Then he pulls a knife from his sleeve. Mog catches his wrist and knocks the knife away.",
            "cards/Mog.png"),
        panel("Narrator",
            "Reed ties Victor's hands. Mog steps aside for a child carrying a torn blanket. She reaches for his hand. He walks her to the waiting boats.",
            "cards/Mog.png")};
}

StoryMission blackthornCapstone()
{
    const MissionEntry entry = {
        "bt17_natural_order", "Blackthorn: Card Review",
        "Optional card review after the story",
        "PRACTICE EVERY STARTER CARD ACTION",
        "Follow the glowing steps to try the starter cards.",
        "Practice each card's actions, including extra actions, summons, attacks, healing, hiding, and Disable.",
        "Thaeron Baelstone", "Ashenfang", "Grask",
        "Reed Baelstone", "Donella of the Marsh", "Fizzlewick Gearwright",
        "Try every Blackthorn starter-card action on one practice board.",
        "You are ready to choose your own moves in the next battle.",
        "cards/fizzlewickGearwright.png"};
    StoryMission mission = playableFromEntry(entry);
    mission.briefing = {
        panel("Optional practice",
            "Mog's story is complete. Try 26 guided actions covering the starter deck, or skip this review. You can replay it from Missions.",
            "cards/thaeronBaelstone.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Coach",
            "Follow the glowing instructions. Turns pass automatically between actions. Watch Gather, Tax, healing, cooldowns, and Disable as each turn starts or ends.",
            "cards/blackthornForeman.png")};
    mission.aftermath = {
        panel("Coach",
            "You used every action in the Blackthorn starter deck. Next, choose your own moves in a small open battle.",
            "cards/thaeronBaelstone.png")};
    mission.pieces = {
        piece("victor", "Victor Greyshard", 1, 6, 7),
        piece("thaeron", "Thaeron Baelstone", 1, 5, 0),
        piece("foreman", "Blackthorn Foreman", 1, 6, 0),
        piece("grask", "Grask", 1, 5, 3),
        piece("sister", "Grove Sister", 1, 7, 3),
        piece("alchemist", "Blackthorn Alchemist", 1, 2, 0),
        piece("mog", "Mog", 1, 2, 1, 1),
        piece("worker", "Blackthorn Lumberjack", 1, 0, 4),
        piece("sapling", "Sapling", 1, 0, 7),
        piece("sapling_heal_target", "Blackthorn Lumberjack", 1, 1, 7, 1),
        piece("ashenfang", "Ashenfang", 1, 4, 7),
        piece("collector", "Blackthorn Debt Collector", 1, 3, 7),
        piece("ambusher", "Goblin Ambusher", 1, 3, 4),
        piece("braun", "Braun Stonefist", 1, 7, 4),
        piece("sharpshooter", "Goblin Sharpshooter", 1, 7, 5),
        piece("first_guard", "Blackthorn Debt Collector", 2, 6, 6),
        piece("second_guard", "Blackthorn Debt Collector", 2, 5, 5),
        piece("cage_guard", "Bull Gator", 2, 4, 2),
        piece("knife_target", "Bull Gator", 2, 4, 0),
        piece("charge_target", "Bull Gator", 2, 1, 3),
        piece("paralysis_target", "Bull Gator", 2, 1, 0),
        piece("worker_target", "Bull Gator", 2, 0, 5),
        piece("sapling_disable_target", "Bull Gator", 2, 0, 6),
        piece("ashenfang_target", "Bull Gator", 2, 5, 6),
        piece("collector_target", "Bull Gator", 2, 4, 6),
        piece("mog_target", "Bull Gator", 2, 3, 2),
        piece("braun_target", "Bull Gator", 2, 4, 4),
        piece("sharpshooter_target", "Bull Gator", 2, 6, 3),
        piece("ambusher_stab_target", "Bull Gator", 2, 3, 5)};
    mission.playerResources = 0;
    mission.enemyResources = 50;
    mission.requiredSurvivorRoles = {
        "thaeron", "foreman", "grask", "sister", "alchemist", "mog",
        "worker", "sapling", "sapling_heal_target", "ashenfang", "collector",
        "ambusher", "braun", "sharpshooter"};
    mission.autoResolvePlayerEndTurns = true;
    mission.catchUpForSkippedRehearsals = false;
    mission.optionalRehearsal = true;
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 6, 6,
            "Axe Dance", "VICTOR - AXE DANCE", "Use Axe Dance on the Debt Collector at G7.",
            "Choose Victor's Axe Dance and target G7.", "first_guard"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 5, 5,
            "Axe Dance", "VICTOR - EXTRA ATTACK", "Victor defeated an enemy, so Relentless offers another action. Take it: use Axe Dance on the Debt Collector at F6.",
            "Use Victor again. In normal play, you may use End Turn to skip this extra action.", "second_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END THE EXTRA ATTACKS", "Your turn ends, passing up another Relentless action.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.",
            "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "thaeron", "command", "Command",
            "THAERON - COMMAND", "Select Thaeron and use Command.",
            "Use Thaeron's Command. The ready Foreman beside him can take the extra action."),
        scriptedAbility(1, "foreman", "summon", "Summon",
            "FOREMAN - SUMMON", "Use the Foreman's Summon through Command. A Lumberjack appears at B7.",
            "Choose the Foreman. B7, the square in front of it, is empty.",
            "Blackthorn Lumberjack"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 4, 2,
            "Swing Axes", "GRASK - SWING AXES", "Command left your normal action free. Use Grask's Swing Axes on the Bull Gator at C5. This Capture action attacks diagonally.",
            "Choose Grask's Swing Axes and target C5.", "cage_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COMMAND LEAVES AN ACTION FREE", "Your turn ends after the commanded action and your normal piece action.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "sister", 5, 1,
            "Glide", "GROVE SISTER - GLIDE", "Use Glide to move diagonally to B6. Trail leaves a Sapling at D8.",
            "Choose the Grove Sister's Glide and move to B6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 2, 1,
            "Healing Elixer", "ALCHEMIST - HEALING ELIXER", "Use Healing Elixer on your wounded ally Mog at B3.",
            "Choose the Alchemist's Healing Elixer and target Mog.", "mog"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LUMBERJACK GATHERS", "The opponent passes. Gather adds Resources at the start of your new turn.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "thaeron", 4, 0,
            "Knife Stab", "THAERON - KNIFE STAB",
            "Use Thaeron's Knife Stab on the Bull Gator at A5.",
            "Choose Knife Stab and target A5.", "knife_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "foreman", 7, 0,
            "Slide", "FOREMAN - SLIDE",
            "Use Slide to move the Foreman from A7 to A8.",
            "Choose Slide and move one square down to A8."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 1, 3,
            "Charge", "GRASK - CHARGE",
            "Use Grask's Charge on the Bull Gator at D2.",
            "Choose Charge and target D2 along the clear column.", "charge_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 1, 0,
            "Paralysis Potion", "ALCHEMIST - PARALYSIS POTION",
            "Use Paralysis Potion on the Bull Gator at A2. It deals no damage and makes the gator miss its next two turns.",
            "Choose Paralysis Potion and target A2.", "paralysis_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PARALYSIS: FIRST TURN",
            "Your turn ends. The gator misses its first turn while Disabled.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE GATOR CANNOT ACT",
            "The gator misses this turn. One Disabled turn remains.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "worker", 0, 5,
            "Axe Swing", "LUMBERJACK - AXE SWING",
            "Use the Lumberjack's Axe Swing on the Bull Gator at F1.",
            "Choose Axe Swing and target F1, one square along the row.", "worker_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PARALYSIS: SECOND TURN",
            "Your turn ends. The gator must miss one more turn.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "PARALYSIS ENDS",
            "The gator misses its second turn. Disable now ends.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 1, 7,
            "Heal", "SAPLING - HEAL",
            "Use the Sapling's Heal on your wounded Lumberjack at H2.",
            "Choose Heal and target the friendly Lumberjack at H2.", "sapling_heal_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 0, 6,
            "Entangle", "SAPLING - ENTANGLE",
            "Use Entangle on the Bull Gator at G1. The gator misses two turns. The Sapling needs one turn to cool down.",
            "Choose Entangle and target G1. This action deals no damage.", "sapling_disable_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ENTANGLE: FIRST TURN",
            "Your turn ends. The gator misses its first turn while Disabled.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SAPLING COOLS DOWN",
            "The gator cannot act. On your next turn, the Sapling cannot act while it cools down.",
            "This step could not finish. Restart the practice battle."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ENTANGLE: SECOND TURN",
            "Your turn ends. The gator must miss one more turn.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "ENTANGLE ENDS",
            "The gator misses its second turn. Disable ends. Your Sapling can act again.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ashenfang", 5, 6,
            "Entangling Lunge", "ASHENFANG - ENTANGLING LUNGE",
            "Use Entangling Lunge on the Bull Gator at G6, one square diagonally away.",
            "Choose Ashenfang's Entangling Lunge and target G6.", "ashenfang_target", {},
            {panel("Coach",
                "Now try the remaining cards. First, use Ashenfang's Entangling Lunge.",
                "cards/Ashenfang.png")}),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ASHENFANG: FIRST DISABLED TURN",
            "Your turn ends. The gator struck by Ashenfang must miss its first turn.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE GATOR CANNOT ACT",
            "Ashenfang's target misses this turn. One Disabled turn remains.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "collector", 4, 6,
            "Knife Stab", "COLLECTOR - KNIFE STAB",
            "Use the Debt Collector's Knife Stab diagonally on the Bull Gator at G5.",
            "Choose Knife Stab and target G5.", "collector_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ASHENFANG: SECOND DISABLED TURN",
            "Your turn ends. Ashenfang's target must miss one more turn, then Disable ends.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE COLLECTOR USES TAX",
            "The opponent passes. Tax takes Resources at the start of your new turn, up to the amount the opponent has.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "mog", 3, 2,
            "Axe Swing", "MOG - AXE SWING",
            "Use Mog's Axe Swing on the Bull Gator at C4.",
            "Choose Axe Swing and target C4.", "mog_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "braun", 4, 4,
            "Charge", "BRAUN - CHARGE",
            "Use Braun's Charge on the Bull Gator at E5.",
            "Choose Charge and target E5 along the clear column.", "braun_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "sharpshooter", 6, 5,
            "Advance", "SHARPSHOOTER - ADVANCE",
            "With the gun lowered, use Advance to move from F8 to F7.",
            "Choose Advance and move one square up to F7."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.", "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "sharpshooter", "transform", "Raise Gun",
            "SHARPSHOOTER - RAISE GUN",
            "Use Raise Gun so the Sharpshooter can Fire.",
            "Select the Sharpshooter and use Raise Gun.", {}, "Lower Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sharpshooter", 6, 3,
            "Fire", "SHARPSHOOTER - FIRE",
            "Use Fire on the Bull Gator at D7. The row is clear.",
            "Choose Fire and target D7.", "sharpshooter_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.", "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "sharpshooter", "transform", "Lower Gun",
            "SHARPSHOOTER - LOWER GUN",
            "Use Lower Gun so the Sharpshooter can use Advance again.",
            "Select the Sharpshooter and use Lower Gun.", {}, "Raise Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ambusher", 3, 5,
            "Stab", "AMBUSHER - STAB",
            "Use the visible Ambusher's Stab on the Bull Gator at F4.",
            "Choose Stab while the Ambusher is visible.", "ambusher_stab_target", {},
            {panel("Coach",
                "The Ambusher has different actions while visible and hidden. Use Stab, then hide with Dematerialize. Pass through the blocker with Sneak Around, then strike with Ambush.",
                "cards/goblinAmbusher.png")}),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "ambusher", "dematerialize", "Dematerialize",
            "AMBUSHER - DEMATERIALIZE",
            "Use Dematerialize. The Ambusher becomes hidden and adds no influence to nearby empty squares.",
            "Select the Ambusher and use Dematerialize."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "ambusher", 3, 6,
            "Sneak Around", "AMBUSHER - SNEAK AROUND",
            "Use Sneak Around from E4 to G4, passing through the Bull Gator at F4.",
            "Choose Sneak Around and move to empty G4, beyond the gator."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Your turn ends automatically.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ambusher", 4, 6,
            "Ambush", "AMBUSHER - AMBUSH",
            "Use Ambush on the Bull Gator at G5. Your Ambusher becomes visible after the attack.",
            "Choose Ambush and target G5.", "collector_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "FINAL PRACTICE COMPLETE",
            "Your turn ends. You have used every action in the Blackthorn starter deck.",
            "The turn ends automatically."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "READY FOR OPEN BATTLE", "The opponent passes. You have finished the Blackthorn card practice.",
            "This step could not finish. Restart the practice battle.")};
    mission.masteryCards = {
        "Thaeron Baelstone", "Ashenfang", "Blackthorn Debt Collector",
        "Blackthorn Alchemist", "Blackthorn Foreman", "Blackthorn Lumberjack",
        "Grove Sister", "Mog", "Grask", "Goblin Ambusher",
        "Braun Stonefist", "Goblin Sharpshooter"};
    mission.masteryIncludesDependentProfiles = true;
    mission.masteryRules = {
        "relentless-optional-extra-action", "relentless-action-lock", "command",
        "command-preserves-normal-activation", "summon-front-square",
        "summoned-unit-arrives-exhausted", "capture", "capture-staging",
        "long-orthogonal-attacking-movement", "trail", "friendly-heal",
        "maximum-health", "zero-damage-disable", "disable-two-turns",
        "positive-damage-disable", "status-duration", "cooldown", "gather",
        "tax", "transform", "state-specific-actions", "line-of-sight",
        "dematerialize", "hidden-adds-no-control", "pass-through",
        "visible-state-action"};
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission blackthornFieldJudgment()
{
    StoryMission mission;
    mission.id = "bt17a_field_judgment";
    mission.title = "Blackthorn: Open Practice";
    mission.sourceChapter = "Optional open practice after the story";
    mission.lesson = "OPEN BATTLE - CHOOSE YOUR MOVES";
    mission.objective =
        "Defeat four opposing units while keeping Thaeron alive.";
    mission.hint =
        "Double-click a unit, or focus it and press I, to read its card. Choose an action, then use End Turn to let the opponent act.";
    mission.briefing = {
        panel("Practice battle",
            "Choose your moves in this optional battle. Four Blackthorn cards face four training opponents. The opponent will fight back.",
            "cards/blackthornAlchemist.png"),
        panel("Coach",
            "Read a friendly card by double-clicking its unit, or focus it and press I. Choose a legal action. Use End Turn when you are done. Earlier drills are available from Missions.",
            "cards/thaeronBaelstone.png"),
        panel("Coach",
            "Defeat all four enemies and keep Thaeron alive. Choose when to move, heal, attack, use Command, or Summon a new unit.",
            "cards/blackthornForeman.png")};
    mission.aftermath = {
        panel("Coach",
            "You won by choosing your own moves. Next is an optional full match with decks, Hero placement, Resources, and clocks. You can skip it before the timers start.",
            "cards/thaeronBaelstone.png")};
    mission.pieces = {
        piece("thaeron", "Thaeron Baelstone", 1, 5, 0),
        piece("alchemist", "Blackthorn Alchemist", 1, 6, 1),
        piece("foreman", "Blackthorn Foreman", 1, 7, 2),
        piece("grask", "Grask", 1, 6, 3),
        piece("reed", "Reed Baelstone", 2, 0, 0),
        piece("donella", "Donella of the Marsh", 2, 1, 1),
        piece("veteran", "Marshland Veteran", 2, 0, 2),
        piece("spearman", "Bog Spearman", 2, 1, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.requiredSurvivorRoles = {"thaeron"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission blackthornOpenMastery()
{
    StoryMission mission;
    mission.id = "bt17b_open_mastery";
    mission.title = "Blackthorn: Full Match";
    mission.sourceChapter = "Optional full match after the story";
    mission.lesson = "OPTIONAL FULL MATCH - DECKS, HEROES, RESOURCES, AND CLOCKS";
    mission.objective =
        "Place Heroes in 2 minutes. Each turn has a 2-minute timer. Your match clock starts at 15 minutes. Defeat the enemy Heroes or run out their match clock to win.";
    mission.hint =
        "Place all your Heroes before the 2-minute deadline. Drag each Hero card to an empty glowing home square. With a keyboard, focus a Hero, press Enter, choose a glowing square with the arrows, and press Enter again.";
    mission.briefing = {
        panel("Coach",
            "Put your Blackthorn skills to work in a full match. You choose your Hero positions, card plays, and attacks. There are no guided moves.",
            "cards/thaeronBaelstone.png"),
        panel("Practice match",
            "Your Blackthorn starter deck faces Maggie and Joni. The story is over; this match is for practice.",
            "cards/Grask.png"),
        panel("Your deck",
            "Both decks are ready to play. Yours has twenty regular cards and two Heroes: Thaeron and Ashenfang. Their Traits let you play the Units in your hand.",
            "cards/blackthornForeman.png"),
        panel("Place your Heroes",
            "You have 2 minutes to place all Heroes. Hold a Hero card, drag it to an empty glowing home square, then release. Or focus it, press Enter, choose a square with the arrows, then press Enter. Placing one does not reset the deadline. Enemy Heroes stay hidden until both sides finish. Match clocks pause during placement.",
            "cards/maggieMudroot.png"),
        panel("Your hand",
            "You start with four cards your opponent cannot see. Player 1 goes first. Each hand holds up to four cards and does not refill on its own. Pay 50 Resources to draw. Once per turn, you may discard a card to the bottom of your deck.",
            "cards/blackthornDebtCollector.png"),
        panel("Earn Resources",
            "A piece controls the squares it stands on and adds influence to empty squares beside it. The side with more influence controls an empty square. A tie keeps its current owner. Each square you control gives 1 Resource at your turn's start, before Tax, Gather, and other automatic effects.",
            "cards/groveSister.png"),
        panel("Watch the clocks",
            "Each side starts with a 15-minute match clock and a 2-minute turn timer. A successful card play, piece action, ability, paid draw, discard, or pass resets the turn timer. The first 30 of these actions also add 10 seconds each to your match clock.",
            "cards/Ashenfang.png"),
        panel("When time runs out",
            "If your turn timer runs out, play passes to the opponent. Repeated timeouts cut your next turn limits to 1 minute, then 30 seconds. If your match clock runs out, you lose. You can use Exit or Restart during this practice match.",
            "cards/thaeronBaelstone.png"),
        panel("How to win",
            "Defeat every Hero that started on the enemy side, or let their match clock run out. Defeating ordinary Units alone will not win. You lose if both Thaeron and Ashenfang fall, or if your match clock runs out.",
            "cards/thaeronBaelstone.png"),
        panel("Ready to start",
            "Start Timed Match starts the 2-minute Hero placement timer at once. Hold a Hero card, drag it to a glowing home square, then release. Or focus a Hero, press Enter, choose its square, and press Enter again. You can skip this match before starting its timers.",
            "cards/Ashenfang.png")};
    mission.aftermath = {
        panel("Coach",
            "You won a full match with decks, Hero placement, Resources, and clocks. You can replay any lesson or match from Missions.",
            "cards/thaeronBaelstone.png")};
    mission.standardMatch = true;
    mission.playerDeck = {
        "Thaeron Baelstone", "Ashenfang",
        "Blackthorn Debt Collector", "Blackthorn Debt Collector",
        "Blackthorn Alchemist", "Blackthorn Alchemist",
        "Blackthorn Foreman", "Blackthorn Foreman",
        "Blackthorn Lumberjack", "Blackthorn Lumberjack",
        "Blackthorn Lumberjack", "Blackthorn Lumberjack",
        "Grove Sister", "Grove Sister", "Grove Sister",
        "Mog", "Grask",
        "Goblin Ambusher", "Goblin Ambusher",
        "Braun Stonefist",
        "Goblin Sharpshooter", "Goblin Sharpshooter"};
    mission.enemyDeck = {
        "Maggie Mudroot", "Joni Pumpernickel",
        "Bog Spearman", "Bog Spearman", "Bog Spearman", "Bog Spearman",
        "Maggie's Recruit", "Maggie's Recruit",
        "Maggie's Recruit", "Maggie's Recruit",
        "Resistance Smuggler", "Resistance Smuggler", "Resistance Smuggler",
        "Bull Gator", "Bull Gator", "Bull Gator",
        "Mirewatch Informant", "Mirewatch Informant",
        "Scooter", "Erevan the Shadow", "Donella of the Marsh",
        "Telos the Merchant"};
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission blackthornVictorReckoning()
{
    StoryMission mission;
    mission.id = "bt17c_victor_reckoning";
    mission.title = "Victor's Last Strike";
    mission.sourceChapter = "Book One, Chapter 28 - A Natural Order";
    mission.lesson = "STORY SCENE";
    mission.objective =
        "Watch Reed offer to take Victor alive and Mog stop his final attack.";
    mission.hint =
        "No board moves in this scene. Reed wants Victor taken alive.";
    mission.briefing = victorReckoningPanels();
    mission.objectiveSpec.kind = StoryObjectiveKind::StoryOnly;
    return mission;
}

const std::vector<MissionEntry> MirewatchEntries = {
    {
        "mw04_watcher_protects",
        "Get the Children Out",
        "Chapter 3",
        "RANGED AND CLOSE ATTACKS",
        "Defeat both guards with Juniper and Erevan.",
        "Use the marked attacks to keep the guards away from the children.",
        "Juniper Flash",
        "Erevan the Shadow",
        "Donella of the Marsh",
        "Goblin Sharpshooter",
        "Goblin Ambusher",
        "Erevan the Shadow",
        "Juniper, keep them back. I will get the children to the boat.",
        "The children reach the boat. Juniper carries the smaller one, who will not let go of her sleeve.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw05_watched_office",
        "Hara's Map",
        "Chapter 6",
        "HIDE, PASS THROUGH, AND PUSH",
        "Hide Erevan, cross the blocked path, and push the guard away.",
        "Use Dematerialize, then Fade Through Shadow. Finish with Hidden Shove.",
        "Erevan the Shadow",
        "Mirewatch Informant",
        "Reed Baelstone",
        "Grask",
        "Goblin Sharpshooter",
        "Hara Dole",
        "Take the map. Find my mother. I will hold the guards here.",
        "Hara holds the hatch so Erevan can get the map out. Juniper brings Hara's mother to safety, but Hara does not return.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw06_cost_seen",
        "Break Open the Wagon",
        "Chapter 7",
        "COOLDOWN AND REPEAT",
        "Help Birdie and Scooter open a way out of the wagon.",
        "Birdie buys Scooter time to reach the wagon's back.",
        "Birdie the Wise",
        "Scooter",
        "Swamp Tracker",
        "Grask",
        "Blackthorn Debt Collector",
        "Birdie the Wise",
        "Scooter, go around the back. I will keep that guard away from you.",
        "Scooter lifts the latch at the back of the wagon. The family climbs out while Birdie covers them.",
        "cards/birdieTheWise.png"
    },
    {
        "mw08_lesson_night",
        "His Uncle's Offer",
        "Chapter 10",
        "STORY SCENE",
        "Follow Reed to his uncle's meeting.",
        "Reed has left his friends to meet his uncle alone.",
        "Reed Baelstone",
        "Vanya Bluewater",
        "Birdie the Wise",
        "Thaeron Baelstone",
        "Blackthorn Foreman",
        "Thaeron Baelstone",
        "I ordered them to spare you. Come home, Reed.",
        "Thaeron admits ordering Reed's family killed. Reed refuses him. Company boats follow his rescue home and burn the camp. The families escape, and the giant alligator carries the children away.",
        "cards/thaeronBaelstone.png"
    },
    {
        "mw10_beautiful_plan",
        "Follow Hara's Map",
        "Chapter 12",
        "REPEAT AND DIAGONAL ATTACKS",
        "Defeat both guards with Vanya, then use Sidestep.",
        "Repeat 1 allows one more use of the same action, or Pass, before another piece acts.",
        "Vanya Bluewater",
        "Joni Pumpernickel",
        "Birdie the Wise",
        "Grask",
        "Blackthorn Foreman",
        "Young prisoner",
        "I nearly have this lock open. Keep those guards away!",
        "The young prisoner opens her cage and gives Donella the stolen Root Key. The guards open a floodgate and water pours into the prison.",
        "cards/vanyaBluewater.png"
    },
    {
        "mw11_no_plan_saves_all",
        "Hold the Escape Route",
        "Chapter 13",
        "SPRINT AND AUTOMATIC INTERCEPT",
        "Clear two paths and move Juniper beside the runner.",
        "Juniper's Intercept lets her swap with a nearby ally and take an enemy piece's damaging attack.",
        "Reed Baelstone",
        "Juniper Flash",
        "Birdie the Wise",
        "Goblin Sharpshooter",
        "Grask",
        "Erevan the Shadow",
        "Get the children through. We will hold the path.",
        "Juniper is badly hurt protecting the runner. He carries her onto the raft alive. Mog helps the prisoners escape. Hara is still missing.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw13_making_credit",
        "Find a Hidden Enemy - Optional Practice",
        "After the rescue / optional rules practice",
        "JUMP AND REVEAL",
        "Practice finding a hidden enemy with the Swamp Tracker.",
        "Jump beside the hidden Ambusher. End Turn to trigger Reveal.",
        "Scooter",
        "Swamp Tracker",
        "Erevan the Shadow",
        "Goblin Ambusher",
        "Goblin Sharpshooter",
        "Mog",
        "Jump beside the hidden enemy and use End Turn.",
        "The Tracker reveals the hidden enemy without harming it.",
        "cards/scooter.png"
    },
    {
        "mw14_public_lie",
        "Reed's Secret",
        "Chapter 17",
        "STORY SCENE",
        "Face Thaeron before the town.",
        "Reed's secret puts his friends in danger.",
        "Joni Pumpernickel",
        "Mirewatch Informant",
        "Erevan the Shadow",
        "Thaeron Baelstone",
        "Blackthorn Debt Collector",
        "Mara",
        "You should have told us about your uncle.",
        "Reed tells the town what his uncle offered and admits hiding the meeting. He stays with the rescuers as Victor's soldiers close in.",
        "cards/joniPumpernickel.png"
    },
    {
        "mw18_allies_dishonestly",
        "Let the Families Through",
        "Chapter 22",
        "OPEN BATTLE",
        "Defeat all four Company guards. Keep Reed, Vanya, and Birdie alive.",
        "Use the familiar team to open the road for the families. Inspect a card to check its moves.",
        "Reed Baelstone",
        "Pavo Quickstep",
        "Nettle Starbright",
        "Blackthorn Debt Collector",
        "Goblin Ambusher",
        "Reed",
        "Victor can wait. These people cannot.",
        "The families pass through the gate and help the wounded reach the river. Reed gives them the group's food. Then his friends follow him toward the World Tree.",
        "cards/reedBaelstone.png"
    },
    {
        "mw20_monster_rules",
        "The Tree's Keeper",
        "Chapter 24",
        "STORY SCENE",
        "Find out what is forcing Sylvara to attack.",
        "A cable tightens when Sylvara attacks.",
        "Reed Baelstone",
        "Donella of the Marsh",
        "Birdie the Wise",
        "Grask",
        "Blackthorn Foreman",
        "Donella of the Marsh",
        "That is Sylvara. They forced her into this shape. We have to find what controls her.",
        "Donella pulls a prisoner out of Sylvara's path and spots the cable forcing her to attack.",
        "cards/donellaOfTheMarsh.png"
    },
    {
        "mw21_four_losses",
        "Gearjaw's Choice",
        "Chapter 25",
        "STORY SCENE",
        "Help Scooter escape the machine.",
        "Gearjaw can break Victor's control, but it will lose its memories.",
        "Erevan the Shadow",
        "Scooter",
        "Donella of the Marsh",
        "Grask",
        "Blackthorn Foreman",
        "Gearjaw",
        "I will not help him hurt you.",
        "Gearjaw breaks its core to free itself from Victor's control. It saves Scooter but loses the memory of their friendship.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw22_clean_shot",
        "One Arrow Left",
        "Chapter 26",
        "STORY SCENE",
        "Take the shot that can free Sylvara.",
        "The cable is harder to hit than the beast. Birdie must wait for a clear shot.",
        "Birdie the Wise",
        "Scooter",
        "Reed Baelstone",
        "Grask",
        "Goblin Sharpshooter",
        "Birdie the Wise",
        "The cable is controlling her. I have to hit it.",
        "Birdie cuts Sylvara's control cable with her last arrow. Sylvara lives, but Birdie's right arm is badly hurt.",
        "cards/birdieTheWise.png"
    },
    {
        "mw23_choice_not_cure",
        "Free Sylvara",
        "Chapter 27",
        "STORY SCENE",
        "Help Sylvara return to her own shape.",
        "Volunteers lend strength through the roots. Joni stays with Birdie.",
        "Donella of the Marsh",
        "Erevan the Shadow",
        "Reed Baelstone",
        "Blackthorn Alchemist",
        "Blackthorn Foreman",
        "Sylvara",
        "We can share the strain. Anyone who wants to stop can let go.",
        "The Root Key heals the broken root. Sylvara returns to her own shape. The freed prisoners can return home.",
        "cards/donellaOfTheMarsh.png"
    },
    {
        "mw24_agent_not_heir",
        "Open the Prisoners' Path",
        "At the World Tree / before the rescue",
        "OPEN FINALE",
        "Defeat Grask. Keep Reed, Vanya, Joni, and Victor alive until the battle ends.",
        "Push Grask away from the cages. Victor must survive so the rescuers can take him prisoner.",
        "Reed Baelstone",
        "Pavo Quickstep",
        "Nettle Starbright",
        "Victor Greyshard",
        "Grask",
        "Reed Baelstone",
        "Push Grask back. Give the prisoners room!",
        "Grask is defeated. The prisoners have room to work on their cages while the rescuers surround Victor.",
        "cards/reedBaelstone.png"
    },
    {
        "mw25_deed_own_hand",
        "The Last Offer",
        "Chapter 29",
        "STORY SCENE",
        "Keep the town free of Thaeron's control.",
        "The town needs food. Thaeron must still answer for his crimes.",
        "Reed Baelstone",
        "Joni Pumpernickel",
        "Vanya Bluewater",
        "Thaeron Baelstone",
        "Blackthorn",
        "Thaeron Baelstone",
        "Let me leave, and the grain is yours. Come with me, Reed. Blackthorn can still be yours.",
        "The town takes the grain and keeps Thaeron in custody. Neighbors run the storehouse together.",
        "cards/thaeronBaelstone.png"
    }
};

const std::vector<MissionEntry> BlackthornEntries = {
    {
        "bt02_customs_bell", "Blackthorn on the Docks",
        "Chapters 1-2 - patrol rules reconstruction",
        "RELENTLESS, CHARGE, GUNS, AND TAX",
        "Practice extra attacks, long moves, gun actions, and Tax.",
        "Resources pay for cards. Tax takes Resources from the other side when your turn starts. Raise Gun before using Fire.",
        "Braun Stonefist", "Goblin Sharpshooter", "Blackthorn Debt Collector",
        "Reed Baelstone", "Donella of the Marsh", "Narrator",
        "Mog lets a family's boat escape with Reed aboard.",
        "You practiced Relentless, Charge, gun actions, and Tax.",
        "cards/blackthornDebtCollector.png"
    },
    {
        "bt04_terms_conditions", "Strike from Hiding",
        "Chapter 3 - non-canon Ambusher rules drill",
        "HIDE, PASS THROUGH UNITS, AND AMBUSH",
        "Hide two Ambushers. Move through enemies, then strike a hidden scout.",
        "Dematerialize hides an Ambusher. Sneak Around passes through blockers. Ambush deals damage and can hit even a hidden enemy.",
        "Goblin Ambusher", "Blackthorn Alchemist", "Blackthorn Debt Collector",
        "Erevan the Shadow", "Juniper Flash", "Narrator",
        "Juniper rescues a captive father's children. Mog brings him water.",
        "You practiced hiding, passing through blockers, and attacking a hidden training target.",
        "cards/goblinAmbusher.png"
    },
    {
        "bt05_freight_office", "The Last Boat Out",
        "Chapter 6 - operational rules reconstruction",
        "CAPTURE, DAMAGE, AND CHARGE",
        "Use Grask's Swing Axes and Charge, then practice Mog's Axe Swing.",
        "A target that survives keeps its square. Grask stops just before it. Charge needs a clear row or column.",
        "Grask", "Mog", "Goblin Sharpshooter",
        "Reed Baelstone", "Donella of the Marsh", "Victor Greyshard",
        "Hara gives Reed a stolen prison map, then holds the hatch against the guards so he can escape.",
        "You used Swing Axes, Axe Swing, and Charge against training targets.",
        "cards/Grask.png"
    },
    {
        "bt06_receipt_book", "Blackthorn: Optional Battle",
        "Optional open practice",
        "OPTIONAL OPEN BATTLE - CHOOSE YOUR MOVES",
        "Defeat three opposing units while keeping Thaeron alive.",
        "Read each card and choose your own moves. You can skip this battle.",
        "Grask", "Mog", "Blackthorn Debt Collector",
        "Birdie the Wise", "Scooter", "Practice battle",
        "Use a Blackthorn team against three training opponents.",
        "You chose your own moves and won the practice battle.",
        "cards/Grask.png"
    },
    {
        "bt07_debtor_prison", "Escape the Flood",
        "Chapter 9",
        "STORY SCENE",
        "Follow Hara's map into Blackthorn's prison.",
        "Get the prisoners to the boats before the hall floods.",
        "Braun Stonefist", "Goblin Sharpshooter", "Blackthorn Alchemist",
        "Reed Baelstone", "Marshland Veteran", "Narrator",
        "Hara's map leads to Blackthorn's prison in the old mill. A freed prisoner gives Donella the stolen Root Key.",
        "Juniper is badly hurt saving the father from the docks. He carries her onto Mog's raft as Victor floods the prison.",
        "cards/blackthornAlchemist.png"
    },
    {
        "bt08_north_lock", "The Uncle's Offer",
        "Chapter 10",
        "STORY SCENE",
        "Follow Reed into a meeting with Thaeron.",
        "Reed hopes his uncle will stop Victor.",
        "Thaeron Baelstone", "Blackthorn Foreman", "Blackthorn Debt Collector",
        "Reed Baelstone", "Vanya Bluewater", "Thaeron Baelstone",
        "Thaeron offers Reed Blackthorn and admits ordering his parents' deaths.",
        "Reed rejects his uncle. His flare brings Mangletooth to save him, but enemy boats follow them to camp and burn its tents. Reed confesses his secret, and Vanya insists they choose their next step together.",
        "cards/thaeronBaelstone.png"
    },
    {
        "bt10_stolen_road", "Two Taps",
        "Chapters 15-16",
        "STORY SCENE",
        "Watch Mog bring Gearjaw to its old friend.",
        "Gearjaw is a metal guard that once played with Scooter.",
        "Goblin Ambusher", "Braun Stonefist", "Mog",
        "Reed Baelstone", "Swamp Tracker", "Narrator",
        "Mog and Braun bring Gearjaw to Scooter. The machine answers their two-tap signal.",
        "Victor's orders make Gearjaw grab Scooter. Mog pulls Scooter free, but the machine must return to Victor.",
        "cards/goblinAmbusher.png"
    },
    {
        "bt12_break_charter", "Mog Says No",
        "Chapter 18",
        "STORY SCENE",
        "Follow Braun and Mog as they refuse Victor's order.",
        "No board moves in this scene. Victor attacks the town after losing power.",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Goblin Sharpshooter",
        "Reed Baelstone", "Birdie the Wise", "Braun Stonefist",
        "Victor orders Braun to fire on the crowd.",
        "Grask kills Braun for refusing. Mog protects the families, then guides the rescuers after Victor under guard.",
        "cards/braunStonefist.png"
    },
    {
        "bt15_monster_rules", "The Tree's Keeper",
        "Chapter 24 - isolated card demonstration",
        "DIAGONAL ATTACKS AND DISABLE",
        "Use Entangling Lunge and watch the enemy miss two turns.",
        "Ashenfang moves and attacks one or two squares diagonally. Entangling Lunge leaves a surviving enemy Disabled for its next two turns.",
        "Ashenfang", "Marshland Veteran", "Rules demonstration",
        "Company coercion", "Neutral timing marker", "Narrator",
        "At the World Tree, a cable forces Sylvara to attack. Mog knows a way to reach the cages.",
        "You used the Ashenfang card's Entangling Lunge and watched two turns of Disable.",
        "cards/Ashenfang.png"
    },
    {
        "bt16_clean_shot", "The Shot That Frees Her",
        "Chapters 25-27",
        "STORY SCENE",
        "Follow Gearjaw's rescue and Birdie's shot at the command cable.",
        "No board moves in this scene. Saving Sylvara means reaching the cable that forces her to attack.",
        "Grask", "Goblin Sharpshooter", "Blackthorn Alchemist",
        "Birdie the Wise", "Nettle Starbright", "Gearjaw",
        "Mog opens the escape gate while Gearjaw struggles against Victor's control to save Scooter.",
        "Gearjaw destroys its memory core to free its arms and save Scooter. Birdie's shot frees Sylvara, but her arm is broken. Donella returns the Root Key to the Tree.",
        "cards/Grask.png"
    },
    {
        "bt18_deed_own_hand", "The Price of Food",
        "Chapter 29",
        "STORY SCENE",
        "Follow Thaeron's offer to feed the hungry town.",
        "Reed refuses forgiveness or an inheritance in return for food.",
        "Thaeron Baelstone", "Reed Baelstone", "Mara Mudfen",
        "Mirewatch", "Society", "Thaeron Baelstone",
        "Thaeron offers grain in return for freedom and asks Reed to inherit Blackthorn.",
        "Reed refuses to free his uncle or inherit Blackthorn. The town keeps the grain and keys, and Thaeron remains under guard.",
        "cards/thaeronBaelstone.png"
    }
};

StoryMission mirewatchWatcher()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[0]);
    mission.objective = "Defeat both guards. Use Juniper's Spark from a distance, then Erevan's Shadow Blade up close.";
    mission.briefing = {
        panel("Narrator",
            "At the boatyard, children hide beneath loose timber as Company guards search for them. Juniper, a young rescuer, climbs under the beams. Erevan moves to stop the guards.",
            "characters/juniperFlash.png"),
        panel("Juniper",
            "I have you. Crawl toward my voice. Erevan, do not let them follow us.",
            "characters/juniperFlash.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "The children reach the boat. Juniper carries the smaller one, who will not let go of her sleeve.",
            "characters/juniperFlash.png")
    };
    mission.pieces = {
        piece("juniper", "Juniper Flash", 1, 3, 1),
        piece("erevan", "Erevan the Shadow", 1, 5, 1),
        piece("donella", "Donella of the Marsh", 1, 6, 1),
        piece("far_guard", "Blackthorn Lumberjack", 2, 3, 3, 1),
        piece("near_guard", "Blackthorn Debt Collector", 2, 5, 2, 1)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"juniper", "erevan", "donella"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "juniper", 3, 3,
            "Spark", "JUNIPER'S RANGE", "Use Spark on the Lumberjack at D4, two squares along Juniper's row. It has 1 Health. Spark deals 1 damage and defeats it.",
            "Choose the marked guard at D4. Juniper stays in place when she uses Spark.", "far_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END YOUR TURN", "End Turn. Erevan can act on your next turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The children crawl toward the boat. Your next turn begins.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "erevan", 5, 2,
            "Shadow Blade", "EREVAN AT CLOSE RANGE", "Use Shadow Blade on the Debt Collector beside Erevan at C6.",
            "Use Shadow Blade on the guard at C6.", "near_guard", {},
            {panel("Narrator",
                "A beam slips. Juniper catches it on her shoulder. The children crawl free, and Erevan turns to stop the last guard.",
                "cards/erevanTheShadow.png")})
    };
    // Juniper's full card mastery is awarded in Chapter 13 after Sprint and
    // automatic Intercept have both been demonstrated. This early lesson
    // actively reinforces Erevan's adjacent Shadow Blade instead.
    mission.masteryCards = {};
    mission.masteryRules = {
        "orthogonal-ranged-attack", "range-two", "one-activation-per-turn"};
    return mission;
}

StoryMission mirewatchOffice()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[1]);
    mission.objective = "Hide Erevan, move through two Lumberjacks, then use Hidden Shove on the Debt Collector.";
    mission.briefing = {
        panel("Narrator",
            "Erevan finds Hara at a canal hatch. She has stolen a map of Blackthorn's prisons. Guards are coming down the passage behind her.",
            "cards/mirewatchInformant.png"),
        panel("Hara",
            "Take the map. Find my mother. I will hold them here.",
            "cards/mirewatchInformant.png"),
        panel("Erevan",
            "I will come back for you.",
            "cards/erevanTheShadow.png"),
        panel("Coach",
            "Use Erevan's shadow actions to carry the map through the guards.",
            "cards/erevanTheShadow.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "Hara holds the hatch shut as Erevan escapes. He brings the map home. Juniper returns with Hara's mother. They wait by the dock, but Hara does not come.",
            "characters/juniperFlash.png")
    };
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
    mission.script = {
        scriptedAbility(1, "erevan", "dematerialize", "Dematerialize",
            "DEMATERIALIZE EREVAN", "Use Dematerialize to hide Erevan. Mouse: click Erevan's card, then click the Dematerialize button. Keyboard: focus him with the arrows, press Enter, then press A.",
            "Use Dematerialize before moving past the guards."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDING AND CONTROL", "End Turn. Hidden Erevan adds no control to nearby squares. If both sides have equal control there, the square keeps its current owner.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE GUARDS WATCH THE DOOR", "The guards stay where they are.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "erevan", 3, 4,
            "Fade Through Shadow", "MOVE THROUGH THE GUARDS", "Use Fade Through Shadow to move from B4 to E4. Erevan passes through the guards at C4 and D4.",
            "Use Fade Through Shadow to reach E4."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "STAY UNSEEN", "End Turn while Erevan remains hidden beside the Debt Collector.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "erevan", 2, 4,
            "Hidden Shove", "HIDDEN SHOVE", "Use Hidden Shove on the Debt Collector at E3 to push it away from Erevan.",
            "Use Hidden Shove to push the guard at E3.", "ledger_guard")
    };
    mission.masteryCards = {"Erevan the Shadow"};
    mission.masteryRules = {
        "dematerialize", "hidden-adds-no-control", "pass-through", "push"};
    return mission;
}

StoryMission mirewatchWagonRescue(bool fullPractice = false)
{
    StoryMission mission = playableFromEntry(MirewatchEntries[2]);
    mission.objective = "Use Birdie's Longbow Shot, then Scooter's River Dash and its repeat to reach the back of the wagon.";
    mission.briefing = {
        panel("Narrator",
            "A prisoner wagon blocks the street. A family calls for help from inside. Birdie climbs above the road with her bow. Her beaver friend Scooter slips toward the wagon's rear.",
            "cards/birdieTheWise.png"),
        panel("Birdie",
            "Scooter, go around the back. I will keep that guard away from you.",
            "cards/birdieTheWise.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "Scooter reaches the back of the wagon and lifts its latch. The family climbs out. Birdie covers them all the way to the river.",
            "cards/scooter.png")
    };
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
        piece("rebirth_gator", "Bull Gator", 2, 5, 3),
        piece("rush_target", "Blackthorn Debt Collector", 2, 4, 4, 1)};
    mission.playerDrawPile = {"Mirewatch Informant", "Bog Spearman"};
    mission.playerResources = 50;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {
        "birdie", "scooter", "veteran", "smuggler", "spearman"};
    mission.script = {
        scripted(StoryActionKind::DrawCard, 1, {}, -1, -1,
            "BIRDIE - FORESIGHT",
            "Choose Draw. It costs 50 Resources. Birdie's Foresight shows the top two cards so you can choose which one to draw.",
            "Choose Draw to use Foresight."),
        scriptedCard(StoryActionKind::ChooseForesight, 1, "Bog Spearman", -1, -1,
            "CHOOSE WITH FORESIGHT",
            "Choose Bog Spearman. Mirewatch Informant returns to the bottom of the draw pile.",
            "Choose Bog Spearman from the two revealed cards."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "birdie", 3, 4,
            "Longbow Shot", "BIRDIE - LONGBOW SHOT", "Use Longbow Shot on the Debt Collector at E4, three squares along Birdie's row.",
            "Use Longbow Shot on marked E4.", "notice_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COOLDOWN BEGINS", "End Turn. Longbow Shot has Cooldown 1. Birdie cannot act on your next turn. She becomes ready again when the turn after that begins.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "scooter", 7, 3,
            "River Dash", "SCOOTER - RIVER DASH", "Use River Dash diagonally from B6 to D8. Repeat 1 gives one extra use of the same action. Take it in the next step. In normal play, you may choose Pass instead.",
            "River Dash only moves. Choose empty D8."),
        scriptedPieceAction(StoryActionKind::Move, 1, "scooter", 6, 4,
            "River Dash", "SCOOTER - REQUIRED REPEAT", "Repeat River Dash from D8 to E7. Finish the same action before choosing another piece.",
            "Use River Dash again to reach E7."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "FINISH THE DASH", "End Turn after both Dash actions. Scooter has reached the far side of the guarded wagon.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "veteran", 6, 3,
            "Advance", "VETERAN - ADVANCE", "Use Advance on the Lumberjack at D7, two squares along the Veteran's row. It survives, so the Veteran stops before its square.",
            "Use Advance. The guard survives, so the Veteran stops before its square.",
            "durable_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SURVIVING DEFENDER", "End Turn. The Veteran has stopped in front of the surviving guard.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "smuggler", 2, 2,
            "Swashbuckle Blade", "SMUGGLER - FIRST BLADE", "Use Swashbuckle Blade diagonally from B2 into the Debt Collector at C3.",
            "Use Swashbuckle Blade on the first guard.", "route_guard_one"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "smuggler", 1, 3,
            "Swashbuckle Blade", "SMUGGLER - SECOND BLADE", "Repeat diagonally from C3 into the Debt Collector at D2.",
            "Use Swashbuckle Blade again on the guard at D2.", "route_guard_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE PATH BEHIND THE WAGON CLEARS", "End Turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "spearman", 2, 3,
            "Hooked Spear", "SPEARMAN - HOOKED SPEAR",
            "Use Hooked Spear from B5 to hit the Debt Collector at D3. The guard has 1 Health and is defeated. Pull cannot move a defeated target. The Spearman stays at B5.",
            "Use Hooked Spear on the guard at D3, two diagonal squares away.", "spear_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE TRACKER", "End Turn for the mounted scout's jump.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The enemy turn ends.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "tracker", 6, 3,
            "Frogback Leap", "TRACKER - FROGBACK LEAP", "Jump from B8 into the weakened Lumberjack at D7.",
            "Use Frogback Leap to reach D7. The Tracker jumps over any squares in the way.", "durable_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "BRACE FOR THE BITE", "End Turn. The bite will defeat the mount; Rebirth leaves the Tracker alive on foot.",
            "End Turn after the Tracker's Leap."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "rebirth_gator", 6, 3,
            "Bite", "BITE TRIGGERS REBIRTH", "The Bull Gator bites the wounded Tracker. Losing the mount triggers Rebirth.",
            "This step could not finish. Restart the battle.", "tracker"),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "UNMOUNTED TRACKER READIES",
            "The gator ends its turn. The Tracker is now on foot and ready to act.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "tracker", 5, 3,
            "Tracker's Spear", "UNMOUNTED TRACKER - SPEAR",
            "Use Tracker's Spear from D7 to hit the gator at D6. The gator survives, so the Tracker stays at D7.",
            "Use Tracker's Spear on the gator at D6.", "rebirth_gator"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE SPEAR LINE", "End Turn after the unmounted Tracker acts.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The remaining targets hold.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "scooter", 4, 4,
            "River Rush", "SCOOTER - RIVER RUSH",
            "Use River Rush from E7 to hit the Debt Collector at E5. Move two squares up column E and defeat the wounded guard.",
            "Use River Rush along column E to the guard at E5.", "rush_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE RIVER LINE", "End Turn after River Rush.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The Bull Gator holds.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "smuggler", 1, 4,
            "Step", "SMUGGLER - STEP",
            "Use Step from D2 to empty E2. The Smuggler guards the path behind the wagon.",
            "Use Step to move the Smuggler one square to E2."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE REAR LINE", "End Turn after the Smuggler steps.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The Bull Gator holds.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "birdie", 0, 4,
            "Scout", "BIRDIE - SCOUT",
            "Use Scout diagonally from B4 to E1. Birdie moves toward the wagon lock.",
            "Use Scout to move Birdie diagonally to E1.")
    };
    mission.masteryCards = {
        "Birdie the Wise", "Scooter", "Marshland Veteran", "Resistance Smuggler",
        "Bog Spearman"};
    mission.masteryRules = {
        "foresight", "draw-cost", "cooldown", "repeat-lock",
        "orthogonal-movement", "diagonal-movement",
        "attacking-move-survivor-staging", "ranged-diagonal-attack",
        "pull-requires-surviving-target", "knight-jump", "pass-through", "rebirth",
        "dependent-rebirth-action", "normal-enemy-attack"};
    if (fullPractice)
    {
        mission.id = "mw06a_card_workshop";
        mission.title = "Mirewatch: Card Workshop";
        mission.lesson = "DRAW, REPEAT, JUMP, AND REBIRTH";
        mission.sourceChapter = "Optional card practice after the story";
        mission.objective = "Try the full set of moves with Birdie, Scooter, and their allies. This practice is separate from the wagon rescue.";
        mission.hint = "Each step names the real card action and target. You can skip this practice and return from Missions.";
        mission.optionalRehearsal = true;
        mission.briefing = {
            panel("Coach", "Start with Foresight. Then follow the named actions to see how these cards work together.", "cards/birdieTheWise.png"),
            panel("Coach", "Choose Start Practice when you are ready. Skipping this lesson leaves the story complete.", "cards/birdieTheWise.png")};
        mission.aftermath = {panel("Coach", "Practice complete. You can inspect these cards during any match to check their moves.", "cards/birdieTheWise.png")};
        for (auto& step : mission.script)
        {
            step.panelsBefore.clear();
            if (step.kind == StoryActionKind::EndTurn && step.owner == 2)
            {
                step.heading = "YOUR NEXT TURN";
                step.instruction = "The opposing side ends its turn.";
            }
        }
    }
    else
    {
        std::vector<StoryScriptAction> rescueSteps;
        for (const std::size_t index : {2u, 3u, 4u, 5u, 6u, 7u})
        {
            rescueSteps.push_back(mission.script[index]);
        }
        mission.script = std::move(rescueSteps);
        mission.playerHand.clear();
        mission.playerDrawPile.clear();
        mission.playerResources = 0;
        mission.masteryCards.clear();
        mission.masteryRules = {"cooldown", "repeat-lock"};
        mission.lesson = "COVER SCOOTER'S DASH";
        mission.objective = "Use Birdie's Longbow Shot to clear a guard. Dash Scooter to the back of the wagon.";
        mission.hint = "Use Longbow Shot, then use River Dash twice. Scooter reaches the wagon while Birdie covers him.";
    }
    return mission;
}

StoryMission mirewatchAuction()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[4]);
    mission.objective = "Use Blade Dance twice to defeat both guards. Then move Vanya with Sidestep.";
    mission.briefing = {
        panel("Narrator",
            "Hara's map leads to the Vault, a prison inside the old mill. Mog, a Company soldier, meets the rescuers at the door. He holds out the key.",
            "cards/Mog.png"),
        panel("Mog",
            "I locked people in these cells. Then I thought Victor would lock me in too. I was afraid long before I was brave. Let me open the door.",
            "cards/Mog.png"),
        panel("Narrator",
            "Inside, a young prisoner works a fish bone into her lock. Vanya moves between her and the guards. Juniper finds children hiding beneath the stairs.",
            "cards/vanyaBluewater.png"),
        panel("Young prisoner",
            "I nearly have it open. Keep those guards away!",
            "cards/vanyaBluewater.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "The young prisoner opens her cage. She gives Donella a piece of living root hidden in her coat.",
            "cards/donellaOfTheMarsh.png"),
        panel("Young prisoner",
            "They stole this Root Key from the World Tree. Victor is hurting the Tree's keeper. Take it back to her. It can heal the roots.",
            "cards/donellaOfTheMarsh.png"),
        panel("Narrator",
            "An alarm sounds. The guards open a floodgate above the prison. River water pours into the hall. Mog pulls a raft toward the captives who cannot swim. Reed runs for the exit gate.",
            "cards/Mog.png")
    };
    mission.pieces = {
        piece("vanya", "Vanya Bluewater", 1, 5, 1),
        piece("joni", "Joni Pumpernickel", 1, 6, 1),
        piece("birdie", "Birdie the Wise", 1, 7, 1),
        piece("first_guard", "Blackthorn Debt Collector", 2, 4, 2),
        piece("second_guard", "Blackthorn Debt Collector", 2, 3, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"vanya", "joni", "birdie"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 4, 2,
            "Blade Dance", "VANYA - BLADE DANCE", "Use Blade Dance on the guard at C5. Vanya defeats it and moves diagonally into its square. Repeat 1 gives one more Blade Dance. This lesson uses that second attack.",
            "Use Blade Dance on the guard at C5.", "first_guard"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 3, 3,
            "Blade Dance", "FINISH THE DANCE", "Use Blade Dance again on the guard at D4. Finish this repeat before choosing another piece.",
            "Use Blade Dance again on the guard at D4. This lesson requires the second attack.", "second_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE DANCE ENDS", "End Turn after both required Blade Dance uses.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE GUARDS END THEIR TURN", "The opposing side passes.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "vanya", 2, 3,
            "Sidestep", "VANYA - SIDESTEP",
            "Use Sidestep one square up from D4 to empty D3.",
            "Use Sidestep to move Vanya straight up to D3.")
    };
    mission.masteryCards = {"Vanya Bluewater"};
    mission.masteryRules = {
        "repeat-lock", "attacking-movement", "orthogonal-movement"};
    return mission;
}

StoryMission mirewatchVaultCost()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[5]);
    mission.lesson = "SPRINT, POSITIONING, AND INTERCEPT";
    mission.objective =
        "Clear two paths. Move Juniper beside the runner so she can protect him with Intercept.";
    mission.hint =
        "Move Juniper beside the runner. Her Intercept swaps her with an adjacent ally and takes a damaging enemy piece attack for them. Focus her and press I to inspect the full card rule.";
    mission.briefing = {
        panel("Narrator",
            "Water rises beneath the prison balcony. Mog loads the raft while the young prisoner helps the children climb down. The father they freed knows the path to the boats. Juniper has been hurt, but she stays beside him. This runner will lead the children out.",
            "characters/juniperFlash.png"),
        panel("Reed",
            "I can hold this gate open. Keep moving!",
            "cards/reedBaelstone.png"),
        panel("Coach",
            "Clear the paths. Then move Juniper beside the runner. When a guard attacks him, Intercept swaps her into his place. She takes the hit. This happens automatically. Focus her and press I to read her card.",
            "characters/juniperFlash.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "The runner catches Juniper as she falls. He carries her onto the raft. She is badly hurt, but breathing. Vanya presses a clean cloth against her wound.",
            "characters/juniperFlash.png"),
        panel("Vanya",
            "Stay with us, Juniper. Children, sit beside me. We are getting out together.",
            "cards/vanyaBluewater.png"),
        panel("Narrator",
            "Mog brings the last captives through the gate. Reed lets it slam shut behind them. The raft reaches the boats. Hara is not among the people they rescued.",
            "cards/reedBaelstone.png")
    };
    mission.pieces = {
        piece("birdie", "Birdie the Wise", 1, 2, 1),
        piece("juniper", "Juniper Flash", 1, 3, 1),
        piece("reed", "Reed Baelstone", 1, 6, 1),
        piece("runner", "Mirewatch Informant", 1, 4, 2),
        piece("north_guard", "Blackthorn Debt Collector", 2, 2, 4, 1),
        piece("south_guard", "Blackthorn Debt Collector", 2, 6, 4, 1),
        piece("intercept_attacker", "Blackthorn Debt Collector", 2, 5, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"birdie", "reed", "runner"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "birdie", 2, 4,
            "Longbow Shot", "OPEN THE NORTH LANE",
            "Use Birdie's Longbow Shot from B3 through C3 and D3 to the Debt Collector at E3.",
            "Use Longbow Shot on the guard at E3.", "north_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE NORTH LANE", "End Turn after Birdie acts.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FIRST PATH IS CLEAR", "The first children cross the cleared path toward the runner.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "reed", 6, 4,
            "Bow", "OPEN THE SOUTH LANE",
            "Use Reed's Bow from B7 through C7 and D7 to the Debt Collector at E7.",
            "Use Bow on the guard at E7.", "south_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE SOUTH LANE", "End Turn after Reed acts.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST CHILDREN CROSS", "The last children reach the runner. The guard raises his knife.", "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "juniper", 4, 1,
            "Sprint", "JUNIPER - SPRINT INTO POSITION",
            "Use Sprint from B4 to B5. Juniper will stand beside the runner at C5.",
            "Move Juniper one square down to highlighted B5."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "LET JUNIPER PROTECT THE RUNNER",
            "End Turn. Juniper is beside the runner as the guard attacks.",
            "End Turn to continue the escape."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "intercept_attacker", 4, 2,
            "Knife Stab", "JUNIPER TAKES THE ATTACK",
            "The guard uses Knife Stab on the runner at C5. Intercept swaps Juniper into his place. The attack hits her, and the runner is safe.",
            "This step could not finish. Restart the battle.", "runner")};
    mission.script[6].panelsBefore = {
        panel("Juniper",
            "He is going for the runner. I will get between them. Keep the children moving!",
            "characters/juniperFlash.png")};
    mission.masteryCards = {"Reed Baelstone", "Juniper Flash"};
    mission.masteryRules = {
        "orthogonal-movement", "intercept-automatic", "intercept-adjacent-ally",
        "intercept-position-swap", "intercept-receives-damage"};
    return mission;
}

StoryMission mirewatchRevealReconstruction()
{
    StoryMission mission = chronicleFromEntry(MirewatchEntries[6]);
    mission.title = "Find a Hidden Enemy - Optional Practice";
    mission.sourceChapter = "After the rescue / optional rules practice";
    mission.lesson = "OPTIONAL PRACTICE - HIDE, JUMP, AND REVEAL";
    mission.objective = "Let the Ambusher hide. Jump the Swamp Tracker beside it, then End Turn to trigger Reveal.";
    mission.hint = "Reveal happens at the end of its owner's turn. It makes adjacent enemies hidden by Dematerialize visible again.";
    mission.briefing = {
        panel("Coach",
            "Try a quiet practice with the Swamp Tracker. A hidden Ambusher waits nearby. Jump beside it, then End Turn to reveal it. You can skip this lesson.",
            "cards/swampTracker_mounted.png")
    };
    mission.aftermath = {
        panel("Coach",
            "At the end of your turn, Reveal made the nearby hidden enemy visible. It dealt no damage.",
            "cards/swampTracker_mounted.png")
    };
    mission.pieces = {
        piece("tracker", "Swamp Tracker", 1, 4, 1),
        piece("hidden_ambusher", "Goblin Ambusher", 2, 2, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.firstPlayer = 2;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"tracker"};
    mission.optionalRehearsal = true;
    mission.script = {
        scriptedAbility(2, "hidden_ambusher", "dematerialize", "Dematerialize",
            "AMBUSHER - DEMATERIALIZE",
            "The enemy uses Dematerialize and becomes hidden.",
            "This step could not finish. Restart the battle."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HIDDEN TURN ENDS",
            "The Ambusher ends its turn while hidden.",
            "This step could not finish. Restart the battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "tracker", 2, 2,
            "Frogback Leap", "TRACKER - FROGBACK LEAP",
            "Use Frogback Leap from B5 to C3. This knight jump moves two squares up and one across. It lands beside the hidden Ambusher at D3.",
            "Move the Swamp Tracker to highlighted C3."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END TURN TO TRIGGER REVEAL",
            "End Turn. Reveal checks nearby enemies and makes the hidden Ambusher visible again.",
            "Use End Turn with the Tracker beside the hidden enemy.")};
    mission.masteryCards = {"Swamp Tracker"};
    mission.masteryRules = {
        "dematerialize", "knight-jump", "passive-reveal-at-owner-end-turn",
        "reveal-adjacent-enemy", "reveal-materializes-without-damage-or-disable"};
    return mission;
}

StoryMission blackthornCustoms()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[0]);
    mission.objective =
        "Practice extra attacks, long moves, gun actions, and Tax.";
    mission.briefing = {
        panel("Narrator",
            "Victor sends Mog to take the same family's boat. Reed, a boy helping prisoners escape, stands on its deck. Mog sees the torn hull he helped drag ashore.",
            "cards/Mog.png"),
        panel("Narrator",
            "Mog lets the rope slide through his fingers. Reed pushes off before the other guards turn. For the first time, Mog is glad he failed an order.",
            "cards/reedBaelstone.png"),
        panel("Optional practice",
            "Practice extra attacks, long moves, gun actions, and Tax, or continue the story.",
            "cards/victorGreyshard.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Practice battle",
            "Try the patrol's cards against training targets. Victor's Relentless lets him act again after he defeats an enemy. Take the first extra action. Then use End Turn to skip the next one.",
            "cards/victorGreyshard.png"),
        panel("Coach",
            "Resources pay for cards. Tax takes Resources from the other side when your turn starts. Raise Gun before using Fire.",
            "cards/blackthornDebtCollector.png"),
        panel("Coach",
            "Pieces control the squares they stand on and add influence to empty squares beside them. More influence wins an empty square. A tie keeps its current owner. Each square you control earns 1 Resource when your turn starts.",
            "cards/blackthornDebtCollector.png")};
    mission.aftermath = {
        panel("Coach",
            "You used Relentless, Charge, the Sharpshooter's gun actions, and Knife Stab. Tax works at the start of your turn.",
            "cards/blackthornDebtCollector.png")};
    mission.pieces = {
        piece("victor", "Victor Greyshard", 1, 0, 0),
        piece("braun", "Braun Stonefist", 1, 3, 1),
        piece("sharpshooter", "Goblin Sharpshooter", 1, 5, 1),
        piece("collector", "Blackthorn Debt Collector", 1, 6, 1),
        piece("relentless_target_one", "Blackthorn Debt Collector", 2, 0, 1, 1),
        piece("relentless_target_two", "Blackthorn Debt Collector", 2, 1, 1, 1),
        piece("signal_guard", "Bull Gator", 2, 5, 7),
        piece("crest_guard", "Blackthorn Debt Collector", 2, 7, 2)};
    mission.playerResources = 0;
    mission.enemyResources = 12;
    mission.requiredSurvivorRoles = {"victor", "braun", "sharpshooter", "collector"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 0, 1,
            "Axe Dance", "VICTOR - AXE DANCE",
            "Use Axe Dance on the enemy at B1. It has 1 Health. Defeat it to trigger Relentless.",
            "Choose Victor's Axe Dance and target B1.", "relentless_target_one"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 1, 1,
            "Axe Dance", "VICTOR - EXTRA ATTACK",
            "Relentless lets Victor act again. Use Axe Dance on the enemy at B2. It also has 1 Health.",
            "Use Victor again. The extra action belongs to him. In normal play, you may use End Turn to skip it.", "relentless_target_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SKIP THE EXTRA ACTION",
            "The second defeat triggers Relentless again. Use End Turn to skip this extra action.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS",
            "The opponent passes.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "braun", 3, 4,
            "Charge", "BRAUN - CHARGE", "Use Charge to move Braun from B4 to E4, three squares along the row.",
            "Choose Charge, then empty E4. Charge can move along a row or column."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Use End Turn to continue.", "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "sharpshooter", "transform", "Raise Gun",
            "SHARPSHOOTER - RAISE GUN", "Select the Goblin Sharpshooter. Press A or choose Raise Gun to use its ability.",
            "Use Raise Gun. With the gun raised, the Sharpshooter can Fire.", {}, "Lower Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Use End Turn to continue.", "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sharpshooter", 5, 7,
            "Fire", "SHARPSHOOTER - FIRE", "Use Fire on the Bull Gator at H6. The row is clear. The hit deals 3 damage, leaving 1 Health.",
            "Use the raised Sharpshooter's Fire on H6.", "signal_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "WATCH TAX", "Use End Turn. The Debt Collector takes Resources when your next turn starts.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "TAX TAKES AVAILABLE RESOURCES", "Tax takes Resources from the opponent. It cannot take more than the opponent has.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "collector", 7, 2,
            "Knife Stab", "COLLECTOR - KNIFE STAB", "Use Knife Stab diagonally on the enemy Debt Collector at C8.",
            "Choose your Collector's Knife Stab and target C8.", "crest_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Use End Turn to continue.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "sharpshooter", "transform", "Lower Gun",
            "SHARPSHOOTER - LOWER GUN",
            "Use Lower Gun. The Sharpshooter can now use Advance to move, but cannot Fire.",
            "Select the Sharpshooter and use Lower Gun.", {}, "Raise Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Use End Turn to continue.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "sharpshooter", 5, 2,
            "Advance", "SHARPSHOOTER - ADVANCE",
            "Use Advance to move from B6 to C6. The gun must be lowered to use this action.",
            "Choose Advance and move to C6.")
    };
    mission.masteryCards = {
        "Victor Greyshard", "Braun Stonefist", "Goblin Sharpshooter",
        "Blackthorn Debt Collector"};
    mission.masteryRules = {
        "relentless-optional-extra-action", "relentless-action-lock",
        "orthogonal-long-movement", "transform", "state-specific-actions",
        "line-of-sight", "tax", "health-and-destruction"};
    mission.optionalRehearsal = true;
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission blackthornTerms()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[1]);
    mission.objective =
        "Practice hiding, moving past blockers, and attacking a hidden target.";
    mission.hint =
        "Use Dematerialize to hide. Sneak Around passes through the Veterans. Use Ambush at C6 to damage the hidden scout and make him miss his next turn.";
    mission.briefing = {
        panel("Narrator",
            "Blackthorn catches the father from the boat and locks him in a wagon. His children hide beneath loose timber. Juniper, one of Reed's friends, pulls them clear while another rescuer brings a boat.",
            "characters/juniperFlash.png"),
        panel("Mog",
            "Here. Drink this. Juniper got your children out. They're safe.",
            "cards/Mog.png"),
        panel("Narrator",
            "The father grips Mog's cup. Braun, another guard, steps in front of Grask and asks about the next wagon. Mog leaves the cup with the prisoner. Braun gives him a quick nod.",
            "cards/braunStonefist.png"),
        panel("Optional practice",
            "Practice hiding, moving past blockers, and attacking hidden targets, or continue the story.",
            "cards/goblinAmbusher.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Practice battle",
            "Now use two Ambushers on the practice board. The opposing Erevan the Shadow card is your training scout. Use the named action even if its target square looks empty.",
            "cards/goblinAmbusher.png"),
        panel("Coach",
            "Dematerialize hides an Ambusher. While hidden, Sneak Around can pass through pieces. Ambush deals damage and can hit even a hidden enemy. Sneak Around does not deal that damage.",
            "cards/goblinAmbusher.png")};
    mission.aftermath = {
        panel("Coach",
            "You hid, passed through blockers, and attacked a hidden target. You also used Stab while visible.",
            "cards/goblinAmbusher.png")};
    mission.pieces = {
        piece("ambusher_path", "Goblin Ambusher", 1, 3, 1),
        piece("ambusher_collision", "Goblin Ambusher", 1, 5, 1),
        piece("lane_blocker_one", "Marshland Veteran", 2, 3, 2),
        piece("lane_blocker_two", "Marshland Veteran", 2, 3, 3),
        piece("erevan", "Erevan the Shadow", 2, 5, 2),
        piece("stab_target", "Blackthorn Debt Collector", 2, 4, 0, 1)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"ambusher_path", "ambusher_collision", "erevan"};
    mission.script = {
        scriptedAbility(1, "ambusher_path", "dematerialize", "Dematerialize",
            "HIDE THE FIRST AMBUSHER", "Select the Ambusher at B4 and use Dematerialize.",
            "Use Dematerialize on the upper Ambusher at B4."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDING AND CONTROL", "Use End Turn. Hidden pieces add no influence to nearby empty squares. Tied squares keep their current owner.", "Use End Turn to continue."),
        scriptedAbility(2, "erevan", "dematerialize", "Dematerialize",
            "THE TRAINING SCOUT HIDES", "The opposing Erevan the Shadow card uses Dematerialize at C6.", "This step could not finish. Restart the practice battle."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SCOUT IS HIDDEN", "The training scout is hidden at C6.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "ambusher_path", 3, 4,
            "Sneak Around", "SNEAK THROUGH THE GUARDS", "Use Sneak Around from B4 to E4, passing through the Veterans at C4 and D4.",
            "Choose Sneak Around. A hidden Ambusher can pass through these blockers."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "TRY THE SECOND AMBUSHER", "Use End Turn before trying the lower path.", "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SCOUT WAITS", "The training scout stays hidden at C6.", "This step could not finish. Restart the practice battle."),
        scriptedAbility(1, "ambusher_collision", "dematerialize", "Dematerialize",
            "HIDE THE SECOND AMBUSHER", "Select the Ambusher at B6 and use Dematerialize.",
            "Use Dematerialize on the lower Ambusher at B6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "AN EMPTY-LOOKING SQUARE", "Use End Turn. The glowing move preview does not reveal hidden enemies.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE TRAINING SCOUT WAITS", "The training scout remains hidden.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Move, 1, "ambusher_collision", 5, 2,
            "Ambush", "AMBUSH THE HIDDEN SCOUT", "Choose Ambush, then C6. The training scout is hidden there. The attack deals 1 damage and makes both units visible.",
            "Choose Ambush before targeting C6. Sneak Around will not deal this damage."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "AFTER THE AMBUSH",
            "Use End Turn. Your Ambusher is visible again.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE TRAINING SCOUT CANNOT ACT",
            "The damage made the training scout Disabled. He misses this turn.",
            "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ambusher_collision", 4, 0,
            "Stab", "AMBUSHER - STAB",
            "Your Ambusher is visible again. Use Stab diagonally from B6 to the wounded Debt Collector at A5.",
            "Choose Stab and target A5.",
            "stab_target")
    };
    mission.masteryCards = {"Goblin Ambusher"};
    mission.masteryRules = {
        "dematerialize", "hidden-adds-no-control", "pass-through",
        "hidden-collision-reveal", "hidden-collision-damage", "collision-stun",
        "visible-state-action"};
    mission.optionalRehearsal = true;
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission blackthornOffice()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[2]);
    mission.objective =
        "Practice Capture, damage, and long attacks.";
    mission.briefing = {
        panel("Narrator",
            "Hara, a dock worker, steals a map of Blackthorn's prison. Guards chase her to a canal hatch.",
            "cards/mirewatchInformant.png"),
        panel("Narrator",
            "Hara pushes the map through the hatch to Erevan. Then she holds the hatch shut while guards pull at the other side. Erevan's boat slips away. Mog watches Hara struggle to hold on.",
            "cards/mirewatchInformant.png"),
        panel("Mog",
            "She stayed so they could get away.",
            "cards/Mog.png"),
        panel("Narrator",
            "Hara does not come out of the yard. Erevan brings her map to Reed. They will use it to free the prisoners, then search for her.",
            "cards/reedBaelstone.png"),
        panel("Optional practice",
            "Practice Capture, damage, and long attacks, or continue the story.",
            "cards/Grask.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Practice battle",
            "Try Swing Axes, Grask's diagonal Capture action. Then use Mog's Axe Swing and Grask's Charge against the training targets.",
            "cards/Mog.png"),
        panel("Coach",
            "Capture moves toward a target and attacks. If the target falls, Grask takes its square. If it survives, he stops just before it. Damage makes a survivor miss its next turn. Charge needs a clear row or column.",
            "cards/Grask.png")};
    mission.aftermath = {
        panel("Coach",
            "You saw a target survive Capture and keep its square. Then you used Axe Swing and a long Charge.",
            "cards/Grask.png")};
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
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 4, 2,
            "Swing Axes", "GRASK - SWING AXES", "Use Swing Axes on the Veteran at C5. It deals 2 damage against 3 Health. The Veteran survives, so Grask stays at B4.",
            "Choose Swing Axes, Grask's diagonal Capture action, and target C5.", "capture_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEXT TURN", "Use End Turn to continue.", "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "mog", 5, 2,
            "Axe Swing", "MOG - AXE SWING", "Use Mog's Axe Swing on the Veteran at C6. It deals 2 damage and leaves 1 Health.",
            "Choose Mog's Axe Swing and target C6.", "mog_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SURVIVORS KEEP THEIR SQUARES", "Use End Turn. The Veteran survived and still blocks its square.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OPPONENT WAITS", "The opponent passes.", "This step could not finish. Restart the practice battle."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 3, 5,
            "Charge", "GRASK - CHARGE", "Use Charge on the Bog Spearman at F4, four squares along the row.",
            "Choose Grask's Charge and target F4. The path must be clear.", "charge_target")
    };
    mission.masteryCards = {"Grask", "Mog"};
    mission.masteryRules = {
        "capture", "capture-staging", "health-and-destruction",
        "long-orthogonal-attacking-movement"};
    mission.optionalRehearsal = true;
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission blackthornMonsterRules()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[8]);
    mission.objective =
        "Practice a diagonal attack and watch Disable last two turns.";
    mission.pieces = {
        piece("ashenfang", "Ashenfang", 1, 3, 1),
        piece("veteran", "Marshland Veteran", 2, 5, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"ashenfang"};
    mission.briefing = {
        panel("Narrator",
            "The World Tree rises above Victor's cages. Sylvara, its keeper, blocks the exit. A cable pulls tight at her neck, forcing her to strike whenever a prisoner moves.",
            "cards/Sylvara.png"),
        panel("Mog",
            "I know the side gate. I'll hold it open and keep Grask away. I wore their badge. Let me be the one he has to face.",
            "cards/Mog.png"),
        panel("Birdie the Wise",
            "I'll cut Sylvara's cable with an arrow. Reed, take the prisoners through Mog's gate. Donella, get the Root Key to the Tree. We move together.",
            "cards/birdieTheWise.png"),
        panel("Optional practice",
            "Practice a diagonal attack and two turns of Disable, or continue the rescue.",
            "cards/Ashenfang.png")};
    const std::vector<StoryPanel> practiceInstructions = {
        panel("Coach",
            "Use the Ashenfang card for this drill. Entangling Lunge moves and attacks one or two squares diagonally. A surviving target keeps its square and misses its next two turns.",
            "cards/Ashenfang.png")};
    mission.aftermath = {
        panel("Coach",
            "Entangling Lunge left the Veteran Disabled for two turns. You watched both turns and saw Disable end.",
            "cards/Ashenfang.png")};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "ashenfang", 5, 3,
            "Entangling Lunge", "ASHENFANG - ENTANGLING LUNGE", "Use Entangling Lunge on the Veteran at D6, two squares diagonally from Ashenfang.",
            "Choose Entangling Lunge and target D6.", "veteran"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: FIRST TURN", "Use End Turn. The Veteran must miss his first turn.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE VETERAN CANNOT ACT", "The Veteran misses this turn. One Disabled turn remains.", "This step could not finish. Restart the practice battle."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: SECOND TURN", "Use End Turn again. The Veteran must miss one more turn.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "DISABLE ENDS", "The Veteran misses his second turn. Disable now ends.",
            "This step could not finish. Restart the practice battle.")
    };
    // A player who completed the earlier Alchemist application and healing
    // drills now has the generic two-turn expiry proof needed for that card's
    // cumulative mastery. Players who skipped those drills receive the same
    // complete Alchemist proof if they play the optional BT17 card review.
    mission.masteryCards = {"Ashenfang"};
    mission.masteryRules = {
        "diagonal-range-two", "disable-two-turns", "status-duration"};
    mission.optionalRehearsal = true;
    // Detailed teaching appears only after Start Practice. Preserve any
    // existing first-action panels after these introductory instructions.
    auto& firstActionPanels = mission.script.front().panelsBefore;
    firstActionPanels.insert(firstActionPanels.begin(),
        practiceInstructions.begin(), practiceInstructions.end());
    return mission;
}

StoryMission mirewatchTollAlliance()
{
    StoryMission mission = chronicleFromEntry(MirewatchEntries[8]);
    mission.lesson = "OPEN BATTLE - TEAMWORK AND SURVIVAL";
    mission.objective = "Defeat all four Company guards. Keep Reed, Vanya, and Birdie alive.";
    mission.hint = "Use the team you know. Reed and Birdie shoot from a distance. Vanya can repeat Blade Dance. Donella heals allies. Double-click a card, or focus it and press I, to inspect it.";
    mission.briefing = {
        panel("Narrator",
            "Reed sees Victor's wagons turn toward the World Tree. At the gate behind him, Company guards block families trying to leave. He can chase the wagons or turn back to help.",
            "cards/reedBaelstone.png"),
        panel("Reed",
            "Victor can wait. These people cannot. Open that road!",
            "cards/reedBaelstone.png"),
        panel("Coach",
            "Defeat the guards to open the road. Keep Reed, Vanya, and Birdie alive. Inspect a card if you need to check its moves.",
            "cards/vanyaBluewater.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "The families pass through the gate and help the wounded reach the river. Reed gives them the group's food. Then his friends follow him toward the World Tree.",
            "cards/reedBaelstone.png")
    };
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 3, 0),
        piece("vanya", "Vanya Bluewater", 1, 2, 1),
        piece("birdie", "Birdie the Wise", 1, 4, 1),
        piece("donella", "Donella of the Marsh", 1, 5, 0),
        piece("collector", "Blackthorn Debt Collector", 2, 2, 5),
        piece("ambusher", "Goblin Ambusher", 2, 3, 6),
        piece("lumberjack", "Blackthorn Lumberjack", 2, 4, 5),
        piece("foreman", "Blackthorn Foreman", 2, 5, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.requiredSurvivorRoles = {"reed", "vanya", "birdie"};
    return mission;
}

StoryMission mirewatchVictorCapture()
{
    StoryMission mission = storyNode(
        "mw24a_victor_in_custody", "Take Victor Alive", "At the World Tree", {});
    mission.briefing = victorReckoningPanels();
    return mission;
}

StoryMission mirewatchWorldTreeReckoning()
{
    StoryMission mission = chronicleFromEntry(MirewatchEntries[13]);
    mission.lesson = "OPEN FINALE - FREE THE PRISONERS' PATH";
    mission.objective = "Defeat Grask. Keep Reed, Vanya, Joni, and Victor alive until this battle ends.";
    mission.hint = "Use your allies to clear a path to Grask. Donella can heal, and Joni heals nearby allies at the end of your turn. Leave Victor alive so the rescuers can take him prisoner.";
    mission.briefing = {
        panel("Narrator",
            "Grask, Victor's chief guard, blocks the cages beneath the World Tree. The prisoners need room to loosen the bars and reach one another. Reed's friends spread out around him.",
            "cards/Grask.png"),
        panel("Reed",
            "Push Grask back! Give them room. Leave Victor alive. He is coming with us.",
            "cards/reedBaelstone.png"),
        panel("Coach",
            "Defeat Grask. Reed, Vanya, Joni, and Victor must survive until the battle ends. Other enemies may remain.",
            "cards/joniPumpernickel.png")
    };
    mission.aftermath = {
        panel("Narrator",
            "Grask falls. The prisoners pull loose boards from the cages and reach toward their neighbors.",
            "cards/Mog.png")
    };
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 3, 0),
        piece("vanya", "Vanya Bluewater", 1, 2, 1),
        piece("joni", "Joni Pumpernickel", 1, 4, 1),
        piece("donella", "Donella of the Marsh", 1, 5, 0),
        piece("erevan", "Erevan the Shadow", 1, 1, 1),
        piece("birdie", "Birdie the Wise", 1, 6, 1),
        piece("mog", "Mog", 1, 6, 2),
        piece("victor", "Victor Greyshard", 2, 0, 7),
        piece("grask", "Grask", 2, 3, 6),
        piece("foreman", "Blackthorn Foreman", 2, 2, 6),
        piece("lumberjack", "Blackthorn Lumberjack", 2, 4, 6),
        piece("collector_north", "Blackthorn Debt Collector", 2, 1, 5),
        piece("collector_south", "Blackthorn Debt Collector", 2, 5, 5),
        piece("alchemist", "Blackthorn Alchemist", 2, 6, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatRole;
    mission.objectiveSpec.targetRole = "grask";
    mission.requiredSurvivorRoles = {"reed", "vanya", "joni", "victor"};
    return mission;
}

StoryMission seelieTwoMenOneVacancy()
{
    StoryMission mission;
    mission.id = "se28a_two_men_one_vacancy";
    mission.title =
        "Hold the Way Out";
    mission.sourceChapter = "Book Three - The last gate";
    mission.lesson =
        "REACH THE MARKED SQUARE";
    mission.objective =
        "Move Rowan Leafbound to G4. Keep him alive so the workers can keep leaving the lifts. You can leave other enemies standing.";
    mission.hint =
        "Keep Rowan near allies he can protect with Bodyguard. The Knight can protect allies too. The Sister heals nearby friends and enemies. The Messenger can fly past pieces. Double-click a unit, or highlight it and press I, to read its card.";
    mission.briefing = {
        panel("Reed",
            "The lifts are still coming. Keep this landing clear. The wounded need room to pass.",
            "cards/reedBaelstone.png"),
        panel("Coach",
            "Move Rowan Leafbound onto glowing G4. Keep him alive. You do not need to defeat every enemy.",
            "cards/rowanLeafbound.png")};
    mission.aftermath = {
        panel("Narrator",
            "The defenders keep the landing open. Ruvan calls for the last lift from below the raised stage. Nettle runs toward him as Lash turns toward the shelter.",
            "cards/feyMessenger.png")};
    mission.pieces = {
        piece("rowan", "Rowan Leafbound", 1, 3, 1),
        piece("knight", "Starbloom Knight", 1, 2, 1),
        piece("sister", "Heartwood Sister", 1, 4, 1),
        piece("messenger", "Fey Messenger", 1, 5, 1),
        piece("harvester", "Dusk Harvester", 2, 3, 4),
        piece("warden", "Warden", 2, 2, 5),
        piece("bristle", "Bristlejack", 2, 4, 5),
        piece("fairy", "Gloom Fairy", 2, 5, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::ReachSquare;
    mission.objectiveSpec.targetRole = "rowan";
    mission.objectiveSpec.targetRow = 3;
    mission.objectiveSpec.targetColumn = 6;
    mission.requiredSurvivorRoles = {"rowan"};
    return mission;
}

StoryMission seelieCathedralDefense()
{
    StoryMission mission;
    mission.id = "se01_cathedral_last_lights";
    mission.title =
        "Defend the Families";
    mission.sourceChapter = "Book Two, Chapter 17 - The Cathedral of Last Lights";
    mission.lesson = "FLY, JUMP, AND ATTACK";
    mission.objective =
        "Follow the glowing ACT unit and TARGET. Defeat all four Gloom Fairies and keep your four allies alive.";
    mission.hint =
        "Mouse: hold the left mouse button on ACT, drag to TARGET, then release. Keyboard: Tab until a board square is highlighted. Use arrows to ACT, Enter, then arrows to TARGET and Enter. Use End Turn after one unit acts.";
    mission.briefing = {
        panel("Vesper",
            "Enemy fairies are coming through the windows! Keep them away from the families hiding behind those benches.",
            "cards/princeVesper.png"),
        panel("Coach",
            "This guided fight teaches normal play: one ready piece acts each turn. Then use End Turn. Blue Health is yours; red belongs to enemies. Each 1 damage removes 1 Health. At 0, a piece leaves play.",
            ""),
        panel("Coach",
            "Mouse: hold the left mouse button on the glowing ACT unit, drag to TARGET, then release. Double-click a unit to read its card.",
            ""),
        panel("Coach",
            "Keyboard: press Tab until a board square is highlighted. Use arrows to highlight ACT, then Enter. Highlight TARGET with arrows and press Enter. To read a unit's card, highlight it and press I.",
            "")};
    mission.aftermath = {
        panel("Cathedral witness",
            "I know the prison road. Caltheriel's cage follows her light. If friends hold that light outside the bars, the cage cannot find her. Keep it outside until she leaves.",
            ""),
        panel("Narrator",
            "Reed, the team's guide, opens the road gates his family kept locked for tolls. He waves the waiting families through. Nettle helps him lift a fallen gate.",
            "cards/reedBaelstone.png")};
    mission.pieces = {
        piece("messenger", "Fey Messenger", 1, 2, 1),
        piece("pavo", "Pavo Quickstep", 1, 2, 2),
        piece("duchess", "Duchess Dewbell", 1, 5, 1),
        piece("lark", "Quinberry Lark", 1, 6, 0),
        piece("fairy_dart", "Gloom Fairy", 2, 2, 4),
        piece("fairy_jump", "Gloom Fairy", 2, 3, 4),
        piece("fairy_decree", "Gloom Fairy", 2, 5, 3),
        piece("fairy_swoop", "Gloom Fairy", 2, 3, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"messenger", "pavo", "duchess", "lark"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Move, 1, "messenger", 2, 3,
            "Courier Flight", "FLY TO D3",
            "Drag the Messenger from B3 to D3. Keyboard: press Tab until a square is highlighted. Use arrows to B3 and press Enter, then arrows to D3 and Enter. Courier Flight crosses Pavo.",
            "Move the Messenger from B3 to D3. Keyboard: Tab to highlight the board, arrows to B3, Enter, then arrows to D3 and Enter."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END YOUR TURN", "One unit has acted. Use End Turn to let the enemy take its turn.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FAIRY WAITS", "The Fairy stays beside the Messenger.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "messenger", 2, 4,
            "Star Dart", "FIRE STAR DART",
            "Use Star Dart on the Fairy at E3. The Messenger attacks from its own square.",
            "Attack the glowing Fairy with Star Dart.", "fairy_dart"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "LET THE ENEMY ACT", "The Messenger has fired. Use End Turn.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "PAVO HAS A TARGET", "The next Fairy is within Pavo's jump.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "pavo", 3, 4,
            "Quickstep", "JUMP TO E4",
            "Use Quickstep from C3 to E4. Jump two squares right and one down. Pavo attacks the Fairy where he lands.",
            "Use Quickstep on the glowing Fairy at E4.", "fairy_jump"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DEWBELL IS NEXT", "Use End Turn. Dewbell will guard the lower aisle next.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LOWER AISLE", "A Fairy is two squares from Dewbell.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "duchess", 5, 3,
            "Dewbell's Decree", "USE DEWBELL'S DECREE",
            "Attack the Fairy at D6 with Dewbell's Decree. Dewbell moves toward it. A surviving target keeps its square and gains Disable 2: it cannot act on its side's next two turns.",
            "Use Dewbell's Decree on the glowing Fairy at D6.", "fairy_decree"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE FAIRY LEFT", "Use End Turn. Lark has a clear diagonal to the last Fairy.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "LARK SEES AN OPENING", "The last Fairy stands clear of the others.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "lark", 3, 3,
            "Swoop", "SWOOP TO D4",
            "Use Lark's Swoop from A7 to D4. She attacks along a clear diagonal.",
            "Use Swoop on the glowing Fairy at D4.", "fairy_swoop")};
    mission.masteryCards = {"Fey Messenger"};
    mission.masteryRules = {
        "one-piece-action-per-turn", "pass-through", "ranged-attack",
        "line-of-sight", "knight-jump", "attacking-movement", "diagonal-long-movement",
        "health-and-destruction"};
    return mission;
}

StoryMission seelieBrokenBridge()
{
    StoryMission mission;
    mission.id = "se02_broken_bridge";
    mission.title = "Across the Broken Bridge";
    mission.sourceChapter = "Book Two, Chapter 18 - Road of Small Lights";
    mission.lesson = "HEAL AND PROTECT YOUR TEAM";
    mission.objective =
        "Clear the path. Keep the Messenger, Sister, Knight, and Unicorn alive.";
    mission.hint =
        "Move the Sister beside the wounded Unicorn. She heals adjacent pieces, including enemies, at the end of your turn. Adjacent means one square away, including diagonals. Keep the Knight beside the Messenger so Bodyguard can take the hit.";
    mission.briefing = {
        panel("Ruvan, a courier",
            "The bridge has fallen. I can lead the families along the bank. Help us clear the path!",
            "cards/feyMessenger.png"),
        panel("Heartwood Sister",
            "Bring the wounded close. Knight, stay with the courier. I will help the Unicorn.",
            "cards/heartwoodSister.png"),
        panel("Coach",
            "At the end of your turn, Healing Aura restores 1 Health to each piece adjacent to the Sister, up to its maximum. It heals enemies too. Adjacent means one square away, including diagonals.",
            "cards/heartwoodSister.png"),
        panel("Coach",
            "Bodyguard takes damage aimed at an adjacent ally that lacks Bodyguard. It also takes Disable caused by that hit. Effects that deal no damage stay on their target. Keep all four allies alive.",
            "cards/starbloomKnight.png")};
    mission.aftermath = {
        panel("Ruvan",
            "The bank is clear. I will take the first group. Nettle, help the people who cannot walk.",
            "cards/feyMessenger.png")};
    mission.pieces = {
        piece("sister", "Heartwood Sister", 1, 6, 0),
        piece("knight", "Starbloom Knight", 1, 6, 3),
        piece("messenger", "Fey Messenger", 1, 5, 2),
        piece("unicorn", "Crystal Unicorn", 1, 3, 3, 1),
        piece("bristle", "Bristlejack", 2, 5, 3),
        piece("widow", "Widowroot", 2, 3, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"sister", "knight", "messenger", "unicorn"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Move, 1, "sister", 4, 2,
            "Grovewalk", "MOVE BESIDE THE WOUNDED",
            "Use the Sister's Grovewalk from A7 to C5. This diagonal move puts her beside the wounded Unicorn.",
            "Move the Sister to C5."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HEAL AT END TURN",
            "Use End Turn. The Sister heals every nearby piece by 1 Health, including enemies. Health cannot rise above the card's maximum.",
            "Use End Turn to heal nearby pieces."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "bristle", 5, 2,
            "Bristle Charge", "THE MESSENGER IS HIT",
            "The Bristlejack attacks the Messenger for 2 damage.",
            "This step could not run.", "messenger"),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE KNIGHT TAKES THE HIT",
            "Bodyguard sends the 2 damage and its Disable to the nearby Knight. The Knight loses Health and cannot act next turn.",
            "This step could not run."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE KNIGHT MUST REST",
            "The hit left the Knight Disabled. It cannot act this turn. Use End Turn.",
            "Use End Turn while the Knight recovers."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "READY NEXT TURN",
            "The enemies wait. The Knight can act on your next turn.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "knight", 5, 3,
            "Starbloom Charge", "CHARGE THE BRISTLEJACK",
            "Use Starbloom Charge one square up to D6. Defeat the Bristlejack.",
            "Use Starbloom Charge on the glowing Bristlejack.", "bristle"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE UNICORN IS NEXT", "Use End Turn. The Unicorn can attack down the long path next.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE WIDOWROOT WAITS", "The Widowroot stays at the end of the path.", "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "unicorn", 3, 6,
            "Prismatic Charge", "CHARGE THE WIDOWROOT",
            "Use Prismatic Charge on the Widowroot at G4. It has 4 Health and takes 2 damage. It survives, so the Unicorn stops before its square.",
            "Use Prismatic Charge on the glowing Widowroot.", "widow"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "A SURVIVOR HOLDS ITS GROUND", "The Widowroot still holds its square. Use End Turn.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE WIDOWROOT CANNOT ACT",
            "The hit Disabled the Widowroot for this turn. The enemy passes. The Unicorn is ready to attack again.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "unicorn", 3, 6,
            "Prismatic Charge", "FINISH THE CHARGE",
            "Use Prismatic Charge on the same Widowroot. It loses its last 2 Health. The Unicorn moves onto the cleared square.",
            "Attack the glowing Widowroot again.", "widow")};
    mission.masteryCards = {"Heartwood Sister", "Starbloom Knight", "Crystal Unicorn"};
    mission.masteryRules = {
        "diagonal-movement", "healing-aura", "maximum-health", "bodyguard",
        "damage-redirection", "surviving-target-stops-movement", "defeated-target-occupation"};
    return mission;
}

StoryMission seelieRoadOfSmallLights(bool fullPractice = false)
{
    StoryMission mission;
    mission.id = "se03_road_small_lights";
    mission.title = "Get Them to Shelter";
    mission.sourceChapter = "Book Two, Chapter 18 - Road of Small Lights";
    mission.lesson = "NETTLE RETURNS ON FOOT";
    mission.objective =
        "Follow Nettle's glowing actions. Keep Nettle, the Griffin, and Vesper alive while you clear a way to shelter.";
    mission.hint =
        "Rebirth replaces the defeated mounted card with Nettle on foot. She can act on her next turn. Zippy survives and rests out of the fight.";
    mission.briefing = {
        panel("Nettle",
            "Zippy is my flying snail. He carries me because my wings are gone. Zippy, take me over the broken road. Ruvan needs our help.",
            "cards/nettleStarbright_mounted.png"),
        panel("Coach",
            "Follow the glowing unit and target. Nettle will be knocked off Zippy. Her Rebirth replaces the defeated mounted card with Nettle Starbright Unmounted on the same square.",
            "cards/thornGriffin.png")};
    mission.aftermath = {
        panel("Narrator",
            "Ruvan brings the last family into shelter. A worker with one arm joins them. He points to a little lamp that has followed the group all the way.",
            "cards/feyMessenger.png"),
        panel("Tavin, a worker",
            "That is Lash's spy lamp. I worked on his engine until it took my arm. Do not let him follow us again.",
            "")};
    mission.pieces = {
        piece("nettle", "Nettle Starbright", 1, 5, 1),
        piece("griffin", "Thorn Griffin", 1, 5, 2),
        piece("vesper", "Prince Vesper", 1, 2, 1),
        piece("flight_screen", "Fey Messenger", 1, 2, 2),
        piece("fairy_sling", "Gloom Fairy", 2, 5, 4),
        piece("bristle_rebirth", "Bristlejack", 2, 6, 3),
        piece("widow_griffin", "Widowroot", 2, 5, 6),
        piece("fairy_spark", "Gloom Fairy", 2, 4, 4)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"nettle", "griffin", "vesper"};
    mission.script = {
        scriptedPieceAction(StoryActionKind::Move, 1, "nettle", 5, 3,
            "Fly", "FLY PAST THE GRIFFIN",
            "Use Nettle's Fly from B6 to D6. She can cross the Griffin's square and land on empty D6.",
            "Move mounted Nettle to D6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE SLINGSHOT", "Use End Turn. Nettle is beside the Fairy.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE BRISTLEJACK WAITS", "The Bristlejack stays close, ready to strike.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "nettle", 5, 4,
            "Slingshot", "FIRE THE SLINGSHOT",
            "Use Nettle's Slingshot on the Fairy at E6. She attacks without moving.",
            "Use Slingshot on the glowing Fairy.", "fairy_sling"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE BRISTLEJACK CAN REACH HER", "Use End Turn. The Bristlejack is now close enough to attack Nettle.",
            "Use End Turn."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "bristle_rebirth", 5, 3,
            "Bristle Charge", "NETTLE RETURNS ON FOOT",
            "The Bristlejack knocks Nettle off Zippy. Zippy flies clear and survives. The hit deals 2 damage and defeats the mounted card. Rebirth places Nettle Starbright Unmounted on the same square. She cannot act again this turn.",
            "This step could not run.", "nettle"),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "NETTLE GETS READY", "The enemy turn ends. Nettle is ready to act on foot.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "nettle", 6, 3,
            "Punch", "PUNCH THE BRISTLEJACK",
            "Nettle now has 1 Health. Use Punch on the Bristlejack beside her at D7.",
            "Use Nettle's Punch on the glowing Bristlejack.", "bristle_rebirth"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SEND IN THE GRIFFIN", "Use End Turn. The Griffin can cross the row next.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FAR PATH IS BLOCKED", "The Widowroot holds the far end of the row.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "griffin", 5, 6,
            "Griffin Flight", "FLY AND ATTACK",
            "Use Griffin Flight on the Widowroot at G6. The Griffin can cross pieces. The Widowroot survives this hit, so the Griffin cannot take its square.",
            "Use Griffin Flight on the glowing Widowroot.", "widow_griffin"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "VESPER IS NEXT", "Use End Turn to prepare Vesper's move.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST FAIRY WAITS", "The Fairy is two squares below Vesper's landing spot.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Move, 1, "vesper", 2, 4,
            "Royal Flight", "FLY TO E3",
            "Use Vesper's Royal Flight from B3 to E3. Cross the Messenger's square and land on empty E3.",
            "Move Vesper to E3."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PREPARE VESPER'S SPARK", "Use End Turn. Vesper will be ready to attack on your next turn.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FAIRY IS IN RANGE", "The Fairy stays within Vesper Spark's range.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vesper", 4, 4,
            "Vesper Spark", "FIRE VESPER SPARK",
            "Use Vesper Spark on the Fairy at E5, two squares down the column. Vesper attacks without moving.",
            "Use Vesper Spark on the glowing Fairy.", "fairy_spark")};
    mission.masteryCards = {"Nettle Starbright", "Thorn Griffin", "Prince Vesper"};
    mission.masteryRules = {
        "mounted", "rebirth", "replacement-acts-next-turn", "ranged-line-of-sight",
        "flying-pass-through", "surviving-target-stops-movement", "hero-actions"};
    if (fullPractice)
    {
        mission.id = "se03a_flight_workshop";
        mission.title = "Practice Flight and Rebirth";
        mission.sourceChapter = "Optional card practice after the story";
        mission.lesson = "FLIGHT, REBIRTH, AND HERO ACTIONS";
        mission.objective = "Practice flying attacks with Nettle, the Griffin, and Vesper. Keep all three alive.";
        mission.hint = "Follow each glowing piece and target. Flying can cross occupied squares. Read each card to check its range.";
        mission.optionalRehearsal = true;
        mission.briefing = {panel("Coach", "Try the full flight lesson with Nettle, the Griffin, and Vesper. You can skip this practice and return from Missions.", "cards/thornGriffin.png")};
        mission.aftermath = {panel("Coach", "Nettle fought on foot after losing her mount. The Griffin attacked as it flew. Vesper flew, then fired from a distance.", "cards/thornGriffin.png")};
        for (auto& step : mission.script)
        {
            step.panelsBefore.clear();
            if (step.kind == StoryActionKind::EndTurn && step.owner == 2)
            {
                step.heading = "YOUR NEXT TURN";
                step.instruction = "The opposing side ends its turn.";
            }
        }
    }
    else
    {
        mission.script.resize(8);
        mission.masteryCards = {"Nettle Starbright"};
        mission.masteryRules = {
            "mounted", "rebirth", "replacement-acts-next-turn",
            "ranged-line-of-sight", "flying-pass-through"};
    }
    return mission;
}

StoryMission seelieReadingRoomRehearsal()
{
    StoryMission mission;
    mission.id = "se04_reading_room";
    mission.title = "Practice with Mosswake";
    mission.sourceChapter = "Optional rules practice";
    mission.lesson = "DEPLOY, DRAW, AND CHOOSE A CARD";
    mission.objective =
        "Practice deploying Mosswake, choosing a draw with Foresight, discarding, and using Rootwalk.";
    mission.hint =
        "Follow the glowing cards and squares. Choose Start Practice to try this board, or Skip Practice to leave this lesson.";
    mission.briefing = {
        panel("Coach",
            "Optional practice: play a card, draw with Foresight, and discard. Choose Start Practice to try it, or Skip Practice to leave this lesson.",
            "cards/archivistMosswake.png"),
        panel("Coach",
            "Your banner shows your Resources. Each square you control earns 1 Resource at the start of your turn. This practice starts with 100. Spend Resources to play cards or draw.",
            ""),
        panel("Coach",
            "Deploy Units on empty ground you control. Match each of the Unit's required Traits to a Trait on one of your living Heroes. Different Heroes can supply different Traits. Pay the Unit's Resource cost.",
            ""),
        panel("Coach",
            "Mouse: drag the card to B4 with the left mouse button held. Keyboard: press Tab until your hand is highlighted. Choose the card with arrows and press Enter. Choose B4 with arrows, then press Enter again.",
            "cards/archivistMosswake.png")};
    mission.aftermath = {
        panel("Coach",
            "Foresight let you choose between two cards. The other card went to the bottom of the deck. Discarding also sent a card there. Neither used your unit action.",
            "")};
    mission.pieces = {
        piece("sylvara", "Sylvara", 1, 4, 0),
        piece("root_target", "Gloom Fairy", 2, 3, 3)};
    mission.playerHand = {"Archivist Mosswake"};
    mission.playerDrawPile = {"Starbloom Knight", "Fey Messenger"};
    mission.playerResources = 100;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"sylvara"};
    mission.optionalRehearsal = true;
    mission.script = {
        scriptedCard(StoryActionKind::PlayCard, 1, "Archivist Mosswake", 3, 1,
            "PUT MOSSWAKE INTO PLAY",
            "Play Archivist Mosswake on B4 for 35 Resources.",
            "Play Archivist Mosswake from your hand onto B4.", "mosswake"),
        scripted(StoryActionKind::DrawCard, 1, {}, -1, -1,
            "DRAW WITH FORESIGHT",
            "Click DRAW, or highlight it and press D or Enter. Drawing costs 50 Resources. Foresight shows two cards. Keep one; the other goes to the deck's bottom.",
            "Use DRAW to see two cards."),
        scriptedCard(StoryActionKind::ChooseForesight, 1, "Fey Messenger", -1, -1,
            "CHOOSE FEY MESSENGER",
            "Choose Fey Messenger for this practice step. Starbloom Knight goes to the bottom of the deck.",
            "Choose Fey Messenger."),
        scriptedCard(StoryActionKind::DiscardCard, 1, "Fey Messenger", -1, -1,
            "TRY DISCARDING",
            "Put Fey Messenger in DISCARD, or highlight the card and press X. It goes to the deck's bottom. Discarding leaves your unit action available.",
            "Put Fey Messenger in DISCARD, or highlight the card and press X."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "NEW UNITS WAIT",
            "Use End Turn. Mosswake cannot act on the turn he enters play.",
            "Use End Turn before moving Mosswake."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MOSSWAKE IS READY",
            "The enemy passes. Mosswake can now act.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "mosswake", 3, 3,
            "Rootwalk", "ATTACK WITH ROOTWALK",
            "Use Rootwalk from B4 to D4, two squares along the row. Mosswake attacks the Fairy on D4.",
            "Use Rootwalk on the glowing Fairy at D4.", "root_target")};
    mission.masteryCards = {"Archivist Mosswake"};
    mission.masteryRules = {
        "deployment-cost", "trait-gate", "foresight", "draw-cost",
        "discard-to-bottom", "arrival-exhaustion", "orthogonal-attacking-movement"};
    return mission;
}

StoryMission seelieNyxaraInfestReconstruction()
{
    StoryMission mission = storyNode(
        "se07a_queens_price_scene",
        "Practice with Nyxara",
        "Optional rules practice",
        {panel("Coach",
            "Optional practice: mark a target with Infest, then defeat it. Choose Start Practice to try it, or Skip Practice to leave this lesson.",
            "cards/queenNyxara.png")});
    mission.lesson =
        "OPTIONAL PRACTICE: INFEST";
    mission.objective =
        "Use Nyxaran Infest on the Warden. Defeat it with the Fairy to create a Bristlejack for your side.";
    mission.hint =
        "Infest deals 1 damage at a diagonal range of one or two squares. A marked non-Hero becomes a Bristlejack only after it dies.";
    mission.aftermath = {
        panel("Coach",
            "The Warden died with an Infest mark. A Bristlejack appeared on its square for Nyxara's side. It has 1 Health and must wait until its next turn to act.",
            "cards/queenNyxara.png")};
    mission.pieces = {
        piece("nyxara", "Queen Nyxara", 1, 4, 1),
        piece("fairy", "Gloom Fairy", 1, 2, 2),
        piece("infest_target", "Warden", 2, 2, 3, 2)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.firstPlayer = 1;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"nyxara", "fairy"};
    mission.optionalRehearsal = true;
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "nyxara", 2, 3,
            "Nyxaran Infest", "MARK THE WARDEN",
            "Use Nyxaran Infest from B5 to the Warden at D3. The diagonal attack deals 1 damage. The Warden survives with an Infest mark.",
            "Use Nyxaran Infest on the glowing Warden at D3.",
            "infest_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE MARKED ENEMY IS STILL ALIVE",
            "Use End Turn. Infest will create a Bristlejack only when the marked Warden dies.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE MARK STAYS",
            "The enemy passes. The Warden keeps its Infest mark.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "fairy", 2, 3,
            "Shoot Spark", "FINISH THE MARKED ENEMY",
            "Use the Fairy's Shoot Spark on D3. The Warden loses its last Health. Infest replaces it with a friendly Bristlejack on the same square.",
            "Use Shoot Spark on the glowing Warden at D3.",
            "infest_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE NEW UNIT WAITS",
            "The Bristlejack arrives with 1 Health and cannot act this turn. Use End Turn.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE ENEMY PASSES", "The enemy ends its turn.", "This step could not run."),
        scriptedPieceAction(StoryActionKind::Move, 1, "nyxara", 4, 3,
            "Royal Advance", "MOVE THE QUEEN",
            "Use Royal Advance from B5 to D5, two squares along the row.",
            "Move Nyxara to empty D5.")};
    mission.masteryCards = {"Queen Nyxara"};
    mission.masteryRules = {
        "ranged-diagonal-attack", "infest-marks-a-surviving-nonhero",
        "infest-persists-until-target-death", "infest-spawns-allied-unit-on-death",
        "infestation-replacement-arrives-acted", "orthogonal-movement"};
    return mission;
}

StoryMission seelieQueensPriceRehearsal()
{
    StoryMission mission;
    mission.id = "se07_queens_price";
    mission.title =
        "Practice Control and Hiding";
    mission.sourceChapter = "Optional rules practice";
    mission.lesson = "CONTROL AN ENEMY AND MOVE WHILE HIDDEN";
    mission.objective =
        "Use Pavo's Control, hide Lark, and move her past the blockers. Watch the affected unit return to its side.";
    mission.hint =
        "Control changes a non-Hero's side now and through your next two turns. It returns when the enemy's following turn starts. Hidden Lark can cross pieces but must stop on empty ground.";
    mission.briefing = {
        panel("Coach",
            "Optional practice: Control and Hidden. Choose Start Practice to try these rules, or Skip Practice to leave this lesson.",
            "cards/pavoQuickstep.png"),
        panel("Coach",
            "Enchanting Pipes deals no damage. Control changes a non-Hero's side through your next two turns. It cannot affect a Hero. This is the game rule used when Pavo briefly breaks the Emperor's hold in the story.",
            "cards/pavoQuickstep.png"),
        panel("Coach",
            "While Hidden, Lark adds no territory influence. Sneak Around can pass through pieces, but she must stop on an empty square.",
            "cards/quinberryLark.png")};
    mission.aftermath = {
        panel("Coach",
            "Control ended, so the Bristlejack returned to its original side. Hidden Lark crossed the blockers and stopped on empty ground.",
            "")};
    mission.pieces = {
        piece("pavo", "Pavo Quickstep", 1, 2, 0),
        piece("lark", "Quinberry Lark", 1, 5, 0),
        piece("pipes_target", "Bristlejack", 2, 2, 2),
        piece("lark_screen_one", "Gloom Fairy", 2, 5, 2),
        piece("lark_screen_two", "Warden", 2, 5, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"pavo", "lark"};
    mission.optionalRehearsal = true;
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "pavo", 2, 2,
            "Enchanting Pipes", "USE CONTROL",
            "Use Enchanting Pipes on the Bristlejack at C3. It takes no damage. Control changes its side now and through your next two turns. It returns when the enemy's following turn starts.",
            "Use Enchanting Pipes on the glowing Bristlejack at C3.", "pipes_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "CONTROL HAS A LIMIT",
            "Use End Turn. The Bristlejack is on your side while Control lasts.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "LARK'S PATH IS BLOCKED",
            "The enemy passes. Two pieces still block Lark's row.",
            "This step could not run."),
        scriptedAbility(1, "lark", "dematerialize", "Dematerialize",
            "HIDE LARK",
            "Select Lark and use Dematerialize, or press A for her ability. While Hidden, she adds no territory influence. She can use actions marked for Hidden units.",
            "Select Lark, then choose Dematerialize or press A."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "WAIT BEFORE SNEAKING",
            "Use End Turn. Lark can use Sneak Around on a later turn while she is Hidden.",
            "Use End Turn before moving Lark."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE BLOCKERS WAIT",
            "The two pieces stay in Lark's path.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Move, 1, "lark", 5, 5,
            "Sneak Around", "SLIP PAST THE BLOCKERS",
            "Use Sneak Around from A6 to F6. Hidden Lark can pass through both pieces, but she must stop on an empty square.",
            "Move Hidden Lark to F6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE BRISTLEJACK RETURNS",
            "Use End Turn. Control's last turn is over. The Bristlejack returns to its original side when that side's turn starts.",
            "Use End Turn to watch Control end.")};
    mission.masteryCards = {"Pavo Quickstep", "Quinberry Lark"};
    mission.masteryRules = {
        "zero-damage-control", "control-two-turns",
        "control-restores-at-original-owner-turn-start", "dematerialize",
        "hidden-adds-no-control", "state-specific-actions", "pass-through"};
    return mission;
}

StoryMission seelieNoCrownRehearsal()
{
    StoryMission mission;
    mission.id = "se10_no_crown_holds_light";
    mission.title = "Practice with Caltheriel";
    mission.sourceChapter = "Optional rules practice";
    mission.lesson = "HEAL, FLY, AND CHARM";
    mission.objective =
        "Heal the nearby Knight, fly to C2, and use Charm on the Bristlejack.";
    mission.hint =
        "This optional board teaches Caltheriel's card. Charm deals no damage and cannot affect a Hero.";
    mission.briefing = {
        panel("Coach",
            "Optional practice: Caltheriel's Healing Aura, Fey Flight, and Charm. Choose Start Practice to try her card, or Skip Practice to leave this lesson.",
            "characters/caltheriel.png"),
        panel("Coach",
            "Healing Aura restores 1 Health to every adjacent piece at the end of your turn, up to its maximum. It heals friends and enemies. Fey Flight moves her diagonally.",
            ""),
        panel("Coach",
            "Charm deals no damage. It Controls a non-Hero through your next two turns. The unit then returns to its original side.",
            "")};
    mission.aftermath = {
        panel("Coach",
            "You healed at End Turn, flew along a diagonal, and used Charm without moving.",
            "")};
    mission.pieces = {
        piece("caltheriel", "Caltheriel", 1, 3, 0),
        piece("wounded_knight", "Starbloom Knight", 1, 3, 1, 3),
        piece("charm_target", "Bristlejack", 2, 1, 4)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"caltheriel", "wounded_knight"};
    mission.optionalRehearsal = true;
    mission.script = {
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HEAL THE KNIGHT",
            "Use End Turn. Caltheriel's Healing Aura restores 1 Health to the nearby Knight, up to its card's maximum.",
            "Use End Turn to heal the Knight."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "CALTHERIEL IS READY",
            "The enemy passes. Caltheriel can act again.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Move, 1, "caltheriel", 1, 2,
            "Fey Flight", "FLY TO C2",
            "Use Fey Flight diagonally from A4 to C2. This uses your one unit action for the turn.",
            "Move Caltheriel to C2."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "WAIT BEFORE CHARM",
            "Use End Turn. Caltheriel can use Charm on your next turn.",
            "Use End Turn after Fey Flight."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE TARGET IS IN RANGE",
            "The Bristlejack is two squares along the row from Caltheriel.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "caltheriel", 1, 4,
            "Charm", "CHARM THE BRISTLEJACK",
            "Use Charm on the Bristlejack at E2. It deals no damage. Control changes its side now and through your next two turns. The Bristlejack returns when the enemy's following turn starts.",
            "Use Charm on the glowing Bristlejack at E2.", "charm_target")};
    mission.masteryCards = {"Caltheriel"};
    mission.masteryRules = {
        "healing-aura", "maximum-health", "diagonal-movement",
        "zero-damage-control", "control-two-turns"};
    return mission;
}

StoryMission seeliePearlAndRootRehearsal()
{
    StoryMission mission;
    mission.id = "se13_pearl_and_root";
    mission.title = "Practice Holding an Enemy Still";
    mission.sourceChapter = "Optional rules practice";
    mission.lesson = "DAMAGE AND DISABLE 2";
    mission.objective =
        "Use Sylvara's Sovereign Thorns. Watch the Warden miss two turns, then attack with Dewbell's Decree.";
    mission.hint =
        "Sovereign Thorns must deal damage to cause Disable. Disable 2 keeps a unit from acting on its side's next two turns.";
    mission.briefing = {
        panel("Coach",
            "Optional practice: damage and Disable 2. Choose Start Practice to try it, or Skip Practice to leave this lesson.",
            "cards/Sylvara.png"),
        panel("Coach",
            "A unit with Disable 2 cannot act on its side's next two turns. Hit the Warden, then watch both turns pass.",
            "")};
    mission.aftermath = {
        panel("Coach",
            "The Warden missed two turns before Disable wore off. Keep that timing in mind when you choose your next target.",
            "")};
    mission.pieces = {
        piece("sylvara", "Sylvara", 1, 4, 0),
        piece("duchess", "Duchess Dewbell", 1, 2, 1),
        piece("thorns_target", "Warden", 2, 6, 2),
        piece("decree_target", "Warden", 2, 2, 3, 2)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    mission.requiredSurvivorRoles = {"sylvara", "duchess"};
    mission.optionalRehearsal = true;
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "sylvara", 6, 2,
            "Sovereign Thorns", "STRIKE WITH THORNS",
            "Use Sovereign Thorns on C7, two diagonal squares from Sylvara. It deals 1 damage. The Warden survives and gains Disable 2.",
            "Use Sovereign Thorns on the glowing Warden at C7.", "thorns_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "WATCH THE FIRST MISSED TURN",
            "Use End Turn. Disable keeps the Warden from acting on its side's next turn.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "ONE TURN MISSED",
            "The Warden cannot act. It must miss one more turn before Disable ends.",
            "This step could not run."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "WATCH THE SECOND MISSED TURN",
            "Use End Turn again. The Warden must miss a second turn.",
            "Use End Turn again."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "TWO TURNS MISSED",
            "The Warden misses its second turn. Disable now wears off.",
            "This step could not run."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "duchess", 2, 3,
            "Dewbell's Decree", "TRY DEWBELL'S DECREE",
            "Use Dewbell's Decree on the second Warden, at D3, two squares along the row. It takes 1 damage, survives, and gains Disable 2.",
            "Use Dewbell's Decree on the glowing Warden at D3.",
            "decree_target")};
    mission.masteryCards = {"Sylvara", "Duchess Dewbell"};
    mission.masteryRules = {
        "positive-damage-disable", "disable-two-turns", "status-duration",
        "range-two", "attacking-movement"};
    return mission;
}

StoryMission seelieRulesUnderPressure()
{
    StoryMission mission;
    mission.id = "se14_rules_under_pressure";
    mission.title =
        "Make Room at the Harbor";
    mission.sourceChapter = "Harbor rescue - territory control";
    mission.lesson = "CONTROL 22 SQUARES";
    mission.objective =
        "Control 22 board squares and keep Pavo alive. You do not need to defeat both enemies.";
    mission.hint =
        "Spread your team to turn more ground your color. Taking ground is different from using Pavo's Control on an enemy.";
    mission.briefing = {
        panel("Pavo writes",
            "The barrier stopped the cannon fire. These last attackers are still among the boats. Give the families room to land.",
            "cards/pavoQuickstep.png"),
        panel("Coach",
            "Each visible unit claims its square and affects the eight squares around it. On empty ground, the side with more nearby influence takes control. A tie leaves ownership as it is.",
            ""),
        panel("Coach",
            "You start with 15 squares. Reach 22 and keep Pavo alive. Choose one unit action, then use End Turn.",
            ""),
        panel("Coach",
            "The Sister heals adjacent friends and enemies at the end of your turn. The Unicorn can charge down a clear line. Double-click a unit, or highlight it and press I, to read its actions.",
            "cards/heartwoodSister.png")};
    mission.aftermath = {
        panel("Narrator",
            "The families reach the docks. Behind them, another cannon shot bursts against Lash's bright wall. A child claps. Nettle cannot help smiling too.",
            "")};
    mission.pieces = {
        piece("pavo", "Pavo Quickstep", 1, 2, 1),
        piece("sister", "Heartwood Sister", 1, 3, 0),
        piece("unicorn", "Crystal Unicorn", 1, 4, 1),
        piece("bristle", "Bristlejack", 2, 2, 4),
        piece("fairy", "Gloom Fairy", 2, 4, 4)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::ControlSquares;
    mission.objectiveSpec.amount = 22;
    mission.requiredSurvivorRoles = {"pavo"};
    return mission;
}

StoryMission seelieWarmChannel()
{
    StoryMission mission;
    mission.id = "se17_warm_channel";
    mission.title = "Blind the Invasion Fleet";
    mission.sourceChapter = "Book Three, Chapter 9 - The Warm Channel";
    mission.lesson = "CHOOSE YOUR OWN ATTACKS";
    mission.objective =
        "Defeat every enemy in the signal group. Their lights are guiding the invasion ships toward the city.";
    mission.hint =
        "There are no guided highlights. The Griffin and mounted Nettle can fly past pieces. Lark needs a clear diagonal. Vesper attacks at range. Read a card by double-clicking it, or highlight it and press I.";
    mission.briefing = {
        panel("Vesper",
            "Lash's wall stops the ships ahead of us. But those Fairies are showing them a gap along the shore. Put out their signals!",
            "cards/princeVesper.png"),
        panel("Nettle",
            "Zippy has rested. We will take the lower path. Vesper, keep those ships away from the boats we just saved.",
            "cards/nettleStarbright_mounted.png"),
        panel("Coach",
            "Defeat the whole enemy group. Choose your own actions. Griffin Flight crosses pieces. Lark's Swoop needs a clear diagonal. Vesper attacks from range. Nettle's Slingshot hits a nearby target. Use End Turn after one unit acts.",
            "cards/thornGriffin.png")};
    mission.aftermath = {
        panel("Narrator",
            "The signals go dark. The ships turn away from the hidden gap. Then the barrier flares brighter. On the dock, a woman falls to her knees. Light is pulling out of her hands.",
            "")};
    mission.pieces = {
        piece("griffin", "Thorn Griffin", 1, 2, 1),
        piece("vesper", "Prince Vesper", 1, 3, 1),
        piece("lark", "Quinberry Lark", 1, 4, 1),
        piece("nettle", "Nettle Starbright", 1, 5, 1),
        piece("signal_one", "Gloom Fairy", 2, 2, 5),
        piece("bristle_screen", "Bristlejack", 2, 3, 4),
        piece("widow_anchor", "Widowroot", 2, 4, 6),
        piece("signal_two", "Gloom Fairy", 2, 5, 5)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    return mission;
}

StoryMission seelieCryptDefense()
{
    StoryMission mission;
    mission.id = "se19_crypt_under_order";
    mission.title =
        "Protect Caltheriel's Way Out";
    mission.sourceChapter = "Book Two - The cage escape";
    mission.lesson = "PROTECT ALLIES AND CHOOSE TARGETS";
    mission.objective =
        "Defeat every attacker in the prison hall so Caltheriel and the other prisoners can leave.";
    mission.hint =
        "Knights can protect adjacent allies. The Sister and Caltheriel heal adjacent friends and enemies at End Turn. Pavo's Enchanting Pipes deals no damage and Controls a non-Hero through your next two turns.";
    mission.briefing = {
        panel("Caltheriel",
            "I am through the bars. The cage still commands the guards! Pavo, weaken its hold while we get the other prisoners out.",
            "characters/caltheriel.png"),
        panel("Coach",
            "Defeat every attacker. There are no guided highlights. Choose one unit action, then use End Turn. Double-click a unit, or highlight it and press I, to read its card.",
            ""),
        panel("Coach",
            "Bodyguard takes damage aimed at an adjacent ally that lacks Bodyguard. If several Knights can help, they split the damage. Keep your healers close. Their auras heal adjacent enemies too.",
            "cards/starbloomKnight.png"),
        panel("Coach",
            "Pavo's Enchanting Pipes deals no damage. Control changes a non-Hero's side through your next two turns. Caltheriel can use Charm for the same effect.",
            "cards/pavoQuickstep.png")};
    mission.aftermath = {
        panel("Narrator",
            "The guards no longer block the hall. The prison keeper leads the other prisoners outside. Nettle and Reed carry their shares of Caltheriel's light through the final doorway.",
            "")};
    mission.pieces = {
        piece("knight_north", "Starbloom Knight", 1, 2, 1),
        piece("knight_south", "Starbloom Knight", 1, 5, 1),
        piece("sister", "Heartwood Sister", 1, 3, 0),
        piece("caltheriel", "Caltheriel", 1, 3, 2),
        piece("pavo", "Pavo Quickstep", 1, 4, 1),
        piece("widow_north", "Widowroot", 2, 2, 5),
        piece("warden", "Warden", 2, 3, 5),
        piece("bristle", "Bristlejack", 2, 4, 4),
        piece("harvester", "Dusk Harvester", 2, 5, 5),
        piece("fairy_flank", "Gloom Fairy", 2, 1, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    return mission;
}

StoryMission seelieControlledFall()
{
    StoryMission mission;
    mission.id = "se23_controlled_fall";
    mission.title =
        "Guard the Lift Landing";
    mission.sourceChapter = "Book Three - The workers escape";
    mission.lesson = "REACH A SQUARE UNDER ATTACK";
    mission.objective =
        "Move the marked Starbloom Knight to G5. Keep that Knight alive so the workers can escape.";
    mission.hint =
        "The Griffin and Unicorn can clear the path. Keep the Sister close enough to heal. You can leave enemies standing once the marked Knight reaches G5.";
    mission.briefing = {
        panel("Ruvan",
            "The workers are reaching the lifts. Guards attack whenever the doors open. Get to the landing and cover them as they come up.",
            "cards/feyMessenger.png"),
        panel("Coach",
            "Move the marked Starbloom Knight onto glowing G5. Keep that Knight alive. The enemies will fight back. Clear only as much space as you need to reach it.",
            "cards/starbloomKnight.png")};
    mission.aftermath = {
        panel("Narrator",
            "The Knight reaches the landing. Ruvan brings the first workers up, then goes back down for the next group. Nettle and Reed stay close to help.",
            "cards/starbloomKnight.png")};
    mission.pieces = {
        piece("route_knight", "Starbloom Knight", 1, 4, 1),
        piece("channel_griffin", "Thorn Griffin", 1, 5, 1),
        piece("breach_unicorn", "Crystal Unicorn", 1, 3, 1),
        piece("route_sister", "Heartwood Sister", 1, 2, 0),
        piece("warden_north", "Warden", 2, 3, 4),
        piece("warden_center", "Warden", 2, 4, 4),
        piece("warden_south", "Warden", 2, 5, 4),
        piece("widow_flank", "Widowroot", 2, 2, 5)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::ReachSquare;
    mission.objectiveSpec.targetRole = "route_knight";
    mission.objectiveSpec.targetRow = 4;
    mission.objectiveSpec.targetColumn = 6;
    mission.requiredSurvivorRoles = {"route_knight"};
    return mission;
}

StoryMission seelieEmperorAtTree()
{
    StoryMission mission;
    mission.id = "se27_emperor_at_tree";
    mission.title =
        "Cover the Next Lift";
    mission.sourceChapter = "Book Three - The theater steps";
    mission.lesson = "KEEP SYLVARA ALIVE";
    mission.objective =
        "Defeat every attacker and keep Sylvara alive. Hold the landing while the workers come up.";
    mission.hint =
        "Keep Sylvara alive. Sovereign Thorns hits one or two squares diagonally. If it deals damage, the target gains Disable 2 and cannot act on its side's next two turns. Use Bodyguard and healing to protect your line.";
    mission.briefing = {
        panel("Narrator",
            "The next lift arrives as the Emperor's army reaches the theater. Workers hurry out behind the defenders. The landing must stay clear.",
            ""),
        panel("The Emperor",
            "Lash promised me this city. Move aside, or I will take you with it.",
            ""),
        panel("Vesper",
            "Keep the workers behind us. They have come too far to stop here!",
            "cards/princeVesper.png"),
        panel("Coach",
            "Defeat every enemy on this board. Sylvara must survive. Keep guards adjacent to the allies they protect. Your healers restore Health to adjacent friends and enemies, so watch where each piece stands.",
            "cards/starbloomKnight.png")};
    mission.aftermath = {
        panel("Narrator",
            "The attackers fall back. Some drop their weapons. Caltheriel leads them to shelter. Lash raises his stage above the defenders. From there, he can aim straight into the shelter across the street.",
            "characters/caltheriel.png")};
    mission.pieces = {
        piece("sylvara", "Sylvara", 1, 3, 0),
        piece("caltheriel", "Caltheriel", 1, 4, 0),
        piece("knight_north", "Starbloom Knight", 1, 2, 1),
        piece("knight_south", "Starbloom Knight", 1, 5, 1),
        piece("griffin", "Thorn Griffin", 1, 1, 1),
        piece("unicorn", "Crystal Unicorn", 1, 6, 1),
        piece("sister", "Heartwood Sister", 1, 3, 1),
        piece("vesper", "Prince Vesper", 1, 4, 1),
        piece("warden_north", "Warden", 2, 2, 5),
        piece("warden_south", "Warden", 2, 5, 5),
        piece("harvester_north", "Dusk Harvester", 2, 3, 6),
        piece("harvester_south", "Dusk Harvester", 2, 4, 6),
        piece("eyeblight", "Eyeblight", 2, 1, 6),
        piece("bristle_north", "Bristlejack", 2, 0, 6),
        piece("bristle_south", "Bristlejack", 2, 6, 5),
        piece("gloom_flank", "Gloom Fairy", 2, 7, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.requiredSurvivorRoles = {"sylvara"};
    return mission;
}

StoryMission seelieHoldWitnessRail()
{
    StoryMission mission;
    mission.id = "se29e_hold_the_witness_rail";
    mission.title = "Protect the Soldiers Who Surrender";
    mission.sourceChapter = "Book Three - The last escape";
    mission.lesson = "FINAL BATTLE: CONTROL 28 SQUARES";
    mission.objective =
        "Control 28 board squares. Keep Reed and Rowan alive. You do not need to defeat every enemy.";
    mission.hint =
        "Spread out to claim ground, but protect Reed and Rowan. Each visible piece claims its square and influences all eight nearby squares. More influence wins empty ground. A tie leaves its owner unchanged. Hidden pieces add no influence.";
    mission.briefing = {
        panel("Narrator",
            "The last lift brings the wounded workers up. Beside the landing, the Emperor's captain attacks soldiers who drop their weapons. Reed stands between them.",
            "cards/reedBaelstone.png"),
        panel("Reed",
            "Put your weapons down and come behind us. We will get you out.",
            "cards/reedBaelstone.png"),
        panel("Coach",
            "Control 28 squares and keep Reed and Rowan alive. Spread out to make room. Keep guards and healers near the people they protect. The Sister heals adjacent enemies too.",
            ""),
        panel("Coach",
            "Pavo's Enchanting Pipes deals no damage. Control changes a non-Hero's side through your next two turns. Choose one unit action, then use End Turn. You do not need to defeat every enemy.",
            "cards/pavoQuickstep.png")};
    mission.aftermath = {
        panel("Narrator",
            "Ruvan and the last workers reach the street. The lifts are empty. Nettle and Reed return to the engine.",
            "cards/reedBaelstone.png")};
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 3, 0),
        piece("rowan", "Rowan Leafbound", 1, 4, 0),
        piece("pavo", "Pavo Quickstep", 1, 2, 1),
        piece("nettle", "Nettle Starbright", 1, 5, 1),
        piece("knight", "Starbloom Knight", 1, 3, 1),
        piece("sister", "Heartwood Sister", 1, 4, 1),
        piece("captain", "Warden", 2, 3, 6),
        piece("warden", "Warden", 2, 4, 6),
        piece("harvester_north", "Dusk Harvester", 2, 2, 5),
        piece("harvester_south", "Dusk Harvester", 2, 5, 5),
        piece("eyeblight", "Eyeblight", 2, 1, 6),
        piece("gloom_fairy", "Gloom Fairy", 2, 6, 6),
        piece("bristlejack", "Bristlejack", 2, 0, 7)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::ControlSquares;
    mission.objectiveSpec.amount = 28;
    mission.requiredSurvivorRoles = {"reed", "rowan"};
    return mission;
}

const std::vector<StoryMission> MirewatchMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(25);
    missions.push_back(mirewatchOpening());
    missions.push_back(mirewatchDeployment());
    missions.push_back(mirewatchWatcher());
    missions.push_back(sharedS01());
    missions.push_back(mirewatchOffice());
    missions.push_back(mirewatchWagonRescue());
    missions.push_back(chronicleFromEntry(MirewatchEntries[3]));
    missions.push_back(mirewatchAuction());
    missions.push_back(mirewatchVaultCost());
    missions.push_back(sharedS03());
    missions.push_back(sharedS04());
    missions.push_back(chronicleFromEntry(MirewatchEntries[7]));
    missions.push_back(mirewatchCapstone());
    missions.push_back(mirewatchTollAlliance());
    missions.push_back(chronicleFromEntry(MirewatchEntries[9]));
    missions.push_back(mirewatchWorldTreeReckoning());
    missions.push_back(chronicleFromEntry(MirewatchEntries[10]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[11]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[12]));
    missions.push_back(mirewatchVictorCapture());
    missions.push_back(chronicleFromEntry(MirewatchEntries[14]));
    missions.push_back(sharedS06());
    missions.push_back(mirewatchFirstOpenCheck());
    missions.push_back(mirewatchWagonRescue(true));
    missions.push_back(mirewatchRevealReconstruction());
    missions.push_back(mirewatchCapstone(true));
    attachGeneratedScenarioArt(StoryCampaign::Mirewatch, missions);
    return missions;
}();

const std::vector<StoryMission> BlackthornMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(21);
    missions.push_back(blackthornOpening());
    missions.push_back(blackthornCustoms());
    missions.push_back(blackthornDeployment());
    missions.push_back(blackthornTerms());
    missions.push_back(blackthornS01());
    missions.push_back(blackthornOffice());
    missions.push_back(blackthornFirstOpenCheck(BlackthornEntries[3]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[5]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[4]));
    missions.push_back(blackthornS03());
    missions.push_back(chronicleFromEntry(BlackthornEntries[6]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[7]));
    missions.push_back(blackthornPoisonedRootRoad());
    missions.push_back(blackthornMonsterRules());
    missions.push_back(chronicleFromEntry(BlackthornEntries[9]));
    missions.push_back(blackthornVictorReckoning());
    missions.push_back(chronicleFromEntry(BlackthornEntries[10]));
    missions.push_back(blackthornS06());
    missions.push_back(blackthornCapstone());
    missions.push_back(blackthornFieldJudgment());
    missions.push_back(blackthornOpenMastery());
    attachGeneratedScenarioArt(StoryCampaign::Blackthorn, missions);
    return missions;
}();

const std::vector<StoryMission> SeelieMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(32);

    missions.push_back(storyNode(
        "se00_what_mirror_remembers",
        "The Cat and the Cage",
        "Book Two - The rescue begins",
        {
            panel("Narrator",
                "Prince Vesper is an orange cat. Caltheriel, the woman he loves, is trapped in a cage of light. Every day, she grows weaker.",
                "characters/caltheriel.png"),
            panel("Vesper",
                "She asked Queen Nyxara for shelter. Now the Queen will not let her leave. I heard Caltheriel calling for help. We have to reach her.",
                "cards/princeVesper.png"),
            panel("Narrator",
                "Nettle, a fairy who has lost her wings, finds another way past the Queen's gates. She leads Vesper toward a cathedral crowded with fleeing families.",
                "cards/nettleStarbright_mounted.png")
        }));

    missions.push_back(seelieCathedralDefense());

    missions.push_back(seelieBrokenBridge());

    missions.push_back(seelieRoadOfSmallLights());

    missions.push_back(storyNode(
        "se04a_lash_reads_the_road",
        "Lash Sells the Way In",
        "Book Two - The stolen road",
        {
            panel("Narrator",
                "Nettle smashes the spy lamp. Far away, the merchant Lash bends over a map. His engineer has marked every tunnel the families used to reach their home city, Emberhaven.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash's engineer",
                "We have lost the lamp. But the whole road is here. The Emperor's army can follow it into the city.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash",
                "Then sell it to him. The families could never pay that much.",
                "")
        }));

    missions.push_back(storyNode(
        "se05_court_behind_curtain",
        "The Queen's Bargain",
        "Book Two - The Queen blocks the road",
        {
            panel("Queen Nyxara",
                "Caltheriel is safe with me. My honey ends pain. Once you taste it, you will understand why she should stay.",
                "cards/queenNyxara.png"),
            panel("Vesper",
                "She asked to leave. Hear her, even if you do not like her answer.",
                "cards/princeVesper.png"),
            panel("Nettle",
                "My back hurts every day. I will try your honey. But if I still choose to leave, you open the prison road.",
                "cards/nettleStarbright_mounted.png"),
            panel("Queen Nyxara",
                "Agreed. One spoon will be enough.",
                "cards/queenNyxara.png")
        }));

    missions.push_back(storyNode(
        "se06_honey_without_hunger",
        "Sweet as a Trap",
        "Book Two - Honey Without Hunger",
        {
            panel("Nettle",
                "One spoon. Ten minutes. Pavo, take away the spoon and lead me away when time is up, even if I beg to stay.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "The honey takes away her pain. Nettle smiles for the first time in days. After ten minutes, she begs for more. Pavo takes the spoon from her hand and leads her away.",
                "cards/pavoQuickstep.png"),
            panel("Nettle",
                "I still want it. I still choose to leave. Caltheriel deserves that choice too.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Nyxara watches Nettle leave the honey behind. Then she opens the road herself. She walks beside Nettle to the prison.",
                "cards/queenNyxara.png")
        }));

    missions.push_back(storyNode(
        "se08_nine_lives_one_choice",
        "No One Takes Her Place",
        "Book Two - Nine Lives, One Choice",
        {
            panel("Narrator",
                "The prison keeper unlocks the cells as Nyxara orders. Only Caltheriel remains trapped. Her cage feeds on her light. A key cannot open it.",
                "cards/theTockman.png"),
            panel("Vesper",
                "Caltheriel, you can barely stand! Let the cage take me instead.",
                "cards/princeVesper.png"),
            panel("Caltheriel",
                "No. Stay outside. Pass my light to our friends. The cage cannot follow it when many people hold it. Help me leave.",
                "characters/caltheriel.png"),
            panel("Vesper",
                "I was afraid I would lose you. All right. I will hold one share. Who will help me?",
                "cards/princeVesper.png")
        }));

    missions.push_back(storyNode(
        "se09_dusk_crown",
        "Hold Her Light",
        "Book Two - The shared light",
        {
            panel("Narrator",
                "Caltheriel reaches through the bars. A little light passes into each waiting hand. The bars grow dim. Nettle's fingers begin to shake.",
                "characters/caltheriel.png"),
            panel("Nettle",
                "Reed, take some of mine. I cannot hold it all.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Reed takes part of Nettle's share. Nyxara holds another. Caltheriel slips through the fading bars. The guards still obey the cage. They rush toward the opening.",
                "cards/reedBaelstone.png")
        }));

    missions.push_back(seelieCryptDefense());

    missions.push_back(storyNode(
        "se10a_caltheriel_walks_out",
        "Caltheriel Walks Free",
        "Book Two - No Crown Can Hold Light",
        {
            panel("Caltheriel",
                "I am outside. Open your hands. Let my light come home.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "The light returns to her beyond the cage's reach. Vesper takes one step forward, then waits. Caltheriel kneels and holds out her hand.",
                "cards/princeVesper.png"),
            panel("Caltheriel",
                "Yes, come here. Then take me home. All of us.",
                "characters/caltheriel.png")
        }));

    missions.push_back(storyNode(
        "se12_first_boats_burn",
        "The Wall That Saves Them",
        "Book Three - The city under attack",
        {
            panel("Narrator",
                "At dawn, the team reaches Emberhaven. Empire ships are already firing at the harbor. Families crowd small boats. Lash waits on the dock beside a switch.",
                ""),
            panel("Lash",
                "My engine is ready beneath the theater. It can raise a wall of light across this harbor. Say yes before the next shot.",
                ""),
            panel("Nettle",
                "You hid spy lamps among those families. Now you want us to trust you?",
                "cards/nettleStarbright_mounted.png"),
            panel("Lash",
                "You want them alive. So do I, for today.",
                ""),
            panel("Reed",
                "My family paid for that engine. Our guards stay with it. I keep the switch. Raise the wall.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The wall flashes up. A cannon shot bursts against it above a boat full of children. They are alive. Pavo runs down to help them land.",
                "cards/pavoQuickstep.png")
        }));

    missions.push_back(seelieRulesUnderPressure());

    missions.push_back(seelieWarmChannel());

    missions.push_back(storyNode(
        "se18a_complete_is_a_label",
        "The Wall Takes Their Strength",
        "Book Three - Lash's betrayal",
        {
            panel("Narrator",
                "Reed pulls the switch. The wall fades, but light keeps pouring out of people's hands. Lash's men slam the theater doors against the city guards. The engine is still running.",
                "cards/reedBaelstone.png"),
            panel("Tavin",
                "That switch only stopped the wall. The engine beneath the theater keeps taking our strength. Workers are strapped to it. I know a way in.",
                ""),
            panel("Nettle",
                "Then we break it. Right now.",
                "cards/nettleStarbright_mounted.png"),
            panel("Tavin",
                "It also runs the lifts out of the workrooms. Fire has blocked the stairs below. Stop it now, and the wounded will be trapped there.",
                ""),
            panel("Reed",
                "Then the people come out first. Show us the way.",
                "cards/reedBaelstone.png")
        }));

    missions.push_back(storyNode(
        "se15_bell_and_order",
        "Let Her Through",
        "Book Three - The shelter door",
        {
            panel("Narrator",
                "Near the theater, an Empire soldier staggers toward a shelter. Liora, an archer, draws her bow. Her sister is inside, caring for the wounded.",
                ""),
            panel("Empire soldier",
                "His voice is in my head. He wants me to kill. I do not want to hurt anyone. Please stop my sword arm.",
                ""),
            panel("Pavo writes",
                "My tune can weaken the Emperor's spell for a moment. Put your sword down while it lasts.",
                "cards/pavoQuickstep.png"),
            panel("Narrator",
                "Pavo plays. The soldier drops her sword. From inside, Liora's sister waves the woman toward a bed. Liora lowers her bow and follows. Pavo stays to help the healers break the spell.",
                "cards/pavoQuickstep.png")
        }));

    missions.push_back(storyNode(
        "se20_harness_anyone_can_leave",
        "Make the Harness Safe",
        "Book Three - A Harness Anyone Can Leave",
        {
            panel("Narrator",
                "Nettle ties a rescue harness around Tavin to lower him beside the lifts. Before he goes down, he tries to open its buckle with his one working hand.",
                "cards/nettleStarbright_mounted.png"),
            panel("Tavin",
                "I cannot reach it. Put it here, in front. I need to get out of this without waiting for you.",
                ""),
            panel("Nettle",
                "I thought it was ready. I should have asked you. Show me where.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "They move the buckle and test it again on the floor. Tavin opens it easily. Nettle lowers him to the workers. Ruvan flies down beside him to guide them into the lifts.",
                "cards/feyMessenger.png")
        }));

    missions.push_back(seelieControlledFall());

    missions.push_back(seelieEmperorAtTree());

    missions.push_back(seelieTwoMenOneVacancy());

    missions.push_back(storyNode(
        "se29a_baalzapub_ascendant",
        "Liora Takes the Shot",
        "Book Three - The raised stage",
        {
            panel("Narrator",
                "Lash holds his stage high with stolen magic. Below it, Ruvan helps a worker into the lift. Liora draws her bow from the theater door.",
                ""),
            panel("Nettle",
                "Wait! Lash is holding that stage up. Ruvan is under it!",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Lash points at the shelter across the street. Fire pours from his hand into its doorway. Liora sees her sister carrying a stretcher toward the flames.",
                ""),
            panel("Liora",
                "I can stop his hand. I cannot wait.",
                ""),
            panel("Narrator",
                "She fires. The arrow hits Lash. The fire goes out, but his stage crashes down. Ruvan cries out beneath a fallen beam. Lash lies wounded beside the controls.",
                ""),
            panel("Narrator",
                "Liora drops her bow and runs into the dust. She finds Ruvan's hand and holds it. She cannot pull him free.",
                "")
        }));

    missions.push_back(storyNode(
        "se29b_fizzlewick_says_no",
        "The Engineer Stops Obeying",
        "Book Three - The workers refuse",
        {
            panel("Narrator",
                "Lash reaches for the controls. His engineer blocks his hand. Behind them, workers struggle against the bands that feed their strength into the engine.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash",
                "Turn it all the way up. Take what they have left. I can still win this.",
                ""),
            panel("Lash's engineer",
                "No. I kept working when Tavin begged me to stop. I will not do it again.",
                "cards/fizzlewickGearwright.png"),
            panel("Narrator",
                "The engineer pulls the release. The bands fall open. Tavin guides the freed workers toward the lifts. City guards seize Lash. The engineer stays to help carry the wounded.",
                "")
        }));

    missions.push_back(storyNode(
        "se29c_all_authority_returns",
        "Get Ruvan Out",
        "Book Three - Under the stage",
        {
            panel("Narrator",
                "A fallen beam pins Ruvan's leg. Liora tries to lift it alone. It does not move.",
                ""),
            panel("Nettle",
                "Zippy, take this rope through the gap. Come back when you are across.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Zippy carries the rope under the beam. Nettle and Liora pull together. Tavin reaches in with his good arm and draws Ruvan clear.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Ruvan's leg is broken. They strap him into the harness, with the buckle where he can reach it. Liora carries him to the last lift. Nettle goes to help Reed keep the exit clear.",
                "cards/feyMessenger.png")
        }));

    missions.push_back(seelieHoldWitnessRail());

    missions.push_back(storyNode(
        "se29d_reed_ends_the_house",
        "The Emperor's Last Choice",
        "Book Three - The last command",
        {
            panel("Narrator",
                "The Emperor enters the engine room. He pulls stolen light toward himself. It burns his hands. Outside, the people he controls sink to their knees.",
                ""),
            panel("Caltheriel",
                "Let their strength go. There are healers outside. You can still walk out of here.",
                "characters/caltheriel.png"),
            panel("The Emperor",
                "And let my army leave me?",
                ""),
            panel("Caltheriel",
                "Yes. Open your hands.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "He opens one hand. Outside, a soldier stands and walks away. The Emperor reaches after her. Then he closes his fist.",
                ""),
            panel("The Emperor",
                "Come back! I said come back!",
                ""),
            panel("Narrator",
                "He pulls harder. Too much light pours into him. He falls beside the engine. Caltheriel calls a healer, but the Emperor is dead. Outside, his soldiers rise. His voice has stopped.",
                "")
        }));

    missions.push_back(storyNode(
        "se29_no_complete_bearer",
        "Stop the Engine",
        "Book Three - No new master",
        {
            panel("Lash",
                "Reed! Your family owns this engine. Let it run. Take my place, and the city is yours.",
                ""),
            panel("Reed",
                "My family has taken enough. Nettle, stop it.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Nettle turns the wheel. The engine stops. The last stolen light returns to the people outside. Reed breaks its controls with a hammer. No one will start it again.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "The guards carry Lash to the healers, then lock his door. His engineer goes with them. Tavin walks beside the freed workers into the morning air.",
                "")
        }));

    missions.push_back(storyNode(
        "se30_names_we_keep",
        "A Song to Come Home To",
        "Book Three - The ordinary stage",
        {
            panel("Liora",
                "Ruvan, Nettle told me you were below the stage. I fired anyway. I stopped the fire, but I nearly killed you.",
                ""),
            panel("Ruvan",
                "I know. I am not ready to talk about it. Help me get home. Please walk slowly.",
                "cards/feyMessenger.png"),
            panel("Narrator",
                "Liora carries one end of his stretcher. In the weeks that follow, she helps repair the streets he uses. When Ruvan asks her to wait, she waits.",
                ""),
            panel("Narrator",
                "Months later, Nettle rides Zippy into the reopened theater. Reed and Tavin move chairs to make room. Vesper curls up beside Caltheriel. Pavo starts the song in the wrong key. Everyone laughs and begins again.",
                "cards/nettleStarbright_mounted.png")
        }));

    missions.push_back(seelieRoadOfSmallLights(true));

    missions.push_back(seelieReadingRoomRehearsal());

    missions.push_back(seelieNyxaraInfestReconstruction());

    missions.push_back(seelieQueensPriceRehearsal());

    missions.push_back(seelieNoCrownRehearsal());

    missions.push_back(seeliePearlAndRootRehearsal());

    attachGeneratedScenarioArt(StoryCampaign::Seelie, missions);
    return missions;
}();

} // namespace

std::span<const StoryMission> storyMissions(StoryCampaign campaign)
{
    switch (campaign)
    {
    case StoryCampaign::Blackthorn: return BlackthornMissions;
    case StoryCampaign::Mirewatch: return MirewatchMissions;
    case StoryCampaign::Seelie: return SeelieMissions;
    }
    return {};
}

} // namespace bayou::client
