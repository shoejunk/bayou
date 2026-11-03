// Player-facing adaptation of Gloomthorn Complete Revised Manuscript (P1,
// 2026-10-17). Stable mission IDs preserve saves and the tested card lessons.
// The builders above supply board mechanics; this is the authoritative story
// for Mirewatch and Blackthorn. Seelie and post-story practice are independent.
bool novelGuidePanel(const StoryPanel& entry)
{
    return entry.speaker == "Coach" || entry.speaker == "Controls" ||
        entry.speaker == "How to place a card" ||
        entry.speaker == "Optional practice" || entry.speaker == "Practice battle";
}

StoryMission& novelScene(
    std::vector<StoryMission>& missions, std::string_view id,
    std::string_view title, std::string_view source, std::string_view goal,
    std::initializer_list<StoryPanel> briefing,
    std::initializer_list<StoryPanel> aftermath = {})
{
    const auto found = std::find_if(missions.begin(), missions.end(),
        [id](const StoryMission& mission) { return mission.id == id; });
    if (found == missions.end())
        throw std::logic_error("Novel scene refers to an unknown mission");
    StoryMission& mission = *found;
    const auto guides = [](const std::vector<StoryPanel>& oldPanels) {
        std::vector<StoryPanel> kept;
        std::copy_if(oldPanels.begin(), oldPanels.end(), std::back_inserter(kept),
            novelGuidePanel);
        return kept;
    };
    const auto oldBriefingGuides = guides(mission.briefing);
    const auto oldAftermathGuides = guides(mission.aftermath);
    mission.title = title;
    mission.sourceChapter = source;
    mission.briefing.assign(briefing);
    // Chronicles finish after briefing, and optional boards may be skipped.
    // Neither path may hide a plot outcome in a combat-only aftermath popup.
    const bool storyBeforePractice =
        mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly ||
        mission.optionalRehearsal;
    if (storyBeforePractice)
        mission.briefing.insert(mission.briefing.end(), aftermath.begin(), aftermath.end());
    mission.briefing.insert(mission.briefing.end(),
        oldBriefingGuides.begin(), oldBriefingGuides.end());
    mission.aftermath.clear();
    if (!storyBeforePractice)
        mission.aftermath.assign(aftermath);
    mission.aftermath.insert(mission.aftermath.end(),
        oldAftermathGuides.begin(), oldAftermathGuides.end());
    for (StoryScriptAction& action : mission.script)
        action.panelsBefore = guides(action.panelsBefore);
    if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
    {
        mission.lesson = "THE STORY CONTINUES";
        mission.objective = goal;
        mission.hint = "Continue to hear the next part of the story. You can go back to read a line again.";
    }
    return mission;
}

#include "client_story_novel_mirewatch.inl"
#include "client_story_novel_blackthorn.inl"

void attachNovelScenarioArt(std::vector<StoryMission>& missions)
{
    // Card art is the visual source of truth, including optional practice.
    // Only the reference-based illustrations below replace card portraits.
    for (StoryMission& mission : missions)
    {
        mission.scenarioArtPath = {};
        if (mission.id == "s03_published_mystery")
            mission.scenarioArtPath = "story/revised/mirror_memories_v1.png";
        else if (mission.id == "bt07_debtor_prison")
            mission.scenarioArtPath = "story/revised/vault_escape_v1.png";
        else if (mission.id == "mw23_choice_not_cure" || mission.id == "bt17c_victor_reckoning")
            mission.scenarioArtPath = "story/revised/save_living_root_v1.png";
    }
}
