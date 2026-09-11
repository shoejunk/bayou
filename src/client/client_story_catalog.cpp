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
        return "story/se/se29d_reed_ends_the_house/scenario_v1.png";
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se29_no_complete_bearer")
    {
        return "story/se/se29_no_complete_bearer/scenario_v1.png";
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
        "Read the characters' decision and its immediate consequence. No board action or player choice is required.";
    mission.hint =
        "No board action is required. The next tactical lesson begins when the story reaches one.";
    mission.briefing.assign(panels.begin(), panels.end());
    mission.objectiveSpec.kind = StoryObjectiveKind::StoryOnly;
    return mission;
}

std::string_view chronicleContextFor(std::string_view missionId)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 28> Contexts = {{
        {"mw02_gilded_hold",
            "Following Pedros's killers inland, the travelers find six dead Company workers and the Gilded Hold: a covered gold cage whose captive has already prepared her own escape."},
        {"mw07_twenty_debtors",
            "After the Mudfen wagon rescue exposes Victor's retaliation plan, twenty debtors remain inside the Company prison. The rescuers must let the prisoners' own votes shape the escape."},
        {"mw08_lesson_night",
            "Thaeron traps Reed in the north lock and offers a private bargain while a Blackthorn search closes on Bluewater. Reed has no sealed amnesty and no authority to bargain away the refuge."},
        {"mw09_invitations",
            "With Bluewater destroyed, three survivors and the captured courier Remy reach a gate opened only by freely offered blood. Rescue, custody, and ownership must remain separate."},
        {"mw11_no_plan_saves_all",
            "At the Gilded Vault, five rescue routes fail at once while the Company trap closes. The group can save many people, but not without permanent loss."},
        {"mw13_making_credit",
            "Back in Mirewatch, forty-three survivors form a provisional Society and test what evidence can be trusted. Mog, Braun, Scooter, and Gearjaw each provide only the part they choose."},
        {"mw14_public_lie",
            "At a public hearing, Thaeron offers selective truths while Vanya enters custody to keep the charter and evidence in public view. Reed's hidden name now affects who may speak for the group."},
        {"mw16_gossiping_trees",
            "Victor's abducted crew poisons a Feyward root road. The travelers can stop the pipe only by spending pursuit time and accepting Briar's narrow terms for carrying the Root Key."},
        {"mw17_factory_heaven",
            "Across the Feyward road, the party finds a factory built from Vesper's false memory of safety. Freeing him means breaking the mechanism without treating comfort or grief as an enemy."},
        {"mw18_allies_dishonestly",
            "At a Company toll gate, Reed meets Pavo, Nettle, and Zippy - detainees with their own terms. A borrowed royal badge may open the gate, but it cannot make them allies or property."},
        {"mw19_road_keeps_one",
            "A gray boundary advances behind fleeing families. The group must divide food, route, and retreat responsibilities before Rowan chooses to remain with the refugees."},
        {"mw20_monster_rules",
            "At the World Tree, Blackthorn has forced the wounded Sylvara into the form called Ashenfang. The party studies the marks and hidden tack controlling whom she attacks."},
        {"mw21_four_losses",
            "Beneath the World Tree, six cages, a poisoned root, Company marks, and Gearjaw's learning core fail at once. The rescuers divide their attention while every choice leaves another danger moving."},
        {"mw22_clean_shot",
            "Birdie reaches a clear shot at Ashenfang's throat, then sees the real mechanism: a six-strand command cable. Waiting for that harder target will cost her the use of her right arm."},
        {"mw23_choice_not_cure",
            "Donella offers Sylvara a revocable network that can carry the Root Key without making one body the bridge. Every person may refuse, and Nettle's refusal leaves her free to cut Lash's wire."},
        {"mw24_agent_not_heir",
            "With the cages failing and the World Tree wounded, the Society's writ reaches Victor's gate. Reed's blood can identify the old Baelstone road, but cannot own what everyone else must do."},
        {"mw25_deed_own_hand",
            "Three weeks after Charter Day, Thaeron arrives with six hundred sacks of grain and a deed conditioned on a living Baelstone steward. Reed brings the offer to the Society instead of deciding alone."},
        {"bt06_receipt_book",
            "Victor receives six measured reports from the failed Mudfen wagon ambush and turns them into a seven-part retaliation plan against ferries, medicine, wages, tools, food, Torren, and Missus Vale."},
        {"bt07_debtor_prison",
            "Blackthorn's prison uses a Guardian to predict which rescuer will move first. Inside, twenty debtors argue over whether to destroy the ledger and flood the mill."},
        {"bt08_north_lock",
            "Thaeron draws Reed into the north lock with care, family truth, and an unsealed bargain while Company forces close on Bluewater. Reed refuses, but his escape flare exposes the refuge."},
        {"bt09_published_mystery",
            "Lash turns the Gilded Vault into a published trap: four routes tempt rescuers toward the Root Key while buyers, prisoners, and witnesses become parts of his measurement."},
        {"bt10_stolen_road",
            "After the prison and Vault losses, Blackthorn bills Victor for every failed asset. In Mirewatch, forty-three people form a Society and hear Mog, Braun, Gearjaw, and Scooter as separate witnesses."},
        {"bt11_public_lie",
            "At Charter Day's public hearing, Thaeron reveals selected truths about Reed while Vanya surrenders herself and a dead route so the evidence can remain public."},
        {"bt12_break_charter",
            "The magistrate voids Blackthorn's charter. Victor answers with a staged bombing and orders Braun to fire through children while Company crews abduct people toward Feyward."},
        {"bt13_feyward_transit",
            "Briar's bounded stair ends at Blackthorn's moth-harvester factory, where false rooms offer each traveler a private version of safety. Breaking the machine cannot decide whether its captives choose to leave."},
        {"bt14_move_boundary",
            "At a toll gate and shifting boundary, Pavo, Nettle, Zippy, refugees, and Rowan negotiate limited cooperation while the Society's writ travels toward Victor."},
        {"bt16_clean_shot",
            "At the World Tree, four failures converge: cages fall, command cables tighten, Gearjaw's core is exposed, and Ashenfang remains Sylvara under coerced pain."},
        {"bt18_deed_own_hand",
            "Three weeks after Charter Day, Thaeron offers six hundred sacks of grain under a living-Baelstone condition. Reed takes the deed to public witnesses instead of settling alone."}
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
        "Follow the decision, its cost, and what it changes for the people involved.";
    mission.hint =
        "No board action is required. The next tactical lesson begins when the story reaches one.";

    if (entry.id == "mw02_gilded_hold")
    {
        mission.briefing = {
            panel("Narrator",
                "Donella finds Pedros murdered. Six Blackthorn workers lie dead around a covered gold cage; the living captive inside is already testing its rear seam.",
                entry.art),
            panel("Donella of the Marsh",
                "Pedros's death does not make his knife, his grief, or this captive ours. We clear a road; she decides whether to use it.",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "The captive reverses the hidden splinter herself. She gives no name; the record keeps that refusal instead of inventing one. She steps onto a road beneath another sky, then breaks the splinter so nobody can follow.",
                entry.art),
            panel("Reed Baelstone",
                "The three-knotted mark was in my uncle Thaeron's study. I thought every Baelstone dead; he may be alive behind Blackthorn.",
                "cards/reedBaelstone.png"),
            panel("Asta",
                "I am a Mirewatch fishmonger. Put the empty Hold inside my return crate; Telos keeps his freight weight while the prison and its evidence enter town on our terms.",
                entry.art)};
    }
    else if (entry.id == "mw07_twenty_debtors")
    {
        mission.briefing = {
            panel("Narrator",
                "Twenty debtors wait inside Blackthorn's mill. Tommy has broken his own hand to open the infirmary route; a Guardian watches for the next brave body.",
                entry.art),
            panel("Garrett Saye",
                "Twenty by names. Nobody becomes a number because we are in a hurry. Little Fen stays where I can reach him.",
                "cards/marshlandVeteran.png"),
            panel("Garrett Saye",
                "The red point is on Little Fen. Get behind the steam manifold; I will cross its line first.",
                "cards/marshlandVeteran.png"),
            panel("Rowan Leafbound",
                "Garrett cannot be healed. Saving the ledger means holding the mill; flooding it destroys the proof and opens the escape.",
                "cards/rowanLeafbound.png"),
            panel("Grizzel Ironmouth",
                "Sixteen say drop it; three say keep it. We flood the mill. Twenty cross by our own count, and nineteen answer alive.",
                entry.art)};
    }
    else if (entry.id == "mw08_lesson_night")
    {
        mission.briefing = {
            panel("Narrator",
                "Reed leaves Bluewater after promising not to go. At the north lock, Thaeron waits with dry socks, pear broth, an unsigned amnesty, and an inheritance.",
                entry.art),
            panel("Thaeron Baelstone",
                "Come home. I register the amnesty, curb Victor, and name you heir. I cared for you; I also permit the danger making this offer urgent.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "Seal the amnesty first. When Thaeron closes the portfolio instead, I refuse and take the west stair.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The west stair is a prepared flood trap. Reed's flare brings Mangletooth, but Company boats follow its green fire to Bluewater, where Missus Vale holds the cellar door until she is shot as seven escape.",
                entry.art),
            panel("Vanya Bluewater",
                "Reed does not choose routes now. Erevan holds route security; Birdie holds signals; Rowan confirms a breach. Until the next refuge, Reed will not know where it is.",
                "cards/vanyaBluewater.png")};
    }
    else if (entry.id == "mw09_invitations")
    {
        mission.briefing = {
            panel("Narrator",
                "After Bluewater burns, Sedge staggers into Vanya's refuge with a buyer token stitched beneath his skin. Remy offers a route to the people that token sold.",
                entry.art),
            panel("Sedge",
                "My sister inherits my debt if I die. Ask before you take the token; use it only if it opens the cellar and gets her free.",
                entry.art),
            panel("Erevan the Shadow",
                "Three trapped plates guard the prisoners. I take the left before anyone assigns me. Vanya takes center, Donella the right; we pull together.",
                "cards/erevanTheShadow.png"),
            panel("Hessa Quill",
                "Three of us are alive. Remy changed the music whenever we screamed together. He fears the person above him, but fear does not make him safe.",
                entry.art),
            panel("Vanya Bluewater",
                "Remy enters custody, not our alliance. The prisoners leave first. Sedge's sister is released from the debt, and the buyer records burn behind us.",
                "cards/vanyaBluewater.png")};
    }
    else if (entry.id == "mw11_no_plan_saves_all")
    {
        mission.briefing = {
            panel("Narrator",
                "The Vault reverses its currents and seals five rescue lanes. Donella can reach the children or Nima, Cal, and Dessa; the Root Key closes each road behind her.",
                entry.art),
            panel("Dessa",
                "You could bring one of us and leave the others. Do not. Get the children off the gallery; we will hold the rope as long as we can.",
                entry.art),
            panel("Reed Baelstone",
                "The north gate needs the Baelstone seal hand. I put my right hand in the bronze moon and keep it there until the last body clears.",
                "cards/reedBaelstone.png"),
            panel("Juniper Flash",
                "Mae, you carried eight children through every drill I gave you. Follow my light, take them to the gate, and do not turn back for me.",
                "characters/juniperFlash.png"),
            panel("Vanya Bluewater",
                "Sixteen out. Nima, Cal, and Dessa missing. Juniper dead. Reed's right hand is warm but without movement. Record every loss separately.",
                "cards/vanyaBluewater.png")};
    }
    else if (entry.id == "mw13_making_credit")
    {
        mission.briefing = {
            panel("Narrator",
                "Forty-three people ask Mirewatch to act in public. Mara opens the meeting with one warning: a new Society must never become another Company.",
                entry.art),
            panel("Reed Baelstone",
                "Anyone may leave without losing food or standing. No route, witness list, or decision rests on one person. If those rules fail, dismiss us.",
                "cards/reedBaelstone.png"),
            panel("Erevan the Shadow",
                "Mog and Braun speak separately. I write down what each saw and what each refuses to answer. Agreement can support a fact; it cannot make either witness ours.",
                "cards/erevanTheShadow.png"),
            panel("Scooter",
                "Two taps, Gearjaw. Remember our game. You know me even if Victor calls you property, and I will not call you mine for knowing me back.",
                "cards/scooter.png"),
            panel("Narrator",
                "The blue trace ignites and burns the kitchen route. Everyone escapes. Three ferries divide the records, proving the new Society can survive losing any single road.",
                entry.art)};
    }
    else if (entry.id == "mw14_public_lie")
    {
        mission.briefing = {
            panel("Narrator",
                "Thaeron feeds Certification Square and tells only true stories, each trimmed to omit its cost. The Society arrives hungry with claims instead of matching coats.",
                entry.art),
            panel("Thaeron Baelstone",
                "Reed is an alias and the Baelstone heir to the Company interest he condemns. The claimants deserve to know whose voice they chose.",
                "cards/thaeronBaelstone.png"),
            panel("Vanya Bluewater",
                "I surrender myself, my fishbone key, and the burned south route. Pull your officers from the crowd and let the hearing continue.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "Reed is chosen, not legal. Thaeron arranged my family's deaths and offered me Blackthorn if the Society withdrew. I publish both.",
                "cards/reedBaelstone.png"),
            panel("Mara Mudfen",
                "Joni has the charter; Vanya remains in custody. Your inheritance enters our lockbox. You lose the advocate rail, not your witness voice.",
                "cards/joniPumpernickel.png")};
    }
    else if (entry.id == "mw16_gossiping_trees")
    {
        mission.briefing = {
            panel("Narrator",
                "Victor's road poisons the bayou into a skin of dead creatures. Half-Ear lies beneath a discharge pipe; stopping it will widen Blackthorn's lead.",
                entry.art),
            panel("Birdie the Wise",
                "Victor has half a bell. We need to move - but leaving this pipe makes us the reason Half-Ear's brood keeps dying.",
                "cards/birdieTheWise.png"),
            panel("Donella of the Marsh",
                "Pedros used bait to draw hungry mouths away from people. Blackthorn turned that mercy into a harness and killed him with it. Crimp the pipe. Reed braces, Rowan hammers, and I witness. The last hatchling dies; the poison stops.",
                "cards/donellaOfTheMarsh.png"),
            panel("Briar Whisperthorn",
                "I keep the Mirror. Donella may take the Root Key only to carry it whole to the living World Tree - no use, service, silence, or life owed.",
                "cards/briarWhisperthorn.png"),
            panel("Maggie Mudroot",
                "Mangletooth lends his return bone and remains with the Hold. Seven travelers cross on chosen names and borrowed voices; no solitary command closes the road.",
                entry.art)};
    }
    else if (entry.id == "mw17_factory_heaven")
    {
        mission.briefing = {
            panel("Narrator",
                "Blackthorn's sky-road harvests living moth voices and folds intruders into false rooms built from the safety they most want. Victor's cages move farther ahead.",
                entry.art),
            panel("Erevan the Shadow",
                "The false kitchen gives me Vanya's father alive and forgiving. I know it is false. I still want one more minute where I cost him nothing.",
                "cards/erevanTheShadow.png"),
            panel("Erevan the Shadow",
                "Donella, help me. Hold me to the road. I drive the copper tube through the table rather than accept the memory's easy pardon.",
                "cards/erevanTheShadow.png"),
            panel("Rowan Leafbound",
                "The harvester will keep feeding on other lives. We take the retaining pin together, free the moths, and lose the road behind us.",
                "cards/rowanLeafbound.png"),
            panel("Vesper",
                "Elliot is dead, and my memories contradict one another. I will leave this tower with you; do not make that a promise to guide or obey.",
                "cards/princeVesper.png")};
    }
    else if (entry.id == "mw19_road_keeps_one")
    {
        mission.briefing = {
            panel("Narrator",
                "A gray boundary advances three paces per engine stroke behind thirty-one refugees. The west arch is shut; Seelie officers arrive to arrest everyone.",
                entry.art),
            panel("Seelie Patrol Captain",
                "The writ is received. Admit displaced travelers by chosen name or description. No arrests until the road is clear; pursuit resumes afterward.",
                entry.art),
            panel("Reed Baelstone",
                "Revenge will not return my name. Reed is the name I chose, and Baelstone was my father's before Thaeron used it. I still answer tomorrow.",
                "cards/reedBaelstone.png"),
            panel("Rowan Leafbound",
                "The officers cannot hold the arch and carry every body. I stay. Give me the road, food, bodies, and retreat - not your fight.",
                "cards/rowanLeafbound.png"),
            panel("Birdie the Wise",
                "You have the road. I leave the white cord and meal packets. We go without your shield or healing; you keep the last route home where people need it.",
                "cards/birdieTheWise.png")};
    }
    else if (entry.id == "mw20_monster_rules")
    {
        mission.briefing = {
            panel("Narrator",
                "Blackthorn's engine wounds Aelon while Ashenfang attacks company-marked targets. She spares one unmarked woman; a guard hammers a green tack into Fen's collar.",
                entry.art),
            panel("Donella of the Marsh",
                "She chose around one unmarked person. I do not know whether she will again. Fen, strip every tool and mark; this test may kill you.",
                "cards/donellaOfTheMarsh.png"),
            panel("Fen Oake",
                "I have just escaped. I heard the risk. Sula and Siv wait until I pass. I go empty-handed so the others may know whether this road exists.",
                entry.art),
            panel("Narrator",
                "Ashenfang kills Fen, tears the hidden tack from his collar, crushes its silver-tree mark, and passes the grass-woman without striking.",
                "cards/Sylvara.png"),
            panel("Donella of the Marsh",
                "I saw the guard hammer that tack in and called it a repair. Our rule was incomplete, and Fen paid. Record that; Birdie keeps the shot held.",
                "cards/donellaOfTheMarsh.png")};
    }
    else if (entry.id == "mw21_four_losses")
    {
        mission.briefing = {
            panel("Narrator",
                "Four failures move at once: six cages, Aelon's last rooted fibers, company marks, and Gearjaw's learning core. Blackthorn engineer Fizzlewick Gearwright commands the machines.",
                "cards/fizzlewickGearwright.png"),
            panel("Fizzlewick Gearwright",
                "Acquire the marked animal. Return. Every tactic used before is now a correction in my Guardians; improvisation is another measurement.",
                "cards/fizzlewickGearwright.png"),
            panel("Gearjaw",
                "Scooter's two taps are mine to remember. I block the capture fork and open my learning core. If I lose what I learned, Victor cannot use it against him again.",
                "cards/gearjaw.png"),
            panel("Mog",
                "Open her frame, Grask. You do not get to hide behind her. I take his knife and keep the doorway open while the freed prisoners pass.",
                "cards/Mog.png"),
            panel("Donella of the Marsh",
                "The service rail is open and the cage groups are moving. Mog lives. Lash's page makes the Key dangerous, but Sylvara still chooses; I will give her a place to answer.",
                "cards/donellaOfTheMarsh.png")};
    }
    else if (entry.id == "mw22_clean_shot")
    {
        mission.briefing = {
            panel("Birdie the Wise",
                "One arrow. Ashenfang's throat is clear now; the shared cable appears only after the bent spoke. Below, Grask raises his axe over Scooter.",
                "cards/birdieTheWise.png"),
            panel("Gearjaw",
                "Gearjaw destroys the learning core that makes it itself, drives Grask aside, and throws Scooter by the harness into the prisoner net.",
                "cards/gearjaw.png"),
            panel("Victor Greyshard",
                "Take the good answer. Kill her before she punishes you for believing your eyes.",
                "cards/victorGreyshard.png"),
            panel("Birdie the Wise",
                "The monster is not the mechanism. I drop my aim four inches, wait for the spoke, and send the arrow through all six cable strands.",
                "cards/birdieTheWise.png"),
            panel("Birdie the Wise",
                "The cable breaks and the root settles. The yoke tears my right shoulder; that arm no longer answers. Pavo, do not pull it. Get me down.",
                "cards/birdieTheWise.png")};
    }
    else if (entry.id == "mw23_choice_not_cure")
    {
        mission.briefing = {
            panel("Narrator",
                "Donella tries to graft the Root Key into Aelon. Sylvara rejects the imposed shape, throws her back, and answers through Ashenfang: no.",
                "cards/Sylvara.png"),
            panel("Donella of the Marsh",
                "Show us what you will accept. Unequal shares, revocable at any time; the roots wait, and standing nearby is not an answer.",
                "cards/donellaOfTheMarsh.png"),
            panel("Nettle Starbright",
                "No. Blackthorn forced my scars into its diagram. I will not fit them to a better machine, and I do not consent to this one.",
                "cards/nettleStarbright.png"),
            panel("Narrator",
                "Donella honors every refusal; Birdie withholds Scooter's assent. Sylvara uses only offered roads, leaving Nettle free to ground and cut Lash's wire.",
                entry.art),
            panel("Sylvara",
                "I return scarred, not cured. The dead stay dead and every mark remains. Do not ask me to forgive. Take Victor alive; make him answer.",
                "cards/Sylvara.png")};
    }
    else if (entry.id == "mw25_deed_own_hand")
    {
        mission.briefing = {
            panel("Narrator",
                "Three weeks after Charter Day, Thaeron waits by a stool placed for Reed's injured leg. His handwritten deed offers six hundred sacks from the south granary.",
                entry.art),
            panel("Thaeron Baelstone",
                "The granary opens only to a living Baelstone steward. Six days of flour will make the town say yes; private refusal will not erase that pressure.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "Not here. I bring the deed to the Society and call Mara and Adie. I am angry enough to misreport you, so they hear every condition.",
                "cards/reedBaelstone.png"),
            panel("Mara Mudfen",
                "Adie reads every condition and objection. The deed goes under our seal; the unsigned inheritance stays locked, and Reed gets no vote on his claim.",
                entry.art),
            panel("Thaeron Baelstone",
                "I am glad you survived. I would still do it all again. If the Society limits my estate through wording, I will appeal the wording.",
                "cards/thaeronBaelstone.png")};
    }
    else if (entry.id == "bt06_receipt_book")
    {
        mission.briefing = {
            panel("Narrator",
                "The Mudfen wagon carries a six-position recorder beneath its cloth. Grask's escort is bait; every helper reaching for a captive gives it a hand to remember.",
                entry.art),
            panel("Grask",
                "Mr. Greyshard thanks you for the count. The copper ribbon has already kept the rescuers' tools, distances, and timings.",
                "cards/Grask.png"),
            panel("Birdie the Wise",
                "Torren raises the bridge; I put an arrow through the rear lock. Donella and the captives jump to a net as the wheel folds under the wagon.",
                "cards/birdieTheWise.png"),
            panel("Victor Greyshard",
                "Cut the ribbon into seven strips. Close ferries, seize medicine and wages, take tools and food, erase Torren, and charge Vale's kitchen.",
                "cards/victorGreyshard.png"),
            panel("Adie Pike",
                "They say Torren resigned, but his tea is warm and the copied signature is wrong. The recorder's report survived; we keep this page where Juniper can witness it.",
                entry.art)};
    }
    else if (entry.id == "bt07_debtor_prison")
    {
        mission.briefing = {
            panel("Narrator",
                "A many-eyed Guardian predicts the next rescue position. Garrett has learned all twenty prisoners' names; Little Fen grips his coat as the machine turns toward them.",
                "cards/fizzlewickGearwright.png"),
            panel("Garrett Saye",
                "Twenty people, twenty names. Little Fen, stay at my shoulder. If the red point finds you, get behind the manifold and let it follow me.",
                entry.art),
            panel("Fizzlewick Gearwright",
                "Every rescue teaches my Guardian where courage moves next. Garrett crosses first; the machine predicts him and fires.",
                "cards/fizzlewickGearwright.png"),
            panel("Grizzel Ironmouth",
                "Garrett is dead. We can hold for the ledger, or drop its weight down the hoist and open the flood. Prisoners decide.",
                entry.art),
            panel("Grizzel Ironmouth",
                "Sixteen say drop it; three say no. The mill and ledger go under. Twenty cross by our count, nineteen breathing, Garrett carried with us.",
                entry.art)};
    }
    else if (entry.id == "bt08_north_lock")
    {
        mission.briefing = {
            panel("Narrator",
                "Thaeron brings dry socks, pear broth, a correct list of nineteen debtors, and an unsigned amnesty. His care is precise; so is the room around it.",
                entry.art),
            panel("Thaeron Baelstone",
                "Come home. I register amnesty, curb Victor, restore your name, and leave you Blackthorn. I permitted Victor's room to act; I can close it.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "Seal the amnesty first. When Thaeron closes the portfolio instead, I refuse and take the west stair.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The stair is a prepared flood trap. Reed's flare calls Mangletooth and exposes the drowned-bell cut; Blackthorn boats follow it to Bluewater.",
                entry.art),
            panel("Vanya Bluewater",
                "Missus Vale is dead, seven are out, and the refuge is gone. Reed loses route authority. Thaeron's kindness does not reduce his trap's cost.",
                "cards/vanyaBluewater.png")};
    }
    else if (entry.id == "bt09_published_mystery")
    {
        mission.briefing = {
            panel("Narrator",
                "Four invitations lead to one Vault: Hollis's cipher, Sedge's buried token, Remy's letter, and a route only Baelstone credit can open. Agreement is the bait.",
                entry.art),
            panel("Remy Croche",
                "The west door closes in nineteen minutes. I admit one broker, one buyer, and one guarantor. Every extra rescuer makes the cover costlier.",
                entry.art),
            panel("Donella of the Marsh",
                "Dessa refuses a rescue that saves only one. I carry the Root Key back toward the children, though every completed groove narrows the exits.",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "Reed sacrifices movement in his right hand to open the gate. Juniper gives Mae the flare; the bell cable kills her after the others cross.",
                entry.art),
            panel("Vanya Bluewater",
                "Sixteen alive. Nima, Cal, and Dessa missing. Juniper dead. The buyer register came out damaged. Keep the account whole until we name its author.",
                "cards/vanyaBluewater.png")};
    }
    else if (entry.id == "bt10_stolen_road")
    {
        mission.briefing = {
            panel("Narrator",
                "In the burned prison office, Victor receives Blackthorn's bill for the Guardian, mill, and debt ledger - and notice that he will not speak at Charter Day.",
                entry.art),
            panel("Victor Greyshard",
                "Price the prison, the fire, and my failure on separate lines. A Company that exempts its own officer cannot claim the right to price anyone else.",
                "cards/victorGreyshard.png"),
            panel("Mara Mudfen",
                "We are not building a kinder Company. Forty-three people asked us to act, and every one keeps the right to withdraw, question us, and send us home.",
                entry.art),
            panel("Erevan the Shadow",
                "I hear Mog and Braun separately. I record what each saw and what each refuses, then compare the facts without making either witness belong to us.",
                "cards/erevanTheShadow.png"),
            panel("Narrator",
                "Gearjaw answers Scooter's rhythm. Its blue trace ignites the kitchen road, but everyone escapes; the Society divides its records among three ferries and keeps working.",
                entry.art)};
    }
    else if (entry.id == "bt11_public_lie")
    {
        mission.briefing = {
            panel("Narrator",
                "Thaeron fills the square with food and selected truths. Each kindness is real; each account stops before the cost that would turn gratitude into a claim.",
                entry.art),
            panel("Thaeron Baelstone",
                "Reed is an alias and heir to the Blackthorn interest he condemns. The claimants deserve that truth, even if I omit my private offer.",
                "cards/thaeronBaelstone.png"),
            panel("Vanya Bluewater",
                "I surrender my fishbone key, the dead south route, and myself. Withdraw your officers from the crush and keep the hearing open.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "I publish the orders proving Thaeron arranged my family's deaths and the inheritance offered for withdrawal. I have not decided whether to reject it.",
                "cards/reedBaelstone.png"),
            panel("Mara Mudfen",
                "Then uncertainty is not refusal. The papers enter our lockbox. Reed loses advocate authority but remains a witness; Thaeron stays for third bell.",
                entry.art)};
    }
    else if (entry.id == "bt12_break_charter")
    {
        mission.briefing = {
            panel("Magistrate",
                "Blackthorn took payment for five duties; Mirewatch did the work. Burning the charter cannot burn that labor. The Company charter is void here.",
                entry.art),
            panel("Narrator",
                "Victor overturns the powder cart, plants bombs among the programs, calls the blast insurrection, and sends the walking mill across the children's exits.",
                entry.art),
            panel("Braun Stonefist",
                "At Kestrel Ford, you had not mined children. That is not a target order. It is an excuse written as one.",
                "cards/braunStonefist.png"),
            panel("Mog",
                "Grask, weapons down; custody after. When he flees, I name the powder, child lanes, and living cargo, then offer my wrists to Society custody.",
                "cards/Mog.png"),
            panel("Narrator",
                "Grask kills Braun. Victor, Grask, Fizzlewick, and Gearjaw flee with three harvest cases and a measurement crew. The buyer register remains; a municipal writ follows.",
                entry.art)};
    }
    else if (entry.id == "bt13_feyward_transit")
    {
        mission.briefing = {
            panel("Narrator",
                "Briar's bounded stair ends at Blackthorn's moth-harvester factory. Its false rooms offer each traveler a private version of safety while the machine fills sacks from their memories.",
                entry.art),
            panel("Erevan the Shadow",
                "The factory offers me a kitchen where Vanya's father forgives me. I want it. Donella, hold me to the real road while Rowan and I break the harvester.",
                "cards/erevanTheShadow.png"),
            panel("Donella of the Marsh",
                "The smoke collar is feeding the room from the harvester. I will cut the collar; Rowan, take the frame only after Erevan says he is ready.",
                "cards/donellaOfTheMarsh.png"),
            panel("Vesper",
                "Elliot is dead, and my memories disagree. I still choose to leave this tower with you. Rescue gives you no right to my route, my past, or what I choose next.",
                "cards/princeVesper.png"),
            panel("Narrator",
                "Rowan offers an open hand, and Vesper chooses it. The travelers break the moth harvester, leave its false rooms behind, and continue toward the World Tree.",
                entry.art)};
    }
    else if (entry.id == "bt14_move_boundary")
    {
        mission.briefing = {
            panel("Narrator",
                "Stolen royal badges free detainees only because Pavo, Nettle, and Zippy choose how to help. A badge opens a gate; it buys nobody.",
                entry.art),
            panel("Nettle Starbright",
                "I travel only to first sight of the grove, then choose again aloud. Zippy is not equipment or mine; ask him before putting him in a plan.",
                "cards/nettleStarbright.png"),
            panel("Seelie Patrol Captain",
                "The writ is received. Travelers may give chosen names or descriptions. No arrests until the moving boundary road is clear.",
                entry.art),
            panel("Rowan Leafbound",
                "I stay. The officers cannot hold the arch and carry every body. Give me food, retreat, and the road; I will not leave them for Victor's fall.",
                "cards/rowanLeafbound.png"),
            panel("Birdie the Wise",
                "You have the road and Mangletooth's bone. All detainees cross. We continue without Rowan's shield, healing, or the last route home.",
                "cards/birdieTheWise.png")};
    }
    else if (entry.id == "bt16_clean_shot")
    {
        mission.briefing = {
            panel("Narrator",
                "At Aelon's wound, four failures converge: cages drop, cables tighten, Gearjaw's core opens, and Ashenfang moves where coerced pain points.",
                entry.art),
            panel("Gearjaw",
                "Scooter's two taps are mine. I destroy my learning core so Victor cannot turn them against him, knock Grask aside, and throw Scooter into the prisoner net.",
                "cards/gearjaw.png"),
            panel("Mog",
                "Open her frame, Grask. You do not get to hide behind her. I take his knife and hold the doorway while the freed prisoners cross.",
                "cards/Mog.png"),
            panel("Birdie the Wise",
                "The monster is not the mechanism. I wait one beat, cut the six-strand cable instead of Sylvara's throat, and lose my right arm's use.",
                "cards/birdieTheWise.png"),
            panel("Sylvara",
                "The cable falls, and my body answers me again. Birdie spared my life and lost her arm's use. I choose custody and judgment; obedience would not repay her.",
                "cards/Sylvara.png")};
    }
    else if (entry.id == "bt18_deed_own_hand")
    {
        mission.briefing = {
            panel("Narrator",
                "Thaeron places a stool on the ferry step Reed's injured leg can manage. His own hand offers six hundred sacks in a granary deed.",
                entry.art),
            panel("Thaeron Baelstone",
                "A living Baelstone must accept stewardship. Hunger will make the town vote yes, even if Reed arranges to look privately clean.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "Not here. Mara and Adie will hear you; I am angry enough to misstate this later. The claimants, not I, decide the grain.",
                "cards/reedBaelstone.png"),
            panel("Mara Mudfen",
                "Adie records every condition. The deed and unsigned inheritance stay locked under Society custody; Reed gets no vote in his interest.",
                entry.art),
            panel("Thaeron Baelstone",
                "I am glad Reed lives and would repeat every act that kept him so. The Society may limit my wording; I will appeal the wording.",
                "cards/thaeronBaelstone.png")};
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
            panel("Narrator",
                "Blackthorn search boats close behind the travelers. They bring the covered Gilded Hold into Bluewater, a hidden refuge built inside Mangletooth - Maggie Mudroot's enormous undead alligator-house - and ask its owner for shelter.",
                "cards/maggieMudroot.png"),
            panel("Maggie Mudroot",
                "Mangletooth is my home and my partner, not a headquarters waiting to be claimed. He will shelter people tonight. If he says no to a room or a repair, that answer holds.",
                "cards/maggieMudroot.png"),
            panel("Nibsy",
                "I am Maggie's undead apprentice. I can mend the cut in his hide after he agrees; knowing how to repair a wound does not give me permission to touch it.",
                "cards/nibsy.png"),
            panel("Birdie the Wise",
                "At the Infamous Mouse, Joni's inn above the refuge, we feed the arrivals, record their names, and divide the work before anyone calls this a headquarters.",
                "cards/birdieTheWise.png"),
            panel("Vanya Bluewater",
                "Bluewater's thirty-seven household keys and the laundry line form an evacuation network: one visible wash signal sends each family toward a different safe door. No route belongs to the person who remembers it.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "My uncle, Thaeron Baelstone, finances Blackthorn while offering me selective help. His puzzle box contains a route map; we compare it with Vanya's keys and Birdie's reports before trusting it.",
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
            panel("Narrator",
                "Juniper brings Hollis Vale through two searched handoffs. Wet, missing his Blackthorn signet, and carrying an oilskin folio, Victor's accountant asks Bluewater to let him defect.",
                "cards/mirewatchInformant.png"),
            panel("Hollis",
                "Tommy Siltwater. Garrett Saye. Siv Kettle. Grizzel Ironmouth. Fen Oake. Briar Whisperthorn - and fourteen more. Here are their injuries, households, and the two who cannot swim.",
                "cards/mirewatchInformant.png"),
            panel("Hollis",
                "Victor moved all twenty from labor debt to secured transfer. Three are children; two entered as dependents. I changed the heading. My replacement arrived yesterday, and Victor gave me tomorrow off.",
                "cards/mirewatchInformant.png"),
            panel("Hollis",
                "The infirmary ordered twenty-four sleeping cordials: twenty prisoners, the physician, the turnkey, and two night clerks. These copied keys open the medicine cache and the crypt holding withheld wages.",
                "cards/mirewatchInformant.png"),
            panel("Vanya Bluewater",
                "That can prove your access, not your loyalty. Two crews test the keys at once, carry no word between them, and leave at the first wrong shelf, extra guard, or charm.",
                "cards/vanyaBluewater.png"),
            panel("Narrator",
                "Both keys open. Medicine and wages match except for five bandage rolls and four coppers. Two workers confirm the transfer; a third confirms Victor's bounty. Every independent count returns twenty.",
                "cards/mirewatchInformant.png"),
            panel("Hollis",
                "I want a boat north, a different name, and work that does not turn children into arithmetic. Fear arrived before my conscience. That is not innocence.",
                "cards/mirewatchInformant.png"),
            panel("Vanya Bluewater",
                "All twenty transfer at ninth bell. We carry a quiet road and a flood road inside, tell the prisoners what each costs, and open neither until they answer. No one enters alone.",
                "cards/vanyaBluewater.png")
        });
}

StoryMission mirewatchFirstOpenCheck()
{
    StoryMission mission = sharedS02();
    mission.title = "No One Alone - Optional Open Check";
    mission.sourceChapter =
        "Chapter 8 / optional non-canon open tactics check";
    mission.lesson =
        "OPTIONAL OPEN CHECK - CHOOSE ANY UNIT AND PRINTED ACTION";
    mission.objective =
        "OPTIONAL NON-CANON SKIRMISH. Defeat the three opposing units with any legal sequence of ordinary actions.";
    mission.hint =
        "There are no glowing moves or prescribed order. Inspect any unit, choose one of its printed actions, and End Turn when your activation is complete.";
    mission.briefing.push_back(panel("Rules boundary",
        "The defection, the independent key tests, and the prisoners' right to choose their road are the canonical Chapter 8 events above. This optional board is only an early test of independent play; its result changes no story event.",
        "cards/mirewatchInformant.png"));
    mission.briefing.push_back(panel("Controls",
        "Mouse: hold a unit, drag it to a lit legal square or target, and release. Keyboard: focus a unit and press Enter, use the arrows to focus a legal target, then press Enter again. Press I to inspect a focused card and A to use an available printed ability.",
        "cards/donellaOfTheMarsh.png"));
    mission.briefing.push_back(panel("Coach",
        "Start this open three-against-three skirmish if you want to test out now, or Skip it and continue the story. Skipping awards no mastery and does not rewrite the events you just read.",
        "cards/reedBaelstone.png"));
    mission.aftermath = {panel("Coach",
        "You completed an early open fight without prescribed moves. Later guided practice remains available when you want a focused rules demonstration; this check awards no card or rule mastery.",
        "cards/donellaOfTheMarsh.png")};
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
    StoryMission mission = chronicleFromEntry(entry);
    mission.title = "The Receipt Book - Optional Open Check";
    mission.sourceChapter =
        "Chapter 7 / optional non-canon open tactics check";
    mission.lesson =
        "OPTIONAL OPEN CHECK - CHOOSE ANY UNIT AND PRINTED ACTION";
    mission.objective =
        "OPTIONAL NON-CANON SKIRMISH. Defeat the three opposing units with any legal sequence of ordinary actions.";
    mission.hint =
        "There are no glowing moves or prescribed order. Inspect any unit, choose one of its printed actions, and End Turn when your activation is complete.";
    mission.briefing.push_back(panel("Rules boundary",
        "The recorder's six positions, the wagon escape, Victor's seven-part retaliation, and Torren's disappearance are the canonical Chapter 7 events above. This optional board is only an early test of independent play; its result changes none of them.",
        "cards/Grask.png"));
    mission.briefing.push_back(panel("Controls",
        "Mouse: hold a unit, drag it to a lit legal square or target, and release. Keyboard: focus a unit and press Enter, use the arrows to focus a legal target, then press Enter again. Press I to inspect a focused card and A to use an available printed ability.",
        "cards/blackthornAlchemist.png"));
    mission.briefing.push_back(panel("Coach",
        "Start this open three-against-three skirmish if you want to test out now, or Skip it and continue the story. Skipping awards no mastery and does not rewrite the events you just read.",
        "cards/thaeronBaelstone.png"));
    mission.aftermath = {panel("Coach",
        "You completed an early open fight without prescribed moves. The later optional drills remain available for focused practice; this check awards no card or rule mastery.",
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
        "The Mystery Was Published",
        "Chapter 14",
        {
            panel("Vanya Bluewater",
                "Juniper is buried. Nima, Cal, and Dessa remain missing, not dead; the account keeps both the rescue and its cost.",
                "characters/juniperFlash.png"),
            panel("Erevan the Shadow",
                "I released Remy on a private mark and sent an uninformed runner after him. He escaped because I spent proof on the theory I preferred.",
                "cards/erevanTheShadow.png"),
            panel("Joni Pumpernickel",
                "The scraps share a hidden moth watermark, inward-drawing ash, and one staged running order. The mystery was manufactured for its readers.",
                "cards/joniPumpernickel.png"),
            panel("Maggie Mudroot",
                "Lash LeGrim is the theater engineer who built the lower room and arranged its four false roads. Thaeron paid for hands without learning who owned the stage beneath them.",
                "cards/maggieMudroot.png"),
            panel("Narrator",
                "Briar cuts through Joni's cart and escapes through joined reflections with both the Mirror and the Root Key. Mangletooth's refusal remains recorded.",
                "cards/briarWhisperthorn.png")
        });
}

StoryMission sharedS04()
{
    return storyNode(
        "s04_wounds_that_vote",
        "Wounds That Vote",
        "Chapter 15",
        {
            panel("Birdie the Wise",
                "Minnow gets the full fever-bark course. Reed's usefulness does not make his body weigh more than a child's.",
                "cards/birdieTheWise.png"),
            panel("Delphine Nettle",
                "My unauthorized arithmetic saves two patients and costs Noll the hearing in one ear. A better outcome does not erase the unasked risk.",
                "cards/delphineNettle.png"),
            panel("Mara",
                "Forty-three people vote. Twenty-two choose public action; every dissent and every narrower proposal stays in the record.",
                "cards/mirewatchInformant.png"),
            panel("Reed Baelstone",
                "Charter Day can join channels, ferries, burial, fire response, and safe passage. The claimants own the case, not me.",
                "cards/reedBaelstone.png")
        });
}

StoryMission sharedS05()
{
    return storyNode(
        "s05_memory_contradicts",
        "The Memory That Contradicts",
        "Chapter 21",
        {
            panel("Narrator",
                "At the moonfruit tree, one question yields one recollection, never history. Reed must surrender a private sensory memory, and a second person must eat the fruit to witness his question exactly as held.",
                "cards/reedBaelstone.png"),
            panel("Donella of the Marsh",
                "I witness only the question you hold now. You may keep the uncertainty. Once you choose, the tree chooses what the truth costs.",
                "cards/donellaOfTheMarsh.png"),
            panel("Reed Baelstone",
                "What did Thaeron choose when he made my family's deaths serve his succession? Take the winter room.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Rosemary rinse, fever vinegar, cedar smoke, candied pear sugar rasping Reed's lip, his mother's cold wedding ring, and her laughter felt through two fingers at his throat leave him together.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Reed chills his signet and presses it to both cheeks. He feels metal and stone grit, but no echo of her ring. He knows rosemary by name; no scent comes. The room remains as facts his body cannot enter.",
                "cards/reedBaelstone.png"),
            panel("Donella of the Marsh",
                "Thaeron ordered the senior branch removed and payment if no claimant returned. In the same bargain, he demanded the youngest be brought to him alive. He chose the murders and Reed's preservation together.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "That is the memory's contradiction: his care was real, and so were the deaths it served. One truth acquits nothing. Write both, write what they cost, and then we stop the harm in the grove.",
                "cards/reedBaelstone.png")
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
            panel("Narrator - SEQUEL THREAD",
                "In Lash's Baalzepub theater, Fizzlewick records that Mirewatch won but one route measurement survived. The Root Key remains inside Aelon; Caltheriel's prison and the southern war lie ahead.",
                "cards/fizzlewickGearwright.png"),
            panel("Erevan's murdered mentor - preserved voice",
                "The milk-white pearl holding his stolen craft cracks toward the north. The refusal Lash could not erase speaks again: No one alone.",
                "cards/erevanTheShadow.png")
        });
}

// Blackthorn follows the same public events as Mirewatch, but never through
// the same camera. These five connective scenes keep Victor's organization,
// loyalties, and fractures in view instead of replaying the resistance copy.
StoryMission blackthornS01()
{
    return storyNode(
        "s01_hospitality",
        "The Road Victor Cannot Price",
        "Chapters 4-5 - Blackthorn perspective",
        {
            panel("Company search ledger",
                "The covered Hold vanishes at Bluewater. Patrols count thirty-seven household keys, one laundry signal, and no headquarters. Mangletooth shelters people only when Maggie says he agrees.",
                "cards/maggieMudroot.png"),
            panel("Victor Greyshard",
                "Fizzlewick, every report names a different door. Build me a method that survives a witness changing her mind; I will not lose Reed because a town refuses to behave like freight.",
                "cards/victorGreyshard.png"),
            panel("Fizzlewick Gearwright",
                "A living house is not a lock with teeth. Mangletooth's refusal changes the route, so the instrument can describe his answer but cannot make it predictable.",
                "cards/fizzlewickGearwright.png"),
            panel("Thaeron Baelstone",
                "My puzzle box gives Reed a road, not a leash. He will compare it with Bluewater's keys before he trusts anything carrying my hand.",
                "cards/thaeronBaelstone.png"),
            panel("Victor Greyshard",
                "You call that care because the exception bears your name. I call it an unpriced route. Grask searches the water; I will learn which of us your nephew believes.",
                "cards/victorGreyshard.png")
        });
}

StoryMission blackthornS02()
{
    return storyNode(
        "s02_no_one_alone",
        "Tomorrow Off",
        "Chapter 8 - Blackthorn perspective",
        {
            panel("Victor Greyshard",
                "Hollis changed twenty debtors to secured transfer, copied two keys, and ordered enough cordial for every prisoner and night clerk. His replacement is seated. I gave him tomorrow off.",
                "cards/victorGreyshard.png"),
            panel("Grask",
                "Say hunted, not absent. Give me the north road, the medicine count, and the names of anyone who cannot swim. I will close the distance.",
                "cards/Grask.png"),
            panel("Mog",
                "Twenty names, Victor. Three are children. Call them loads again and your arithmetic tells me more about you than it tells me where Hollis went.",
                "cards/Mog.png"),
            panel("Company report",
                "Both copied keys open. Medicine and withheld wages are disturbed by exact counts. Two workers confirm the transfer; a third confirms Victor's bounty. Hollis reaches Bluewater alive.",
                "cards/mirewatchInformant.png"),
            panel("Victor Greyshard",
                "Fear moved him before conscience. It still moved him beyond my wall. Price the failure, keep Mog's twenty names, and never again let one accountant hold the whole road.",
                "cards/victorGreyshard.png")
        });
}

StoryMission blackthornS03()
{
    return storyNode(
        "s03_published_mystery",
        "The Trap Had an Audience",
        "Chapter 14 - Blackthorn perspective",
        {
            panel("Victor Greyshard",
                "The Vault returned sixteen captives, one dead rescuer, three missing names, and my buyer register in public hands. That is not victory hidden inside acceptable loss.",
                "cards/victorGreyshard.png"),
            panel("Grask",
                "Give me Reed, the Root Key, and a road that does not fold into reflections. Juniper's death proves the trap had teeth; it does not prove who held the chain.",
                "cards/Grask.png"),
            panel("Mog",
                "Juniper is dead. Nima, Cal, and Dessa are missing. If your report makes those four costs sound alike, I will not sign it.",
                "cards/Mog.png"),
            panel("Fizzlewick Gearwright",
                "The scraps share one moth watermark, inward ash, and a staged running order. Someone manufactured the mystery for Mirewatch - and arranged where Blackthorn would stand while they solved it.",
                "cards/fizzlewickGearwright.png"),
            panel("Company ledger",
                "The name Lash LeGrim enters the case beside the lower theater. Thaeron paid for hands without learning who owned the stage; Victor orders every remaining road audited.",
                "cards/fizzlewickGearwright.png")
        });
}

StoryMission blackthornPoisonedRootRoad()
{
    return storyNode(
        "bt13a_poisoned_root_road",
        "Poisoned Root Road",
        "Book One, Chapter 19 - The Poisoned Root Road",
        {
            panel("Narrator",
                "Beyond the moving border, Blackthorn's poisoned root road is killing Half-Ear's brood. Stopping the pipe widens Victor's lead; leaving it running kills whatever hatches next.",
                "cards/donellaOfTheMarsh.png"),
            panel("Donella of the Marsh",
                "Crimp it. Reed braces the source, Rowan hammers, and I cut through the pipe. We lose the chase's clean road, but the poison stops here.",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "The crushed pipe ends the clean pursuit road. Seven travelers reach Briar's living-tree stair, the only bounded crossing that still leads toward Blackthorn's factory.",
                "cards/briarWhisperthorn.png"),
            panel("Briar Whisperthorn",
                "Donella may carry the Root Key whole to the living World Tree. No use, service, silence, or life is owed. I keep the Mirror; those are the road's terms.",
                "cards/briarWhisperthorn.png"),
            panel("Narrator",
                "Briar opens one stair under those terms. Donella carries the seven-toothed Root Key; Briar keeps the Mirror, and Mangletooth stays with the Hold. The stair ends at the moth-harvester factory.",
                "cards/briarWhisperthorn.png")});
}

StoryMission blackthornS05()
{
    return storyNode(
        "s05_memory_contradicts",
        "The Exception in Thaeron's Hand",
        "Chapter 21 - Blackthorn perspective",
        {
            panel("Recovered moonfruit account",
                "Reed gives the tree his winter room and loses its scents, textures, and his mother's ring against his skin. Donella witnesses the question, not a complete history.",
                "cards/reedBaelstone.png"),
            panel("Thaeron Baelstone - original order",
                "Remove the senior branch and pay if no claimant returns. Bring the youngest to me alive. Both instructions bear my hand; neither cancels the other.",
                "cards/thaeronBaelstone.png"),
            panel("Victor Greyshard",
                "You built succession with one preserved child and called the child love. Do not ask Blackthorn to pretend your exception stood outside the order it protected.",
                "cards/victorGreyshard.png"),
            panel("Thaeron Baelstone",
                "I ask you to pretend nothing. I chose the deaths. I chose Reed alive. My care was real, and I would still preserve him from what you make of that fact.",
                "cards/thaeronBaelstone.png"),
            panel("Company ledger",
                "Reed publishes both choices and the price of learning them. The contradiction acquits nobody; it breaks Thaeron's last private account into evidence others can judge.",
                "cards/reedBaelstone.png")
        });
}

StoryMission blackthornS06()
{
    return storyNode(
        "s06_town_owns_itself",
        "What Remains of Blackthorn",
        "Chapter 30 and Epilogue - Blackthorn perspective",
        {
            panel("Mog",
                "Braun refused an order against children. Grask killed him. I surrendered, named what I did, and offered evidence without asking the town to call me good.",
                "cards/Mog.png"),
            panel("Siv Kettle - public postmortem",
                "Grask killed Braun after Braun refused the order against children. At the World Tree, six prisoner groups collapsed three cage levels across Grask; I found him pinned beneath the scaffold and confirmed him dead.",
                "cards/Grask.png"),
            panel("Thaeron Baelstone",
                "Six hundred sacks, granary only. Reed sends my condition to a public vote. I am glad he lives; I do not repent the choices that made him refuse me.",
                "cards/thaeronBaelstone.png"),
            panel("Fizzlewick Gearwright - SEQUEL THREAD",
                "Mirewatch governs itself, and I carry one surviving route measurement to Lash's Baalzepub theater. The Root Key remains inside Aelon; Caltheriel's prison and the southern war wait ahead.",
                "cards/fizzlewickGearwright.png"),
            panel("Erevan's murdered mentor - preserved voice",
                "The milk-white pearl cracks toward the north. One refusal remains in the stolen craft: hands separate. No one alone.",
                "cards/erevanTheShadow.png")
        });
}

StoryMission mirewatchOpening()
{
    StoryMission mission;
    mission.id = "mw01_river_teeth";
    mission.title = "River Teeth";
    mission.sourceChapter = "Chapter 1 - A Cage with a Bill of Sale";
    mission.lesson = "MOVEMENT, TURN CADENCE, ATTACKS, HEALTH, AND DEATH";
    mission.objective =
        "Move Reed one square on each of two turns. Defeat three wounded gators with printed attacks; Telos's Travel must clear Reed's final shot.";
    mission.hint =
        "Only the highlighted story action is accepted. Mouse: hold the named piece, drag to the marked target, and release. Keyboard: focus the piece and press Enter, then focus the target and press Enter again.";
    mission.briefing = {
        panel("Narrator",
            "Merchant Telos ferries injured archer Reed, healer-tracker Donella, and shadow scout Erevan toward Mirewatch when three harnessed gators close around the skiff. Donella recognizes burnt-pine bait used by her missing friend Pedros.",
            "story/mw/mw01/01_gator_ambush_v2.png"),
        panel("Donella of the Marsh",
            "Someone copied Pedros's bait. Reed, two squares away. Move one square now; the next when the water gives you a turn.",
            "story/mw/mw01/02_reed_retreat_v2.png"),
        panel("Coach",
            "Guided Actions count completed game actions, not mouse motions. For a piece action, hold the left mouse button on the piece, drag to the target, and release. Or use keyboard focus + Enter: Enter selects the focused piece; Enter on the focused target confirms. This lesson accepts only the glowing scripted action. In ordinary play, one ready piece takes one normal action before End Turn, except when its printed rule grants more.",
            "story/mw/mw01/02_reed_retreat_v2.png"),
        panel("Coach",
            "Reed's injury is story context, not a status effect. The center gator will pursue him after the first End Turn. Number badges show current Health: blue for your pieces, red for enemies. These gators begin wounded at 1, and a 1-damage hit removes a unit at 0.",
            "story/mw/mw01/02_reed_retreat_v2.png")};
    mission.aftermath = {
        panel("Erevan the Shadow",
            "Someone taught those mouths what Blackthorn cargo smells like.",
            "story/mw/mw01/04_harness_aftermath_v2.png"),
        panel("Chronicle adaptation",
            "Story Mode lets all four travelers help defeat the three trained gators. The novel's exchange is shorter: Reed kills one, and the other two withdraw.",
            "story/mw/mw01/04_harness_aftermath_v2.png"),
        panel("Narrator",
            "On shore, Donella finds Pedros - her friend and fellow Blackthorn resister - murdered beside the trail. Beyond him, plants crowd a covered Gilded Hold and an unnamed captive already working on her own escape.",
            "story/mw/mw01/04_harness_aftermath_v2.png")};
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
            "Step", "GET REED CLEAR - MOVE 1 OF 2",
            "Mouse: hold the left button on Reed, drag to teal TARGET: C6, then release. Keyboard: focus starts on Reed; press Enter to select him, then Enter again when focus jumps to C6. Step moves one square in any direction.",
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
                    "story/mw/mw01/01_gator_ambush_v2.png")
            }),
        scriptedPieceAction(
            StoryActionKind::Move, 2, "center_gator", 4, 3,
            "Bite", "THE GATOR LUNGES",
            "The center gator moves from E5 to D5.",
            "Mission data error."),
        scripted(
            StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE WATER SHIFTS",
            "The gator ends its turn.",
            "Mission data error."),
        scriptedPieceAction(
            StoryActionKind::Move, 1, "reed", 5, 1,
            "Step", "GET REED CLEAR - MOVE 2 OF 2",
            "Move Reed from C6 to B6. He is now two squares from where he began and farther from the threat.",
            "Reed must finish the second highlighted square himself."),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "HOLD THE SAFE LINE", "End Turn so Donella can answer the lunge.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATOR TURNS", "The gator loses Reed's trail and faces Donella.",
            "Mission data error."),
        scriptedPieceAction(
            StoryActionKind::Attack, 1, "donella", 4, 3,
            "Spark", "DONELLA - SPARK",
            "Use Donella's Spark from F5 on the wounded gator at D5. It reaches two squares along the row, and its 1 damage reduces the gator's last Health to 0.",
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
        scriptedPieceAction(
            StoryActionKind::Attack, 1, "erevan", 3, 3,
            "Shadow Blade", "EREVAN - SHADOW BLADE",
            "Use Erevan's Shadow Blade on the wounded gator beside him at D4. It is an attacking move: after the target reaches 0 Health, Erevan enters D4.",
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
        scriptedPieceAction(
            StoryActionKind::Move, 1, "telos", 4, 0,
            "Travel", "TELOS - TRAVEL",
            "Move Telos from B4 to A5 with Travel. This ordinary one-square move gets him out of Bite range and clears Reed's shot along column B.",
            "Use Telos's printed Travel action on highlighted A5.",
            {}, {},
            {
                panel("Telos the Merchant",
                    "Reed, take the clear line. I am stepping out of your shot.",
                    "story/mw/mw01/03_telos_travel_v2.png"),
                panel("Coach",
                    "Travel is Telos's printed one-square move. Going from B4 to A5 clears column B for Reed's Bow.",
                    "story/mw/mw01/03_telos_travel_v2.png")
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
        scriptedPieceAction(
            StoryActionKind::Attack, 1, "reed", 2, 1,
            "Bow", "REED - BOW",
            "Use Reed's Bow from B6 on the wounded gator at B3. It reaches three squares along column B; Telos vacated B4, so the normal line-of-sight path is clear.",
            "Finish with Reed's printed Bow attack.",
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
    mission.title = "Harness the Hunger";
    mission.sourceChapter = "Chapter 1 - post-attack rules reconstruction";
    mission.lesson = "ORTHOGONAL MOVEMENT, HEAL, DISABLE APPLICATION, DAMAGE";
    mission.objective =
        "Move a Lumberjack, heal the worker, disable the north gator, and make one Lumberjack attack. Do not kill a gator.";
    mission.hint =
        "This operation is evidence of cruelty, not a heroic victory. Do not kill a gator.";
    mission.briefing = {
        panel("Narrator",
            "Three trained gators strike Telos's freight skiff. The survivors find damaged harnesses, burnt pine, and sugar paste: evidence that hunger was conditioned to hunt unrecorded freight.",
            "cards/bullGator.png"),
        panel("Victor Greyshard",
            "A Baelstone presses a ring to an old gate and it opens. My blood leaves only a stain. I have work, so I need an order that bows to no name - not even mine. Make the harness repeat.",
            "cards/victorGreyshard.png"),
        panel("Rules reconstruction",
            "This real-card drill represents the method inferred from the damaged harness; no witness saw this exact pen crew. Scent timber is already in place as story context, not a player action or game rule.",
            "cards/blackthornLumberjack.png"),
        panel("Controls",
            "Mouse: drag the ready Lumberjack from B5 to the glowing C5 square and release. Keyboard: focus it with arrows or Tab, press Enter, move focus to C5, then press Enter again. Double-click a unit, or focus it and press I, to inspect its printed card.",
            "cards/blackthornLumberjack.png"),
        panel("Coach",
            "Guided Actions count completed game actions, not mouse motions. This drill accepts only the glowing scripted action. In ordinary play, one ready piece takes one normal action before End Turn, except when its printed rule grants more.",
            "cards/blackthornLumberjack.png"),
        panel("Coach",
            "Red badges show current Health. Healing Elixer restores Health only up to the printed maximum. Paralysis Potion applies Disable 2 without damage: the target misses its next two activations on turns belonging to its owner. This opening observes the first; Ashenfang's later drill follows both. Ordinary positive damage disables a survivor for one activation.",
            "cards/blackthornLumberjack.png"),
        panel("Chronicle",
            "The recovered harness proves that Blackthorn industrialized a defense Donella once used to protect people. The following board is a recommended rules reconstruction, not a witnessed scene.",
            "cards/blackthornAlchemist.png")};
    mission.aftermath = {
        panel("Narrator",
            "The reconstruction explains the attack that already happened; it does not turn inference into a witnessed Company scene.",
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
        scriptedPieceAction(StoryActionKind::Move, 1, "lumberjack", 4, 2,
            "Axe Swing", "OPEN THE WORK LANE", "Move B5 to glowing C5. Mouse: drag and release on C5. Keyboard: focus the Lumberjack, press Enter, focus C5, then press Enter. Orthogonal means the same row or column.",
            "Use the Lumberjack's ordinary one-square orthogonal Axe Swing action."),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "OBSERVE THE PEN", "End Turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE GATORS WAIT", "The penned animals hold.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 3, 2,
            "Healing Elixer", "HEAL THE WORKER", "Target the wounded friendly worker at C4.",
            "Use Healing Elixer on the friendly worker; it heals three without overheal.", "worker"),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "OPEN THE NORTH GATE", "End Turn.", "End Turn to release the north gator."),
        scriptedPieceAction(StoryActionKind::Move, 2, "north_gator", 2, 2,
            "Bite", "THE NORTH GATOR LUNGES", "The north gator moves from D3 to C3.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "THE HARNESS HOLDS", "The gator ends its turn.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 2, 2,
            "Paralysis Potion", "DISABLE WITHOUT DAMAGE", "Target the enemy gator at C3.",
            "Use Paralysis Potion: zero damage and Disable 2.", "north_gator"),
        scripted(StoryActionKind::EndTurn, 1, "", -1, -1,
            "MEASURE THE FIRST SKIPPED TURN", "End Turn.", "End Turn to observe Disable."),
        scripted(StoryActionKind::EndTurn, 2, "", -1, -1,
            "FIRST ACTIVATION DISABLED", "The north gator misses the first of two owner activations. Ashenfang's later drill follows a full Disable 2 to expiry.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "lumberjack", 4, 3,
            "Axe Swing", "ONE MEASURED HIT", "Hit the center gator at D5 once.",
            "Land exactly one ordinary hit; the surviving gator stays in place.", "center_gator")
    };
    // This optional opening introduces the Alchemist, but its two-turn
    // Paralysis Potion is not followed to expiry here. Full card mastery waits
    // for the required all-path synthesis, which proves every printed profile.
    mission.masteryCards = {};
    mission.masteryRules = {
        "orthogonal-movement", "friendly-heal", "maximum-health",
        "zero-damage-disable", "positive-damage-disable"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission mirewatchDeployment()
{
    StoryMission mission;
    mission.id = "mw03_town_under_company";
    mission.title = "The Town Under the Company";
    mission.sourceChapter = "Chapter 2 - rules rehearsal";
    mission.lesson = "CONTROL, DEPLOYMENT, TAX, AURA, PULL, SPEAR THRUST";
    mission.objective =
        "Deploy the Informant, pull a guard with Hooked Spear, finish it with Spear Thrust, then observe Joni's aura, income, and Tax.";
    mission.hint =
        "Joni controls adjacent ground and supplies Arcane and Civilized. A deployed unit arrives exhausted, meaning it cannot act until its owner's next turn.";
    mission.briefing = {
        panel("Reed Baelstone",
            "A number beside a workboat is not proof of inherited debt. The torn wrap reveals my Baelstone crest, and the official records it.",
            "cards/reedBaelstone.png"),
        panel("Joni Pumpernickel",
            "Blackthorn suspended the Infamous Mouse's license. It did not inherit my rooms. Move the guests.",
            "cards/joniPumpernickel.png"),
        panel("Rules rehearsal",
            "This board teaches control, deployment, Tax, and aura timing with real cards. It compresses the town's pressure into a rules lesson; it is not a literal Chapter 2 battle.",
            "cards/mirewatchInformant.png"),
        panel("Coach",
            "Controlled squares generate Resources. A Unit may deploy only when each Trait on its card also appears on at least one of your living Heroes; matching words appear on both cards. It must enter on empty controlled ground and cannot act on arrival.",
            "cards/mirewatchInformant.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The workboat stays with the Mudfens. Joni moves the Hold while the Informant's Tax makes the next Company turn more expensive.",
            "cards/joniPumpernickel.png")};
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
            "DEPLOY ON CONTROLLED GROUND", "Play the Informant card from your hand on highlighted C4.",
            "Deploy the Mirewatch Informant on C4. Joni supplies Civilized, and three nearby friendly pieces control that empty square.", "informant"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "spearman", 6, 3,
            "Hooked Spear", "SPEARMAN - HOOKED SPEAR",
            "Use ranged Hooked Spear from B5 through empty C6 to the Lumberjack at D7. One damage leaves it alive, so Pull draws it to C6 while the Spearman stays at B5.",
            "Card plays do not consume the turn's normal piece action; use Hooked Spear on the highlighted Lumberjack.", "notice_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "JONI'S AURA", "End Turn. Joni heals adjacent pieces, but never above their maximum health.",
            "End Turn to resolve Joni's healing aura."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "START-TURN ECONOMY", "The opponent passes; your next turn adds controlled-square income and collects Tax.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "informant", 4, 3,
            "Lunge", "INFORMANT - LUNGE", "Use Lunge diagonally from C4 into the Debt Collector at D5.",
            "Use the newly readied Informant's printed Lunge.", "tax_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE OPEN LANE",
            "End Turn after the Informant acts; only one piece can take a normal action in a turn.",
            "End Turn before activating the Spearman."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE COMPANY LINE HOLDS",
            "The opponent passes; the pulled guard remains beside the Spearman.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "spearman", 5, 2,
            "Spear Thrust", "SPEARMAN - SPEAR THRUST",
            "Now use adjacent Spear Thrust from B5 into the pulled Lumberjack at C6. This separate two-damage action moves the Spearman into the defeated target's square.",
            "Use the Spearman's adjacent Spear Thrust on the guard that Hooked Spear pulled to C6.", "notice_guard")
    };
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
    mission.title = "Sanctuary Debt";
    mission.sourceChapter = "Chapter 2 - rules simulation";
    mission.lesson = "INCOME, GATHER, TAX, SUMMON, TRAIL, COMMAND";
    mission.objective =
        "Deploy the Alchemist; use Command, Summon, and Trail; then use both printed Sapling profiles while Gather and Tax resolve.";
    mission.hint =
        "Thaeron supplies Arcane, Civilized, and Corrupt. Routine turn passes resolve automatically; every input is a printed card action or ability.";
    mission.briefing = {
        panel("Joni Pumpernickel",
            "Blackthorn spent my old ward, suspended the Mouse, and added sanctuary debt. I cut the seal myself; the rooms are still mine.",
            "cards/joniPumpernickel.png"),
        panel("Victor Greyshard",
            "Thaeron calls sanctuary a gift. Gifts belong to the hand that can withdraw them. Put the cost where Joni can see it; I trust an admitted debt more than a nobleman's kindness.",
            "cards/victorGreyshard.png"),
        panel("Thaeron Baelstone",
            "Victor calls sanctuary an advance to collect later. I enter the ward, the rooms, and the seal as separate claims; a debt entry does not itself make the Mouse ours.",
            "cards/thaeronBaelstone.png"),
        panel("Coach",
            "Place the Alchemist on C4. A Unit may deploy only when each Trait on its card also appears on at least one of your living Heroes; Thaeron supplies Arcane and Civilized here. It arrives exhausted - unable to act until its owner's next turn - and card plays do not consume the one normal piece action.",
            "cards/blackthornAlchemist.png"),
        panel("Rules simulation",
            "This recommended office simulation teaches the starter deck's real economy and dependent-unit rules. The ready Sapling demonstrates Heal and Entangle; the Grove Sister creates a separate Sapling through Trail. It is not a battle at the Mouse.",
            "cards/blackthornDebtCollector.png")};
    mission.aftermath = {
        panel("Chronicle",
            "Hospitality is entered as a liability, but the Mouse remains Joni's. The seal records pressure, not lawful ownership.",
            "cards/blackthornDebtCollector.png")};
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
            "DEPLOY THE ALCHEMIST", "Play the Alchemist card from your hand on highlighted C4.",
            "Deploy on C4; Thaeron supplies Arcane and Civilized.", "alchemist"),
        scriptedPieceAction(StoryActionKind::Move, 1, "foreman", 4, 2,
            "Slide", "FOREMAN - SLIDE",
            "Use Slide one square from B5 to empty C5. The Foreman's new front square at D5 stays open for Summon.",
            "Use the Foreman's printed Slide on highlighted C5."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE COMMAND", "End Turn after the Foreman moves.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OFFICE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedAbility(1, "thaeron", "command", "Command",
            "THAERON - COMMAND", "Select Thaeron and use Command.",
            "Command must begin with Thaeron and then activate one adjacent ready unit."),
        scriptedAbility(1, "foreman", "summon", "Summon",
            "COMMAND THE FOREMAN", "Use the adjacent Foreman's Summon while Command is active.",
            "Choose the adjacent ready Foreman; its front square at D5 is open.",
            "Blackthorn Lumberjack"),
        scriptedPieceAction(StoryActionKind::Move, 1, "sister", 5, 2,
            "Glide", "GROVE SISTER - GLIDE AND TRAIL", "Use Glide diagonally from B7 to C6. Trail leaves a Sapling on the origin.",
            "Command preserved the normal activation; use the Grove Sister's printed Glide."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END THE WORK SHIFT", "End Turn. The deployed and summoned units will ready next turn.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "GATHER AND TAX", "The opponent passes. Lumberjacks Gather; the Collector transfers Tax; controlled ground pays income.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 2, 1,
            "Healing Elixer", "ALCHEMIST - HEALING ELIXER", "Target the wounded friendly Lumberjack at B3.",
            "Use Healing Elixer on the adjacent friendly Lumberjack, not Paralysis Potion.", "lumberjack"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THAERON'S BLADE", "End Turn after the Alchemist heals.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE DEBT LINE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 7, 4,
            "Heal", "SAPLING - HEAL",
            "Use the ready Sapling's printed Heal on the wounded Lumberjack at E8. It restores Health only to the printed maximum.",
            "Use the Sapling's Heal on the highlighted friendly Lumberjack.", "sapling_patient"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY SAPLING ENTANGLE",
            "The ordinary turn ends automatically after the Sapling heals.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE TIMING TARGET HOLDS",
            "The opposing side passes; the Sapling becomes ready again.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 6, 5,
            "Entangle", "SAPLING - ENTANGLE",
            "Use Entangle on the adjacent Bull Gator at F7. The printed action deals no damage, applies Disable 2, and puts the Sapling on cooldown for one turn.",
            "Use the Sapling's zero-damage Entangle on the highlighted target.", "sapling_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ENTANGLE: FIRST OWNER TURN",
            "The turn advances automatically so the target misses its first disabled activation.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "SAPLING COOLDOWN CONSUMED",
            "The target cannot act. On the returning Blackthorn turn, the Sapling's one-turn cooldown consumes its activation.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ENTANGLE: SECOND OWNER TURN",
            "The cooldown turn advances automatically so the target reaches its second disabled activation.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "DEPENDENT PROFILE PROOF COMPLETE",
            "The target misses its second activation and Disable expires. Both printed Sapling profiles are now proven.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "thaeron", 2, 0,
            "Knife Stab", "THAERON - KNIFE STAB",
            "Use Knife Stab diagonally from B4 to the wounded Debt Collector at A3.",
            "Use Thaeron's printed attack on the highlighted target.", "thaeron_target")
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
    return mission;
}

StoryMission mirewatchCapstone()
{
    const MissionEntry entry = {
        "mw15_title_follows_burden", "Title Follows Burden",
        "Chapter 18 - combined-arms rules dramatization",
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
        "Deploy a Bog Spearman, clear six Company blockers, then reposition and heal with Donella's printed actions.";
    mission.hint =
        "Card play does not spend the turn's normal piece action. Vanya must complete both Blade Dance uses before anyone else acts.";
    mission.briefing = {
        panel("Joni Pumpernickel",
            "The charter can burn. Performed work, burial silver, ferry records, and forty-three witnesses remain.",
            "cards/joniPumpernickel.png"),
        panel("Rules boundary",
            "This board compresses the armed Charter Day escape into a starter-deck exam. The ruling, crowd evacuation, Braun's refusal, and Mog's surrender remain fixed story events.",
            "cards/mirewatchInformant.png"),
        panel("Coach",
            "Every required input in this final exam is a normal action from an authoritative card: deploy, Blade Dance, Longbow Shot, Hooked Spear, Lunge, and Walking Stick.",
            "cards/birdieTheWise.png")};
    mission.aftermath = {
        panel("Narrator",
            "The magistrate voids Blackthorn's charter. Victor's pre-staged charges split the square and force an evacuation; the attack was waiting before the ruling.",
            "cards/victorGreyshard.png"),
        panel("Narrator",
            "Victor orders fire through the children's lane. Braun refuses. Grask kills him; Mog lowers his weapon, surrenders, and names the powder, living cargo, and routes.",
            "cards/braunStonefist.png"),
        panel("Chronicle",
            "Under the bombing, Victor, Grask, Fizzlewick, and Gearjaw escape toward Feyward with three harvest cases and an abducted measurement crew. The intact buyer register earns a municipal writ; recovering those people and carrying the writ sends Reed's party after them along the poisoned road.",
            "cards/joniPumpernickel.png")};
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
            "Deploy the Bog Spearman on B7. It arrives exhausted, so it cannot act until its owner's next turn.",
            "Place the Spearman on highlighted B7.", "reinforcement"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 5, 1,
            "Blade Dance", "VANYA - BLADE DANCE",
            "Attack the Debt Collector on B6 with Blade Dance.",
            "Use Vanya's printed diagonal Blade Dance.", "vanya_guard_one"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 4, 2,
            "Blade Dance", "VANYA - COMPLETE THE REPEAT",
            "Use Blade Dance again on the Debt Collector at C5.",
            "The repeat lock requires the same action or Pass.", "vanya_guard_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE REINFORCEMENT", "End Turn after Vanya's required repeat.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HEARING HOLDS", "The remaining blockers hold their lanes.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "birdie", 5, 3,
            "Longbow Shot", "BIRDIE - LONGBOW SHOT",
            "Fire three squares along the row at the Alchemist on D6.",
            "Use Birdie's printed Longbow Shot.", "birdie_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE LINE", "End Turn after Birdie's shot.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SPEARMAN READIES", "The remaining blockers hold.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "reinforcement", 4, 3,
            "Hooked Spear", "SPEARMAN - HOOKED SPEAR",
            "Use ranged Hooked Spear from B7 to the Alchemist on D5. The one-Health target is destroyed, so there is no survivor for Pull to move.",
            "Choose the highlighted range-two diagonal target for Hooked Spear.", "spear_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "OPEN THE WITNESS LANE", "End Turn after the Spearman acts.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE RECORD STAYS PUBLIC", "The last two blockers hold.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "informant", 3, 2,
            "Lunge", "INFORMANT - LUNGE",
            "Use Lunge on the adjacent Debt Collector at C4.",
            "Use the Informant's printed Lunge.", "informant_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE FINAL LANE", "End Turn so Joni can act next turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST BLOCKER WAITS", "The final Debt Collector holds.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "joni", 6, 3,
            "Walking Stick", "JONI - WALKING STICK",
            "Use Walking Stick on the Debt Collector at D7.",
            "Use Joni's printed adjacent action.", "joni_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "OPEN THE HEALING LANE", "End Turn so Donella can reposition.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HEARING HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "donella", 5, 3,
            "Sprint", "DONELLA - SPRINT",
            "Use Sprint diagonally from E7 to D6, beside the wounded Veteran at E5.",
            "Use Donella's printed diagonal Sprint on highlighted D6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE MEND", "End Turn with Donella beside Vanya.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HEARING HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "donella", 4, 4,
            "Marshlight Mend", "DONELLA - MARSHLIGHT MEND",
            "Use Marshlight Mend on the adjacent wounded Veteran. It restores Health only to the printed maximum.",
            "Use Donella's printed heal on the highlighted Veteran.", "mend_target")};
    mission.masteryCards = {
        "Joni Pumpernickel", "Vanya Bluewater", "Birdie the Wise",
        "Mirewatch Informant", "Bog Spearman", "Donella of the Marsh"};
    mission.masteryRules = {
        "deployment-cost", "controlled-square-deployment",
        "arrival-exhaustion", "card-play-plus-activation", "repeat-lock",
        "ranged-attack", "ranged-diagonal-attack", "diagonal-attacking-movement",
        "diagonal-movement", "friendly-heal", "maximum-health"};
    return mission;
}

std::vector<StoryPanel> victorReckoningPanels()
{
    return {
        panel("Narrator",
            "Six prisoner groups collapse three cage levels across Grask. Siv Kettle finds him pinned, confirms him dead, and files the postmortem. The Society closes the Baelstone gate; Fizzlewick escapes with measurement scraps.",
            "cards/Grask.png"),
        panel("Reed Baelstone",
            "Society custody. Public record. Treatment fit for testimony. No sentence here, no immunity. Sylvara owns her answer.",
            "cards/reedBaelstone.png"),
        panel("Victor Greyshard",
            "The register names Northglass and sixty-one loads - households, physicians, full office names. Forty-three loads Mog signed; the Mirewatch families begin on page thirty-eight. I surrender it and myself for a mortal court.",
            "cards/victorGreyshard.png"),
        panel("Narrator",
            "Victor kneels and testifies. Then the guarded capsule answers matching glass against Victor's back molar and burns the register. He rises in smoke, takes Nettle's hook, and turns it beneath her ribs. Pavo steps between them.",
            "cards/nettleStarbright.png"),
        panel("Reed Baelstone",
            "Victor shoves Nettle aside and attacks Pavo, reaching for his neck. Pavo drives the blade beneath Victor's raised arm. Victor reaches once more for Nettle, then falls. No pulse. Record surrender, breach, and death.",
            "cards/pavoQuickstep.png")};
}

StoryMission blackthornCapstone()
{
    const MissionEntry entry = {
        "bt17_natural_order", "Blackthorn Mastery Proof: Natural Order",
        "All-path catch-up synthesis before Chapter 28",
        "BLACKTHORN CATCH-UP - EVERY PRINTED PROFILE",
        "Complete this starter-deck catch-up if any recommended drill lacks a completion stamp; otherwise continue without replay.",
        "The catch-up demonstrates every printed profile claimed here, including Relentless, Command, Summon, Capture, Charge, Trail dependents, healing, and two-turn Disable.",
        "Thaeron Baelstone", "Ashenfang", "Grask",
        "Reed Baelstone", "Donella of the Marsh", "Fizzlewick Gearwright",
        "This exercise combines printed actions; it does not claim these characters fought together at the World Tree.",
        "The rehearsal ends before the World Tree reckoning begins; no result on this board changes the novel.",
        "cards/fizzlewickGearwright.png"};
    StoryMission mission = playableFromEntry(entry);
    mission.briefing = {
        panel("Rules boundary",
            "This is a non-canon Blackthorn starter-deck mastery simulation using only printed actions. Thaeron, Ashenfang, and this assembled team are not a force from Chapter 28.",
            "cards/thaeronBaelstone.png"),
        panel("Coach",
            "Use Victor's Relentless once, then complete every printed profile on each card mastered here: Command and Knife Stab; Summon and Slide; Capture and Charge; Trail, healing, cooldown, and two-turn Disable. Every step is an existing printed action.",
            "cards/victorGreyshard.png"),
        panel("Coach",
            "Every earlier Blackthorn drill may be skipped. If any lacks a completion stamp, this catch-up supplies the missing full evidence. If all seven are complete, the last page offers Continue Without Replay. The required open field follows either path.",
            "cards/blackthornForeman.png"),
        panel("Coach",
            "The Guided Actions counter tracks the 26 printed actions and abilities you perform. Routine End Turn passes and the defenders' matching passes resolve automatically through the ordinary turn engine, so their cooldown, Disable, Gather, Tax, and healing timing still happens without 24 extra clicks.",
            "cards/thaeronBaelstone.png")};
    mission.aftermath = {
        panel("Rules reflection",
            "You combined the Blackthorn starter deck using only printed actions. The smaller open field now removes the prescribed move order.",
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
    mission.catchUpForSkippedRehearsals = true;
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 6, 6,
            "Axe Dance", "VICTOR - FIRST AXE DANCE", "Use Axe Dance on the Debt Collector at G7.",
            "Use Victor's printed Axe Dance on the adjacent enemy.", "first_guard"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 5, 5,
            "Axe Dance", "VICTOR - OPTIONAL RELENTLESS ACTION", "Relentless allows Victor to act again after his defeat. This lesson asks you to take that optional Axe Dance on the Debt Collector at F6; ordinary play also allows End Turn.",
            "Demonstrate the optional Relentless action with Victor. In ordinary play, End Turn may decline it.", "second_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "RELENTLESS LOCKS", "End Turn after resolving both Relentless actions.",
            "Finish Victor's second action first, then End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE LINE HOLDS", "The defenders pass while the training formation holds.",
            "Mission data error."),
        scriptedAbility(1, "thaeron", "command", "Command",
            "COMMAND", "Use Thaeron's Command beside the Foreman.",
            "Select Thaeron and use Command."),
        scriptedAbility(1, "foreman", "summon", "Summon",
            "COMMAND THE SUMMON", "Use the adjacent Foreman's Summon while Command is active.",
            "Choose the Foreman; for player 1 its front square at B7 is open.",
            "Blackthorn Lumberjack"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 4, 2,
            "Swing Axes", "GRASK - SWING AXES", "Use Grask's diagonal Capture action, Swing Axes, on the Bull Gator at C5.",
            "Choose the adjacent diagonal enemy, not a long orthogonal target.", "cage_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SEQUENCE THE TEAM", "End Turn after the commanded action and normal activation.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "A ROOTWAY OPENS", "The opposing line passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "sister", 5, 1,
            "Glide", "GROVE SISTER - GLIDE AND TRAIL", "Use Glide diagonally to B6; Trail leaves a Sapling at D8.",
            "Use the Grove Sister's printed diagonal Glide."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PREPARE THE HEAL", "End Turn before treating the wounded ally.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "FRIENDLY RECOVERY", "The opposing formation passes before the friendly heal.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 2, 1,
            "Healing Elixer", "ALCHEMIST - HEALING ELIXER", "Use Healing Elixer on the wounded friendly Mog at B3.",
            "Target Mog with the Alchemist's printed friendly heal.", "mog"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE SECOND PROFILES", "End Turn after the Alchemist heals Mog.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "LUMBERJACK GATHERS", "The opposing line passes; the real Lumberjack's Gather resolves at the next Blackthorn turn start.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "thaeron", 4, 0,
            "Knife Stab", "THAERON - KNIFE STAB",
            "Use Thaeron's printed Knife Stab on the adjacent Bull Gator at A5.",
            "Choose Thaeron's adjacent Knife Stab target.", "knife_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE FOREMAN", "End Turn after Thaeron's ordinary printed action.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE LINE HOLDS", "The defenders pass.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "foreman", 7, 0,
            "Slide", "FOREMAN - SLIDE",
            "Use the Foreman's printed Slide from A7 to A8.",
            "Move the Foreman one square down the open file."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY GRASK'S LONG PROFILE", "End Turn after the Foreman slides.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE DISTANT TARGET HOLDS", "The defenders pass.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 1, 3,
            "Charge", "GRASK - CHARGE",
            "Use Grask's printed long orthogonal Charge on the Bull Gator at D2.",
            "Choose the clear vertical Charge lane and its highlighted target.", "charge_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE PARALYSIS TEST", "End Turn after Grask completes Charge.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE LINE HOLDS", "The defenders pass.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "alchemist", 1, 0,
            "Paralysis Potion", "ALCHEMIST - PARALYSIS POTION",
            "Use Paralysis Potion on the adjacent Bull Gator at A2. It deals zero damage and applies Disable for two owner activations.",
            "Choose the Alchemist's printed zero-damage Paralysis Potion.", "paralysis_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PARALYSIS: FIRST OWNER TURN",
            "End Turn. The Bull Gator misses the first of two owner activations.",
            "End Turn to observe the first Disabled activation."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "FIRST PARALYZED ACTIVATION MISSED",
            "The Disabled Bull Gator cannot act; the opposing side passes.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "worker", 0, 5,
            "Axe Swing", "LUMBERJACK - AXE SWING",
            "Use the dependent Lumberjack form's printed Axe Swing on the adjacent Bull Gator at F1.",
            "Choose the Lumberjack's adjacent orthogonal Axe Swing.", "worker_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PARALYSIS: SECOND OWNER TURN",
            "End Turn. The Bull Gator misses its second owner activation and Disable expires.",
            "End Turn to observe the second Disabled activation."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "SECOND PARALYZED ACTIVATION MISSED",
            "The second Disabled activation is consumed; the opposing side passes.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 1, 7,
            "Heal", "SAPLING - HEAL",
            "Use the Trail dependent Sapling's printed Heal on the wounded friendly Lumberjack at H2.",
            "Choose the Sapling's adjacent ranged friendly Heal.", "sapling_heal_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE SAPLING'S SECOND PROFILE", "End Turn after the Sapling heals.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE LINE HOLDS", "The defenders pass.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sapling", 0, 6,
            "Entangle", "SAPLING - ENTANGLE",
            "Use the Sapling's printed Entangle on the adjacent Bull Gator at G1. It applies two-turn Disable and the Sapling's one-turn cooldown.",
            "Choose the Sapling's zero-damage ranged Entangle.", "sapling_disable_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ENTANGLE: FIRST OWNER TURN",
            "End Turn. The target misses its first owner activation.",
            "End Turn to observe the first Entangle activation."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "SAPLING COOLDOWN CONSUMED",
            "The target passes; on the returning Blackthorn turn, Entangle's one-turn cooldown consumes the Sapling's activation.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ENTANGLE: SECOND OWNER TURN",
            "End Turn. The target misses its second owner activation and Disable expires.",
            "End Turn to observe the full status duration."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "DEPENDENT PROFILE PROOF COMPLETE",
            "The second Entangle activation is consumed. The dependent-form section is complete; the remaining starter roster now takes the field.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ashenfang", 5, 6,
            "Entangling Lunge", "ASHENFANG - ENTANGLING LUNGE",
            "Use Ashenfang's printed diagonal Entangling Lunge on the Bull Gator at G6.",
            "Choose Ashenfang's adjacent diagonal target.", "ashenfang_target", {},
            {panel("Coach - remaining roster",
                "The core and dependent profiles are complete. This final compact section proves the seven starter cards that could otherwise be lost when every optional drill is skipped: Ashenfang, Debt Collector, Lumberjack, Mog, Ambusher, Braun, and Sharpshooter.",
                "cards/Ashenfang.png")}),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ASHENFANG DISABLE BEGINS",
            "End Turn. Entangling Lunge's real positive-damage Disable begins its owner-turn timing.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "ASHENFANG TARGET MISSES ACTIVATION",
            "The Disabled target misses its first owner activation; the opposing side passes.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "collector", 4, 6,
            "Knife Stab", "DEBT COLLECTOR - KNIFE STAB",
            "Use the Debt Collector's printed diagonal Knife Stab on the Bull Gator at G5.",
            "Choose the adjacent diagonal target.", "collector_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ASHENFANG DISABLE COMPLETES",
            "End Turn. Ashenfang's target misses its second owner activation and Disable expires.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "DEBT COLLECTOR TAXES",
            "The opposing side passes; the Debt Collector's real capped Tax resolves at the returning Blackthorn turn start.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "mog", 3, 2,
            "Axe Swing", "MOG - AXE SWING",
            "Use Mog's printed Axe Swing on the adjacent Bull Gator at C4.",
            "Choose Mog's adjacent target.", "mog_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY BRAUN", "End Turn after Mog's printed profile.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LONG LANE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "braun", 4, 4,
            "Charge", "BRAUN - CHARGE",
            "Use Braun's printed long orthogonal Charge on the Bull Gator at E5.",
            "Choose the clear vertical Charge lane.", "braun_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE SHARPSHOOTER", "End Turn after Braun's Charge.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FIRING LANE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "sharpshooter", 6, 5,
            "Advance", "SHARPSHOOTER - ADVANCE",
            "Use the lowered Sharpshooter's printed Advance from F8 to F7.",
            "Move one square up the open file."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY RAISE GUN", "End Turn after Advance.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FIRING LANE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedAbility(1, "sharpshooter", "transform", "Raise Gun",
            "SHARPSHOOTER - RAISE GUN",
            "Use Raise Gun to enter the Sharpshooter's firing state.",
            "Select the Sharpshooter and use Raise Gun.", {}, "Lower Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "RAISED STATE PERSISTS", "End Turn with the gun raised.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FIRING LANE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sharpshooter", 6, 3,
            "Fire", "SHARPSHOOTER - FIRE",
            "Use Fire through the clear row at the Bull Gator on D7.",
            "Choose the clear ranged target.", "sharpshooter_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "FIRING STATE PERSISTS", "End Turn after Fire.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE LINE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedAbility(1, "sharpshooter", "transform", "Lower Gun",
            "SHARPSHOOTER - LOWER GUN",
            "Use Lower Gun to return to the mobile state where Advance is legal.",
            "Select the Sharpshooter and use Lower Gun.", {}, "Raise Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY THE AMBUSHER", "End Turn after lowering the gun.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HIDDEN LANE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ambusher", 3, 5,
            "Stab", "AMBUSHER - VISIBLE STAB",
            "Use the visible Ambusher's printed Stab on the adjacent Bull Gator at F4.",
            "Use Stab before entering the hidden state.", "ambusher_stab_target", {},
            {panel("Coach - visible and hidden profiles",
                "The last card has one visible profile and two hidden profiles. Use Stab while visible, then Dematerialize, pass through the surviving blocker with Sneak Around, and finish with Ambush.",
                "cards/goblinAmbusher.png")}),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY DEMATERIALIZE", "End Turn after visible Stab.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HIDDEN LANE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedAbility(1, "ambusher", "dematerialize", "Dematerialize",
            "AMBUSHER - DEMATERIALIZE",
            "Use Dematerialize. The Ambusher becomes hidden and stops exerting control.",
            "Select the Ambusher and use Dematerialize."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDDEN STATE PERSISTS", "End Turn while the Ambusher is hidden.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE BLOCKER HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "ambusher", 3, 6,
            "Sneak Around", "AMBUSHER - SNEAK AROUND",
            "Use Sneak Around from E4 through the Bull Gator on F4 to empty G4.",
            "Choose the empty square beyond the real blocker."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "READY HIDDEN AMBUSH", "End Turn while the Ambusher remains hidden.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST TARGET HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ambusher", 4, 6,
            "Ambush", "AMBUSHER - HIDDEN AMBUSH",
            "Use the hidden Ambush profile on the adjacent Bull Gator at G5; the Ambusher materializes after the attack.",
            "Choose Ambush, the attacking hidden-state profile.", "collector_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "FULL STARTER ROSTER PROOF COMPLETE",
            "End Turn after every Blackthorn starter card and dependent form has used every printed profile required by this mastery proof.",
            "End Turn to finish."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "ALL-PATH MASTERY SEALED", "The opposing side passes. The full starter roster proof is complete.",
            "Mission data error.")};
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
    return mission;
}

StoryMission blackthornFieldJudgment()
{
    StoryMission mission;
    mission.id = "bt17a_field_judgment";
    mission.title = "Field Judgment";
    mission.sourceChapter = "Required rules exam - open-choice bridge";
    mission.lesson = "OPEN FIELD EXAM - CHOOSE YOUR OWN UNIT AND ACTION";
    mission.objective =
        "Defeat four opposing units while keeping Thaeron alive.";
    mission.hint =
        "Inspect a unit with double-click or focus + I, then choose one of its printed actions. There is no glowing required move; End Turn passes play to the active opponent.";
    mission.briefing = {
        panel("Coach",
            "The glowing script is finished. On this smaller board, inspect any friendly unit with double-click or focus + I, choose one printed action, then decide when to End Turn. If you skipped a recommended drill, replay it from Missions whenever its rule is unfamiliar.",
            "cards/thaeronBaelstone.png"),
        panel("Rules boundary",
            "This required four-against-four field exam uses ordinary cards, turns, damage, and victory rules. It is not a novel event and changes no character's story.",
            "cards/blackthornAlchemist.png"),
        panel("Coach",
            "Win by defeating all four enemies while Thaeron survives. Use formation, healing, Command, Summon, movement, or attacks in any legal order you choose.",
            "cards/blackthornForeman.png")};
    mission.aftermath = {
        panel("Coach",
            "You won a small battle without a prescribed move. A recommended final mastery drill is now available: an ordinary full match with legal decks, concealed Hero placement, hands, Resources, clocks, and victory by Hero elimination or opponent match-clock expiry. You may skip it before its clocks start and continue the story without its mastery stamp.",
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
    return mission;
}

StoryMission blackthornOpenMastery()
{
    StoryMission mission;
    mission.id = "bt17b_open_mastery";
    mission.title = "Blackthorn Match Mastery";
    mission.sourceChapter = "Recommended rules exam - ordinary match before the World Tree reckoning";
    mission.lesson = "OPTIONAL TIMED FULL MATCH - DECKS, HERO PLACEMENT, ECONOMY, CLOCKS, AND VICTORY";
    mission.objective =
        "TIMED ORDINARY MATCH: 2:00 Hero placement, 2:00 turns, 15:00 match clock. Win by Hero elimination or opponent match-clock expiry.";
    mission.hint =
        "Each side gets one non-resetting two-minute Hero-placement deadline. Mouse: hold a Hero card, drag it to an empty highlighted home square, and release. Keyboard: focus a Hero and press Enter, use arrows to focus a legal square, then press Enter again.";
    mission.briefing = {
        panel("Coach",
            "The earlier recommended drills offer introductions to individual Blackthorn tools; the required synthesis and field exam remove increasing amounts of guidance. This recommended capstone uses ordinary match rules and game actions: no scripted board and no prescribed action order.",
            "cards/thaeronBaelstone.png"),
        panel("Rules boundary",
            "This non-canon exam uses the live Blackthorn starter deck against a legal Maggie-and-Joni deck. It is not a claim that these characters fought together in the novel, and its result changes no story event.",
            "cards/Grask.png"),
        panel("Deck contract",
            "An ordinary deck has exactly 20 non-Hero cards plus one to four Heroes whose combined Hero cost is at most 100. Each printed Deck Limit still applies. Both decks here satisfy that contract before the engine shuffles their 20-card draw piles.",
            "cards/blackthornForeman.png"),
        panel("Hero placement",
            "Each side gets one two-minute Hero-placement deadline while the 15-minute match clocks are paused. Mouse: hold a Hero card, drag it to an empty highlighted home square, and release. Keyboard: focus a Hero and press Enter, use arrows to focus a legal square, then press Enter again. Placing one Hero does not reset the deadline. Every footprint must fit; enemy placements stay concealed until both sides finish.",
            "cards/maggieMudroot.png"),
        panel("Opening hand",
            "After placement, Player 1 begins and each side draws four hidden cards. Four is the hand limit, hands never refill automatically, and a paid draw costs 50 Resources. You may discard at most once per turn; that card goes to the bottom of your deck.",
            "cards/blackthornDebtCollector.png"),
        panel("Control and income",
            "A piece controls its occupied squares and influences adjacent empty squares. The side with more influence controls an empty square; a tie preserves its current controller. At the start of your turn, every controlled square pays one Resource before Tax, Gather, or other live passives resolve.",
            "cards/groveSister.png"),
        panel("Clock and increments",
            "Once play begins, each side has a 15-minute match clock and a two-minute turn timer. Every successful card play, piece action, ability, paid draw, discard, or pass resets the turn timer and, for that player's first 30 commands, adds ten seconds to the match clock. A Foresight selection completes its paid draw without adding time again.",
            "cards/Ashenfang.png"),
        panel("Timeouts and leaving",
            "A turn timeout passes play and shortens that player's consecutive later limits to one minute, then 30 seconds. Exhausting a 15-minute match clock loses. Multiplayer Resign also loses immediately; this Story exam keeps Exit and Restart instead.",
            "cards/thaeronBaelstone.png"),
        panel("Victory",
            "Destroy all Heroes who began on the opposing side to win, or win if that side's 15-minute match clock reaches zero. Control cannot steal a Hero, and defeating ordinary Units does not satisfy Hero elimination. You lose if Thaeron and Ashenfang fall or your own match clock expires.",
            "cards/thaeronBaelstone.png"),
        panel("Choose before clocks start",
            "Start Timed Match begins the real two-minute placement deadline immediately. Before starting: mouse placement means hold a Hero card, drag it to an empty highlighted home square, then release. Keyboard placement means focus a Hero, press Enter, use arrows to focus a legal square, then press Enter again. Continue Story Without Mastery records no mastery stamp.",
            "cards/Ashenfang.png")};
    mission.aftermath = {
        panel("Coach",
            "You completed the entire ordinary match loop: legal decks, timed hidden Hero placement, opening hands, control income, open turns, match clocks, and an ordinary victory. The canonical World Tree reckoning now resumes exactly as recorded.",
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
    mission.title = "Surrender, Breach, and Death";
    mission.sourceChapter = "Book One, Chapter 28 - A Natural Order";
    mission.lesson = "STORY SCENE - A MORTAL COURT AND THE TERMS THAT BREAK";
    mission.objective =
        "Follow Victor's surrender, testimony, hidden contingency, breach, and death as five distinct acts.";
    mission.hint =
        "No board action is required. Reed offers custody without immunity; Victor breaks it by burning the register and attacking, and Pavo's lethal defense begins only when Victor turns the hook on him.";
    mission.briefing = victorReckoningPanels();
    mission.objectiveSpec.kind = StoryObjectiveKind::StoryOnly;
    return mission;
}

const std::vector<MissionEntry> MirewatchEntries = {
    {
        "mw02_gilded_hold", "The Gilded Hold", "Chapter 1 - clearing and customs",
        "CHRONICLE",
        "Follow the clearing, the Hold's self-release, and the customs accounting.",
        "The captive's escape is her own action in the story, not a board interaction.",
        "Resistance Smuggler", "Donella of the Marsh", "Erevan the Shadow",
        "Bull Gator", "Blackthorn Debt Collector", "Donella of the Marsh",
        "Pedros is dead. That does not make his knife, his grief, or the person in this cage ours.",
        "The unnamed captive reverses the hidden splinter and walks out by her own hand. At customs, Asta - a fishmonger and smuggling ally - quietly shifts the seized Hold and the family's necessities out of Telos's freight charge, leaving the manifest smaller and the evidence moving separately.",
        "cards/gildedCage.png"
    },
    {
        "mw04_watcher_protects", "What the Watcher Protects", "Chapter 3",
        "RANGED ATTACKS, RANGE TWO, TURN CADENCE",
        "Use printed attacks in a compressed rules reconstruction; the watcher remains autonomous.",
        "Juniper uses Spark from range two; after the turn cycle, Erevan uses adjacent Shadow Blade.",
        "Juniper Flash", "Erevan the Shadow", "Donella of the Marsh",
        "Goblin Sharpshooter", "Goblin Ambusher", "Juniper Flash",
        "I caused Lio's arrest by asking the question they prepared.",
        "Eight refugees escape. The watcher shields Tench and Seli, gives Erevan a black root that pulses beneath two skies, and leaves alive.",
        "characters/juniperFlash.png"
    },
    {
        "mw05_watched_office", "The Office Everyone Is Watching", "Chapter 6",
        "DEMATERIALIZE, PASS-THROUGH, HIDDEN SHOVE",
        "Use a route simulation to learn the printed actions; Hara's decision and the evidence remain story events.",
        "Use Dematerialize, cross occupied squares with Fade Through Shadow, then use Hidden Shove.",
        "Erevan the Shadow", "Mirewatch Informant", "Reed Baelstone",
        "Grask", "Goblin Sharpshooter", "Hara Dole",
        "Warn my mother. I keep the dye here.",
        "The authentic chits show 920 pounds sent, 524 arrived, and 396 extracted. Hara closes the hatch and stays; the prepared confession is left behind.",
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
        "Four Mudfens escape. A recorder fixes six measured positions; Victor retaliates against ferries, medicine, wages, tools, food, Torren, and Missus Vale.",
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
        "Nineteen living prisoners and Garrett's body leave. Garrett dies protecting Little Fen; the prisoners vote 16-3 to drop the ledger and flood the mill.",
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
        "Reed refuses an unsealed bargain, then his own flare exposes Bluewater after the prepared lock closes. Missus Vale dies, the refuge is destroyed, and Vanya removes Reed's route authority.",
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
        "Nima opens her own lock; Reed rejects a buyer's murder schedule; volunteers choose the hidden wall, and the Root Key crossing springs the trap. The survivor count is not known until Chapter 13.",
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
        "mw13_making_credit", "Making Credit", "Chapter 16",
        "JUMP, REVEAL, RHYTHM EVIDENCE, CORROBORATION",
        "Route both hunters and confirm the Feyward road without converting witnesses into allies.",
        "Use Scooter's lines and the Tracker's knight jump. Mounted Tracker reveals hidden enemies and returns unmounted once.",
        "Scooter", "Swamp Tracker", "Erevan the Shadow",
        "Goblin Ambusher", "Goblin Sharpshooter", "Mog",
        "Ask what we chose before you ask what we saw.",
        "Four rules survive into the Society. Mog and Braun are heard separately; Gearjaw answers Scooter's eight-tooth rhythm, and blue trace fire burns the kitchen route into distributed records.",
        "cards/scooter.png"
    },
    {
        "mw14_public_lie", "The Public Lie", "Chapter 17",
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
        "mw16_gossiping_trees", "The Stair Between Gossiping Trees", "Chapter 19",
        "HAZARDOUS ROUTES, TEMPO COST, EXACT PROMISES",
        "Route both pipe guards, crimp the poison line, and cross on Briar's narrow promise.",
        "Stopping the pipe costs a pursuit turn. Promise only to carry the Key to the living World Tree - never to use or obey it.",
        "Scooter", "Birdie the Wise", "Donella of the Marsh",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Birdie the Wise",
        "Leave the poison running and we become the reason Half-Ear's brood dies.",
        "The pipe is crimped, seven travelers cross, and Mangletooth stays with the Hold. Borrowed voices and chosen relationships leave the closure box without a solitary command to obey.",
        "cards/birdieTheWise.png"
    },
    {
        "mw17_factory_heaven", "A Factory in Heaven", "Chapter 20",
        "PIVOT, TELEPORT, PUSH, RELATIONSHIP RECOVERY",
        "Route both factory guards, free the moth sacks, and approach Vesper without force.",
        "No attack breaks the false kitchen; Erevan must call Donella and admit he wants another minute.",
        "Erevan the Shadow", "Donella of the Marsh", "Scooter",
        "Blackthorn Foreman", "Goblin Sharpshooter", "Erevan the Shadow",
        "I know this life is false. I still want another minute inside it.",
        "The party frees the moths, destroys the harvester together, and plays Elliot's tune. Vesper leaves the false kitchen; his choice to travel with them follows in Chapter 21.",
        "cards/erevanTheShadow.png"
    },
    {
        "mw18_allies_dishonestly", "Allies Acquired Dishonestly", "Chapter 22",
        "NONLETHAL GOALS, TEMPORARY TERMS, ESCORT",
        "Route both Company toll guards and cross without defeating the Seelie captain.",
        "Pavo, Nettle, and Zippy each choose bounded terms. A borrowed badge opens a gate; it buys no person.",
        "Reed Baelstone", "Pavo Quickstep", "Nettle Starbright",
        "Blackthorn Debt Collector", "Goblin Ambusher", "Nettle Starbright",
        "A royal badge can make an unlawful gate look patient.",
        "All detainees cross. Pavo, Nettle, and Zippy agree only to first grove sight and one attempt - no obedience.",
        "cards/nettleStarbright.png"
    },
    {
        "mw19_road_keeps_one", "The Road That Keeps One", "Chapter 23",
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
        "mw20_monster_rules", "The Monster Has Rules", "Chapter 24",
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
        "mw21_four_losses", "Four Ways to Lose", "Chapter 25",
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
        "mw22_clean_shot", "The Clean Shot", "Chapter 26",
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
        "mw23_choice_not_cure", "A Choice, Not a Cure", "Chapter 27",
        "TRANSFORM, OPT-IN NETWORK, RESPECTED REFUSAL",
        "Route both Company interferers, build Sylvara's revocable network, and leave every no outside it.",
        "Ask once. Nettle and several prisoners say no; Scooter cannot understand the bargain, so absence of assent is not a yes. Being unbound lets Nettle cut Lash's wire.",
        "Donella of the Marsh", "Erevan the Shadow", "Reed Baelstone",
        "Blackthorn Alchemist", "Blackthorn Foreman", "Sylvara",
        "Unequal. Revocable. Enough roads that no one body becomes the bridge.",
        "Every refusal holds. Sylvara inserts the Root Key, Nettle grounds the wire, and Ashenfang returns to Sylvara - restored, not cured.",
        "cards/donellaOfTheMarsh.png"
    },
    {
        "mw24_agent_not_heir", "Agent, Not Heir", "Chapter 28",
        "TERRAIN COMBO, SURRENDER, HIDDEN CONTINGENCY",
        "Route both commanders, collapse the cages, close the Baelstone gate collectively, and preserve the north fragment.",
        "Reed's blood identifies the door but owns no work. Offer Victor breath and a mortal venue - never immunity.",
        "Reed Baelstone", "Pavo Quickstep", "Nettle Starbright",
        "Victor Greyshard", "Grask", "Reed Baelstone",
        "My blood identifies the door. It does not own their work. Agent, not heir.",
        "Prisoners collapse cages on Grask. Victor surrenders, triggers the paired molar charge that burns the register, tears Nettle's hook free, and attacks her, then Pavo. Pavo's blade enters beneath Victor's raised arm; Victor dies after one last reach toward Nettle.",
        "cards/reedBaelstone.png"
    },
    {
        "mw25_deed_own_hand", "The Deed in His Own Hand", "Chapter 29",
        "CHRONICLE",
        "Bring Thaeron's conditional granary deed into witnessed Society custody.",
        "Reed does not settle alone; the deed and every objection go to a public vote.",
        "Reed Baelstone", "Mara Mudfen", "Adie Pike",
        "Thaeron Baelstone", "Blackthorn", "Thaeron Baelstone",
        "Six hundred sacks arrive behind a living-Baelstone condition, written throughout in Thaeron's own hand.",
        "Reed calls Mara and Adie instead of deciding alone. The unsigned inheritance stays in the Society lockbox; Thaeron admits his care and his refusal to repent, then leaves to appeal the wording.",
        "cards/thaeronBaelstone.png"
    }
};

const std::vector<MissionEntry> BlackthornEntries = {
    {
        "bt02_customs_bell", "The Town Under the Company",
        "Chapters 1-2 - patrol rules reconstruction",
        "TRANSFORM, LINE OF SIGHT, TAX, OBSERVATION",
        "Use a patrol drill to learn three printed cards after the customs and Mudfen dock reports.",
        "Use Raise Gun, Fire through a clear line, and let the Collector's real Tax trigger at turn start.",
        "Braun Stonefist", "Goblin Sharpshooter", "Blackthorn Debt Collector",
        "Reed Baelstone", "Donella of the Marsh", "Blackthorn Debt Collector",
        "Customs records a young injured traveler called Reed and the stolen Hold; a runner carries the description to Blackthorn. At the Mudfen dock, collectors enforce a false seizure until Reed makes their accounting public.",
        "The workboat stays with the Mudfens. This board is a rules reconstruction, not a claim that Braun, the Sharpshooter, and Collector fought at customs.",
        "cards/blackthornDebtCollector.png"
    },
    {
        "bt04_terms_conditions", "Hidden Transit Drill",
        "Chapter 3 - non-canon Ambusher rules drill",
        "DEMATERIALIZE, PASS-THROUGH, COLLISION REVEAL",
        "Use two Ambushers' printed actions to cross real blockers and reveal a hidden destination collision.",
        "Use Dematerialize and Sneak Around through real Veterans; landing on hidden Erevan reveals him and makes him Disabled for his next owner-turn activation.",
        "Goblin Ambusher", "Blackthorn Alchemist", "Blackthorn Debt Collector",
        "Erevan the Shadow", "Juniper Flash", "Blackthorn Observer",
        "The story's observers are unnamed. This separate deck drill teaches the Ambusher without assigning those actions to them.",
        "In canon, Erevan's prepared question draws Lio into arrest, Juniper gets eight refugees out, and the unidentified watcher chooses the children and escapes alive.",
        "cards/goblinAmbusher.png"
    },
    {
        "bt05_freight_office", "The Freight Office",
        "Chapter 6 - operational rules reconstruction",
        "CAPTURE STAGING, DAMAGE, LONG ATTACK",
        "Control 14 squares to mark Hara's route and force the intruders through the predicted hatch.",
        "Swing Axes demonstrates Capture staging; Mog attacks a survivor; Charge defeats a distant real unit.",
        "Grask", "Mog", "Goblin Sharpshooter",
        "Reed Baelstone", "Donella of the Marsh", "Victor Greyshard",
        "This card drill reconstructs Company capabilities; Grask and Mog do not literally fight this represented team in the office.",
        "Authentic chits leave while the planted confession stays. Hara is marked and chooses to close the hatch; the Guardian restrains Victor and throws Grask into Mog.",
        "cards/Grask.png"
    },
    {
        "bt06_receipt_book", "The Receipt Book", "Chapter 7",
        "CONTROL DENIAL, HIDDEN OBJECTIVE, WITHDRAWAL",
        "Follow the recorder's six measured positions and the seven-way spread of Victor's retaliation.",
        "The wagon is bait, not a kill objective. Use Mog and Collectors to narrow the road while helpers reveal their methods.",
        "Grask", "Mog", "Blackthorn Debt Collector",
        "Birdie the Wise", "Scooter", "Grask",
        "The wagon is bait. Let every helper show the hand they use.",
        "The wagon escapes. Six measured positions reach Victor; the ribbon is cut into seven strips, and retaliation makes Torren disappear.",
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
        "Garrett dies protecting Little Fen. Sixteen prisoners vote to drop the ledger and three vote no; twenty cross, nineteen alive.",
        "cards/blackthornAlchemist.png"
    },
    {
        "bt08_north_lock", "The North Lock", "Chapter 10",
        "COMMAND, ADJACENT READINESS, FLOOD CLOCK, ESCAPE",
        "Follow Reed into the north lock and account for the cost of his unapproved route.",
        "Thaeron's care and coercion coexist. Reed refuses the unsealed bargain; no invented extraction objective is attached.",
        "Thaeron Baelstone", "Blackthorn Foreman", "Blackthorn Debt Collector",
        "Reed Baelstone", "Vanya Bluewater", "Thaeron Baelstone",
        "The care was real. So is the necessity I built around it.",
        "After the prepared lock closes, Reed's own flare exposes Bluewater. Mangletooth saves him; Missus Vale dies, the refuge is destroyed, and Vanya removes Reed's route authority.",
        "cards/thaeronBaelstone.png"
    },
    {
        "bt09_published_mystery", "The Published Mystery", "Chapters 11-13",
        "CAPTURE, MULTI-ROUTE SETUP, PREDICTED APPETITES",
        "Follow four manufactured roads into the Vault and preserve the complete rescue-and-loss account.",
        "Grask Captures a gate piece, not a protagonist. Lash's mechanism needs the Key carried by choice and witnesses alive.",
        "Grask", "Blackthorn Foreman", "Goblin Ambusher",
        "Donella of the Marsh", "Reed Baelstone", "Remy",
        "Four roads look safer than one invitation.",
        "The Root Key crossing triggers the prepared trap. Sixteen survive, Nima, Cal, and Dessa remain missing, Juniper dies, Reed loses use of his right hand, and the buyer register floats out to support Chapter 14.",
        "cards/Grask.png"
    },
    {
        "bt10_stolen_road", "The Bill Comes Due", "Chapters 15-16",
        "CHRONICLE",
        "Follow the clinic vote, Victor's bill, Society rules, and the route that burns.",
        "Mog and Braun are heard separately; Gearjaw's relationship with Scooter is evidence, not ownership.",
        "Goblin Ambusher", "Braun Stonefist", "Mog",
        "Reed Baelstone", "Swamp Tracker", "Briar",
        "Blackthorn prices Victor for the Guardian, mill, and ledger. He accepts the bill because non-exception is the natural order he believes in.",
        "Forty-three people vote, four Society rules survive, and Mog and Braun give bounded evidence. Gearjaw's blue trace burns a kitchen route and forces the Society's records onto separate ferries.",
        "cards/goblinAmbusher.png"
    },
    {
        "bt11_public_lie", "The Public Lie", "Chapter 17",
        "HERO PROTECTION, COMMAND, RESOURCES, NONCOMBAT PRESSURE",
        "Follow the hearing through selective truths, Reed's alias, and Vanya's chosen surrender.",
        "Spend on food and medicine, choose one selective truth, reject outright lies, and keep violence from destroying trust.",
        "Thaeron Baelstone", "Blackthorn Debt Collector", "Braun Stonefist",
        "Reed Baelstone", "Vanya Bluewater", "Thaeron Baelstone",
        "A public truth needs a bowl, a witness, and one omitted name.",
        "Vanya surrenders herself, the fishbone key, and a dead route. Reed publishes Thaeron's murder proof and inheritance offer but has not rejected the inheritance; he loses advocate authority, and Thaeron remains for third bell.",
        "cards/thaeronBaelstone.png"
    },
    {
        "bt12_break_charter", "Title Follows Burden", "Chapter 18",
        "SUMMON, TRANSFORM, CAPTURE, CHANGING ALLEGIANCE",
        "Follow the ruling, Victor's staged bombing, the evacuation, and the Feyward abduction.",
        "The magistrate voids Blackthorn's charter before Victor attacks. The children, ruling, testimony, and surrender are story events.",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Goblin Sharpshooter",
        "Reed Baelstone", "Birdie the Wise", "Braun Stonefist",
        "That is not a target order. It is an excuse written as one.",
        "Braun refuses because Victor mined the children and Grask kills him. Mog surrenders and testifies; Victor abducts three harvest cases and a measurement crew toward Feyward. The register does not burn here.",
        "cards/braunStonefist.png"
    },
    {
        "bt13_feyward_transit", "False Factory Heaven", "Chapter 20",
        "STORY SCENE",
        "Follow the travelers through the moth harvester's false rooms and Vesper's choice to leave.",
        "No board action is required; the factory's traps and each captive's choice remain story events.",
        "Grove Sister", "Blackthorn Foreman", "Blackthorn Lumberjack",
        "Reed Baelstone", "Donella of the Marsh", "Foreman",
        "Briar's bounded stair reaches the moth-harvester factory after the travelers sacrifice their clean pursuit road.",
        "The travelers destroy the moth harvester. Erevan rejects the false kitchen, and Vesper freely leaves the tower; no memory or machine becomes a board objective.",
        "cards/groveSister.png"
    },
    {
        "bt14_move_boundary", "Allies and the Moving Boundary", "Chapters 22-23",
        "LARGE FOOTPRINTS, BLOCKERS, PUSH COLLISION, EXTRACTION",
        "Follow the toll rescue, bounded alliances, delivered writ, and refugee road.",
        "Pavo, Nettle, Zippy, the refugees, and Rowan choose their own terms; no boundary correction is a player interaction.",
        "Blackthorn Foreman", "Blackthorn Lumberjack", "Grove Sister",
        "Pavo Quickstep", "Nettle Starbright", "Nettle Starbright",
        "A royal badge may open a gate, but it buys no person.",
        "All detainees cross; the municipal writ is delivered, and Rowan freely stays with the refugees and Mangletooth's bone.",
        "cards/nettleStarbright.png"
    },
    {
        "bt15_monster_rules", "Ashenfang: Rules Demonstration",
        "Chapter 24 - isolated card demonstration",
        "ASHENFANG, DIAGONAL RANGE, DISABLE",
        "Use Entangling Lunge on a real opposing unit and observe the status duration.",
        "Ashenfang moves and attacks diagonally one or two squares; Entangling Lunge Disables for two owner turns.",
        "Ashenfang", "Marshland Veteran", "Rules demonstration",
        "Company coercion", "Neutral timing marker", "Narrator",
        "The monster follows the hurt hidden under her tack.",
        "Ashenfang is Sylvara under coerced pain and Company marks, never a willing Blackthorn hero. Fen Oake knowingly tests one crossing and dies when a hidden Company tack changes the case; the unmarked grass-woman and Sylvara survive.",
        "cards/Ashenfang.png"
    },
    {
        "bt16_clean_shot", "Four Losses and a Clean Shot", "Chapters 25-27",
        "CHRONICLE",
        "Follow four simultaneous losses through Gearjaw's choice, Birdie's shot, and Sylvara's answer.",
        "Consent, cables, marks, and the shared burden remain narrative events, never game-rule targets.",
        "Grask", "Goblin Sharpshooter", "Blackthorn Alchemist",
        "Birdie the Wise", "Nettle Starbright", "Gearjaw",
        "Gearjaw destroys its own learning core to save Scooter; Mog protects a former captive and Grask stabs her.",
        "Birdie cuts the real cable and loses right-arm function. Every refusal holds; Sylvara restores herself, not cured, and Fizzlewick escapes with measurement scraps.",
        "cards/Grask.png"
    },
    {
        "bt18_deed_own_hand", "The Deed in His Own Hand", "Chapter 29",
        "CHRONICLE",
        "Follow Thaeron's personal grain offer into public, witnessed custody.",
        "The condition and every objection are recorded; no private acceptance is treated as victory.",
        "Thaeron Baelstone", "Reed Baelstone", "Mara Mudfen",
        "Mirewatch", "Society", "Thaeron Baelstone",
        "Thaeron offers six hundred sacks under a living-Baelstone stewardship condition and anticipates the step Reed's injured leg can manage.",
        "Reed summons Mara and Adie rather than settling alone. The unsigned inheritance stays locked; Thaeron is glad Reed lives and still would repeat every coercive act.",
        "cards/thaeronBaelstone.png"
    }
};

StoryMission mirewatchWatcher()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[1]);
    mission.objective =
        "Defeat both Company guards with Juniper's ranged Spark and Erevan's close Shadow Blade.";
    mission.briefing.insert(mission.briefing.begin(), panel("Narrator",
        "Lio, Juniper's wounded-mason contact, is arrested after Erevan asks Blackthorn's prepared question. At the boatyard, his children Tench and Seli face collapsing timber while Company observers test what a horned watcher protects.",
        "characters/juniperFlash.png"));
    mission.briefing.push_back(panel("Rules boundary",
        "The printed attacks below are a compressed rules reconstruction. In Chapter 3, the watcher chooses the children and escapes; no character attacks or owns it.",
        "characters/juniperFlash.png"));
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
        scriptedPieceAction(StoryActionKind::Attack, 1, "juniper", 3, 3,
            "Spark", "JUNIPER'S RANGE", "Fire Spark two squares along the row into the wounded Lumberjack at D4. Its red badge shows 1 Health, so Spark's one damage destroys it.",
            "Juniper is stationary while attacking; choose the highlighted guard.", "far_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE ACTION, THEN PASS", "End Turn so the watcher can choose.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The Company guards hold; play returns to Mirewatch.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "erevan", 5, 2,
            "Shadow Blade", "EREVAN AT CLOSE RANGE", "Use Shadow Blade on the Debt Collector beside Erevan at C6.",
            "Use Erevan's attack on the adjacent guard.", "near_guard", {},
            {panel("Narrator",
                "Beyond the fight, the watcher turns its body toward Tench and Seli instead of protecting itself. This choice is story, not a board action.",
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
    StoryMission mission = playableFromEntry(MirewatchEntries[2]);
    mission.objective =
        "Cross two real Lumberjack blockers while hidden, then use Hidden Shove on the Debt Collector; Hara's choice remains in the story.";
    mission.briefing.insert(mission.briefing.begin(), {
        panel("Narrator",
            "Hara Dole is the freight-office weighing clerk. The Guardian changes its seal test, then marks her wrist with black tracking dye. Erevan rejects a planted confession and finds four authentic freight chits.",
            "cards/mirewatchInformant.png"),
        panel("Narrator",
            "The chits prove 920 living pounds left, 524 arrived, and 396 were removed in transit. The mark can expose shelters, so Hara orders them to warn her mother and shuts the canal hatch behind them, giving up her place in their boat.",
            "cards/mirewatchInformant.png")});
    mission.briefing.push_back(panel("Rules boundary",
        "This route simulation teaches Erevan's printed hidden actions. Hara's warning, evidence, and decision to close the hatch are fixed story events, not player interactions.",
        "cards/erevanTheShadow.png"));
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
        scriptedAbility(1, "erevan", "dematerialize", "Dematerialize",
            "DEMATERIALIZE EREVAN", "Select Erevan and use the printed Dematerialize ability.",
            "Erevan must dematerialize before crossing the occupied lane."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDDEN PIECES ADD NO CONTROL", "End Turn. While hidden, Erevan adds no adjacent control influence; existing square ownership remains when influence is tied.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE OFFICE WATCHES THE WRONG DOOR", "The guards hold their rehearsed sight lines.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "erevan", 3, 4,
            "Fade Through Shadow", "PASS THROUGH THE SCREEN", "Move Erevan from B4 to E4, through both occupied squares.",
            "Only Erevan's hidden Fade Through Shadow can pass through those blockers."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "STAY UNSEEN", "End Turn while Erevan remains hidden beside the Debt Collector.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "erevan", 2, 4,
            "Hidden Shove", "HIDDEN SHOVE", "Use Hidden Shove on the Debt Collector at E3.",
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
        "Use Foresight, clear the guarded route with every printed action profile, then survive a Bull Gator counterattack. The wagon opens only in the aftermath panel.";
    mission.briefing.insert(mission.briefing.begin(), {
        panel("Narrator",
            "Mara Mudfen runs the bakery with Pell and their children, Ollo and Nessa. Blackthorn comes to seize all four under inherited arrears, flight risk, interference with pledged property, and supplemental processing.",
            "cards/birdieTheWise.png"),
        panel("Narrator",
            "Ollo and Nessa move for the oven hatch while the crew forces Mara and Pell into a wagon with six hidden eyes. Birdie's rescue saves the family, but the machine records every helper for Victor's retaliation.",
            "cards/birdieTheWise.png")});
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
    mission.aftermath.push_back(panel("Narrator",
        "With the route clear, the Mudfen wagon opens. The trained gator tore the mount away, but the scout rose on foot and kept the road.",
        "cards/swampTracker.png"));
    mission.script = {
        scripted(StoryActionKind::DrawCard, 1, {}, -1, -1,
            "BIRDIE - FORESIGHT",
            "Draw a card. Birdie's Foresight reveals the top two cards before the 50-Resource draw resolves.",
            "Use Draw to begin Birdie's Foresight choice."),
        scriptedCard(StoryActionKind::ChooseForesight, 1, "Bog Spearman", -1, -1,
            "CHOOSE WITH FORESIGHT",
            "Choose Bog Spearman. Mirewatch Informant returns to the bottom of the draw pile.",
            "Choose Bog Spearman from the two revealed cards."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "birdie", 3, 4,
            "Longbow Shot", "BIRDIE - LONGBOW SHOT", "Shoot the Debt Collector three squares along the row.",
            "Use Birdie's ranged attack on the highlighted target at E4.", "notice_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COOLDOWN BEGINS", "End Turn. Birdie's bow will miss her next activation.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "scooter", 7, 3,
            "River Dash", "SCOOTER - RIVER DASH", "Use River Dash diagonally from B6 to empty D8.",
            "River Dash is move-only; choose highlighted D8."),
        scriptedPieceAction(StoryActionKind::Move, 1, "scooter", 6, 4,
            "River Dash", "SCOOTER - REQUIRED REPEAT", "Continue the same River Dash from D8 to empty E7.",
            "Finish Scooter's printed repeat with the same action."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE BRIDGE PIN MOVES", "End Turn after both Dash actions.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "veteran", 6, 3,
            "Advance", "VETERAN - ADVANCE", "Strike the wounded Lumberjack two squares along the row.",
            "Use the Veteran's printed Advance. The target survives, so the Veteran stops short.",
            "durable_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SURVIVING DEFENDER", "End Turn with the Veteran short of the surviving screen.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The guards hold position; play returns to Mirewatch.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "smuggler", 2, 2,
            "Swashbuckle Blade", "SMUGGLER - FIRST BLADE", "Use Swashbuckle Blade diagonally from B2 into the Debt Collector at C3.",
            "Use Swashbuckle Blade on the first guard.", "route_guard_one"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "smuggler", 1, 3,
            "Swashbuckle Blade", "SMUGGLER - SECOND BLADE", "Repeat diagonally from C3 into the Debt Collector at D2.",
            "The Smuggler must finish the repeated action.", "route_guard_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE REAR OPENING CLEARS", "End Turn.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The remaining guards hold; play returns to Mirewatch.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "spearman", 2, 3,
            "Hooked Spear", "SPEARMAN - HOOKED SPEAR",
            "Use ranged Hooked Spear diagonally from B5 into the Debt Collector at D3. The one-Health guard is destroyed, so the Spearman remains at B5 and Pull has no surviving target to move.",
            "Use the Bog Spearman's printed range-two Hooked Spear.", "spear_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE LOCK LEFT", "End Turn for the mounted scout's jump.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The final guard and gator hold; play returns to Mirewatch.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "tracker", 6, 3,
            "Frogback Leap", "TRACKER - FROGBACK LEAP", "Jump from B8 into the weakened Lumberjack at D7.",
            "The Tracker uses its printed Frogback Leap and ignores intervening squares.", "durable_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "BRACE FOR THE BITE", "End Turn so the surviving gator can counterattack.",
            "End Turn after the Tracker's Leap."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "rebirth_gator", 6, 3,
            "Bite", "BITE TRIGGERS REBIRTH", "The Bull Gator uses its ordinary Bite on the wounded mounted Tracker.",
            "Mission data error.", "tracker"),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "UNMOUNTED TRACKER READIES",
            "The gator ends its turn. The real unmounted replacement now readies for Mirewatch.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "tracker", 5, 3,
            "Tracker's Spear", "UNMOUNTED TRACKER - SPEAR",
            "Use Tracker's Spear from D7 on the adjacent Bull Gator at D6. The surviving target keeps the Tracker on D7.",
            "Use the unmounted Tracker's printed spear on the highlighted gator.", "rebirth_gator"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE SPEAR LINE", "End Turn after the unmounted Tracker acts.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The remaining targets hold.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "scooter", 4, 4,
            "River Rush", "SCOOTER - RIVER RUSH",
            "Use River Rush two squares up the E-file from E7 to defeat the wounded Debt Collector at E5.",
            "Use Scooter's printed orthogonal River Rush on the highlighted target.", "rush_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE RIVER LINE", "End Turn after River Rush.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The Bull Gator holds.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "smuggler", 1, 4,
            "Step", "SMUGGLER - STEP",
            "Use Step from D2 to adjacent empty E2.",
            "Use the Smuggler's printed one-square Step on highlighted E2."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE REAR LINE", "End Turn after the Smuggler steps.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "COMPANY TURN ENDS", "The Bull Gator holds.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "birdie", 0, 4,
            "Scout", "BIRDIE - SCOUT",
            "Use Scout diagonally from B4 to empty E1.",
            "Use Birdie's printed long diagonal Scout on highlighted E1.")
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
    return mission;
}

StoryMission mirewatchAuction()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[7]);
    mission.objective =
        "Use Vanya's printed Blade Dance twice to defeat the two auction guards; Nima's self-release remains a story event.";
    mission.briefing.insert(mission.briefing.begin(), {
        panel("Narrator",
            "At the Gilded Vault's shrouded auction, masked bidders price eleven captive lots, including ten-year-old Nima Salt. Every captive refusal is relabeled as a defect.",
            "cards/vanyaBluewater.png"),
        panel("Narrator",
            "The rescue rule is set before entry: free every breathing captive; take records only when already on an exit path. At Juniper's abort, leave by the nearest route with whoever is beside you.",
            "cards/vanyaBluewater.png"),
        panel("Narrator",
            "The artifacts never share custody: Joni locks the Mirror of Nine Lives in her cart under her own key for one witnessed loan; if the hidden Root Key is found, Donella carries it separately. Neither is a player interaction here.",
            "cards/joniPumpernickel.png"),
        panel("Narrator",
            "Nima has sharpened a fish rib inside her cage. When Rowan's cage authority releases the outer pressure, she opens her own lock; the player does not open it for her.",
            "cards/vanyaBluewater.png")});
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
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 4, 2,
            "Blade Dance", "VANYA - BLADE DANCE", "Use Vanya's Blade Dance on C5 to open Nima's route. It attacks, then moves her diagonally into the target's square. Repeat 1 locks her to one more Blade Dance - or Pass - before another piece can act.",
            "Use Vanya's diagonal Blade Dance on the highlighted enemy.", "first_guard"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vanya", 3, 3,
            "Blade Dance", "FINISH THE DANCE", "Blade Dance is repeat-locked: finish the same action on the second Debt Collector at D4 before choosing anyone else. Nima opens her own lock in the following story panel.",
            "Finish Blade Dance before choosing another piece.", "second_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE DANCE ENDS", "End Turn after both required Blade Dance uses.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE AUCTION LINE HOLDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "vanya", 2, 3,
            "Sidestep", "VANYA - SIDESTEP",
            "Use Sidestep one square up from D4 to empty D3.",
            "Use Vanya's printed orthogonal Sidestep on highlighted D3.")
    };
    mission.masteryCards = {"Vanya Bluewater"};
    mission.masteryRules = {
        "repeat-lock", "attacking-movement", "orthogonal-movement"};
    return mission;
}

StoryMission mirewatchVaultCost()
{
    StoryMission mission = playableFromEntry(MirewatchEntries[8]);
    mission.lesson = "SPRINT, POSITIONING, AUTOMATIC INTERCEPT, PERMANENT COST";
    mission.objective =
        "Clear two ordinary escape lanes, position Juniper beside the route runner, then observe her automatic Intercept when Blackthorn attacks.";
    mission.hint =
        "Intercept is not a button. On an eligible damaging enemy piece attack against an adjacent ally, Juniper swaps places with that ally and takes the damage herself.";
    mission.briefing = {
        panel("Narrator",
            "The Vault reverses its currents and seals five rescue lanes. Donella can reach the children or Nima, Cal, and Dessa; the Root Key closes each road behind her.",
            MirewatchEntries[8].art),
        panel("Dessa",
            "You could bring one of us and leave the others. Do not. Get the children off the gallery; we will hold the rope as long as we can.",
            MirewatchEntries[8].art),
        panel("Reed Baelstone",
            "The north gate needs the Baelstone seal hand. I put my right hand in the bronze moon and keep it there until the last body clears.",
            "cards/reedBaelstone.png"),
        panel("Juniper Flash",
            "Mae, you carried eight children through every drill I gave you. Follow my light, take them to the gate, and do not turn back for me.",
            "characters/juniperFlash.png"),
        panel("Rules boundary",
            "This printed-card reconstruction teaches Juniper's Intercept; it does not claim the Knife Stab below was the literal blow that killed her. The chronicle preserves that she gives Mae the flare and dies keeping the route open.",
            "characters/juniperFlash.png")};
    mission.aftermath = {
        panel("Coach - INTERCEPT TRIGGER",
            "Intercept is automatic, not a button. It triggers when an enemy piece's attack would deal positive damage to an adjacent ally. Juniper swaps into that ally's place and takes the attack; her one remaining Health made this hit lethal.",
            "characters/juniperFlash.png"),
        panel("Coach - FOOTPRINT SWAP",
            "A footprint means every board square a piece occupies. Juniper and the protected ally exchange their complete footprints only when both fit legally. If either placement is blocked or outside the board, no swap occurs and the original target takes the hit. Bodyguard resolves before Intercept.",
            "characters/juniperFlash.png"),
        panel("Coach - RESET AND STATUS",
            "A surviving Interceptor can trigger once, then regains Intercept at the start of its side's next turn. Acting or a Disable, Sleep, or Growth marker does not cancel this automatic defense.",
            "characters/juniperFlash.png"),
        panel("Coach - INTERCEPT EXCLUSIONS",
            "Spells and zero-damage attacks do not trigger Intercept. Neither do attacks aimed directly at a Bodyguard or another Intercept unit. Those limits are printed game rules, not exceptions created for this scene.",
            "characters/juniperFlash.png"),
        panel("Vanya Bluewater",
            "Sixteen out. Nima, Cal, and Dessa missing. Juniper dead after giving Mae the flare. Reed's right hand is warm but without movement. Record every loss separately.",
            "cards/vanyaBluewater.png"),
        panel("Chronicle",
            "The rescue succeeds without becoming a perfect rescue: sixteen of nineteen escape, three remain missing, and neither Juniper's death nor Reed's injury is erased by mission completion.",
            MirewatchEntries[8].art)};
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
            "Use Birdie's attack on the highlighted range-three target.", "north_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE NORTH LANE", "End Turn after Birdie acts.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "BLACKTHORN WAITS", "The remaining guards hold while the second route opens.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "reed", 6, 4,
            "Bow", "OPEN THE SOUTH LANE",
            "Use Reed's Bow from B7 through C7 and D7 to the Debt Collector at E7.",
            "Use Reed's attack on the highlighted range-three target.", "south_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE SOUTH LANE", "End Turn after Reed acts.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LAST GUARD HOLDS", "The Debt Collector watches the route runner at C5.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "juniper", 4, 1,
            "Sprint", "JUNIPER - SPRINT INTO POSITION",
            "Use Sprint from B4 to B5. This ordinary move puts Juniper adjacent to the route runner at C5.",
            "Move Juniper one square down to highlighted B5."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COMMIT THE ROUTES",
            "End Turn. Blackthorn will attack the runner beside Juniper.",
            "End Turn to observe the enemy piece attack."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "intercept_attacker", 4, 2,
            "Knife Stab", "INTERCEPT TRIGGERS AUTOMATICALLY",
            "The Debt Collector uses Knife Stab on the route runner at C5. Juniper automatically swaps into C5 and takes the one damage.",
            "Mission data error.", "runner")};
    mission.masteryCards = {"Reed Baelstone", "Juniper Flash"};
    mission.masteryRules = {
        "orthogonal-movement", "intercept-automatic", "intercept-adjacent-ally",
        "intercept-position-swap", "intercept-receives-damage"};
    return mission;
}

StoryMission mirewatchRevealReconstruction()
{
    StoryMission mission = chronicleFromEntry(MirewatchEntries[9]);
    mission.title = "Making Credit - Reveal Reconstruction";
    mission.sourceChapter =
        "Book One, Chapter 16 / Optional non-canon rules reconstruction";
    mission.lesson =
        "OPTIONAL GUIDED RECONSTRUCTION - DEMATERIALIZE, KNIGHT JUMP, AND PASSIVE REVEAL";
    mission.objective =
        "OPTIONAL NON-CANON RULES RECONSTRUCTION. Let a real Ambusher dematerialize, move the Swamp Tracker adjacent, then end the Tracker's turn to trigger Reveal.";
    mission.hint =
        "Reveal is passive: it checks adjacent enemy Dematerialize units only when the Revealing unit's owner ends their turn.";
    mission.briefing.push_back(panel("Rules boundary",
        "The meeting, separate witness accounts, Gearjaw's rhythm, and the blue trace fire above are canonical. This small board is an optional reconstruction using real cards; it does not claim a Goblin Ambusher entered the Chapter 16 meeting.",
        "cards/swampTracker_mounted.png"));
    mission.aftermath = {
        panel("Coach",
            "The Ambusher used its real Dematerialize ability. The Tracker then made a legal Frogback Leap, and passive Reveal materialized the adjacent enemy at the end of the Tracker owner's turn. Reveal dealt no damage and applied no Disable.",
            "cards/swampTracker_mounted.png"),
        panel("Chronicle", MirewatchEntries[9].result, MirewatchEntries[9].art)};
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
            "The opposing Goblin Ambusher uses its real Dematerialize ability and becomes hidden.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HIDDEN TURN ENDS",
            "The Ambusher ends its ordinary turn while still dematerialized.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "tracker", 2, 2,
            "Frogback Leap", "TRACKER - FROGBACK LEAP",
            "Use Frogback Leap from B5 to empty C3. This legal knight jump ends adjacent to the unseen Ambusher at D3 without entering its square.",
            "Move the Swamp Tracker to highlighted C3."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "END TURN TO TRIGGER REVEAL",
            "End Turn. The Tracker's passive Reveal now checks adjacent enemies and materializes the dematerialized Ambusher.",
            "Use End Turn while the Tracker is adjacent to the hidden enemy.")};
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
        "Use Victor's real Relentless chain, open Braun's lane, change the Sharpshooter's state, fire through a clear line, and observe Tax.";
    mission.briefing.insert(mission.briefing.begin(), panel("Victor Greyshard",
        "Customs records an injured traveler called Reed and the stolen Hold, then sends both descriptions to Blackthorn. That is how a name becomes a road and everyone without one becomes freight. Hold this patrol lane to what its reports can prove.",
        "cards/victorGreyshard.png"));
    mission.briefing.push_back(panel("Rules boundary",
        "This patrol drill teaches printed Blackthorn cards after the customs and Mudfen reports. Victor defeats two one-Health practice opponents, takes the optional Relentless action, then declines another by ending the turn. This exact fight did not occur.",
        "cards/blackthornDebtCollector.png"));
    mission.briefing.push_back(panel("Chronicle",
        BlackthornEntries[0].result, BlackthornEntries[0].art));
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
            "Axe Dance", "VICTOR - FIRST AXE DANCE",
            "Use Axe Dance on the one-Health practice opponent at B1. Defeating it triggers Victor's printed Relentless option.",
            "Use Victor's Axe Dance on the highlighted adjacent target.", "relentless_target_one"),
        scriptedPieceAction(StoryActionKind::Attack, 1, "victor", 1, 1,
            "Axe Dance", "VICTOR - OPTIONAL RELENTLESS ACTION",
            "Take the optional Relentless action: use Axe Dance again on the one-Health opponent at B2.",
            "Use Victor again while Relentless is pending; ordinary play also allows End Turn.", "relentless_target_two"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DECLINE THE NEXT RELENTLESS ACTION",
            "The second defeat offers Relentless again. End Turn to decline that optional extra action.",
            "End Turn is the legal way to decline the pending Relentless action."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE LINE RESETS",
            "The opposing side passes; the patrol lesson continues.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "braun", 3, 4,
            "Charge", "BRAUN - OPEN THE ROW", "Move Braun three squares along the row from B4 to E4.",
            "Use Braun's printed orthogonal Charge as movement into the empty square."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE NORMAL ACTION", "End Turn after Braun opens the observation lane.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing side holds; play returns to Blackthorn.", "Mission data error."),
        scriptedAbility(1, "sharpshooter", "transform", "Raise Gun",
            "RAISE THE GUN", "Select the Goblin Sharpshooter and use Raise Gun.",
            "The lowered gun can move; the raised gun can fire.", {}, "Lower Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "STATE PERSISTS", "End Turn with the gun raised.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing side holds; play returns to Blackthorn.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "sharpshooter", 5, 7,
            "Fire", "SHARPSHOOTER - FIRE", "Use Fire along the clear row at the Bull Gator on H6. Three damage leaves its fourth Health intact.",
            "Use the raised Sharpshooter's printed Fire action.", "signal_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "COLLECTOR'S TAX", "End Turn so the Debt Collector can transfer Tax next turn.",
            "End Turn to observe Tax."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "TAX TRANSFERS, NEVER CREATES DEBT", "The Collector takes at most the resources the other side actually has.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "collector", 7, 2,
            "Knife Stab", "COLLECTOR - KNIFE STAB", "Use Knife Stab diagonally on the opposing Debt Collector at C8.",
            "Use your Debt Collector's attack on the highlighted enemy.", "crest_guard"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "LOWER THE GUN NEXT", "End Turn before changing the Sharpshooter's state.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing side passes.", "Mission data error."),
        scriptedAbility(1, "sharpshooter", "transform", "Lower Gun",
            "LOWER THE GUN",
            "Use Lower Gun. The Sharpshooter returns to the mobile state, where Fire is unavailable and Advance is legal.",
            "Select the Sharpshooter and use Lower Gun.", {}, "Raise Gun"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "MOBILE STATE PERSISTS", "End Turn with the gun lowered.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "sharpshooter", 5, 2,
            "Advance", "SHARPSHOOTER - ADVANCE",
            "Use Advance one square from B6 to empty C6. Advance is available only while the gun is lowered.",
            "Use the lowered Sharpshooter's printed Advance on highlighted C6.")
    };
    mission.masteryCards = {
        "Victor Greyshard", "Braun Stonefist", "Goblin Sharpshooter",
        "Blackthorn Debt Collector"};
    mission.masteryRules = {
        "relentless-optional-extra-action", "relentless-action-lock",
        "orthogonal-long-movement", "transform", "state-specific-actions",
        "line-of-sight", "tax", "health-and-destruction"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission blackthornTerms()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[1]);
    mission.objective =
        "Use two Ambushers' real Dematerialize, Sneak Around, and Ambush actions to cross occupied squares and expose a hidden destination collision.";
    mission.hint =
        "Use Dematerialize and Sneak Around through the Veterans; choose Ambush - not Sneak Around - for hidden Erevan so the collision deals damage and Disables him for his next owner-turn activation.";
    mission.briefing.insert(mission.briefing.begin(), panel("Victor Greyshard",
        "A hidden road can undo nineteen years of work before I know whose hand opened it. Follow the watcher to the exit. I would rather face an enemy than owe my survival to a door I cannot see.",
        "cards/victorGreyshard.png"));
    mission.briefing.insert(mission.briefing.begin(), panel("Narrator",
        "Lio, Juniper's wounded-mason contact, is arrested after Erevan asks Blackthorn's prepared question. At the boatyard, his children Tench and Seli face collapsing timber while Company observers test what a horned watcher protects.",
        "characters/juniperFlash.png"));
    mission.briefing.push_back(panel("Rules boundary",
        "The story's Chapter 3 observers are unnamed. This isolated Ambusher drill teaches real hidden-movement rules without assigning those actions to the observers.",
        "cards/goblinAmbusher.png"));
    mission.briefing.push_back(panel("Chronicle",
        BlackthornEntries[1].result, BlackthornEntries[1].art));
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
            "DEMATERIALIZE THE FIRST AMBUSHER", "Select the Ambusher at B4 and use the printed Dematerialize ability.",
            "Dematerialize the upper Ambusher first."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HIDDEN PIECES ADD NO CONTROL", "End Turn; a hidden piece adds no adjacent control influence, while tied squares keep their prior owner.", "End Turn to continue."),
        scriptedAbility(2, "erevan", "dematerialize", "Dematerialize",
            "EREVAN DISAPPEARS", "The opposing scout hides at C6.", "Mission data error."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "BOTH SIDES HIDE INFORMATION", "The watched lanes now contain one unseen body.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "ambusher_path", 3, 4,
            "Sneak Around", "SNEAK AROUND VISIBLE UNITS", "Use Sneak Around from B4 through the Veterans on C4 and D4 to E4.",
            "Use the hidden Ambusher's printed pass-through move."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "KEEP THE RECORD SEPARATE", "End Turn before testing the second lane.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE HIDDEN SCOUT HOLDS", "Erevan stays concealed in the lower lane.", "Mission data error."),
        scriptedAbility(1, "ambusher_collision", "dematerialize", "Dematerialize",
            "DEMATERIALIZE THE SECOND AMBUSHER", "Select the Ambusher at B6 and use the printed Dematerialize ability.",
            "Dematerialize the lower Ambusher."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "UNKNOWN OCCUPANCY", "End Turn. The destination preview cannot expose an enemy's hidden square.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE COLLISION WAITS", "Erevan remains unseen.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "ambusher_collision", 5, 2,
            "Ambush", "CHOOSE AMBUSH BEFORE THE HIDDEN COLLISION", "Choose the printed Ambush profile, then move the hidden Ambusher toward C6. It looks like movement into an empty square, but the unseen collision executes that same attacking profile: Erevan takes 1 damage and both pieces materialize.",
            "Choose Ambush - not Sneak Around - then use the highlighted route."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "VISIBLE AMBUSHER WAITS",
            "End Turn after the collision materializes the Ambusher.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "EREVAN MISSES THE DAMAGED ACTIVATION",
            "The collision's positive damage Disabled Erevan for this owner turn.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "ambusher_collision", 4, 0,
            "Stab", "AMBUSHER - VISIBLE STAB",
            "Use visible Stab diagonally from B6 to the wounded Debt Collector at A5.",
            "Use the materialized Ambusher's printed Stab on the highlighted target.",
            "stab_target")
    };
    mission.masteryCards = {"Goblin Ambusher"};
    mission.masteryRules = {
        "dematerialize", "hidden-adds-no-control", "pass-through",
        "hidden-collision-reveal", "hidden-collision-damage", "collision-stun",
        "visible-state-action"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission blackthornOffice()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[2]);
    mission.objective =
        "Use Grask's two printed attack geometries and Mog's Axe Swing against real opposing units.";
    mission.briefing = {
        panel("Narrator",
            "Hara Dole is the freight-office weighing clerk. The Guardian changes its seal test, then marks her wrist with black tracking dye. Erevan rejects a planted confession and finds four authentic freight chits.",
            "cards/mirewatchInformant.png"),
        panel("Narrator",
            "The chits prove 920 living pounds left, 524 arrived, and 396 were removed in transit. The mark can expose shelters, so Hara orders them to warn her mother and shuts the canal hatch behind them, giving up her place in their boat.",
            "cards/mirewatchInformant.png"),
        panel("Victor Greyshard",
            "Reed entered my yard under a borrowed seal, and when it failed, every clerk still waited for the Baelstone name instead of mine. Fizzlewick, teach the Guardian to learn the next choice. I will not lose the room that way again.",
            "cards/victorGreyshard.png"),
        panel("Fizzlewick Gearwright",
            "The Guardian can update when rescuers surprise it. That makes it a machine that predicts choices, Victor - not a verdict that owns the people it measures.",
            "cards/fizzlewickGearwright.png"),
        panel("Grask",
            "Name the lane. Mog holds the near body; I cross the long one. Your machine can count what remains after.",
            "cards/Grask.png"),
        panel("Rules boundary",
            "This operational reconstruction teaches Grask's Capture and long Charge plus Mog's Axe Swing. They do not literally fight this represented team in Hara's office.",
            "cards/Mog.png"),
        panel("Coach",
            "Follow each glowing real-card action. Capture can leave Grask in front of a survivor; ordinary damage disables a survivor; Charge attacks along an open row or column.",
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
            "Swing Axes", "GRASK - SWING AXES", "Use Swing Axes on the adjacent Veteran at C5. Three Health exceeds two damage, so the target survives and Grask stays on his current square in front of it.",
            "Choose Grask's printed diagonal Capture action.", "capture_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "CAPTURE IS ITS OWN ACTION", "End Turn after the Capture.", "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The opposing units hold; play returns to Blackthorn.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "mog", 5, 2,
            "Axe Swing", "MOG - AXE SWING", "Use Axe Swing on the adjacent Veteran at C6. Two damage leaves one Health.",
            "Use Mog's attack on the adjacent Veteran.", "mog_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "A SCREEN BUYS TIME", "End Turn; two damage did not remove the three-health screen.",
            "End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "MIREWATCH TURN ENDS", "The final opposing unit holds; play returns to Blackthorn.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "grask", 3, 5,
            "Charge", "GRASK - CHARGE", "Use Charge four squares along the row on the Bog Spearman at F4.",
            "Use Grask's printed orthogonal Charge on the highlighted enemy.", "charge_target")
    };
    mission.masteryCards = {"Grask", "Mog"};
    mission.masteryRules = {
        "capture", "capture-staging", "health-and-destruction",
        "long-orthogonal-attacking-movement"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission blackthornMonsterRules()
{
    StoryMission mission = playableFromEntry(BlackthornEntries[12]);
    mission.objective =
        "Use Ashenfang's printed Entangling Lunge on a real Veteran and observe its two-owner-turn status duration.";
    mission.pieces = {
        piece("ashenfang", "Ashenfang", 1, 3, 1),
        piece("veteran", "Marshland Veteran", 2, 5, 3)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.requiredSurvivorRoles = {"ashenfang"};
    mission.briefing.push_back(panel("Rules boundary",
        "This isolated card demonstration teaches Ashenfang's printed geometry and Disable duration. The opposing Veteran is a neutral timing marker; Sylvara is not duplicated on this board or cast as a willing Blackthorn combatant.",
        "cards/Ashenfang.png"));
    mission.briefing.push_back(panel("Donella's later account",
        "The monster is not hiding Sylvara. This is Sylvara, wounded into a rule.",
        "cards/Ashenfang.png"));
    mission.briefing.push_back(panel("Chronicle",
        BlackthornEntries[12].result, BlackthornEntries[12].art));
    mission.aftermath.push_back(panel("Chronicle",
        "Outside this rules board, the hidden tack is removed and both unmarked women remain alive. Survival is recorded without becoming ownership.",
        "cards/Ashenfang.png"));
    mission.script = {
        scriptedPieceAction(StoryActionKind::Attack, 1, "ashenfang", 5, 3,
            "Entangling Lunge", "ASHENFANG - ENTANGLING LUNGE", "Use Entangling Lunge on the Veteran two diagonal squares away at D6.",
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
    // A player who completed the earlier Alchemist application and healing
    // drills now has the generic two-turn expiry proof needed for that card's
    // cumulative mastery. Players who skipped those drills receive the same
    // complete Alchemist proof in the required BT17 catch-up synthesis.
    mission.masteryCards = {"Ashenfang"};
    mission.masteryRules = {
        "diagonal-range-two", "disable-two-turns", "status-duration"};
    mission.optionalRehearsal = true;
    return mission;
}

StoryMission mirewatchTollAlliance()
{
    StoryMission mission = chronicleFromEntry(MirewatchEntries[13]);
    mission.lesson = "OPEN BATTLE - MIXED ALLIES, TARGET ORDER, AND SURVIVAL";
    mission.objective =
        "Defeat the Company toll patrol and keep Reed, Pavo, and Nettle alive so every detainee can cross on their own terms.";
    mission.hint =
        "Inspect each unit with double-click or focus + I before acting. Reed shoots down clear rows, Donella attacks or heals at range, Pavo jumps or Controls without damage, and mounted Nettle flies through occupied squares.";
    mission.briefing = {
        panel("Narrator",
            "At a Company toll gate, Reed finds Pavo, wingless Nettle, Zippy, and other detainees marked for transport. A borrowed royal badge creates one opening, but the patrol closes before everyone can cross.",
            "cards/nettleStarbright_mounted.png"),
        panel("Pavo's slate",
            "One gate. One crossing. No obedience after it. Help us leave, and ask again when the road is clear.",
            "cards/pavoQuickstep.png"),
        panel("Coach",
            "This is an open battle using ordinary card rules. Defeat all four patrol units; there is no badge, gate switch, prisoner token, or invented interaction on the board.",
            "cards/reedBaelstone.png")};
    mission.aftermath = {
        panel("Chronicle", MirewatchEntries[13].result, MirewatchEntries[13].art)};
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 3, 0),
        piece("pavo", "Pavo Quickstep", 1, 2, 1),
        piece("nettle", "Nettle Starbright", 1, 4, 1),
        piece("donella", "Donella of the Marsh", 1, 5, 0),
        piece("collector", "Blackthorn Debt Collector", 2, 2, 5),
        piece("ambusher", "Goblin Ambusher", 2, 3, 6),
        piece("lumberjack", "Blackthorn Lumberjack", 2, 4, 5),
        piece("foreman", "Blackthorn Foreman", 2, 5, 6)};
    mission.playerResources = 0;
    mission.enemyResources = 0;
    mission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    mission.requiredSurvivorRoles = {"reed", "pavo", "nettle"};
    return mission;
}

StoryMission mirewatchWorldTreeReckoning()
{
    StoryMission mission = chronicleFromEntry(MirewatchEntries[19]);
    mission.lesson = "OPEN FINALE - BREAK THE COMMAND LINE UNDER PRESSURE";
    mission.objective =
        "Defeat Grask's command position while Reed, Pavo, Nettle, and Victor Greyshard survive. Victor is protected for surrender and custody, not a kill objective; other surviving enemies need not be defeated.";
    mission.hint =
        "Inspect units with double-click or focus + I to review their printed cards. Open a line with Birdie, Erevan, and Mog; use Donella's heal, Pavo's Control, and Nettle's flight to reach Grask. Do not attack Victor: he must survive for the surrender and custody aftermath.";
    mission.briefing = {
        panel("Narrator",
            "At the wounded World Tree, six cage groups begin collapsing their own prison frames onto Grask's line. The Society writ has reached Victor's gate, but his commanders still hold the approach.",
            "cards/Grask.png"),
        panel("Reed Baelstone",
            "My blood identifies the old door. It owns none of this work. Break Grask's line; offer Victor breath and a mortal court, not immunity.",
            "cards/reedBaelstone.png"),
        panel("Coach",
            "This seven-versus-seven finale uses only normal unit actions. Defeat the marked Grask role. Victor is a protected required survivor, not a kill objective; other enemies need not be eliminated. Reed, Pavo, Nettle, and Victor must survive.",
            "cards/pavoQuickstep.png")};
    mission.aftermath = victorReckoningPanels();
    mission.pieces = {
        piece("reed", "Reed Baelstone", 1, 3, 0),
        piece("pavo", "Pavo Quickstep", 1, 2, 1),
        piece("nettle", "Nettle Starbright", 1, 4, 1),
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
    mission.requiredSurvivorRoles = {"reed", "pavo", "nettle", "victor"};
    return mission;
}

StoryMission seelieTwoMenOneVacancy()
{
    StoryMission mission;
    mission.id = "se28a_two_men_one_vacancy";
    mission.title = "Two Men and One Vacancy";
    mission.sourceChapter = "Book Three, Chapter 31 - Two Men and One Vacancy";
    mission.lesson = "OPEN OBJECTIVE - HOLD A ROUTE, THEN REACH THE GATE";
    mission.objective =
        "Move Rowan onto G4 after opening the route. Rowan must survive; defeating every attacker is not required.";
    mission.hint =
        "Inspect each unit with double-click or focus + I before acting. Rowan and the Knight Bodyguard adjacent allies, the Sister heals every adjacent piece - friend or foe - at End Turn, and the Messenger can fly through blockers.";
    mission.briefing = {
        panel("Pavo's plain-language recap",
            "The Emperor and Lash are collecting trapped pieces of people because those pieces let them control bodies and roads. The Emperor has already pulled every piece still tied to him back into his body; that act is called Recall. Lash holds 4,106 captured, detached fragments at Baalzapub.",
            "cards/pavoQuickstep.png"),
        panel("Archivist Mosswake",
            "Both are racing for the receiver: the empty central place their machinery needs to control the whole system. Lash calls a system COMPLETE when it leaves no empty place and no way to leave. We keep that center empty so one person can end a link without forcing another to replace them.",
            "cards/archivistMosswake.png"),
        panel("Narrator",
            "Three hundred eleven people shelter below the only gate. Above them, Lash and the Emperor bargain over the captured fragments while Dusk Harvesters move toward the casualty shelf.",
            "cards/rowanLeafbound.png"),
        panel("Rowan Leafbound",
            "I surrendered field command, so I cannot order anyone's shot. Pavo, keep my words exact: I can hold this door, move the cots, and seal the gate after the last person clears.",
            "cards/rowanLeafbound.png"),
        panel("Narrator",
            "Lash asks the Emperor to release one life; the Emperor refuses. The Emperor asks Lash to release one fragment; Lash refuses. Two clerks record both answers while Rowan's crew works.",
            "cards/feyMessenger.png"),
        panel("Coach",
            "Reach highlighted G4 with Rowan using ordinary movement and attacks. The 311 civilians and the gate mechanism remain story facts, not pieces or clickable props.",
            "cards/starbloomKnight.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The two claimants part, each unwilling to prove his system can divide. Rowan seals the gate after the last cot clears; all 311 people below survive. The race turns toward Lash's black glass.",
            "cards/rowanLeafbound.png")};
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
    mission.title = "The Cathedral of Last Lights";
    mission.sourceChapter = "Book Two, Chapter 17 - The Cathedral of Last Lights";
    mission.lesson = "FLIGHT, RANGED ATTACKS, JUMP GEOMETRY, AND ATTACKING MOVEMENT";
    mission.objective =
        "Use each glowing ACT unit on its glowing TARGET and defeat the four Gloom Fairies attacking the Cathedral hall.";
    mission.hint =
        "Follow the glowing ACT unit and TARGET. Select the unit, confirm its target, and use End Turn when its action is finished.";
    mission.briefing = {
        panel("Prince Vesper - RESCUER",
            "Caltheriel is alive inside a clockwork prison. I do not yet know what promise holds her or what she wants now. I came to ask, not to restore a throne.",
            "cards/princeVesper.png"),
        panel("Pavo Quickstep - COURIER",
            "A route is a path that carries people, messages, magic, or orders. Ours leads through a Cathedral witness who remembers the machinery around Caltheriel.",
            "cards/pavoQuickstep.png"),
        panel("Quinberry Lark - SCOUT",
            "A witness keeps a choice and its cost public. This account says Lash supplied routing records used to confine Caltheriel - but Gloom Fairies break through before the account is finished.",
            "cards/quinberryLark.png"),
        panel("Fey Messenger - WITNESS COURIER",
            "I carry the witness's account. My Courier Flight crosses occupied squares, but I must land on an empty one; if I fall, the proof against Lash stops here.",
            "cards/feyMessenger.png"),
        panel("Duchess Dewbell - REARGUARD",
            "I hold the lower aisle. Dewbell's Decree moves or attacks up to two squares and leaves a surviving target Disabled for two of its turns.",
            "cards/duchessDewbell.png"),
        panel("Coach - Board Basics",
            "Blue Health badges belong to your units; red badges mark enemies. The number is current Health, and a unit leaves play at zero. One normal unit acts on each turn, then you use End Turn.",
            ""),
        panel("Coach",
            "Mouse: drag the glowing ACT unit onto TARGET. Keyboard: focus ACT and press Enter, then focus TARGET and press Enter; I inspects a card. Every required action uses a real printed rule.",
            "")};
    mission.aftermath = {
        panel("Cathedral Witness",
            "Lash supplied routing records used to confine Caltheriel. His copied map reaches her custody, but it cannot yet see the path her light would take out.",
            ""),
        panel("Quinberry Lark",
            "A Dusk Harvester stole a copy of the record and fled toward the refugee road.",
            "cards/quinberryLark.png"),
        panel("Prince Vesper",
            "Then we protect the people on that road. I will not reach Caltheriel by abandoning everyone between us.",
            "cards/princeVesper.png")};
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
            "Courier Flight", "MESSENGER - COURIER FLIGHT",
            "Use the glowing Messenger's Courier Flight from B3 to D3. Flight may pass through Pavo, but must finish on an empty square.",
            "Move the Fey Messenger to highlighted D3."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE PIECE ACTION PER TURN", "End Turn so the opposing side receives its normal turn.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SWARM CLOSES", "The first Fairy holds beside the Messenger.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "messenger", 2, 4,
            "Star Dart", "MESSENGER - STAR DART",
            "Use Star Dart on the adjacent Fairy at E3. This ranged action attacks without moving.",
            "Use the Messenger's Dart on the highlighted Fairy.", "fairy_dart"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "PASS THE ACTION", "End Turn after the Messenger's attack.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE SECOND FAIRY DIVES", "The next attacker crosses Pavo's lane.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "pavo", 3, 4,
            "Quickstep", "PAVO - QUICKSTEP",
            "Use Quickstep from C3 to E4. It follows a fixed L-shape: two squares one way, then one across. An enemy on the destination is attacked.",
            "Use Pavo's Jump on the highlighted Fairy.", "fairy_jump"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE DEFENSE ROTATES", "End Turn so Dewbell can answer the lower aisle next.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "LOWER AISLE", "A third Fairy reaches Dewbell's two-square range.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "duchess", 5, 3,
            "Dewbell's Decree", "DEWBELL - DECREE",
            "Use Dewbell's Decree two squares to D6. This action moves toward the target and attacks it; if the target survives, it stays put and becomes Disabled.",
            "Use the printed Dewbell's Decree on the highlighted Fairy.", "fairy_decree"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE LIGHT REMAINS", "End Turn before Lark crosses the final diagonal.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "LARK FINDS THE LINE", "The last Fairy separates from the swarm.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "lark", 3, 3,
            "Swoop", "LARK - SWOOP",
            "Use Swoop from A7 to D4. Lark moves and attacks along one clear diagonal.",
            "Use Quinberry Lark's Swoop on the highlighted Fairy.", "fairy_swoop")};
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
    mission.title = "The Broken Bridge";
    mission.sourceChapter = "Book Two, Chapter 18 - Road of Small Lights";
    mission.lesson = "HEALING AURA, BODYGUARD, HEALTH, AND A SURVIVING TARGET";
    mission.objective =
        "Clear the first refuge lane while keeping the Fey Messenger, Heartwood Sister, Starbloom Knight, and Crystal Unicorn alive.";
    mission.hint =
        "Protect the Messenger: position support first, then watch automatic healing and Bodyguard resolve. Bodyguard redirects a positive-damage hit and any Disable attached to that hit; effects that deal no damage stay on the chosen target.";
    mission.briefing = {
        panel("Fey Messenger",
            "I carry the witness's account. If I fall, the proof against Lash stops here.",
            "cards/feyMessenger.png"),
        panel("Heartwood Sister",
            "The refugees chose a footpath around the broken bridge. Bristlejacks and a Widowroot block our side of the crossing.",
            "cards/heartwoodSister.png"),
        panel("Coach",
            "Blue badges show allied Health; red badges show enemy Health. Bodyguard moves positive damage - and any Disable attached to that damage - to an adjacent guard. A zero-damage effect stays on its chosen target. Keep all four allies alive.",
            "cards/starbloomKnight.png")};
    mission.aftermath = {
        panel("Pavo Quickstep",
            "The first lane is open. The travelers are splitting between smaller paths instead of waiting for one road to carry everyone.",
            "cards/pavoQuickstep.png")};
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
            "Grovewalk", "SISTER - GROVEWALK",
            "Use Grovewalk diagonally from A7 to C5, adjacent to the wounded Crystal Unicorn.",
            "Move the Heartwood Sister to highlighted C5."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "HEALING AURA",
            "End Turn. The Sister's printed aura restores 1 Health to adjacent pieces, up to their printed maximum.",
            "Use End Turn to resolve the aura."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "bristle", 5, 2,
            "Bristle Charge", "BRISTLEJACK STRIKES",
            "The Bristlejack uses its ordinary 2-damage attack on the Messenger.",
            "Mission data error.", "messenger"),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "BODYGUARD REDIRECTS DAMAGE",
            "The adjacent Knight takes the Messenger's two damage and its Disable. Bodyguard redirects positive damage and effects attached to that damage. A zero-damage effect stays on its chosen target.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DAMAGE COSTS AN ACTIVATION",
            "The struck Knight cannot act this turn. End Turn rather than inventing an exception.",
            "Use End Turn while the damaged Knight recovers."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FORMATION HOLDS",
            "The attackers hold; the Knight will be ready on the next Seelie turn.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "knight", 5, 3,
            "Starbloom Charge", "KNIGHT - STARBLOOM CHARGE",
            "Use Starbloom Charge one square upward to defeat the Bristlejack.",
            "Use the Knight's attack on the highlighted Bristlejack.", "bristle"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE WIDOWROOT HOLDS", "End Turn before the Unicorn tests the longer lane.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE ROOT WAITS", "The Widowroot keeps its position.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "unicorn", 3, 6,
            "Prismatic Charge", "UNICORN - PRISMATIC CHARGE",
            "Charge three squares to the Widowroot. Two damage cannot remove its four Health, so the Unicorn stops short.",
            "Use the Crystal Unicorn's attack on the highlighted Widowroot.", "widow"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "A SURVIVING TARGET HOLDS ITS SQUARE", "End Turn after the first charge.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE WOUNDED ROOT MISSES ITS ACTIVATION",
            "The damaged Widowroot cannot act this turn. The opposing side passes; the Unicorn is ready again.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "unicorn", 3, 6,
            "Prismatic Charge", "UNICORN - FINISH THE CHARGE",
            "Use Prismatic Charge again. The remaining two Health reaches zero, so the Unicorn occupies the cleared square.",
            "Use the Unicorn's attack on the highlighted Widowroot.", "widow")};
    mission.masteryCards = {"Heartwood Sister", "Starbloom Knight", "Crystal Unicorn"};
    mission.masteryRules = {
        "diagonal-movement", "healing-aura", "maximum-health", "bodyguard",
        "damage-redirection", "surviving-target-stops-movement", "defeated-target-occupation"};
    return mission;
}

