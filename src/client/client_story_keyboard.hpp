#pragma once

#include "client_story.hpp"

#include <algorithm>

namespace bayou::client
{

// Story Mode keeps keyboard navigation deliberately small and deterministic so
// the same cursor rules can be exercised without constructing an SFML window.
struct StoryBoardCursor
{
    int row = 0;
    int column = 0;

    friend bool operator==(const StoryBoardCursor&, const StoryBoardCursor&) = default;
};

inline StoryBoardCursor moveStoryBoardCursor(
    StoryBoardCursor cursor,
    int rowDelta,
    int columnDelta) noexcept
{
    cursor.row = std::clamp(
        cursor.row + rowDelta, 0, game_data::BoardSize - 1);
    cursor.column = std::clamp(
        cursor.column + columnDelta, 0, game_data::BoardSize - 1);
    return cursor;
}

inline int wrapStoryKeyboardIndex(int current, int delta, int count) noexcept
{
    if (count <= 0)
    {
        return -1;
    }
    if (current < 0 || current >= count)
    {
        return delta < 0 ? count - 1 : 0;
    }
    return (current + delta % count + count) % count;
}

inline bool storyRequiresSeelieSpoilerConfirmation(
    StoryCampaign selectedCampaign,
    int mirewatchCompletedCount,
    int mirewatchMissionCount) noexcept
{
    return selectedCampaign == StoryCampaign::Seelie &&
        mirewatchCompletedCount < mirewatchMissionCount;
}

inline bool storyContinueWithoutMasteryAvailable(
    const StoryMission& mission,
    int genuineDefeatCount) noexcept
{
    return genuineDefeatCount > 0 &&
        mission.objectiveSpec.kind != StoryObjectiveKind::StoryOnly &&
        mission.script.empty() &&
        !mission.optionalRehearsal;
}
} // namespace bayou::client
