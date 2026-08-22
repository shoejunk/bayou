#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace bayou::client
{

struct StoryPanel
{
    std::string_view speaker;
    std::string_view text;
    std::string_view artPath;
};

struct StoryMission
{
    std::string_view title;
    std::string_view sourceChapter;
    std::string_view lesson;
    std::string_view objective;
    std::string_view hint;
    std::array<StoryPanel, 3> briefing;
};

const std::array<StoryMission, 8>& storyMissions();
int loadStoryCompletedCount(std::string_view username);
bool saveStoryCompletedCount(std::string_view username, int completedCount);

} // namespace bayou::client