StoryMission seelieRoadOfSmallLights()
{
    StoryMission mission;
    mission.id = "se03_road_small_lights";
    mission.title = "Road of Small Lights";
    mission.sourceChapter = "Book Two, Chapter 18 - Road of Small Lights";
    mission.lesson = "MOUNTED REBIRTH, FLYING ATTACKS, AND HERO ACTIONS";
    mission.objective =
        "Create a safe crossing window with Nettle, a Thorn Griffin, and Prince Vesper while every action keeps its printed function.";
    mission.hint =
        "Flight crosses pieces but must end legally. Nettle's real Rebirth replaces her mounted form after lethal damage.";
    mission.briefing = {
        panel("Nettle Starbright - WINGLESS ENGINEER",
            "Zippy is the flying snail carrying my harness, and he chooses every route. I build roads with exits: an agreed way either of us can stop or leave. I can mark this crossing; I cannot make anyone use it.",
            "cards/nettleStarbright_mounted.png"),
        panel("Prince Vesper",
            "The Griffin takes the high lane. I take the low one. We reach the same shelter without making either path command the other.",
            "cards/princeVesper.png"),
        panel("Coach",
            "Use only the highlighted unit actions. The refugees, roof rescue, and route choices remain story events rather than board inputs.",
            "cards/thornGriffin.png")};
    mission.aftermath = {
        panel("Nettle Starbright",
            "Thirty-six people reached shelter by several small paths. No single guide had to own the whole rescue.",
            "cards/nettleStarbright_mounted.png"),
        panel("Prince Vesper",
            "Then perhaps Caltheriel can leave the same way: carried by choices that remain separate, not handed from one keeper to another.",
            "cards/princeVesper.png")};
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
            "Fly", "NETTLE - FLY",
            "Use Fly from B6 to D6, passing through the Thorn Griffin at C6 and ending on empty ground.",
            "Move mounted Nettle to highlighted D6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "SET THE SLINGSHOT", "End Turn with Nettle beside the Fairy.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE BRISTLEJACK WAITS", "The lower attacker holds for a clearer strike.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "nettle", 5, 4,
            "Slingshot", "NETTLE - SLINGSHOT",
            "Use the adjacent ranged Slingshot. Nettle attacks without moving.",
            "Use Nettle's Slingshot on the highlighted Fairy.", "fairy_sling"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "A HARDER HIT ANSWERS", "End Turn; the Bristlejack now has an adjacent line.",
            "Use End Turn."),
        scriptedPieceAction(StoryActionKind::Attack, 2, "bristle_rebirth", 5, 3,
            "Bristle Charge", "MOUNTED REBIRTH",
            "The Bristlejack's printed 2 damage defeats mounted Nettle. Rebirth places Nettle Starbright Unmounted on the same square, already acted.",
            "Mission data error.", "nettle"),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE REPLACEMENT WAITS", "The opposing turn ends; unmounted Nettle readies normally.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "nettle", 6, 3,
            "Punch", "UNMOUNTED NETTLE - PUNCH",
            "Use Punch on the adjacent Bristlejack. The replacement is the real one-Health token unit, not a tutorial-only state.",
            "Use unmounted Nettle's attack on the highlighted Bristlejack.", "bristle_rebirth"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE HIGH LANE", "End Turn before the Griffin crosses the file.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE WIDOWROOT HOLDS", "The rooted attacker guards the far end.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "griffin", 5, 6,
            "Griffin Flight", "GRIFFIN - FLYING ATTACK",
            "Use Griffin Flight along the row. It may pass intervening pieces; because the Widowroot survives, the Griffin does not occupy its square.",
            "Use the Thorn Griffin's attack on the highlighted Widowroot.", "widow_griffin"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "VESPER TAKES THE LOW LIGHT", "End Turn before Vesper moves.", "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE FINAL FAIRY WAITS", "The lower target stays two orthogonal squares from the landing.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "vesper", 2, 4,
            "Royal Flight", "VESPER - ROYAL FLIGHT",
            "Use Royal Flight from B3 to E3, passing through the Messenger and ending on empty E3.",
            "Move Prince Vesper to highlighted E3."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "RANGED HERO ATTACK", "End Turn with the Fairy two squares down the file.",
            "Use End Turn."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE LIGHT HOLDS", "The final Fairy stays in Vesper Spark range.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "vesper", 4, 4,
            "Vesper Spark", "VESPER - SPARK",
            "Use Vesper Spark two orthogonal squares to defeat the Fairy without moving.",
            "Use Vesper's Spark on the highlighted Fairy.", "fairy_spark")};
    mission.masteryCards = {"Nettle Starbright", "Thorn Griffin", "Prince Vesper"};
    mission.masteryRules = {
        "mounted", "rebirth", "replacement-acts-next-turn", "ranged-line-of-sight",
        "flying-pass-through", "surviving-target-stops-movement", "hero-actions"};
    return mission;
}

StoryMission seelieReadingRoomRehearsal()
{
    StoryMission mission;
    mission.id = "se04_reading_room";
    mission.title = "The Reading Room";
    mission.sourceChapter = "Book Two, Chapter 19 / non-canon rules rehearsal";
    mission.lesson = "GUIDED REHEARSAL - DEPLOYMENT, FORESIGHT, DRAW, DISCARD, AND ROOTWALK";
    mission.objective =
        "NON-CANON RULES REHEARSAL. Learn Mosswake's real deployment, Foresight, draw, discard, and Rootwalk rules.";
    mission.hint =
        "The story scene is real; the practice board is not. Follow only the highlighted normal game actions.";
    mission.briefing = {
        panel("Archivist Mosswake",
            "I am Mosswake, keeper of the court record. Four plain words will keep us oriented. A route carries movement, magic, information, or orders. A bearer carries one limited part.",
            "cards/archivistMosswake.png"),
        panel("Archivist Mosswake",
            "An exit lets a bearer stop. A witness keeps the limit, the choice, and the cost public.",
            "cards/archivistMosswake.png"),
        panel("Coach",
            "Use a harmless practice board to learn Mosswake's real deployment, Foresight, draw, discard, and Rootwalk rules. No story event can change here.",
            "")};
    mission.aftermath = {
        panel("Coach",
            "Foresight revealed two possible draws, the discarded card returned to the bottom, and Rootwalk used its real straight two-square attack. The story now returns to Nyxara's court.",
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
            "PLAY MOSSWAKE",
            "Play Mosswake on glowing B4. Sylvara controls that empty square and provides his required Ancient and Wild traits.",
            "Play Archivist Mosswake from your hand on glowing B4.", "mosswake"),
        scripted(StoryActionKind::DrawCard, 1, {}, -1, -1,
            "DRAW A CARD",
            "Click DRAW, or Tab to it and press D or Enter. Pay 50 Resources. Foresight reveals two cards; keep one and return the other to the deck's bottom.",
            "Draw from the marked deck: click it, or Tab to DRAW and press D or Enter."),
        scriptedCard(StoryActionKind::ChooseForesight, 1, "Fey Messenger", -1, -1,
            "CHOOSE WITH FORESIGHT",
            "Fey Messenger is required for the next scripted action. Choose it; the other revealed card returns to the bottom of the draw pile.",
            "Fey Messenger is required for this Story step. Choose Fey Messenger, not Starbloom Knight."),
        scriptedCard(StoryActionKind::DiscardCard, 1, "Fey Messenger", -1, -1,
            "DISCARD ONE CARD",
            "Put Fey Messenger in DISCARD, or focus it and press X. It goes to the deck's bottom; your unit may still act.",
            "Discard Fey Messenger: move it to the bin, or focus the card and press X."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DEPLOYED UNITS WAIT",
            "End Turn. Mosswake cannot act on the same turn he entered play.",
            "Use End Turn before moving Mosswake."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE TARGET WAITS",
            "The opposing side passes, and Mosswake becomes ready.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "mosswake", 3, 3,
            "Rootwalk", "MOSSWAKE - ROOTWALK",
            "Use Rootwalk two squares in a straight line from B4 to D4. The move attacks the Fairy on its destination.",
            "Use Mosswake's Rootwalk on the highlighted Fairy at D4.", "root_target")};
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
        "The Queen's Price",
        "Book Two, Chapter 22 / Optional non-canon rules reconstruction",
        {
            panel("Queen Nyxara",
                "Three living helpers and the households of two dead helpers now sit under my seal. I did not ask them; only I can lift it.",
                "cards/queenNyxara.png"),
            panel("Reed Baelstone",
                "Then the protection, the missing consent, and its cost enter the public record. It buys no title, road, favor, or private debt.",
                "cards/reedBaelstone.png"),
            panel("Pipistrel Vane",
                "Do not step behind me. A black blade has already entered that future; moving its path will cost sight I cannot recover.",
                "cards/pipistrelVane.png"),
            panel("Narrator",
                "Pipistrel knocks Reed's cane aside. The black blade crosses where Reed's throat had been, and a dark line clouds the left side of Pipistrel's compound gaze.",
                ""),
            panel("Archivist Mosswake",
                "The withdrawal test loosened one command thread and tightened another. The record keeps both results.",
                "cards/archivistMosswake.png"),
            panel("Reed Baelstone",
                "Open the road under those terms. Help may be accepted. Ownership may not.",
                "cards/reedBaelstone.png")});
    mission.lesson =
        "OPTIONAL GUIDED RECONSTRUCTION - RANGED INFEST AND DEATH REPLACEMENT";
    mission.objective =
        "OPTIONAL NON-CANON RULES RECONSTRUCTION. Mark a surviving Warden with Nyxaran Infest, then defeat it with an ordinary allied attack to spawn a real Bristlejack.";
    mission.hint =
        "Nyxaran Infest deals 1 damage at diagonal range one or two. Its non-Hero target must die before the named Bristlejack replaces it for Nyxara's side.";
    mission.briefing.push_back(panel("Rules boundary",
        "Nyxara's protection, Pipistrel's sacrifice, and Reed's public terms above are canonical. This optional board demonstrates Nyxara's live card rule only; the Warden, Fairy, and infestation are not claims about what happened in Chapter 22.",
        "cards/queenNyxara.png"));
    mission.aftermath = {
        panel("Coach",
            "Nyxaran Infest marked a surviving non-Hero. When a later real attack reduced that Warden to zero Health, it was replaced on the same square by the actual one-Health Bristlejack unit for Nyxara's side. The replacement arrives already acted.",
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
            "Nyxaran Infest", "NYXARA - NYXARAN INFEST",
            "Use Nyxaran Infest diagonally from B5 to the wounded Warden at D3. One damage leaves it at 1 Health and marks it to become a Bristlejack when it dies.",
            "Use Queen Nyxara's attack on the highlighted Warden at D3.",
            "infest_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "INFEST WAITS FOR DEATH",
            "End Turn. The marked Warden is still alive, so no Bristlejack exists yet.",
            "Use End Turn after Nyxaran Infest."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE MARKED WARDEN HOLDS",
            "The opposing side passes while the living Warden keeps the infestation mark.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "fairy", 2, 3,
            "Shoot Spark", "FAIRY - SHOOT SPARK",
            "Use Shoot Spark from C3 on the adjacent Warden at D3. Its last Health is lost, so Infest replaces it on D3 with a one-Health Bristlejack owned by Nyxara's side.",
            "Use the Gloom Fairy's attack on the highlighted Warden at D3.",
            "infest_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "THE REPLACEMENT WAITS",
            "End Turn after the allied attack completes the Infest replacement.",
            "Use End Turn to continue."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE SIDE PASSES", "The opposing side passes.", "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "nyxara", 4, 3,
            "Royal Advance", "NYXARA - ROYAL ADVANCE",
            "Use Royal Advance two squares along the row from B5 to empty D5.",
            "Use Nyxara's printed orthogonal move on highlighted D5.")};
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
    mission.title = "The Queen's Price - Rules Rehearsal";
    mission.sourceChapter = "Book Two, Chapter 22 / non-canon rules rehearsal";
    mission.lesson = "GUIDED REHEARSAL - CONTROL, DEMATERIALIZE, AND HIDDEN MOVEMENT";
    mission.objective =
        "NON-CANON RULES REHEARSAL. Use Pavo's temporary Control, Lark's hidden-state movement, and a normal turn change that restores the borrowed unit.";
    mission.hint =
        "Control changes a surviving unit's side only temporarily. After its final controlled turn, it returns when its original owner's next turn starts. Hidden Lark may cross occupied squares but must land on an empty one.";
    mission.briefing = {
        panel("Pavo Quickstep",
            "The witnessed road into the prison is open. Before we enter, practice two card rules whose names must not be mistaken for ownership of a person.",
            "cards/pavoQuickstep.png"),
        panel("Quinberry Lark",
            "A Controlled enemy remains alive and eventually returns. Hidden changes which actions I may use; it does not remove me from the board.",
            "cards/quinberryLark.png"),
        panel("Coach",
            "On this harmless board, use Enchanting Pipes, Dematerialize, and Sneak Around exactly as printed. The drill changes no story event.",
            "")};
    mission.aftermath = {
        panel("Quinberry Lark",
            "The borrowed unit was never defeated, and it returned to its original side at that side's next turn start. I crossed the occupied lane only while Hidden and still had to finish on empty ground.",
            "cards/quinberryLark.png")};
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
            "Enchanting Pipes", "PAVO - ENCHANTING PIPES",
            "Use Enchanting Pipes on the Bristlejack two squares away. It deals no damage and Controls the survivor for two turns.",
            "Use Pavo's Enchanting Pipes on the highlighted Bristlejack at C3.", "pipes_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "TEMPORARY CONTROL",
            "End Turn. The Bristlejack remains visibly borrowed, not defeated.",
            "Use End Turn after Pavo's action."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE BLOCKED LANE HOLDS",
            "The opposing side passes while two units still occupy Lark's route.",
            "Mission data error."),
        scriptedAbility(1, "lark", "dematerialize", "Dematerialize",
            "LARK - DEMATERIALIZE",
            "Select Quinberry Lark and use Dematerialize. Hidden units stop adding board control and gain any action limited to that state.",
            "Select Lark, then use her Dematerialize ability."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "STATE-SPECIFIC ACTIONS",
            "End Turn. Sneak Around is available only on a later turn while Lark remains hidden.",
            "Use End Turn before moving hidden Lark."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE BLOCKERS STAY",
            "The occupied route remains unchanged.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "lark", 5, 5,
            "Sneak Around", "LARK - SNEAK AROUND",
            "Use Sneak Around from A6 to F6. Hidden Lark may pass through both occupied squares but must finish on empty ground.",
            "Move hidden Lark to the highlighted empty square F6."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "CONTROL RETURNS AT TURN START",
            "End Turn. The Bristlejack's final controlled turn is over, so it returns to its original side when that side's turn begins.",
            "Use End Turn to demonstrate temporary Control ending.")};
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
    mission.title = "No Crown Can Hold Light";
    mission.sourceChapter = "Book Two, Chapter 26 / non-canon rules rehearsal";
    mission.lesson = "GUIDED REHEARSAL - HEALING AURA, FEY FLIGHT, CHARM, AND CONTROL";
    mission.objective =
        "CALTHERIEL'S CHOSEN RULES REHEARSAL. Heal an adjacent ally, fly diagonally, and use zero-damage Charm.";
    mission.hint =
        "The release has already happened by consent in the story. This board teaches only Caltheriel's real match actions.";
    mission.briefing = {
        panel("Caltheriel",
            "My release has already happened by my choice. This board cannot repeat it, and no result here can change what it cost.",
            "characters/caltheriel.png"),
        panel("Coach",
            "Use Caltheriel's Healing Aura, Fey Flight, and zero-damage Charm exactly as printed. No person, memory, or share of light is represented by a board objective.",
            "")};
    mission.aftermath = {
        panel("Prince Vesper",
            "I wanted to carry her across the threshold. Waiting for her decision was harder and more important.",
            "cards/princeVesper.png"),
        panel("Coach",
            "The rehearsal taught Healing Aura, Fey Flight, and Charm. It did not decide Caltheriel's release.",
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
            "CALTHERIEL - HEALING AURA",
            "End Turn. Caltheriel's aura restores 1 Health to the adjacent Knight, never above its printed maximum.",
            "Use End Turn to resolve Caltheriel's Healing Aura."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "THE PRACTICE TARGET WAITS",
            "The opposing side passes and Caltheriel becomes ready.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Move, 1, "caltheriel", 1, 2,
            "Fey Flight", "CALTHERIEL - FEY FLIGHT",
            "Use Fey Flight diagonally from A4 to C2. A normal move uses this turn's one unit action.",
            "Move Caltheriel to highlighted C2."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "ONE UNIT ACTION PER TURN",
            "End Turn so Caltheriel can use a different action on her next turn.",
            "Use End Turn after Fey Flight."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "CHARM RANGE",
            "The Bristlejack remains two straight squares from Caltheriel.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "caltheriel", 1, 4,
            "Charm", "CALTHERIEL - CHARM",
            "Use Charm on the Bristlejack at E2. Charm deals no damage and Controls the survivor for two turns.",
            "Use Caltheriel's Charm on the highlighted Bristlejack at E2.", "charm_target")};
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
    mission.title = "Pearl and Root";
    mission.sourceChapter =
        "Book Three, Chapter 4 / non-canon rules rehearsal";
    mission.lesson = "GUIDED REHEARSAL - POSITIVE DAMAGE, DISABLE 2, AND STATUS TIMING";
    mission.objective =
        "NON-CANON RULES REHEARSAL. Resolve Sylvara's full Disable timing, then use Duchess Dewbell's printed Decree.";
    mission.hint =
        "Sovereign Thorns must deal damage to apply Disable. Sylvara has no usable Summon command, so none is invented here.";
    mission.briefing = {
        panel("Sylvara",
            "The chosen root cut is complete. This harmless board cannot repeat it; it can only demonstrate the printed timing of Sovereign Thorns.",
            "cards/Sylvara.png"),
        panel("Coach",
            "Disable 2 means a surviving target misses its next two activations on turns belonging to its owner. Use Sovereign Thorns against a real unit, then observe both skips. Sylvara has no usable Summon command, so none is invented.",
            "")};
    mission.aftermath = {
        panel("Coach",
            "The rehearsal ends after two genuine disabled activations. The story resumes with the loss of heat and the underroot road already recorded.",
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
            "Sovereign Thorns", "SYLVARA - SOVEREIGN THORNS",
            "Use Sovereign Thorns two diagonal squares to C7. One damage leaves the Warden standing and applies Disable 2.",
            "Use Sylvara's Sovereign Thorns on the highlighted Warden at C7.", "thorns_target"),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: FIRST ACTIVATION",
            "End Turn. The surviving Warden must miss its first disabled activation.",
            "Use End Turn to advance Disable."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "FIRST ACTIVATION MISSED",
            "The Warden cannot act. Disable still has one owner turn remaining.",
            "Mission data error."),
        scripted(StoryActionKind::EndTurn, 1, {}, -1, -1,
            "DISABLE: SECOND ACTIVATION",
            "End Turn again. Disable 2 also removes the Warden's next activation.",
            "Use End Turn to observe the second disabled activation."),
        scripted(StoryActionKind::EndTurn, 2, {}, -1, -1,
            "SECOND ACTIVATION MISSED",
            "The Warden misses the second activation, and the printed status expires.",
            "Mission data error."),
        scriptedPieceAction(StoryActionKind::Attack, 1, "duchess", 2, 3,
            "Dewbell's Decree", "DEWBELL - DECREE",
            "Use Dewbell's Decree two squares along the row on the two-Health Warden at D3. One damage leaves it standing and applies Disable 2.",
            "Use Duchess Dewbell's printed action on the highlighted Warden.",
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
    mission.title = "Territory Under Pressure";
    mission.sourceChapter = "Required non-canon tactical bridge after Book Three, Chapter 4";
    mission.lesson = "FIRST OPEN TERRITORY SIMULATION - REACH 22 CONTROLLED SQUARES";
    mission.objective =
        "TACTICAL SIMULATION - DOES NOT CHANGE THE STORY. Control 22 territory squares while Pavo remains on the board; defeating both training enemies is not required.";
    mission.hint =
        "Territory Influence is separate from Pavo's Unit Control. Move your formation to expand the ground shown in your color; occupied squares and stronger adjacent influence count toward 22.";
    mission.briefing = {
        panel("Archivist Mosswake",
            "The preceding optional drills isolate each unfamiliar rule. Replay any you skipped or do not recognize; now choose the order on a small board where a mistake changes no life and no chapter.",
            "cards/archivistMosswake.png"),
        panel("Territory Influence",
            "A visible piece owns every square it occupies and influences all eight adjacent squares. Greater adjacent influence owns an empty square; a tie keeps its current owner. Hidden pieces exert no influence.",
            ""),
        panel("Coach",
            "Your formation begins with 15 controlled squares. Reach 22 while Pavo remains on the board. No single opening command is enough, so the training opponent will answer before you can finish. Move, attack, or use printed abilities in any legal order; you do not need to defeat both enemies.",
            ""),
        panel("Pavo Quickstep",
            "My Enchanting Pipes use Unit Control: a surviving target temporarily changes sides. That can change its territory influence, but Unit Control and the territory total are two different rules.",
            "cards/pavoQuickstep.png"),
        panel("Heartwood Sister",
            "Any wounded piece beside me - friend or foe - recovers one Health from my Healing Aura at End Turn. Keep enemies outside the aura while spreading our formation.",
            "cards/heartwoodSister.png")};
    mission.aftermath = {
        panel("Prince Vesper",
            "You expanded territory without following an answer key or mistaking Pavo's borrowed unit for the territory objective. The final defense will demand a wider line and required survivors.",
            "cards/princeVesper.png"),
        panel("Narrator",
            "The practice pieces are cleared away. In the city beyond the board, the first imperial boats are already burning.",
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
    mission.title = "The Warm Channel";
    mission.sourceChapter = "Book Three, Chapter 9 - The Warm Channel";
    mission.lesson = "OPEN BATTLE - MIXED FLIGHT, TARGET ORDER, AND SURVIVAL";
    mission.objective =
        "Break this Gloom Fairy signal group so the imperial fleet loses the hidden safe channel.";
    mission.hint =
        "This is the first story battle without highlights. Inspect every unit with double-click or focus + I: Griffin flies straight through pieces, Vesper fires at range, Lark crosses long diagonals, and mounted Nettle flies through blockers or uses an adjacent Slingshot.";
    mission.briefing = {
        panel("Prince Vesper",
            "The invasion fleet is reading a hidden depth line through compelled Gloom Fairies. If this signal group holds, the ships reach Emberhaven's unguarded water.",
            "cards/princeVesper.png"),
        panel("Thorn Griffin flight leader",
            "We break one signal group. We do not pretend that saves every quay or returns the Griffins already lost.",
            "cards/thornGriffin.png"),
        panel("Coach",
            "Inspect a unit with double-click or focus + I before committing. Griffin's straight flight can cross pieces; Lark's Swoop travels long diagonals but cannot cross blockers; Vesper's Spark is ranged; mounted Nettle's Fly passes through pieces while Slingshot needs an adjacent clear line. End Turn after one normal activation.",
            "cards/thornGriffin.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The depth signal breaks and the fleet loses the safe channel. The harbor still pays dearly: three Griffins are dead, defenders fall, the dry dock floods, and the low ward is lost.",
            "cards/thornGriffin.png"),
        panel("Narrator",
            "The invaders call those losses proof that one complete commander is needed. The defenders begin proving the opposite.",
            "cards/gloomFairy.png")};
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
    mission.title = "The Crypt Under the Order";
    mission.sourceChapter = "Book Three, Chapter 15 - The Crypt Under the Order";
    mission.lesson = "OPEN BATTLE - COMBINED ARMS, BLOCKERS, AND MULTIPLE ACTIVATIONS";
    mission.objective =
        "Clear the attackers from this shelter stair while the workers open a second route.";
    mission.hint =
        "Inspect cards with double-click or focus + I before acting. Keep each Starbloom Knight beside the ally it may Bodyguard; the Sister and Caltheriel heal every adjacent piece - friend or foe - at End Turn; Pavo's zero-damage Pipes Controls a survivor for two owner turns.";
    mission.briefing = {
        panel("Starbloom rear guard",
            "Sixty-two people are below us. Sava can open a second exit if this stair holds long enough for the work to finish.",
            "cards/starbloomKnight.png"),
        panel("Coach",
            "This battle is open and less forgiving. A Starbloom Knight's Bodyguard redirects positive damage from an adjacent friendly non-Bodyguard; several eligible Knights split it. Both healing auras restore every adjacent piece, friend or foe, at End Turn. Pavo's range-two Pipes deal zero damage and Control a surviving enemy for two of that unit's owner turns.",
            "cards/starbloomKnight.png"),
        panel("Narrator",
            "The civilians and Echo Lantern remain in the chronicle. They are not pieces, switches, or a hidden timer.",
            "cards/heartwoodSister.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The rear guard holds the stair and sixty-one people escape. One remains missing, the archive burns, and Eyeblight gets away; a local win does not erase the cost.",
            "cards/starbloomKnight.png")};
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
    mission.title = "A Controlled Fall";
    mission.sourceChapter = "Book Three, Chapter 23 - A Controlled Fall";
    mission.lesson = "OPEN OBJECTIVE - REACH A SQUARE UNDER PRESSURE";
    mission.objective =
        "Escort the marked Starbloom Knight to G5. Reaching that square clears this local group through the north withdrawal lane.";
    mission.hint =
        "You do not need to defeat every enemy. Use the Griffin and Unicorn to open a path, the Sister to support it, and move the marked Knight onto G5.";
    mission.briefing = {
        panel("Narrator",
            "On the fungal bridge, Vaeloren throws three prisoners clear and is crushed beneath the Moonshade Stalker's strike. The enemy's throat opens at the same instant.",
            "cards/marrowind.png"),
        panel("Marrowind",
            "The shot is mine - and Vaeloren is alive. I lower the bow and carry my partner. Someone else can finish the clean shot.",
            "cards/marrowind.png"),
        panel("Starbloom route captain",
            "Old Ember cannot all be saved. One local group still needs the north withdrawal lane while four wards make separate decisions.",
            "cards/starbloomKnight.png"),
        panel("Coach",
            "This is an objective battle, not a wipeout. Reach highlighted G5 with the marked Knight. Normal enemy turns continue while you build the route.",
            "cards/starbloomKnight.png"),
        panel("Narrator",
            "Councils, demolition keys, civilian counts, and the decision to abandon a ward stay fixed in the story panels.",
            "cards/Warden.png")};
    mission.aftermath = {
        panel("Chronicle",
            "This withdrawal lane stays open and the local group clears. Old Ember still falls: thirty-one are dead, twelve remain missing, and seven are presumed beneath the bathhouse.",
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
    mission.title = "The Emperor at the Tree";
    mission.sourceChapter = "Book Three, Chapter 30 - The Emperor at the Tree";
    mission.lesson = "ADVANCED OPEN BATTLE - FULL FORMATION AND TARGET PRIORITY";
    mission.objective =
        "Defeat the attackers in this Pump Four sector and keep Sylvara alive so the surrender lane and north stair remain usable.";
    mission.hint =
        "Story boundary: Caltheriel's eight minutes and the signal Sprite are chronicle events, not board rules. There is no battle timer or Sprite unit. Inspect every card with double-click or focus + I; protect Sylvara and defeat the local force. The final witness-rail capstone combines still more rule families.";
    mission.briefing = {
        panel("The Emperor",
            "People of Emberhaven, lay down arms and accept stabilization. Hosts returning to formation receive pardon.",
            ""),
        panel("Sylvara",
            "The Emperor is not the target on this board. People and defectors need Pump Four and the north stair; we defend those exits.",
            "cards/Sylvara.png"),
        panel("Caltheriel",
            "In the chronicle, I lend the court light for eight minutes. The narration marks that story deadline with a Sun Sprite's flash; my role ends there, and no crown or successor receives what remains.",
            "characters/caltheriel.png"),
        panel("Prince Vesper",
            "No single bearer commands this formation. Each unit keeps its printed limits, and each lane can continue if another must withdraw.",
            "cards/princeVesper.png"),
        panel("Coach - STORY / RULES BOUNDARY",
            "Caltheriel's eight minutes and the Sun Sprite flash are chronicle events, not battle mechanics. There is no battle timer, no Sun Sprite unit, and no signal to wait for or click. No guided highlights remain: protect Sylvara and defeat the local force. The Emperor and Lash are not targets on this board.",
            "cards/starbloomKnight.png")};
    mission.aftermath = {
        panel("Vanya Bluewater",
            "Pump Four and the north stair remain open. Iria's testimony starts defections, but every soldier crosses separately and every officer answers for personal orders.",
            "cards/vanyaBluewater.png"),
        panel("Narrator",
            "Nyxara relinquishes the old nursery path instead of taking it for herself. Half her remaining wing loses its color; Vespara retreats carrying the damaged, empty chrysalis.",
            "cards/queenNyxara.png"),
        panel("Narrator",
            "As command links loosen, demon fragments begin leaving the imperial formation. Lash's debt marks, service contracts, and the defensive road he sold the Empire pull them west into Baalzapub, where his stage machinery turns them into new bearer readings.",
            ""),
        panel("The Emperor",
            "A body that can be commanded cannot be abandoned. My lattice answers for ten thousand lives. Your exits cannot answer for them all.",
            ""),
        panel("Vanya Bluewater",
            "I cannot answer for ten thousand people. I can name Jalen Orro, who crossed this white-painted line by choice. I will not hand him back.",
            "cards/vanyaBluewater.png"),
        panel("Narrator",
            "Lash has left the Emperor two choices: let the loosened fragments feed the stage, or drag them home through his own body. He turns toward Baalzapub and says, 'Recall.' Still-bound fragments return with their wounds; people who fully renounced remain outside him.",
            "")};
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
    mission.title = "Weapons on Stone";
    mission.sourceChapter = "Book Three, Chapter 35 - No Complete Bearer";
    mission.lesson = "FINAL SEELIE CAPSTONE - HIGHEST-COMPLEXITY CONTROL OBJECTIVE";
    mission.objective =
        "Control 28 board squares while Reed and Rowan survive, opening a witnessed surrender lane without defeating every imperial unit.";
    mission.hint =
        "Territory control is separate from Pavo's unit Control: each visible piece claims its occupied squares and influences all eight adjacent squares; greater adjacent influence owns an empty square, while ties keep its current owner. Reach 28 territory squares while Reed and Rowan survive.";
    mission.briefing = {
        panel("The Emperor",
            "Total Recall. All authority returns.",
            ""),
        panel("Narrator",
            "Below the burning composite, an imperial captain orders archers to shoot anyone scraping off a gold command mark. Rowan opens a surrender station while Reed holds the witness rail.",
            "cards/rowanLeafbound.png"),
        panel("Rowan",
            "Surrender station north. Approach separately. Weapons on stone.",
            "cards/rowanLeafbound.png"),
        panel("Coach",
            "This final capstone combines ordinary movement, attacks, abilities, Bodyguard, unit Control, and territory influence. Hidden pieces exert no influence. The Sister heals adjacent friend and foe. Civilians, clerks, command marks, and the surrender station are story facts - none is a piece or clickable switch.",
            "cards/reedBaelstone.png")};
    mission.aftermath = {
        panel("Chronicle",
            "The captain fires. Archers who lower their bows surrender, and eleven clerks keep writing in the ash. Every coerced command still attached returns with its wounds intact; people who freely renounced remain outside the pull.",
            "cards/rowanLeafbound.png"),
        panel("Reed",
            "The composite finds court, Mirror, light, and time closed. Its last path is mortal. Open the records and bring the rejected imperial offer.",
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
    missions.reserve(30);
    missions.push_back(mirewatchOpening());
    missions.push_back(chronicleFromEntry(MirewatchEntries[0]));
    missions.push_back(mirewatchDeployment());
    missions.push_back(mirewatchWatcher());
    missions.push_back(sharedS01());
    missions.push_back(mirewatchOffice());
    missions.push_back(mirewatchWagonRescue());
    missions.push_back(mirewatchFirstOpenCheck());
    missions.push_back(chronicleFromEntry(MirewatchEntries[4]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[5]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[6]));
    missions.push_back(mirewatchAuction());
    missions.push_back(mirewatchVaultCost());
    missions.push_back(sharedS03());
    missions.push_back(sharedS04());
    missions.push_back(mirewatchRevealReconstruction());
    missions.push_back(chronicleFromEntry(MirewatchEntries[10]));
    missions.push_back(mirewatchCapstone());
    missions.push_back(chronicleFromEntry(MirewatchEntries[11]));
    missions.push_back(chronicleFromEntry(MirewatchEntries[12]));
    missions.push_back(sharedS05());
    for (std::size_t index = 13; index < MirewatchEntries.size(); ++index)
    {
        if (index == 13)
        {
            missions.push_back(mirewatchTollAlliance());
        }
        else if (index == 19)
        {
            missions.push_back(mirewatchWorldTreeReckoning());
        }
        else
        {
            missions.push_back(chronicleFromEntry(MirewatchEntries[index]));
        }
    }
    missions.push_back(sharedS06());
    attachGeneratedScenarioArt(StoryCampaign::Mirewatch, missions);
    return missions;
}();

const std::vector<StoryMission> BlackthornMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(27);
    missions.push_back(blackthornOpening());
    missions.push_back(blackthornCustoms());
    missions.push_back(blackthornDeployment());
    missions.push_back(blackthornTerms());
    missions.push_back(blackthornS01());
    missions.push_back(blackthornOffice());
    missions.push_back(blackthornFirstOpenCheck(BlackthornEntries[3]));
    missions.push_back(blackthornS02());
    missions.push_back(chronicleFromEntry(BlackthornEntries[4]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[5]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[6]));
    missions.push_back(blackthornS03());
    missions.push_back(chronicleFromEntry(BlackthornEntries[7]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[8]));
    missions.push_back(chronicleFromEntry(BlackthornEntries[9]));
    missions.push_back(blackthornPoisonedRootRoad());
    missions.push_back(chronicleFromEntry(BlackthornEntries[10]));
    missions.push_back(blackthornS05());
    missions.push_back(chronicleFromEntry(BlackthornEntries[11]));
    missions.push_back(blackthornMonsterRules());
    missions.push_back(blackthornCapstone());
    missions.push_back(blackthornFieldJudgment());
    missions.push_back(blackthornOpenMastery());
    missions.push_back(chronicleFromEntry(BlackthornEntries[13]));
    missions.push_back(blackthornVictorReckoning());
    missions.push_back(chronicleFromEntry(BlackthornEntries[14]));
    missions.push_back(blackthornS06());
    attachGeneratedScenarioArt(StoryCampaign::Blackthorn, missions);
    return missions;
}();

const std::vector<StoryMission> SeelieMissions = [] {
    std::vector<StoryMission> missions;
    missions.reserve(61);

    missions.push_back(storyNode(
        "se00_what_mirror_remembers",
        "What the Mirror Remembers",
        "Book Two, Chapter 16 - What the Mirror Remembers",
        {
            panel("Previously - Book One",
                "Reed's Mirewatch resistance pursued Blackthorn along roads into Feyward, freed Sylvara, and broke the forced World Tree system. The victory did not end the danger around the people those roads could reach.",
                "cards/reedBaelstone.png"),
            panel("Now - Book Two",
                "Vesper is a small orange cat and the current life of a former Seelie prince. He joins Reed, Donella, Erevan, and their allies to free Caltheriel, the former Seelie ruler still alive inside a clockwork refuge whose surviving orders will not let her leave.",
                "cards/princeVesper.png"),
            panel("Narrator",
                "The Tockman is the clockwork keeper enforcing Caltheriel's custody. Lash supplied the prison roads, and copied measuring plates drain her named memories to keep those roads obedient.",
                "characters/caltheriel.png"),
            panel("Prince Vesper",
                "The Mirror can show roads remembered by nine former Vespers, but they are not my commands. I am Vesper in this life. Show only a road to the Tockman that requires neither sovereign command nor a lover's presumed privilege.",
                "cards/princeVesper.png"),
            panel("Narrator",
                "Vesper rejects a direct bridge that answers to a former prince. Then Unseelie Queen Nyxara's hidden tracking mark threatens every crossing, so Marrowind - a Seelie archer riding the nine-tailed Vaeloren - cuts it. The cut destroys the direct bridge and an unknown servant road. Only the Cathedral of Last Lights remains.",
                "cards/princeVesper.png"),
            panel("Three names to carry forward",
                "Vesper - the orange cat seeking Caltheriel's freedom. Caltheriel - the prisoner whose present choice matters. Lash - supplied routes used around her prison. The next board introduces its defenders as they act.",
                "cards/princeVesper.png")
        }));
    missions.push_back(seelieCathedralDefense());
    missions.push_back(seelieBrokenBridge());
    missions.push_back(seelieRoadOfSmallLights());

    missions.push_back(storyNode(
        "se04a_lash_reads_the_road",
        "Lash Reads the Road",
        "Book Two, Chapter 19 - The Reading Room",
        {
            panel("Fizzlewick Gearwright",
                "Ten northern plates answer. The black concordance half and plate eleven answer. But the lower turn is still blank.",
                "cards/fizzlewickGearwright.png"),
            panel("Mave",
                "The delivery condition says SUCCESSFUL RELEASE. I am leaving it on the rail where neither of you can rename it.",
                ""),
            panel("Lash",
                "Then stop asking the plates where the road ends. Ask what moved tonight that never moved in our tests.",
                ""),
            panel("Fizzlewick Gearwright",
                "Thirty-six people crossed living roots in separate groups. Each group chose its stretch, and each stop left a different reading.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash",
                "Caltheriel's rescuers measured my missing turn for me. Send the reading south. Let them keep calling it an escape.",
                "")
        }));

    missions.push_back(seelieReadingRoomRehearsal());

    missions.push_back(storyNode(
        "se05_court_behind_curtain",
        "The Court Behind the Curtain",
        "Book Two, Chapter 20 - The Court Behind the Curtain",
        {
            panel("Prince Vesper",
                "The curtain may take only what I offer. I am Vesper in this life. It receives no old title and no prettier name.",
                "cards/princeVesper.png"),
            panel("Queen Nyxara",
                "Caltheriel accepted my protection when her light was dying. Lash bought the roads that carried it. Why should one later refusal erase that assent?",
                "cards/queenNyxara.png"),
            panel("Lady Mirrorglass",
                "Because the Mirror shows a missing choice. The copied agreement preserves protection and removes withdrawal.",
                "cards/ladyMirrorglace.png"),
            panel("Queen Nyxara",
                "Give me Ivara's private identity and I will open the road to the keeper who witnessed the change.",
                "cards/queenNyxara.png"),
            panel("Erevan the Shadow",
                "No. Ivara's evidence may travel under limits. Her private identity is not our fare.",
                "cards/erevanTheShadow.png"),
            panel("Archivist Mosswake",
                "Enter it witnessed, then. The road opens, the dispute remains, and every offered thing is written beside every refusal.",
                "cards/archivistMosswake.png")
        }));

    missions.push_back(storyNode(
        "se06_honey_without_hunger",
        "Honey Without Hunger",
        "Book Two, Chapter 21 - Honey Without Hunger",
        {
            panel("Narrator",
                "Behind the black curtain stands Queen Nyxara, the insect sovereign of the Unseelie Court. She defends Caltheriel's original refuge as binding even after Caltheriel asks to leave. Nettle proposes a smaller test of that claim with an exit chosen in advance.",
                "cards/queenNyxara.png"),
            panel("Nettle Starbright",
                "One spoon. Ten minutes. Two taps, pause, one means take me out, even if I argue that I no longer need the exit.",
                "cards/nettleStarbright_mounted.png"),
            panel("Sister Honeygrave",
                "The relief is real. Stay, and you need not carry pain or loneliness as one small body ever again.",
                "cards/sisterHoneygrave.png"),
            panel("Nettle Starbright",
                "It is gone. All of it. Pavo, stop counting. I can finally think without pain.",
                "cards/nettleStarbright_mounted.png"),
            panel("Pavo Quickstep's slate",
                "Ten minutes. You chose the signal before the honey. I am keeping that promise, not choosing your life for you.",
                "cards/pavoQuickstep.png"),
            panel("Nettle Starbright",
                "I hate you for one breath. Use the signal anyway.",
                "cards/nettleStarbright_mounted.png"),
            panel("Nettle Starbright",
                "I still want another spoon. I also still choose the exit. Write both.",
                "cards/nettleStarbright_mounted.png")
        }));

    missions.push_back(seelieNyxaraInfestReconstruction());

    missions.push_back(seelieQueensPriceRehearsal());

    missions.push_back(storyNode(
        "se08a_sella_receives_herself",
        "Sella Receives Herself",
        "Book Two, Chapter 23 - The Clockwork Domain",
        {
            panel("Sella Morn",
                "I withdraw from this cell, this duty, and every keeper who claims I need permission to leave.",
                ""),
            panel("The Tockman",
                "Release requires a receiving custodian. Name the office or person accepting responsibility.",
                "cards/theTockman.png"),
            panel("Sella Morn",
                "Sella Morn receives Sella Morn. I am not an office, and I am enough.",
                ""),
            panel("Narrator",
                "Lord Pallid Thread is a name-eater working through the prison's records. If he erases the link between Sella's name and her living self, the Tockman can claim that no receiving person exists.",
                "cards/lordPallidThread.png"),
            panel("Narrator",
                "The Warden strikes. Sella falls to one knee but remains conscious and breathing. Pallid's mouth crosses the page and her written name vanishes, but the witnesses still know who chose to leave.",
                "cards/lordPallidThread.png"),
            panel("Pavo Quickstep's slate",
                "Say it aloud. Sella Morn withdrew. Sella Morn receives herself. One voice can be eaten; six witnesses cannot. The Tockman accepts the living withdrawal, opens the road, and Sella crosses under her own name.",
                "cards/pavoQuickstep.png"),
        }));

    missions.push_back(storyNode(
        "se08_nine_lives_one_choice",
        "Nine Lives, One Choice",
        "Book Two, Chapter 24 - Nine Lives, One Choice",
        {
            panel("Prince Vesper",
                "Open only the ninth life. If I lose this room, your names, or our purpose, close it without waiting for my consent to sound graceful.",
                "cards/princeVesper.png"),
            panel("Lady Mirrorglass",
                "The ninth road opens. The other eight will close permanently when you cross.",
                "cards/ladyMirrorglace.png"),
            panel("The Tockman",
                "Original condition recovered: protection ends by withdrawal or stabilization. No substitute or inherited assent may continue it.",
                "cards/theTockman.png"),
            panel("Prince Vesper",
                "Caltheriel, if one prisoner must remain, let the road take me instead.",
                "cards/princeVesper.png"),
            panel("Caltheriel",
                "No. I will not purchase freedom with another solitary captive. Return my light through willing bearers, each with a separate exit.",
                "characters/caltheriel.png"),
            panel("Prince Vesper",
                "Then I accept your answer. I came for the woman here, not the court that remembers us.",
                "cards/princeVesper.png")
        }));

    missions.push_back(storyNode(
        "se09_dusk_crown",
        "The Dusk Crown",
        "Book Two, Chapter 25 - The Dusk Crown",
        {
            panel("Pavo Quickstep's slate",
                "Eleven shares remain separate. Open your hand to withdraw. No bearer inherits another bearer's light.",
                "cards/pavoQuickstep.png"),
            panel("Archivist Mosswake",
                "A southern instrument answers every release. Someone is measuring where each share begins and ends.",
                "cards/archivistMosswake.png"),
            panel("Caltheriel",
                "Tell every bearer. They may still withdraw. I will not hide the danger, and I will not remain in this prison for Lash's convenience.",
                "characters/caltheriel.png"),
            panel("Archduchess Vespara",
                "My chrysalis will carry what your scattered lights cannot. A court needs one heir, not eleven frightened refusals.",
                "cards/archduchessVespara.png"),
            panel("Caltheriel",
                "There is no child in that shell. There is only your desire for one, wearing stolen light.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "Vespara tears the empty chrysalis. Light rips from Caltheriel's right shoulder, but no heir forms and the separate releases continue.",
                "")
        }));

    missions.push_back(storyNode(
        "se10a_caltheriel_walks_out",
        "Caltheriel Walks Out",
        "Book Two, Chapter 26 - No Crown Can Hold Light",
        {
            panel("Pavo Quickstep's slate",
                "My share ends here. I open my hand. Do not pass it to another bearer.",
                "cards/pavoQuickstep.png"),
            panel("Nettle Starbright",
                "Pavo's light is out. I am waiting for the full signal. A missing share is not permission to fill it.",
                "cards/nettleStarbright_mounted.png"),
            panel("Queen Nyxara",
                "Give me one hour of unified command. I can force every remaining path open.",
                "cards/queenNyxara.png"),
            panel("Caltheriel",
                "No crown. No substitute. Open the separate exits and leave every empty place empty. I am walking now.",
                "characters/caltheriel.png"),
            panel("Prince Vesper",
                "You are here. May I come closer as the person I am now?",
                "cards/princeVesper.png"),
            panel("Caltheriel",
                "Yes. No throne and no remembered promise. Come closer.",
                "characters/caltheriel.png")
        }));

    missions.push_back(seelieNoCrownRehearsal());

    missions.push_back(storyNode(
        "se11a_blackthorn_unmade",
        "Blackthorn Unmade",
        "Book Two, Chapter 27 - Four Judgments in One Day",
        {
            panel("Magistrate",
                "Blackthorn authority over labor is void. Authority over debt, roads, grain, and custody will be heard separately.",
                ""),
            panel("Public witnesses",
                "Labor to the worker trust. Debt to restitution. Roads and grain to public custody. Prisoners to reviewed care.",
                ""),
            panel("Reed Baelstone",
                "I surrender every remaining Baelstone and Blackthorn property claim. I accept no estate and no office in exchange.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The final seal breaks. Beneath the floor, a distant road instrument answers with eleven slow pulses.",
                ""),
            panel("Archivist Mosswake",
                "The Company is gone. Its roads, contracts, and copied measurements are not. Keep the case open.",
                "cards/archivistMosswake.png")
        }));

    missions.push_back(storyNode(
        "se11b_city_refuses",
        "The City Refuses",
        "Book Two, Chapter 28 - The City at the Foot of Fire",
        {
            panel("Archivist Mosswake",
                "This failed replacement pin bears the imperial mark. When it broke, the warm channel opened onto a road wrapped around Aelon's living root.",
                "cards/archivistMosswake.png"),
            panel("Caltheriel",
                "The road follows the scar from my release. Lash measured our separate exits and found this approach.",
                "characters/caltheriel.png"),
            panel("Factor Lume",
                "The Empire will restore heat and seal the breach. Military control ends when the Emperor certifies order restored.",
                ""),
            panel("Council witness",
                "Then it has no exit we control. Emberhaven refuses.",
                ""),
            panel("Captain Asha Renn",
                "The troop orders predate this vote. Refusal did not cause the invasion. Two divisions and three demon-bound companies were already moving.",
                ""),
            panel("Archivist Mosswake",
                "Enter the distinction. Lash has a road toward Aelon, the Empire has an army, and dawn is the meeting point.",
                "cards/archivistMosswake.png")
        }));

    missions.push_back(storyNode(
        "se11_price_of_an_army",
        "The Price of an Army",
        "Book Two, Epilogue - The Price of an Army",
        {
            panel("Fizzlewick Gearwright",
                "The release scar gives us the lower turn. It does not give entry, living-root permission, or the missing counter-lock.",
                "cards/fizzlewickGearwright.png"),
            panel("Imperial factor",
                "Then you have sold us an incomplete road.",
                ""),
            panel("Lash",
                "I have sold you the only road Emberhaven cannot close without wounding what it means to defend.",
                ""),
            panel("Imperial factor",
                "Two southern divisions. Three demon-bound companies. Protected movement, and contested first claim over anything found below.",
                ""),
            panel("Lash",
                "Add Southwater and the right to stage my own crews. Seal it now. Your army moves at dawn.",
                "")
        }));

    missions.push_back(storyNode(
        "se11c_tavin_says_stop",
        "Tavin Says Stop",
        "Book Two, Epilogue - The Price of an Army",
        {
            panel("Lash",
                "Hold the post through the next return, Tavin. We need one living ground before the imperial observers arrive.",
                ""),
            panel("Tavin",
                "The frame is ungrounded. I invoke my release. Open the catch.",
                ""),
            panel("Narrator",
                "The catch does not move. Current crosses Tavin's body, and his right arm strikes the boards before the rest of him falls.",
                ""),
            panel("Lash",
                "Clear him and reset the post. The test still owes us a return.",
                ""),
            panel("Mave",
                "No. His wages, treatment, independent examination, and authority over any return are posted conditions. The room has witnesses.",
                ""),
            panel("Fizzlewick Gearwright",
                "Failed release entered. The machine kept a bearer after he withdrew. That fault belongs to the design.",
                "cards/fizzlewickGearwright.png")
        }));

    missions.push_back(storyNode(
        "se12_first_boats_burn",
        "The First Boats Burn",
        "Book Three, Chapter 1 - The First Boats Burn",
        {
            panel("Birdie the Wise",
                "Those refugee boats ride wrong. Close the inner landing for inspection, move the families west, and do not fire through refugee cloth.",
                "cards/birdieTheWise.png"),
            panel("Dock lookout",
                "Scouts are under the coverings. The medicine sacks hold powder kegs, and blue fire is already crossing the quay.",
                ""),
            panel("Kessa Vey",
                "The bell wants my sword at the gate. I can hold my wrist. I cannot hold it long.",
                ""),
            panel("Varo Sen",
                "My refusal is mine. I turn the powder boat now. Get the families behind stone.",
                ""),
            panel("Birdie the Wise",
                "Varo is down. Break the clapper, not Kessa. Rescue first, then copy every line of that false emergency order.",
                "cards/birdieTheWise.png"),
            panel("Fey Messenger",
                "Twelve ships confirmed, seven more possible. I will carry both numbers and Kessa's warning to Emberhaven.",
                "cards/feyMessenger.png")
        }));

    missions.push_back(storyNode(
        "se12a_defender_for_hire",
        "A Defender for Hire",
        "Book Three, Chapter 2 - A Defender for Hire",
        {
            panel("Reed Baelstone",
                "We need your harbor barrier and the warm route tonight. Need is not consent to anything beyond those tasks.",
                "cards/reedBaelstone.png"),
            panel("Lash",
                "A barrier without access is scenery. Give my crews the roads, roots, prisoners, and court contacts required to keep it standing.",
                ""),
            panel("Reed Baelstone",
                "No. Each task receives a public route, an end time, and its own witnesses. No Root Key, court, prison, or refugee authority.",
                "cards/reedBaelstone.png"),
            panel("Lash",
                "And when one task fails, who becomes responsible for the whole?",
                ""),
            panel("Archivist Mosswake",
                "No one. Failure returns to the table as a new decision. It does not create a complete bearer.",
                "cards/archivistMosswake.png"),
            panel("Lash",
                "Very well. I accept the narrow doors exactly as written. Keep my copy legible.",
                "")
        }));

    missions.push_back(storyNode(
        "se13a_pearl_above_stage",
        "The Pearl Above the Stage",
        "Book Three, Chapter 3 - The Pearl Above the Stage",
        {
            panel("Erevan the Shadow",
                "Five objects, five labels: milk-white pearl, black concordance half, plate eleven, false clear crescent, and an empty bearer rig.",
                "cards/erevanTheShadow.png"),
            panel("Archivist Mosswake",
                "The pearl stores craft and one recorded voice. It is not a person, soul, parent, or source of permission.",
                "cards/archivistMosswake.png"),
            panel("Donella of the Marsh",
                "The black half reads active routes. Plate eleven supplies one measurement. Neither is the Root Key living inside Aelon.",
                "cards/donellaOfTheMarsh.png"),
            panel("Archivist Mosswake",
                "The blue fishbone key is elsewhere and may make seven corrections once. It opens no person, prison, Mirror, or root.",
                "cards/archivistMosswake.png"),
            panel("Narrator",
                "The empty rig turns toward the black half. A hidden imperial map brightens beneath the stage.",
                ""),
            panel("Erevan the Shadow",
                "Lash is not collecting relics. He is training one machine to call every separate authority a single bearer.",
                "cards/erevanTheShadow.png")
        }));

    missions.push_back(storyNode(
        "se13b_cut_sylvara_chooses",
        "The Cut Sylvara Chooses",
        "Book Three, Chapter 4 - Fever in the Roots",
        {
            panel("Sylvara",
                "The vibration crosses every broad cure. Cut widely and you injure all of Aelon. I choose the bath-root line beneath your hand.",
                "cards/Sylvara.png"),
            panel("Caltheriel",
                "Name the boundary and the cost again. I will not let urgency enlarge your consent.",
                "characters/caltheriel.png"),
            panel("Sylvara",
                "This line only. The wells remain. The baths lose heat, and the cut may expose a road below them.",
                "cards/Sylvara.png"),
            panel("Caltheriel",
                "This line and no other. My light bears the cut only until the root separates.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "Caltheriel severs the chosen line. The baths cool, her light contracts toward her body, and a dark road opens toward the wells.",
                ""),
            panel("Sylvara",
                "The larger wound is stopped. The smaller wound is real. Record both before anyone calls this clean.",
                "cards/Sylvara.png")
        }));

    missions.push_back(seeliePearlAndRootRehearsal());

    missions.push_back(seelieRulesUnderPressure());

    missions.push_back(storyNode(
        "se15a_bell_inside_soldier",
        "The Bell Inside the Soldier",
        "Book Three, Chapter 5 - The Bell Inside the Soldier",
        {
            panel("Kessa Vey",
                "The bell says take the gate, kill its holder, and join the south line. Bind my sword arm before it rings again.",
                ""),
            panel("Vanya Bluewater",
                "One blink means yes and two means no. Permanent hive joining?",
                "cards/vanyaBluewater.png"),
            panel("Kessa Vey",
                "No. No memory removal either. Varo's last act stays mine.",
                ""),
            panel("Vanya Bluewater",
                "Three minutes of silence and temporary suppression. Nothing continues without a new answer.",
                "cards/vanyaBluewater.png"),
            panel("Kessa Vey",
                "Yes. And when the command returns: I will not take this gate.",
                ""),
            panel("Narrator",
                "The shadow releases her sword wrist. Kessa's hand keeps trembling, and the copper fragment still points toward a larger imperial lattice.",
                "")
        }));

    missions.push_back(storyNode(
        "se15_bell_and_order",
        "The Order at the Door",
        "Book Three, Chapter 6 - The Order at the Door",
        {
            panel("Order clerk",
                "The sealed order names one cabinet and one root junction. This second order names every defender in the precinct.",
                ""),
            panel("Shelter runner",
                "Gas is entering the lower rooms. One hundred eighty-three people are waiting below.",
                ""),
            panel("Rowan Leafbound",
                "Four remain with the named cabinet and junction. Everyone else goes to the shelter. Enter my allocation before we move.",
                "cards/rowanLeafbound.png"),
            panel("Order clerk",
                "All one hundred eighty-three are out. Intruders used the thinner guard and took the deep-route seal.",
                ""),
            panel("Rowan Leafbound",
                "The rescue was right and the loss is mine to answer for. Remove me from field command. Put two independent witnesses at every remaining door.",
                "cards/rowanLeafbound.png")
        }));

    missions.push_back(storyNode(
        "se16a_ward_keeps_heat",
        "The Ward That Keeps Its Heat",
        "Book Three, Chapter 7 - The Ward That Keeps Its Heat",
        {
            panel("Nettle Starbright",
                "The descent harness needs the thread holding your bathhouse and dormitory. Remove it now and this roof may sag in winter.",
                "cards/nettleStarbright_mounted.png"),
            panel("Low Ward resident",
                "Who owns the repair after your expedition leaves?",
                ""),
            panel("Nettle Starbright",
                "You do. The missing thread, future brace, cost, and my limited use all stay on your public record.",
                "cards/nettleStarbright_mounted.png"),
            panel("Ward witness",
                "The ward approves one measured length under those terms.",
                ""),
            panel("Lash",
                "At the next counter: clear these debts and register the Flint Yard passage as a drain easement.",
                ""),
            panel("Nettle Starbright",
                "That drain meets the road we just bounded. Mark Lash's easement beside our thread before either one moves.",
                "cards/nettleStarbright_mounted.png")
        }));

    missions.push_back(storyNode(
        "se16_army_that_can_resign",
        "An Army That Can Resign",
        "Book Three, Chapter 8 - Terms with Monsters",
        {
            panel("Caltheriel",
                "I will not reclaim the Seelie throne. Ask for work I can limit, not a crown that claims the rest of me.",
                "characters/caltheriel.png"),
            panel("Prince Vesper",
                "My eight closed lives remain closed. I bring this life, this body, and no inherited office.",
                "cards/princeVesper.png"),
            panel("Archivist Mosswake",
                "Every role receives an end, a witness, and no automatic successor. Even the captured Gloom Fairy keeps review and the right to refuse a name.",
                "cards/archivistMosswake.png"),
            panel("Reed Baelstone",
                "I delayed the Lower Wells inspection. Medicine goes now. The delay and my reason remain together in the record.",
                "cards/reedBaelstone.png"),
            panel("Pavo Quickstep's slate",
                "Then we are not one bearer. We are an army whose members can stop, fail, hand work onward, or resign.",
                "cards/pavoQuickstep.png"),
            panel("Imperial envoy Sorn",
                "An army that can resign. Yes. The Emperor's will not, and he already knows where your exits lead.",
                "")
        }));

    missions.push_back(seelieWarmChannel());

    missions.push_back(storyNode(
        "se18a_complete_is_a_label",
        "Complete Is a Label",
        "Book Three, Chapter 10 - A Complete Bearer",
        {
            panel("Isca Roen - plain-language record",
                "A receiver is a body the system uses to hold many people's powers. A bearer is the one person made to carry them. The imperial trial forced all eleven powers into me after I said no, then labeled me COMPLETE.",
                ""),
            panel("Erevan the Shadow",
                "COMPLETE does not mean whole, willing, or worthy. It is only the machine's label for the single person it has been trained to obey.",
                "cards/erevanTheShadow.png"),
            panel("Narrator",
                "The witnesses build a public model with eleven separate holders. Its center can coordinate their actions, but no machine can turn eleven separate choices into one person's consent.",
                ""),
            panel("Lord Pallid Thread",
                "How useful. Put every defense into one account, and I need eat only one.",
                "cards/lordPallidThread.png"),
            panel("Narrator",
                "Pallid consumes one copy and the names of two Southwater witnesses. Other copies survive. A Hushkeeper silences his escape route while Wardens close an open cage around him.",
                ""),
            panel("Archivist Mosswake",
                "I cannot restore the eaten names. I can preserve the descriptions their owners choose and leave two visible empty rings where invention would become another theft.",
                "cards/archivistMosswake.png")
        }));

    missions.push_back(storyNode(
        "se18b_kessas_own_no",
        "Kessa's Own No",
        "Book Three, Chapter 11 - Two Shadows in One Skin",
        {
            panel("Kessa Vey",
                "The imperial fragment inside me carries a remote command called Recall. Cut only that command road. No hive, no memory work, and stop if my breath stops or I say Varo's name twice.",
                ""),
            panel("Caltheriel",
                "A fast burn might destroy the fragment. It might also stop your heart or take the living nerve that moves your hand. I cannot promise the light will know the difference.",
                "characters/caltheriel.png"),
            panel("Donella of the Marsh",
                "The last copper strand lies against that nerve. Stop or continue?",
                "cards/donellaOfTheMarsh.png"),
            panel("Kessa Vey",
                "Continue without burning the nerve. You do not carry my hand back to him. My hand remains mine.",
                ""),
            panel("Narrator",
                "Kessa's refusal breaks the final command strand. The separated fragment is sealed inside clear plates with several independent releases; no healer or officer controls the whole container.",
                ""),
            panel("Kessa Vey",
                "Write likely permanent beside the tremor. Then write that I refused a second cut.",
                "")
        }));

    missions.push_back(storyNode(
        "se18c_before_next_dawn",
        "Before the Next Dawn",
        "Book Three, Chapter 12 - Before the Next Dawn",
        {
            panel("Narrator",
                "On the council steps, imperial soldiers open a red chest containing clean wheat, beans, fever bark, and bandages—the supplies Emberhaven lacks most.",
                ""),
            panel("Jarek Sorn, imperial envoy",
                "Accept imperial protection and the food corridors remain open. The Empire receives the harbor, warm channels, underroot surveys, returned fragment hosts, and a hereditary emergency office that cannot resign.",
                ""),
            panel("Reed Baelstone",
                "Then the food is attached to surrender. Every group named in those terms speaks before the council answers.",
                "cards/reedBaelstone.png"),
            panel("Olla Pere, lift mechanic",
                "I want the food. I want my husband home. I will not trade Kessa for either. Enter all three.",
                ""),
            panel("Narrator",
                "The witnesses refuse the separated terms. Sorn closes the chest and declares that Emberhaven has rejected relief before dawn.",
                ""),
            panel("Narrator",
                "Artillery begins before he reaches the gate. A nearby signal rack flashes an unlabeled fourth command and turns a city gun toward Aelon. The crew cuts its own cable after the shot collapses a root gallery around two missing surveyors.",
                "")
        }));

    missions.push_back(storyNode(
        "se18d_kindness_without_debt",
        "Kindness Without Debt",
        "Book Three, Chapter 13 - The Kindness He Kept",
        {
            panel("Reed Baelstone",
                "This visit is public and recorded. I am here for one fact, not forgiveness and not a bargain.",
                "cards/reedBaelstone.png"),
            panel("Thaeron Baelstone",
                "Your emergency grain count will count sacks already promised elsewhere. Add a separate pledged column before anyone hears the total.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "That advice is useful and entered without payment. Now tell me what controls the old trunk road beneath the city.",
                "cards/reedBaelstone.png"),
            panel("Thaeron Baelstone",
                "A family permission does. It is an old public grant allowing a living Baelstone claimant to open the trunk route. The claimant may instead surrender that permission permanently before the road keepers.",
                "cards/thaeronBaelstone.png"),
            panel("Thaeron Baelstone",
                "I can also tell you which memories of your mother were truly hers. No condition. No price. I am the only living person who knows.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "Enter that the offer was unconditional and that I declined to hear it. Keep the grain correction and road facts public. The memories remain yours.",
                "cards/reedBaelstone.png")
        }));

    missions.push_back(storyNode(
        "se18_what_complete_means",
        "A Promise Can Breathe",
        "Book Three, Chapter 14 - A Promise Cannot Breathe",
        {
            panel("Narrator",
                "Before Ebbin speaks, Lady Silkmaw has bound the whole courier relay into a vow-web: an open-ended promise made by one courier pulls every courier sharing that route into the same obligation.",
                "cards/ladySilkmaw.png"),
            panel("Ebbin - FEY MESSENGER",
                "Black oil still stains my legs from Mirewatch, and the outer edge missing from one wing makes every hover bank left. Pavo asks where I will stop. I answer: until I deliver it. Silver closes around my wrist and runs down the shared relay.",
                "cards/feyMessenger.png"),
            panel("Narrator",
                "Anwen's shelter-count road has already closed. Because Ebbin named no stopping point, Silkmaw's vow-web makes every courier keep pushing against every closed leg. It tightens through Anwen until she falls; bells on her frost-damaged feet let rescuers find her, but she does not return.",
                "cards/ladySilkmaw.png"),
            panel("Lady Silkmaw",
                "Give the whole relay one lasting keeper, Pavo. Promise me that office, and I will return the voice you lost.",
                ""),
            panel("Pavo Quickstep",
                "NO. ONE STRIP. ONE NAMED LEG. THE BEARER MAY REFUSE, RETURN IT, OR REPORT WHERE IT STOPPED.",
                "cards/pavoQuickstep.png"),
            panel("Ebbin",
                "I accept this message only to the next table. If that road is closed, I bring it back.",
                ""),
            panel("Ruvan - FEY MESSENGER",
                "My sister vanished on a Mirror road last winter. I swore never to abandon another courier's work. If I stop, she becomes the reason I failed twice.",
                ""),
            panel("Ruvan - FEY MESSENGER",
                "This next road is flooded; refusing it keeps another bearer out of the water. I withdraw it. I cannot carry her miles as well as mine.",
                ""),
            panel("Narrator",
                "Quinberry steals the web's anchor bead from the oldest bell. Pavo's single word—outside—passes from bearer to bearer until the lark drops the bead into a pool that recognizes no owner. The web breaks; Anwen remains dead.",
                "cards/quinberryLark.png"),
            panel("Pavo Quickstep",
                "Three trays: WAITING, CARRIED, STOPPED. Every handoff receives its own entry, and every failure keeps its last witness. The dead do not return, but no new promise binds another courier to their road.",
                "cards/pavoQuickstep.png")
        }));

    missions.push_back(seelieCryptDefense());

    missions.push_back(storyNode(
        "se20_harness_anyone_can_leave",
        "A Harness Anyone Can Leave",
        "Book Three, Chapter 16 - A Harness Anyone Can Leave",
        {
            panel("Narrator",
                "At a Cinderworks test table, Nettle holds the harness's central lever while Loma pulls a side release. Nothing opens. When Nettle lets go, all seven lines spring free together.",
                ""),
            panel("Nettle Starbright",
                "One master release is another trap. Each load segment needs an exit its own bearer can reach, even while somebody else holds the brake.",
                "cards/nettleStarbright_mounted.png"),
            panel("Loma Reedhook, civilian rigger",
                "And how does my sister survive if a bearer releases while her hand is on that brake?",
                ""),
            panel("Nettle Starbright",
                "I know what it is to have somebody else's machine call my body cargo. The bearer keeps the release.",
                "cards/nettleStarbright_mounted.png"),
            panel("Loma Reedhook, civilian rigger",
                "Your experience does not make my question pity. Put this glove in the brake and watch what your design does to it.",
                ""),
            panel("Nettle Starbright",
                "You're right. I answered before I looked. Add a recoil shield, move the catch, and leave the casing clear so every tester can see the danger and the exit.",
                "cards/nettleStarbright_mounted.png")
        }));

    missions.push_back(storyNode(
        "se21a_cinderworks_choice",
        "Leave the Winter Glass",
        "Book Three, Chapter 17 - The Cinderworks",
        {
            panel("Narrator",
                "At the glass-kiln support, one vibration charge has already burst. Water rises around two workers holding pressure doors that protect Emberhaven's winter panes.",
                ""),
            panel("Cinderworks glassworker",
                "If we leave now, the doors stay open and the stored panes crack. Can you stop the second charge before the floor floods?",
                ""),
            panel("Nettle Starbright",
                "Likely, not certain. Is anyone else still inside the kiln row?",
                "cards/nettleStarbright_mounted.png"),
            panel("Cinderworks glassworker",
                "Only us. Everyone else is clear.",
                ""),
            panel("Reed Baelstone",
                "Then both of you leave the doors. Come up alive. We will answer for the windows afterward.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The workers climb out. Nettle disarms the second charge, but the row floods and the panes crack one after another. Saving the workers also costs the expedition its scheduled lift.",
                "")
        }));

    missions.push_back(storyNode(
        "se21_below_last_stair",
        "The Lower Wells",
        "Book Three, Chapter 18 - The Lower Wells",
        {
            panel("Kelm, Lower Wells pump keeper",
                "You delayed our medicine because your map did not verify our road. Four sounding crews waited at the gate. Your inspectors never came.",
                ""),
            panel("Reed Baelstone",
                "You were still here, and my order made waiting cost you. Stop the poisoned water first. Put the delay beside my name afterward.",
                "cards/reedBaelstone.png"),
            panel("Kelm, Lower Wells pump keeper",
                "The small valves may save the machinery, but not before the next pulse. Evacuate the farms. When the last crew clears, I break the pressure wheel.",
                ""),
            panel("Aro, Lower Wells baker",
                "Bread goes on doors, people go in the empty ore bins, and the seed is divided among several carriers. Nobody waits for one perfect cart.",
                ""),
            panel("Narrator",
                "The final farm crew clears. Kelm breaks the wheel that supplies the settlement's heat, water, farms, and ovens. Lower Wells goes dark, but the contaminated pulse stops.",
                ""),
            panel("Narrator",
                "The count closes at 206 living, three recovered bodies, and four missing. A captured route-cylinder records Lash's galleries and the bodies used to test their loads; the settlement itself cannot be restored by that evidence.",
                "")
        }));

    missions.push_back(storyNode(
        "se22a_map_eater",
        "The Map Eater",
        "Book Three, Chapter 19 - The Map Eater",
        {
            panel("Erevan the Shadow",
                "The road name vanished under my hand. The stone remains, but the mark no longer tells anyone where it leads.",
                "cards/erevanTheShadow.png"),
            panel("Archivist Mosswake",
                "Lord Pallid is a map eater. He consumes the relation between a record and what it identifies. Breaking the stone or copying the same label will not restore that relation.",
                "cards/archivistMosswake.png"),
            panel("Lord Pallid Thread",
                "One alias, one road, one clean absence. You will remember traveling and lose the way home.",
                "cards/lordPallidThread.png"),
            panel("Erevan the Shadow",
                "He ate old aliases and the doors attached to them. Record the gaps. Do not invent faces or streets to make my history look whole.",
                "cards/erevanTheShadow.png"),
            panel("The Tockman",
                "HARNESS LABELS LOST. VERIFY EACH RELEASE BY THE LOAD IT ACTUALLY CARRIES.",
                "cards/theTockman.png"),
            panel("Archivist Mosswake",
                "I will grow the record in seven incomplete lines. He may eat one while six remain—but the spring councils, grove fire, and old bird names this costs me will not return.",
                "cards/archivistMosswake.png")
        }));

    missions.push_back(storyNode(
        "se22b_city_spends_at_night",
        "What the City Spends at Night",
        "Book Three, Chapter 20 - What the City Spends at Night",
        {
            panel("Liora Kest",
                "A bridged host is a person whose implanted fragment can open a road for the Empire. Give every shelter knight standing permission to shoot one before suppression arrives.",
                ""),
            panel("Pavo Quickstep",
                "NO. I cannot make fear of a future opening into permission to kill every host at a door. I do not yet have a complete safer answer.",
                "cards/pavoQuickstep.png"),
            panel("Narrator",
                "At the same relief line, Lash arrives with four hundredweight of onions. Hungry people welcome him, and he serves eleven bowls before stepping aside.",
                ""),
            panel("Lash",
                "Your public broadside assigns the winch gallery to the wrong one of my two houses. Correct it. Also enter Emmet Drane: blue work sleeve, former stagehand, dismissed after taking outside work.",
                ""),
            panel("Cera Mott, public examiner",
                "I am entering the correction, the name, the house that employed him, and the fact that giving us this evidence benefits you.",
                ""),
            panel("Pavo Quickstep",
                "SEND DRANE'S NAME TO EVERY STOPPED QUAY ROUTE. ACCEPT THE ONIONS UNDER THE PUBLIC RELIEF SCHEDULE. RECORD THE HELP AND WHAT IT BUYS.",
                "cards/pavoQuickstep.png")
        }));

    missions.push_back(storyNode(
        "se22c_orchard_memory",
        "The Orchard Memory",
        "Book Three, Chapter 21 - The Cathedral of Last Lights",
        {
            panel("Duke Lanternwing",
                "These memorial lights hold the final sights of the dead. You ask me to release Tovan Voss's last sight. I require an equal memory freely offered by a present custodian.",
                ""),
            panel("Prince Vesper",
                "Caltheriel once told me about an orchard where she laughed at a singer's terrible shot. I offer the night she told me—but only if she separately offers the orchard itself.",
                "cards/princeVesper.png"),
            panel("Caltheriel",
                "I consent to the orchard and the telling we have named. Close the shared Mirror if the taking reaches any other memory.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "The two memories meet in the glass and go dark. Caltheriel still knows she told Vesper about an orchard; neither lover can remember the orchard or that evening.",
                ""),
            panel("Prince Vesper",
                "I cannot get the night back. I still know you count my breaths when I fly, and I still choose the life we are in.",
                "cards/princeVesper.png"),
            panel("Narrator",
                "Lanternwing opens the lower stair. Voss's released sight shows Lash making first contact with the prison material, cutting Voss's return line, and leaving a service arch marked by seven separate paths.",
                "")
        }));

    missions.push_back(storyNode(
        "se22_memory_and_last_road",
        "The Last Road",
        "Book Three, Chapter 22 - Below the Last Road",
        {
            panel("Iria Sol, imprisoned quartermaster",
                "Nine of these twelve posts hold living people. Three hold bodies. Cut the common command first, then let each survivor choose a road and whatever restraint still travels with them.",
                ""),
            panel("Narrator",
                "The common chain breaks. Some collars open and others tighten, but nine living prisoners leave their posts before Bristlejacks and a Moonshade Stalker reach the crossing.",
                ""),
            panel("Narrator",
                "The Stalker strikes at the final three prisoners on the fungal bridge. Vaeloren throws them clear; one hind leg is crushed and one of his nine tails is torn away.",
                ""),
            panel("Marrowind",
                "Its throat is open. The shot is mine—and Vaeloren is alive.",
                "cards/marrowind.png"),
            panel("Narrator",
                "Marrowind lowers his bow and lifts his partner instead. Sir Chitinous Vale severs the corrupted bond in the Stalker's wing, and the creature falls as the bridge collapses beneath it.",
                ""),
            panel("Narrator",
                "Nine prisoners survive; the three occupied posts remain counted dead. The fastest road onward is gone, and Marrowind stays with Vaeloren while the others seek the north withdrawal lane.",
                "")
        }));

    missions.push_back(seelieControlledFall());

    missions.push_back(storyNode(
        "se24_first_prison_design",
        "The First Prison Was a Design",
        "Book Three, Chapter 24 - The First Prison",
        {
            panel("Caltheriel",
                "The first prison forced five pairs of powers under one ruler: light and dark, memory and forgetting, growth and boundary, time and change, hunger and restraint. The later eleven-part receiver copied that design in finer pieces.",
                "characters/caltheriel.png"),
            panel("Caltheriel",
                "I carried light and memory. Tockman held time and restraint. Aelon held growth and boundary. The prison registered the three of us as one custodial authority.",
                "characters/caltheriel.png"),
            panel("The Tockman",
                "THE FIRST GUARDIAN COULD WITHDRAW. LATER KEEPERS REMOVED THAT EXIT. I DID NOT WRITE THE CHANGE, BUT I ENFORCED THE SYSTEM AFTER IT BECAME CUSTODY.",
                "cards/theTockman.png"),
            panel("Aro, Lower Wells baker",
                "The test has put my name beside the memory ring. No. I will not give this wall my bakery or the people it already copied into stone.",
                ""),
            panel("Narrator",
                "The crew removes Aro's line and leaves its hook visibly empty. The remaining brace bends around her absence and still opens a narrow corridor.",
                ""),
            panel("A Warden",
                "MY SERVICE HOLDS UNTIL THE TWELFTH MINUTE. ASSIST AT RELEASE. IF THE GATE FALLS AFTERWARD, DO NOT CALL MY ENDING A FAILURE OF WITHDRAWAL.",
                "cards/Warden.png")
        }));

    missions.push_back(storyNode(
        "se25_three_turns_fish_rib",
        "Three Turns and a Fish Rib",
        "Book Three, Chapter 25 - Three Turns and a Fish Rib",
        {
            panel("Narrator",
                "On the eighth morning, a roof collapse traps twenty-seven people below a bent shelter door. Water is rising, and the three-key release cannot move the crushed frame.",
                ""),
            panel("Birdie the Wise",
                "Rook, cut the frame. Pell, count everyone who comes out. We are clearing the room, not searching it.",
                "cards/birdieTheWise.png"),
            panel("Narrator",
                "Twenty-six people escape. The floor falls while Birdie holds Nima's sleeve. Searchers find only the child's sharpened fish rib.",
                ""),
            panel("Birdie the Wise",
                "Write Nima Salt as missing, not dead. Leave room below her name. When we know more, we correct the line without hiding what we first knew.",
                "cards/birdieTheWise.png"),
            panel("Birdie the Wise",
                "Open the clinic store now. Pell and the nurse will count what leaves, and my public order will follow. No patient waits for a missing key.",
                "cards/birdieTheWise.png")
        }));

    missions.push_back(storyNode(
        "se25a_abandoned_receiver",
        "The Abandoned Receiver",
        "Book Three, Chapter 26 - The Abandoned Receiver",
        {
            panel("Narrator",
                "A receiver is a machine that combines separate measures into one decision. This abandoned receiver still has warm chairs, live Baalzapub cable, and eleven sockets around one empty center.",
                ""),
            panel("Nettle Starbright",
                "It calls any contact assent, even when nobody agreed. Keep every real act separate, and give every act its own way to stop.",
                "cards/nettleStarbright_mounted.png"),
            panel("Fizzlewick Gearwright",
                "Lash plans to replace the missing originals with living hosts. My regulator keeps his eleven copied measures from tearing his stage apart.",
                "cards/fizzlewickGearwright.png"),
            panel("Erevan the Shadow",
                "The counter-lock remembers Vanya's blue fishbone key. Seven independent corrections can divide the route and remove Lash's black concordance half, the glass that judges agreement.",
                "cards/erevanTheShadow.png"),
            panel("Narrator",
                "Fizzlewick tears away the regulator. The receiver burns a black route mark into Erevan's left palm, and Lash's voice finds him through it.",
                ""),
            panel("Vanya Bluewater",
                "Bring the sealed key box with its three custodians. I authorize the journey and examination only. Nobody opens it until I verify the box and the intended use.",
                "cards/vanyaBluewater.png")
        }));

    missions.push_back(storyNode(
        "se26a_hunger_has_no_face",
        "Hunger Has No Face",
        "Book Three, Chapter 27 - Hunger Has No Face",
        {
            panel("Narrator",
                "Below the receiver, the breach blurs root, stone, armor, memory, and machine. It has no face or plan. It simply occupies every complete road left open.",
                ""),
            panel("Reed Baelstone",
                "The body-shaped cavity fits my protected name. It does not offer power. It offers relief: every separate hurt would become one burden, and I would never have to choose again.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Reed crosses fully into the cavity. Rowan, Nettle, and Donella pull him free before the protected name finishes forming. He returns with cold bands across both feet; the lost sensation never returns.",
                ""),
            panel("Nettle Starbright",
                "Leave one harness segment unattached. A vacancy is a place kept empty on purpose. If the route holds around it, no person has to become the whole system.",
                "cards/nettleStarbright_mounted.png"),
            panel("Briar Whisperthorn",
                "My hidden house road can carry one return load, then close forever. I choose to break it open; afterward we walk home.",
                "cards/briarWhisperthorn.png"),
            panel("Narrator",
                "Tockman holds the door until the harness clears. His body breaks, two outer gears tear away, his withdrawal hand freezes open, and his main clock stops. He gives no answer but remains present as the tendril enters Emberhaven's one living root.",
                "cards/theTockman.png")
        }));

    missions.push_back(storyNode(
        "se26b_burning_root",
        "The Burning Root",
        "Book Three, Chapter 28 - The Burning Root",
        {
            panel("Birdie the Wise",
                "This root can carry thirty-two shelter residents or the returning expedition, not both. We will interleave them: one civilian group, then one expedition segment.",
                "cards/birdieTheWise.png"),
            panel("Narrator",
                "Children, patients, records, companions, and Tockman's broken body move in separate loads. Every bearer keeps a visible release, and nobody inherits another person's share without choosing it.",
                ""),
            panel("Birdie the Wise",
                "Caro and two children are still missing from this count. Tell the lower line why we are holding. Nobody becomes dead because the clock prefers a finished number.",
                "cards/birdieTheWise.png"),
            panel("Narrator",
                "Caro brings both children out. Larkspur frees the strap caught around Tockman, but the root closes across her. Her pulse stops; all thirty-two residents and every living expedition member clear while Tockman remains broken and unresponsive.",
                ""),
            panel("Sylvara",
                "The living count is clear. Aelon and I choose this cut. The infected root and a residential district burn; I survive, permanently changed.",
                "cards/Sylvara.png"),
            panel("Narrator",
                "After the cut, the Weaver reseats Tockman's frozen withdrawal hand. His governing gear stays cracked and he cannot move himself, but the hand closes and opens and his bell resumes exact intervals. Later borrowed time can end without pretending his body was restored.",
                "cards/theTockman.png")
        }));

    missions.push_back(storyNode(
        "se26_vacancy_that_holds",
        "A Vacancy That Holds",
        "Book Three, Chapter 29 - Terms That Can Be Refused",
        {
            panel("Caltheriel",
                "I carry my remaining court light, and that portion only. Record the dark spaces too. They are part of my present measure.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "Nyxara offers one hour of total court command. A rigger refuses. Vanya records REFUSED and leaves the next line open; refusal does not become consent through urgency.",
                "cards/queenNyxara.png"),
            panel("Maggie Mudroot",
                "Mangletooth refuses the Gilded Hold. It once held people whose guards controlled every exit. Send him better terms; do not turn this refusal into permission.",
                "cards/maggieMudroot.png"),
            panel("Reed Baelstone",
                "Read me as Reed, born Baelstone, and as one witness. No title, command, or key. If the design cannot tell those apart, it does not use me.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The test reads one mortal identity and no rank. Reed's bounded term ends on time; the old house authority remains unused.",
                ""),
            panel("Narrator",
                "A Warden later ends its term, and nobody rushes into its chair. The pattern holds around the vacancy. A person can leave without another person becoming the whole answer.",
                "")
        }));

    missions.push_back(seelieEmperorAtTree());

    missions.push_back(storyNode(
        "se27a_emperor_at_whitewash",
        "The Emperor at Whitewash",
        "Book Three, Chapter 30 - The Emperor at the Tree (Result)",
        {
            panel("Vanya Bluewater",
                "Pump Four and the north stair remain usable. Iria's testimony opens defections, but every soldier must cross separately and every officer must answer for personal orders.",
                "cards/vanyaBluewater.png"),
            panel("Narrator",
                "Nyxara relinquishes the nursery path instead of transferring it. Half her remaining wing loses its color. Vespara retreats with the damaged, empty chrysalis.",
                "cards/queenNyxara.png"),
            panel("Narrator",
                "Demon fragments loosen from the imperial formation. Lash draws them toward Baalzapub through debt marks, service contracts, and the defensive road he sold the Empire.",
                ""),
            panel("Emperor",
                "A body that can be commanded cannot be abandoned. My lattice answers for ten thousand lives. Your exits cannot answer for them all.",
                ""),
            panel("Vanya Bluewater",
                "I cannot answer for ten thousand people. I can name Jalen Orro, who crossed this white-painted surrender line by choice. I will not hand him back.",
                "cards/vanyaBluewater.png"),
            panel("Narrator",
                "The Emperor says Recall. The order forces every still-bound demon fragment back toward him. Those who fully renounced remain outside it; the rest fuse into his living body as he turns toward Baalzapub.",
                "")
        }));

    missions.push_back(seelieTwoMenOneVacancy());

    missions.push_back(storyNode(
        "se28b_seven_corrections",
        "Seven Corrections",
        "Book Three, Chapter 32 - The Left Hand at the Door",
        {
            panel("Narrator",
                "The fishbone key reaches Baalzapub sealed, slowly, and through divided custody. No courier ever holds enough of the box to lift and open it alone.",
                ""),
            panel("Vanya Bluewater",
                "One removal, for one use: divide and remove Lash's black concordance half. Without that master map, he cannot join copied roads inside one bearer. This key opens nothing else.",
                "cards/vanyaBluewater.png"),
            panel("Erevan the Shadow",
                "The black glass judges agreement. The milk-white pearl stores my teacher's craft and voice, not his soul. The blue key carries corrections the receiver cannot learn by copying its shape.",
                "cards/erevanTheShadow.png"),
            panel("Narrator",
                "Seven people each perform one check: witness, body, custody, motion, reflection, memory, and place. No one carries enough authority to finish the removal alone; time stays outside.",
                ""),
            panel("Narrator",
                "The recorded mentor says, 'Hands separate. No one alone.' The black half divides into the lattice; the key breaks, the pearl goes silent, and Erevan permanently loses fine sensation in his left hand.",
                "")
        }));

    missions.push_back(storyNode(
        "se28_one_vacancy_seven_hands",
        "The Empty Chrysalis",
        "Book Three, Chapter 33 - The Empty Chrysalis",
        {
            panel("Lady Silkmaw",
                "No one will leave while another suffers. Give every unfinished name and choice to me, and I will decide the single ending.",
                ""),
            panel("Pavo Quickstep",
                "No. Leave the final place empty. Every person keeps a separate voice and the right to stop; nobody becomes the replacement who carries everyone else.",
                "cards/pavoQuickstep.png"),
            panel("Vanya Bluewater",
                "Pallid offers a clean, finished casualty record by erasing every uncertain name. No. People are still coming to the tables. Keep room for them.",
                "cards/vanyaBluewater.png"),
            panel("Archivist Mosswake",
                "This last road was mine alone. I release it so the record can travel in separate pieces. Name the loss; do not pretend the road returns.",
                "cards/archivistMosswake.png"),
            panel("Narrator",
                "Quinberry withholds the finishing note whenever Pallid reaches for it. Mosswake's divided lichen closes around the unfinished record, containing Pallid without giving him one complete account to eat.",
                "cards/quinberryLark.png"),
            panel("Narrator",
                "Briar, Vale, and Mirrorglass prove the chrysalis empty and cut its claims. Five Mirror roads close permanently. Vespara escapes. Outside, Lash's stage begins to rise.",
                "cards/briarWhisperthorn.png")
        }));

    missions.push_back(storyNode(
        "se29a_baalzapub_ascendant",
        "Baalzapub Ascendant",
        "Book Three, Chapter 34 - Baalzapub Ascendant (Part 1)",
        {
            panel("Narrator",
                "Lash rises through Baalzapub on copied readings, debt, labor, Aelon's root, and the Emperor's demon authority. His replicas repeat old answers and ignore every later refusal.",
                ""),
            panel("Lash LeGrim",
                "The breach chose me before any court knew my name.",
                ""),
            panel("Donella of the Marsh",
                "It found an open route. You needed that to mean it found you. Your body is under the church arch; the other figures are routed images.",
                "cards/donellaOfTheMarsh.png"),
            panel("Donella of the Marsh",
                "No reflected attacks. I place a medical hold until the fourth heat mark. Two people are still traveling through routes in Lash's body; kill him now and those roads close on them.",
                "cards/donellaOfTheMarsh.png"),
            panel("Narrator",
                "At the third mark, Donella is down and Vanya is trapped beneath a rope. Liora fires before the fourth mark. Her arrow strikes Lash below the collarbone.",
                ""),
            panel("Narrator",
                "Two occupied routes tear. Ruvan dies after reporting that Ebbin entered the next road; Ebbin remains missing. Lash remains alive, and Donella leaves the arrow in place to pin his mortal body-route.",
                "")
        }));

    missions.push_back(storyNode(
        "se29b_fizzlewick_says_no",
        "Fizzlewick Says No",
        "Book Three, Chapter 34 - Baalzapub Ascendant (Part 2)",
        {
            panel("Fizzlewick Gearwright",
                "The replicas cannot hold their copied reading. They need a distributed living sink: Baalzapub's workers.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash LeGrim",
                "Use the house. I have always paid everyone who keeps it running. Trigger the circuit.",
                ""),
            panel("Fizzlewick Gearwright",
                "No. The workers are not fuel.",
                "cards/fizzlewickGearwright.png"),
            panel("Narrator",
                "Fizzlewick breaks the relay and opens every worker line. Nessa's emergency release breaks Lash's grip, and the workers bind Fizzlewick. His refusal does not erase the harm his machinery caused.",
                ""),
            panel("Narrator",
                "Nessa places the crew's five nails for a voluntary blackout, then steps away. A freed Gloom Fairy chooses five breaths of darkness, giving the workers time to secure the regulator.",
                "cards/gloomFairy.png"),
            panel("Narrator",
                "The vent tears Aelon and burns archive records, but the board, rail, seven-person lattice, and vent line still hold. Lash remains alive around the arrow. Nettle prepares the release signal.",
                "")
        }));

    missions.push_back(storyNode(
        "se29c_all_authority_returns",
        "All Authority Returns",
        "Book Three, Chapter 35 - No Complete Bearer (Recall)",
        {
            panel("Nettle Starbright",
                "Zippy chose one cool road and pulled one release. He refused a second crossing, so three riggers took the next piece on separate hooks. Movement ends here; no rescue debt follows.",
                "cards/nettleStarbright_mounted.png"),
            panel("Emperor",
                "All authority returns.",
                ""),
            panel("Narrator",
                "Recall pulls at the station for people leaving demon control. Kessa, Daro, and a wounded woman each refuse for themselves. A Hushkeeper isolates the last release so pain cannot seize the next station.",
                ""),
            panel("Narrator",
                "Caltheriel reaches her eight-minute limit and ends her court light. Nyxara separately relinquishes three command paths, losing the last color in her shoulder. Neither transfers what she releases.",
                "cards/queenNyxara.png"),
            panel("Narrator",
                "Briar closes two Mirror roads. Tockman's forward time ends. The Weaver leaves two thread ends apart. Every withdrawal is recorded separately, and no vacancy inherits the previous burden.",
                "cards/theTockman.png"),
            panel("Narrator",
                "The Emperor orders total Recall. Coerced fragments return with every wound intact, while freely renounced fragments remain outside him. The fused body finds no court or reflected authority and reaches for the mortal route.",
                "")
        }));

    missions.push_back(storyNode(
        "se29d_reed_ends_the_house",
        "Reed Ends the House",
        "Book Three, Chapter 35 - No Complete Bearer (The Mortal Route)",
        {
            panel("Vanya Bluewater",
                "Every supernatural route is closed. One ordinary claim remains: Baelstone authority over nine miles. Reed, taking the Emperor's office even to renounce it would make you the next bearer.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "Cross out the offer. I am Reed, born Baelstone. I end the house's permission over these roads. I claim no command for myself or any successor.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The surrender ends only the house's authority. People still own their branches, workers are still owed wages, and Reed remains open to the same claims as everyone else.",
                ""),
            panel("Narrator",
                "With demon bonds, court commands, Mirror roads, and Baelstone ownership ended separately, the imperial magic finds nobody to carry it. Heat tears the Emperor's fused body apart.",
                ""),
            panel("Narrator",
                "Medics find no breath or pulse; two witnesses identify the central body. The Emperor is declared dead and placed under guard. Across the square, Lash is still alive around Liora's earlier arrow.",
                "")
        }));

    missions.push_back(storyNode(
        "se29_no_complete_bearer",
        "No Complete Bearer",
        "Book Three, Chapter 35 - No Complete Bearer (Final Stage)",
        {
            panel("Lash LeGrim",
                "Caltheriel served my house, Nyxara was paid to oppose me, and Reed inherited an old permission. I say those past ties still authorize one person to command the whole system: me.",
                ""),
            panel("Caltheriel",
                "The clerks leave your claim blank. My performance was never an oath to obey you forever.",
                "characters/caltheriel.png"),
            panel("Queen Nyxara",
                "I opposed you because I chose to. No contract owned my hatred.",
                "cards/queenNyxara.png"),
            panel("Reed Baelstone",
                "The title is dead. I am not.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Nettle gives the signal. Every helper ends only their own link, and each empty place stays empty. Lash falls out of the reflected body into one mortal body; Liora's arrow becomes an ordinary wound again.",
                ""),
            panel("Narrator",
                "No second body answers. Medics try to sustain him, then confirm no breath or heartbeat. Witnesses identify Lash and record the earlier arrow as fatal. He dies after the Emperor.",
                "")
        }));

    missions.push_back(storyNode(
        "se30_names_we_keep",
        "The Names We Keep",
        "Book Three, Chapter 36 - The Names We Keep",
        {
            panel("Birdie the Wise",
                "Stop tying unlike lives into one total. Missing, dead, unidentified, surrendered, arrested, and accused each get separate lines. A correction stays beside the first entry instead of erasing it.",
                "cards/birdieTheWise.png"),
            panel("Birdie the Wise",
                "Nima's body is found. Change missing to dead below the first line; do not erase it. Ebbin remains missing. Liora's shot and Ebbin's last sighting stay separate.",
                "cards/birdieTheWise.png"),
            panel("Marrowind",
                "Vespara is within reach, and a child is trapped behind the flour bin. The child first. Someone else can keep the pursuit honest.",
                "cards/marrowind.png"),
            panel("Narrator",
                "The child lives. Wardens take Vespara alive, and Nyxara makes no custody claim. The chrysalis is verified empty and nonliving before its remaining claim is cut and the shell destroyed.",
                ""),
            panel("Vanya Bluewater",
                "The Root Key stays inside Aelon. The reduced Mirror keeps its three custodians. The smoke-colored half enters a boundary vault. The broken fishbone key and silent pearl remain separate in public custody.",
                "cards/vanyaBluewater.png"),
            panel("Reed Baelstone",
                "I will serve as supply liaison for six months, then the workers and I decide whether to continue. The job carries no inherited title and no speech requirement.",
                "cards/reedBaelstone.png")
        }));

    missions.push_back(storyNode(
        "se30a_ordinary_stage",
        "An Ordinary Stage",
        "Book Three, Epilogue - An Ordinary Stage",
        {
            panel("Narrator",
                "Months later, Baalzapub reopens around its damage. New boards stop at the burned gap, and the public archive displays original records beside every correction and unresolved name.",
                ""),
            panel("Nettle Starbright",
                "My workshop has a ramp, benches at two heights, and a door that opens with an elbow. My wings did not return. Dash and Zippy choose every visit and every job.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Birdie's relief barge still carries food and corrections. Pavo serves one season as envoy; his voice does not return, and a Messenger repeats only what he whispers.",
                "cards/pavoQuickstep.png"),
            panel("Vanya Bluewater",
                "My father's memorial has no statue. His broken key and silent pearl sit in separate trays. Erevan and Donella keep separate workrooms, shared meals, and the right to ask for space.",
                "cards/vanyaBluewater.png"),
            panel("Caltheriel",
                "Vesper and I choose the life we have now. Our lost orchard does not return, and neither does my throne. We will make new memories without calling them replacements.",
                "characters/caltheriel.png"),
            panel("Reed Baelstone",
                "My post lasts six months, and no title comes with it. The kitchen serves until ninth bell. After that, Tavin and I can move one more chair together.",
                "cards/reedBaelstone.png")
        }));

    // The source chronicle above intentionally preserves the novel research
    // slices. The player-facing route below selects and rewrites those slices
    // into scene-sized chapters: one decision, one consequence, four panels.
    // Tactical missions are copied without changing any card or engine rule.
    const std::vector<StoryMission> sourceChronicle = std::move(missions);
    missions.clear();
    missions.reserve(57);
    const auto appendMission = [&](std::string_view id) {
        const auto found = std::find_if(
            sourceChronicle.begin(), sourceChronicle.end(),
            [&](const StoryMission& mission) { return mission.id == id; });
        if (found != sourceChronicle.end())
        {
            missions.push_back(*found);
        }
    };
    const auto appendScene = [&](
                                 std::string_view id,
                                 std::string_view source,
                                 std::initializer_list<StoryPanel> panels) {
        const auto found = std::find_if(
            sourceChronicle.begin(), sourceChronicle.end(),
            [&](const StoryMission& mission) { return mission.id == id; });
        if (found == sourceChronicle.end())
        {
            return;
        }
        StoryMission scene = *found;
        scene.sourceChapter = source;
        scene.briefing.assign(panels.begin(), panels.end());
        missions.push_back(std::move(scene));
    };

    // PLAYER ROUTE - ACT I
    appendMission("se00_what_mirror_remembers");
    appendMission("se01_cathedral_last_lights");
    appendMission("se02_broken_bridge");
    appendMission("se03_road_small_lights");
    appendScene(
        "se04a_lash_reads_the_road",
        "Book Two, Chapter 19 - The Reading Room",
        {
            panel("Narrator",
                "At Baalzapub - the old church Lash turned into a theater and workshop - his engineer Fizzlewick tests copied plates made from the thirty-six-person rescue route. Aelon is the living root system beneath Emberhaven; its unmeasured lower turn is the one gap in the copy.",
                "cards/fizzlewickGearwright.png"),
            panel("Fizzlewick Gearwright",
                "Ten copied plates know the upper road. The lower turn through Aelon is still blank, so the machine cannot yet reproduce the whole rescue.",
                "cards/fizzlewickGearwright.png"),
            panel("Mave",
                "I am the worker calibrating the release. My contract says I am paid when a release works, and that term stays visible to both of you.",
                ""),
            panel("Lash",
                "A route carries a person, spell, or order. By copying the measurements from thirty-six freely chosen exits, I can infer Aelon's missing turn without asking the people who made it.",
                ""),
            panel("Lash",
                "Send the copied reading south. Caltheriel's friends saved thirty-six people; they also showed me the road into Emberhaven.",
                "")
        });
    appendMission("se04_reading_room");
    appendMission("se05_court_behind_curtain");
    appendScene(
        "se06_honey_without_hunger",
        "Book Two, Chapters 20-22 - Honey Without Hunger",
        {
            panel("Narrator",
                "Behind the black curtain stands Queen Nyxara, the insect sovereign of the Unseelie Court. She defends Caltheriel's original refuge as binding even after Caltheriel asks to leave. Nettle proposes a smaller test of that claim with an exit chosen in advance.",
                "cards/queenNyxara.png"),
            panel("Nettle Starbright",
                "Queen Nyxara says an old yes should outrank a later no. Test that claim on me: one spoon, ten minutes, then Pavo takes me out.",
                "cards/nettleStarbright_mounted.png"),
            panel("Sister Honeygrave",
                "The relief is real. Stay, and you need never hurt alone again.",
                "cards/sisterHoneygrave.png"),
            panel("Pavo's slate",
                "Ten minutes. An exit is the way you stop. You chose yours before the honey.",
                "cards/pavoQuickstep.png"),
            panel("Nettle Starbright",
                "I hated him for using it. I still want another spoon. I still choose to leave. Protection that cannot survive that answer is a cage.",
                "cards/nettleStarbright_mounted.png")
        });
    appendMission("se07a_queens_price_scene");
    appendMission("se07_queens_price");
    appendMission("se08a_sella_receives_herself");
    appendMission("se08_nine_lives_one_choice");
    appendScene(
        "se09_dusk_crown",
        "Book Two, Chapter 25 - The Dusk Crown",
        {
            panel("Pavo's slate",
                "A bearer is whoever carries one limited share. Eleven people now carry eleven shares of Caltheriel's light. An open hand ends only that bearer's share; nobody inherits it.",
                "cards/pavoQuickstep.png"),
            panel("Mosswake",
                "A southern instrument answers every release. Lash is measuring where each share begins and ends.",
                "cards/archivistMosswake.png"),
            panel("Caltheriel",
                "Tell every bearer. They may still withdraw. I will not hide the danger, and I will not remain in this prison for Lash's convenience.",
                "characters/caltheriel.png"),
            panel("Vespara - HEIR-SEEKER",
                "My chrysalis will carry what your scattered lights cannot. A court needs one heir, not eleven frightened refusals.",
                "cards/archduchessVespara.png"),
            panel("Caltheriel",
                "There is no child in that shell. There is only your desire for one, wearing stolen light.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "Vespara tears the empty chrysalis. Light rips from Caltheriel's right shoulder, but no heir forms and the separate releases continue.",
                "")
        });
    appendScene(
        "se10a_caltheriel_walks_out",
        "Book Two, Chapter 26 - No Crown Can Hold Light",
        {
            panel("Pavo's slate",
                "My share ends here. I open my hand. Do not pass it to another bearer.",
                "cards/pavoQuickstep.png"),
            panel("Nettle",
                "Pavo's light is out. I am waiting for the full signal. A missing share is not permission to fill it.",
                "cards/nettleStarbright_mounted.png"),
            panel("Nyxara - UNSEELIE RULER",
                "Give me one hour of unified command. I can force every remaining path open.",
                "cards/queenNyxara.png"),
            panel("Caltheriel",
                "No crown. No substitute. Open the separate exits and leave every empty place empty. I am walking now.",
                "characters/caltheriel.png"),
            panel("Vesper",
                "I came to learn what you wanted, not to make my rescue your next obligation. May I come closer as the person I am now?",
                "cards/princeVesper.png"),
            panel("Caltheriel",
                "Yes. The rescue ends with my choice - but Lash measured every exit that freed me. Nettle, carry that warning to Emberhaven. What follows belongs to the people living there.",
                "characters/caltheriel.png")
        });
    appendMission("se10_no_crown_holds_light");
    appendMission("se11a_blackthorn_unmade");
    appendScene(
        "se11b_city_refuses",
        "Book Two, Chapter 28 - The City Refuses",
        {
            panel("Mosswake - PUBLIC ARCHIVIST",
                "Aelon is the living root beneath Emberhaven. This failed imperial pin opened a road wrapped around that root.",
                "cards/archivistMosswake.png"),
            panel("Caltheriel",
                "The road follows the scar my release left. Lash measured our separate exits and found an approach to the city.",
                "characters/caltheriel.png"),
            panel("Factor Lume - IMPERIAL ENVOY",
                "The Empire will restore heat and seal the breach. Military control ends when the Emperor declares order restored.",
                ""),
            panel("Council witness",
                "Then the city has no exit it controls. Emberhaven refuses.",
                ""),
            panel("Asha Renn - FORMER IMPERIAL CAPTAIN",
                "The troop orders predate this vote. Refusal did not summon the invasion. Two divisions and three demon-bound companies were already moving.",
                ""),
            panel("Nettle",
                "Then record the cause in order: the army was moving, Lash had the measured road, and the city refused permanent rule. Pavo and I will begin with the roads people can still leave.",
                "cards/nettleStarbright_mounted.png")
        });
    appendScene(
        "se11_price_of_an_army",
        "Book Two, Epilogue - The Price of an Army",
        {
            panel("Fizzlewick",
                "The release scar gives us the lower turn. It does not give entry, living-root permission, or the missing counter-lock.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash",
                "It gives the Empire the only approach Emberhaven cannot close without wounding Aelon. That is enough to sell.",
                ""),
            panel("Imperial factor",
                "Two southern divisions, three demon-bound companies, protected movement, Southwater, and contested first claim below. The army moves at dawn.",
                ""),
            panel("Narrator",
                "At Lash's next test, stagehand Tavin invokes his release from an ungrounded post. The catch stays shut, and current destroys his right arm.",
                ""),
            panel("Mave",
                "No second test. His wages, treatment, independent examination, and authority over any return were posted before he was hurt. Fizzlewick records that the machine kept a bearer after he withdrew.",
                ""),
            panel("Pavo's slate",
                "Caltheriel's rescue exposed a road Lash could measure. That road bought an invasion already planned. Now we keep Emberhaven's many exits open while the Empire tries to make one command carry everyone.",
                "cards/pavoQuickstep.png")
        });
    // PLAYER ROUTE - ACT II
    appendScene(
        "se12_first_boats_burn",
        "BOOK THREE BEGINS - Chapters 1-2 - The First Boats Burn",
        {
            panel("Nettle's field record - BOOK TWO CLOSED",
                "Caltheriel walked free, Blackthorn's charter fell, and Emberhaven refused permanent imperial rule. Lash still owns the measurements stolen from that rescue, and he sold their road to an army already moving.",
                "cards/nettleStarbright_mounted.png"),
            panel("Pavo's slate - BOOK THREE BEGINS",
                "From here, follow Nettle and me through Emberhaven's defense. Our question changes: not how to free one prisoner, but how a city survives without making one person carry every choice.",
                "cards/pavoQuickstep.png"),
            panel("Birdie - RELIEF ORGANIZER",
                "The first refugee boats ride wrong. Move the families west, save the medicine, and do not fire through refugee cloth. The invasion begins at this quay.",
                "cards/birdieTheWise.png"),
            panel("Narrator",
                "A command bell seizes Kessa Vey's sword arm. Varo Sen turns the powder boat away from shore and dies in the blast; Birdie records his refusal beside the false emergency order.",
                ""),
            panel("Lash",
                "My barrier and warm road can defend the city. Give my crews its roots, prisons, and refugees. When your separate doors fail, someone must answer for the whole.",
                ""),
            panel("Nettle Starbright",
                "Reed gives Lash two public jobs with two ending times and nothing more. Pavo, mark the exits. I will take the pump road; the next bell may turn any defender against us.",
                "cards/nettleStarbright_mounted.png")
        });
    missions.back().title = "BOOK THREE - The First Boats Burn";
    appendScene(
        "se13b_cut_sylvara_chooses",
        "Book Three, Chapters 3-4 - The Cut Sylvara Chooses",
        {
            panel("Archivist Mosswake",
                "Lash's black glass reads routes. The white pearl stores a craftsman's voice. Neither is the living Root Key inside Aelon.",
                "cards/archivistMosswake.png"),
            panel("Sylvara",
                "The dangerous vibration crosses every broad cure. I choose this bath-root line and no other.",
                "cards/Sylvara.png"),
            panel("Caltheriel",
                "The wells remain, the baths lose heat, and my light carries the cut only until the root separates. I accept that exact cost.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "Caltheriel cuts the chosen root. The baths cool, her light contracts, and a road opens toward the wells.",
                "cards/Sylvara.png")
        });
    appendMission("se13_pearl_and_root");
    appendMission("se14_rules_under_pressure");
    appendScene(
        "se15_bell_and_order",
        "Book Three, Chapters 5-6 - Bell and Order",
        {
            panel("Kessa",
                "The bell orders me to take the gate. Do not erase my memory or merge me into a hive. Give me three minutes of silence, then let my answer do the work.",
                ""),
            panel("Vanya Bluewater",
                "Three minutes. Nothing continues without a new yes.",
                "cards/vanyaBluewater.png"),
            panel("Rowan Leafbound",
                "The forged order wants every defender. One hundred eighty-three people are choking below. Four guard the seal; everyone else goes to the shelter.",
                "cards/rowanLeafbound.png"),
            panel("Liora Kest - CITY KNIGHT",
                "My sister Bryn sleeps in the glassworks ward above that shelter. I worked its annealing stair before my vows. I know the way in; I lead.",
                ""),
            panel("Narrator",
                "Liora shares Rowan's failing shield while the carrying line brings all 183 people into clean air. During the rescue, someone steals the Order seal.",
                "cards/rowanLeafbound.png"),
            panel("Liora Kest - CITY KNIGHT",
                "The rescue was right, and I knew its terms. Rowan lays down his command pin; mine goes beside it, followed by eight other knights.",
                "")
        });
    appendScene(
        "se16_army_that_can_resign",
        "Book Three, Chapters 7-8 - An Army That Can Resign",
        {
            panel("Nettle Starbright",
                "This harness needs thread from your roof. The ward hears the winter cost before it votes. Its residents approve one measured length.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Lash orders workers to disguise a secret crossing as a drain. Nettle marks the point where it joins Emberhaven's road.",
                ""),
            panel("Caltheriel",
                "I will not take back the throne. Ask what light I can spare, not what crown can claim me.",
                "characters/caltheriel.png"),
            panel("Pavo's slate",
                "Reed's delayed aid stays in the record. Every role gets a way to stop. That makes us an army that can resign.",
                "cards/pavoQuickstep.png")
        });
    appendMission("se17_warm_channel");
    // PLAYER ROUTE - ACT III
    appendScene(
        "se18a_complete_is_a_label",
        "Book Three, Chapters 10-11 - COMPLETE Is a Label",
        {
            panel("Act recap",
                "Caltheriel escaped the Tockman diminished, but Lash copied her rescue route and sold it to the invading Empire. In Emberhaven, Nettle and Pavo help a coalition stop a receiver built to gather separate powers into one commanded body; the next clues come from parallel investigations.",
                "cards/nettleStarbright_mounted.png"),
            panel("Isca",
                "COMPLETE is not praise or truth. It is the receiver machine's label for one body carrying every kind of authority.",
                ""),
            panel("Lord Pallid Thread",
                "Give me one name and I will remove every contradiction around it.",
                "cards/lordPallidThread.png"),
            panel("Archivist Mosswake",
                "No. We keep the different descriptions and the blank where an eaten name belonged.",
                "cards/archivistMosswake.png"),
            panel("Kessa",
                "Your cuts exposed the final command strand. My refusal breaks that strand; the others must still contain the fragment, and my no does not erase the tremor left in my hand.",
                "")
        });
    appendScene(
        "se18c_before_next_dawn",
        "Book Three, Chapter 12 - Before the Next Dawn",
        {
            panel("Narrator",
                "Imperial soldiers open a red chest containing clean wheat, beans, fever bark, and bandages - the supplies Emberhaven lacks most.",
                ""),
            panel("Jarek Sorn",
                "Accept imperial protection and the food corridors remain open. The Empire receives the harbor, warm channels, underroot surveys, returned fragment hosts, and a hereditary emergency office that cannot resign.",
                ""),
            panel("Olla Pere",
                "I want the food. I want my husband home. I will not trade Kessa for either. Enter all three.",
                ""),
            panel("Narrator",
                "The council refuses the attached surrender terms and Sorn closes the chest. Artillery begins before he reaches the gate; an unlabeled fourth command turns a city gun toward Aelon, and its crew cuts the signal cable after the shot buries two surveyors.",
                "")
        });
    appendScene(
        "se18d_kindness_without_debt",
        "Book Three, Chapter 13 - The Kindness He Kept",
        {
            panel("Reed Baelstone",
                "My uncle Thaeron made family truth into ownership. This prison visit is public. I am here for one fact, not forgiveness.",
                "cards/reedBaelstone.png"),
            panel("Thaeron Baelstone",
                "A living Baelstone can open the old trunk road, or surrender that family permission forever.",
                "cards/thaeronBaelstone.png"),
            panel("Thaeron Baelstone",
                "I can also tell you which memories of your mother were truly hers. No price.",
                "cards/thaeronBaelstone.png"),
            panel("Reed Baelstone",
                "Keep the road fact public. Record that I declined the memories. Kindness does not require obedience.",
                "cards/reedBaelstone.png")
        });
    appendScene(
        "se18_what_complete_means",
        "Book Three, Chapter 14 - A Promise Cannot Breathe",
        {
            panel("Narrator",
                "Before Ebbin speaks, Lady Silkmaw has bound the whole courier relay into a vow-web: an open-ended promise made by one courier pulls every courier sharing that route into the same obligation.",
                "cards/ladySilkmaw.png"),
            panel("Ebbin - FEY MESSENGER",
                "Black oil still stains my legs from Mirewatch, and the outer edge missing from one wing makes every hover bank left. Pavo asks where I will stop. I answer: until I deliver it. Silver closes around my wrist and runs down the shared relay.",
                "cards/feyMessenger.png"),
            panel("Narrator",
                "Anwen's shelter-count road has already closed. Because Ebbin named no stopping point, Silkmaw's vow-web makes every courier keep pushing against every closed leg. It tightens through Anwen until she falls; bells on her frost-damaged feet let rescuers find her, but she does not return.",
                "cards/ladySilkmaw.png"),
            panel("Pavo's slate",
                "Silkmaw offers my lost voice for one lasting office over the relay. NO. ONE STRIP. ONE NAMED LEG. THE BEARER MAY REFUSE, RETURN IT, OR REPORT WHERE IT STOPPED. Ebbin limits his next strip to the first table and will return it if that road closes.",
                "cards/pavoQuickstep.png"),
            panel("Ruvan - FEY MESSENGER",
                "My sister vanished on a Mirror road, so I swore never to abandon another courier's work. But this road is flooded; refusing it keeps another bearer out of the water. I withdraw it. I cannot carry her miles as well as mine.",
                "cards/feyMessenger.png"),
            panel("Narrator",
                "The relay sets out WAITING, CARRIED, and STOPPED trays. Quinberry drops Silkmaw's anchor bead into ownerless water, breaking the shared web; Anwen remains dead.",
                "cards/quinberryLark.png")
        });
    appendMission("se19_crypt_under_order");
    appendScene(
        "se20_harness_anyone_can_leave",
        "Book Three, Chapter 16 - A Harness Anyone Can Leave",
        {
            panel("Nettle Starbright",
                "Loma, I used your pain to prove my first harness safe. I was wrong.",
                "cards/nettleStarbright_mounted.png"),
            panel("Loma",
                "Then build the release where I can reach it, and where anyone who sees it can reach it.",
                ""),
            panel("Narrator",
                "Dash tests the rebuilt harness, refuses the first route, then accepts one limited lift. Neither answer closes the door.",
                ""),
            panel("Pavo's slate",
                "That is a handoff: one task, one bearer, and a stop the bearer can reach.",
                "cards/pavoQuickstep.png")
        });
    // PLAYER ROUTE - ACT IV
    appendScene(
        "se21a_cinderworks_choice",
        "Book Three, Chapter 17 - The Cinderworks Choice",
        {
            panel("Act recap",
                "Emberhaven refused the Empire's food-for-rule bargain, and Lash's copied road still feeds the siege. Reed leads the descent team, including Nettle and Vesper, below the Cinderworks; Pavo and Caltheriel remain above, connected by the surface relay and closable light nodes.",
                "cards/reedBaelstone.png"),
            panel("Cinderworks glassworker",
                "On the siege's fifth night, a sabotaged maintenance road floods around us. If we leave these pressure doors, Emberhaven's stored winter panes crack. Can you stop the second charge first?",
                ""),
            panel("Reed Baelstone",
                "Both of you leave the doors. Come up alive. We will answer for the windows afterward.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "The workers climb out. Nettle disarms the second charge, but the row floods, the panes crack, and the pressure window passes. At the next shaft, the maintenance lift has no floor, so the expedition builds a releasable cable cage to descend.",
                "cards/nettleStarbright_mounted.png")
        });
    appendMission("se21_below_last_stair");
    appendScene(
        "se22a_map_eater",
        "Book Three, Chapter 19 - The Map-Eater's Table",
        {
            panel("Narrator",
                "After leaving the 206 Lower Wells survivors at a neutral enclave, Kelm, Aro, and Ulma guide the expedition west. A road name vanishes while its stone remains: Lord Pallid Thread eats the relation between a record and what it identifies.",
                "cards/lordPallidThread.png"),
            panel("Erevan the Shadow",
                "He ate old aliases and the doors attached to them. Record the gaps. Do not invent faces or streets to make my history look whole.",
                "cards/erevanTheShadow.png"),
            panel("Archivist Mosswake",
                "I will grow the route in seven incomplete lines. No single mouth, page, or memory gets the whole.",
                "cards/archivistMosswake.png"),
            panel("Narrator",
                "Pallid consumes one line. Six survive, but he also takes three spring councils, a grove-fire account, and the names of birds no longer nesting above Mosswake's western road.",
                "cards/archivistMosswake.png"),
            panel("Archivist Mosswake",
                "The record can name what I lost; it cannot give it back. Lichen, living thorns, and a moth-wing seal agree on one next place: the flooded Cathedral of Last Lights.",
                "cards/archivistMosswake.png")
        });
    appendMission("se22b_city_spends_at_night");
    appendScene(
        "se22c_orchard_memory",
        "Book Three, Chapters 21-22 - The Orchard Memory",
        {
            panel("Narrator",
                "At the flooded Cathedral, Duke Lanternwing carries the final sights of people whose bodies were never recovered. He blocks the stair to Tovan Voss, the dead surveyor who saw Lash's first descent to the lower receiver.",
                ""),
            panel("Duke Lanternwing",
                "I will release Voss's last sight for an equal memory freely offered by a living custodian. I do not choose which memory you give.",
                ""),
            panel("Prince Vesper",
                "I offer the night Caltheriel told me about the orchard, if she separately offers the orchard itself.",
                "cards/princeVesper.png"),
            panel("Caltheriel - THROUGH LIGHT NODE",
                "I offer only those memories. Close the Mirror if the taking reaches farther.",
                "characters/caltheriel.png"),
            panel("Narrator",
                "The orchard and the night of its telling go dark. Vesper cannot recover that evening, but he still knows Caltheriel counts his breaths when he flies, and he still chooses the life they share now.",
                "cards/princeVesper.png"),
            panel("Narrator",
                "Lanternwing opens the stair. Voss's sight shows Lash deepening first contact with the prison, cutting Voss's return line, and leaving a seven-path service arch. It leads the expedition to twelve posts: nine living prisoners and three bodies.",
                "")
        });
    appendMission("se23_controlled_fall");
    // PLAYER ROUTE - ACT V
    appendMission("se24_first_prison_design");
    appendMission("se25_three_turns_fish_rib");
    appendMission("se25a_abandoned_receiver");
    appendMission("se26a_hunger_has_no_face");
    appendMission("se26b_burning_root");
    appendScene(
        "se26_vacancy_that_holds",
        "Book Three, Chapter 29 - A Vacancy That Holds",
        {
            panel("Archivist Mosswake",
                "A bearer is simply whoever carries one part. Seven bearers stand around one place nobody must fill.",
                "cards/archivistMosswake.png"),
            panel("Queen Nyxara",
                "Give me the center and I can command the whole pattern.",
                "cards/queenNyxara.png"),
            panel("Reed Baelstone",
                "I am Reed, born Baelstone. That is an identity, not an office or key.",
                "cards/reedBaelstone.png"),
            panel("Narrator",
                "Each bearer states a limit and an exit. The receiver keeps working around the empty center.",
                "")
        });
    appendMission("se27_emperor_at_tree");
    // PLAYER ROUTE - ACT VI
    appendMission("se28a_two_men_one_vacancy");
    missions.push_back(storyNode(
        "se28b_seven_corrections",
        "Seven Corrections",
        "Book Three, Chapter 32 - The Left Hand at the Door",
        {
            panel("Narrator",
                "The blue fishbone key reaches Baalzapub sealed in a box no courier can lift and open alone. Nessa Bale, the theater's night steward, brings workers who know its hidden machinery.",
                ""),
            panel("Vanya Bluewater",
                "I authorize one use: take Lash's black concordance glass out of the machine. It judges whether separate people add up to one bearer. We are not opening a prison, the Mirror, Aelon, or any person.",
                "cards/vanyaBluewater.png"),
            panel("Narrator",
                "Seven people take seven different jobs, and each keeps the right to stop: Reed checks the public record, Donella protects bodies, Vanya guards the key, Nessa watches the machinery, Loma breaks the false reflection, Iria supplies her own memory, and Yarrow holds the Wells' map apart.",
                ""),
            panel("Recorded mentor",
                "Door known. Object known. Hands separate. No one alone.",
                ""),
            panel("Erevan the Shadow",
                "Vanya, you trusted me with your father's key after I once stole its companion. Turn it. I lift the glass with that same left hand; if you call stop, I leave it.",
                "cards/erevanTheShadow.png"),
            panel("Narrator",
                "The key breaks; the pearl goes silent; the glass burns Erevan's left hand. Above, ten replicas keep raising Lash's stage. The false agreement cannot last, but it can still raise him. Seven hands bought one heat cycle, not victory.",
                "")
        }));
    missions.push_back(storyNode(
        "se28c_a_vow_can_end",
        "A Vow Can End",
        "Book Three, Chapter 33 - The Vow Beneath the Refusals",
        {
            panel("Silkmaw - VOW-WEAVER",
                "Sing this without an ending: 'No one leaves while anyone suffers.' I will bind every singer to every other, forever.",
                "cards/ladySilkmaw.png"),
            panel("Pavo's slate",
                "Nettle, tap my wrist when the web loosens. Change the vow to what each voice can choose: 'I stay for this breath.' A duet is not a shackle; anyone may stop.",
                "cards/pavoQuickstep.png"),
            panel("Choir singer",
                "I stay for this breath. When the next breath comes, I choose again.",
                "cards/feyMessenger.png"),
            panel("Narrator",
                "The Choir repeats the promise one breath at a time. Briar and Vale cut Silkmaw's claim; her web folds around her. At the next table, four uncertain names begin disappearing.",
                "cards/briarWhisperthorn.png")
        }));
    missions.push_back(storyNode(
        "se28d_an_unfinished_record",
        "A Record Can Stay Open",
        "Book Three, Chapter 33 - The Unfinished Record",
        {
            panel("Pallid - NAME-EATER",
                "Four names in your casualty record may be wrong. Give them to me, and I will leave one clean, finished account.",
                "cards/lordPallidThread.png"),
            panel("Vanya Bluewater",
                "No. People are still coming to the tables. Somebody may know those names. We keep the record unfinished and leave room to correct it.",
                "cards/vanyaBluewater.png"),
            panel("Quinberry Lark",
                "Mosswake, send me the record in phrases. Each time Pallid reaches for an ending, I withhold the last note. He cannot swallow an account we refuse to call complete.",
                "cards/quinberryLark.png"),
            panel("Archivist Mosswake",
                "This private road is the last route only I control. Spending it means I can never use it to return home. I release it so the record can travel in separate pieces.",
                "cards/archivistMosswake.png"),
            panel("Narrator",
                "Quinberry leaves the last note open; Mosswake spends the road home. Their divided record contains Pallid. The uncertain names survive, and Lash's stage clears its first brace.",
                "cards/archivistMosswake.png")
        }));
    appendScene(
        "se28_one_vacancy_seven_hands",
        "Book Three, Chapter 33 - The Empty Chrysalis",
        {
            panel("Vespara",
                "A court requires a child. This chrysalis will carry the heir your separate refusals cannot make.",
                "cards/archduchessVespara.png"),
            panel("Caltheriel",
                "Vesper, stay where I can see you. That shell is not our child or our future. Briar cuts Vespara's ownership; Vale cuts the invented heir, and nothing living falls.",
                "characters/caltheriel.png"),
            panel("Lady Mirrorglass - MIRROR KEEPER",
                "The claims are cut, but Vespara can still reach the shell from this room. I can carry it behind the Mirror's outer skin, beyond her reach. The passage will close five real roads.",
                "cards/ladyMirrorglace.png"),
            panel("Narrator",
                "Mirrorglass names five real roads as they darken. The empty shell passes beyond Vespara's reach; she escapes without it. Outside, Lash's stage breaks the roofline.",
                "")
        });
    missions.push_back(storyNode(
        "se29a_baalzapub_ascendant",
        "Baalzapub Ascendant",
        "Book Three, Chapter 34 - Baalzapub Ascendant",
        {
            panel("Final-act recap",
                "Erevan lifted Lash's black concordance half; seven separate holders secured it in a shared lattice. The coalition refused the Emperor's demand and cut Vespara's claim to the empty chrysalis, but both remain dangerous. Lash still uses ten replicas and captured demon power to raise Baalzapub's stage.",
                "cards/fizzlewickGearwright.png"),
            panel("Narrator",
                "Lash's regulator forces the replicas to replay old readings. Two live roads bridge through his body with unnamed couriers still inside; Liora Kest keeps an arrow on his mortal body while Bryn's glassworks ward burns.",
                ""),
            panel("Donella of the Marsh",
                "His real body is under the church arch; the others are images. Do not shoot a reflection. Hold the mortal shot until the fourth heat mark: kill him now and those two roads close on the couriers.",
                "cards/donellaOfTheMarsh.png"),
            panel("Liora Kest",
                "The third mark is up. Donella is down, Vanya is under the rope, and waiting for the fourth may bury everyone. I heard the hold. I fire now.",
                ""),
            panel("Narrator",
                "Liora's arrow strikes Lash below the collarbone. His body stays on the loose stage board, and the shaft pins his body-route to one place. Two occupied roads tear.",
                ""),
            panel("Narrator",
                "Ruvan comes out of one road dying. He reports that he handed the shelter list to Ebbin six minutes earlier and watched him enter the next road. Ruvan dies; Ebbin remains missing; Lash remains alive around the arrow.",
                "")
        }));
    missions.push_back(storyNode(
        "se29b_fizzlewick_says_no",
        "Fizzlewick Says No",
        "Book Three, Chapter 34 - The Workers Refuse",
        {
            panel("Fizzlewick Gearwright",
                "The replicas cannot hold their copied reading. They need a distributed living sink: Baalzapub's workers.",
                "cards/fizzlewickGearwright.png"),
            panel("Lash LeGrim",
                "Use the house. I have always paid everyone who keeps it running. Trigger the circuit.",
                ""),
            panel("Fizzlewick Gearwright",
                "No. Tavin asked my machine to release him, and it took his arm. I recorded the fault and kept building. Nessa, pull my catch. The workers are not fuel; I break the relay.",
                "cards/fizzlewickGearwright.png"),
            panel("Nessa Bale - NIGHT STEWARD",
                "I hear you, Fizzlewick. Five black nails mean five breaths of blackout. I leave the choice at the rail; nobody is volunteered, including you.",
                "cards/gloomFairy.png"),
            panel("Narrator",
                "Nessa pulls Fizzlewick's emergency release and drops him from Lash's grip. A one-armed Gloom Fairy freely gives five breaths of darkness; the workers bind Fizzlewick and secure the regulator. His refusal saves them now. It does not erase what his machinery did to Tavin.",
                "cards/gloomFairy.png"),
            panel("Narrator",
                "Lash yanks every remaining route. The vent bursts and tears Aelon, burning records and killing people. Even so, the stage board beneath his body, the witness rail, the seven-person frame, and the vent line hold long enough for Nettle to begin the release.",
                "cards/gloomFairy.png")
        }));
    missions.push_back(storyNode(
        "se29c_all_authority_returns",
        "The First Release",
        "Book Three, Chapter 35 - No Complete Bearer",
        {
            panel("Narrator",
                "A melted harness bar is sliding toward a worker's trapped boot. Nettle cannot reach the release, so she pours her last water into the gutter and makes one narrow cool path to the catch.",
                "cards/nettleStarbright_mounted.png"),
            panel("Nettle Starbright",
                "One crossing and one pull, if you choose it, Zippy. Nothing after that is owed.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Zippy crosses, pulls, and then seals himself inside his shell when a second trip is offered. Nettle accepts the no.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Tavin, the stagehand whose earlier machine injury cost him an arm, holds the falling bar while the trapped worker frees his own boot. A Champion and a Giant take the remaining weight on separate lines. The first part of Lash's machine comes loose without choosing one rescuer to carry it all.",
                "")
        }));
    missions.push_back(storyNode(
        "se29c2_separate_withdrawals",
        "Separate Withdrawals",
        "Book Three, Chapter 35 - No Complete Bearer",
        {
            panel("Narrator",
                "As the first released section leaves Lash's machine, the Emperor climbs from the western volcanic channel and declares, 'All authority returns.' Five short pulses and one sustained pull strike every person still bound to his demon lattice.",
                ""),
            panel("Hushkeeper",
                "At the free-host station, Kessa keeps her sword hand. Daro names and refuses his second shadow. A wounded woman reaches her own release. I hold their pain apart so no scream chooses for the next person.",
                ""),
            panel("Caltheriel",
                "My eight minutes end here. The court light ends with me; nobody takes my chair.",
                "characters/caltheriel.png"),
            panel("Queen Nyxara",
                "Caltheriel, mine before Vespara's was a stupid answer; the Emperor's is worse. I relinquish my three old command paths. You do not inherit them. No successor does.",
                "cards/queenNyxara.png"),
            panel("Pavo's slate",
                "Briar shuts two Mirror roads. Tockman's borrowed time stops. The last vow-thread breaks. Each ending leaves an empty place.",
                "cards/pavoQuickstep.png"),
            panel("Narrator",
                "The withdrawals are entered before the Emperor's next order. He is about to call every still-bound fragment home. First, an imperial captain attacks people scraping off gold marks; Rowan opens the north surrender station while Reed holds the witness rail.",
                "")
        }));
    missions.push_back(seelieHoldWitnessRail());
    appendMission("se29d_reed_ends_the_house");
    appendMission("se29_no_complete_bearer");
    appendScene(
        "se30_names_we_keep",
        "Book Three, Chapter 36 and Epilogue - The Names We Keep",
        {
            panel("Birdie the Wise",
                "Do not close the casualty book. Nima's body has been found, so write dead beneath missing and leave the first entry visible. Ebbin is still missing; his search stays open.",
                "cards/birdieTheWise.png"),
            panel("Liora Kest",
                "I fired at the third mark after hearing the hold. Donella was down, Vanya trapped, and I judged the city would not survive the fourth. Write that separately from Ruvan's last sighting of Ebbin.",
                ""),
            panel("Narrator",
                "Marrowind can chase Vespara or rescue the imperial child behind the flour bin. He chooses the child. Wardens later take Vespara alive, and Nyxara makes no custody claim.",
                ""),
            panel("Vanya Bluewater",
                "Witnesses verify the chrysalis empty before its last claim is cut and the shell destroyed. The Root Key stays inside Aelon; Mirror custody, concordance glass, broken fishbone key, and silent pearl remain separate.",
                "cards/vanyaBluewater.png"),
            panel("Narrator",
                "Nettle's wings and Pavo's voice do not return. Vesper and Caltheriel build a new life without the lost orchard. None of those losses is renamed healing.",
                "cards/nettleStarbright_mounted.png"),
            panel("Narrator",
                "Months later, Baalzapub reopens under a worker-and-public trust. The first song starts in the wrong key; the musicians stop, laugh, and begin again. At Nettle's new ramp, untitled Reed and one-armed Tavin free a caught chair together and clear the aisle for the next audience.",
                "")
        });

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
