#include "client_story.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

namespace bayou::client
{
namespace
{

std::string storyProgressKey(std::string_view username)
{
    std::string key;
    key.reserve(username.size());
    for (const unsigned char character : username)
    {
        if (std::isalnum(character) || character == '-' || character == '_')
        {
            key.push_back(static_cast<char>(std::tolower(character)));
        }
        else
        {
            key.push_back('_');
        }
    }
    return key.empty() ? "local" : key;
}

std::filesystem::path storyProgressPath(std::string_view username, StoryCampaign campaign)
{
    const std::string suffix = campaign == StoryCampaign::Blackthorn ? "" : "_mirewatch";
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        return std::filesystem::path(appData) / "SteamTactics" / "story_progress" /
            (storyProgressKey(username) + suffix + ".cfg");
    }
#else
    if (const char* home = std::getenv("HOME"); home && *home)
    {
        return std::filesystem::path(home) / ".config" / "SteamTactics" / "story_progress" /
            (storyProgressKey(username) + suffix + ".cfg");
    }
#endif
    return std::filesystem::path("story_progress") /
        (storyProgressKey(username) + suffix + ".cfg");
}

} // namespace

std::string_view storyCampaignName(StoryCampaign campaign)
{
    return campaign == StoryCampaign::Blackthorn
        ? "The Blackthorns"
        : "The Mirewatch Resistance";
}

int loadStoryCompletedCount(std::string_view username, StoryCampaign campaign)
{
    std::ifstream stream(storyProgressPath(username, campaign));
    if (!stream)
    {
        return 0;
    }

    std::string firstLine;
    std::getline(stream, firstLine);
    if (firstLine != "GLOOMTHORN_STORY_PROGRESS_V2")
    {
        // The retired eight-mission format stored only a position. Those old
        // stages do not map to the expanded campaign's stable mission IDs, so
        // guessing would silently skip unplayed rules (including River Teeth).
        // Preserve the file until the player completes an entry, but begin the
        // revised story at its authored opening.
        return 0;
    }

    std::unordered_set<std::string> completedIds;
    std::string line;
    while (std::getline(stream, line))
    {
        constexpr std::string_view Prefix = "completed=";
        if (line.rfind(Prefix, 0) == 0)
        {
            completedIds.insert(line.substr(Prefix.size()));
        }
    }

    int completedCount = 0;
    for (const StoryMission& mission : storyMissions(campaign))
    {
        if (!completedIds.contains(std::string(mission.id)))
        {
            break;
        }
        ++completedCount;
    }
    return completedCount;
}

bool saveStoryCompletedCount(
    std::string_view username,
    StoryCampaign campaign,
    int completedCount)
{
    const std::filesystem::path path = storyProgressPath(username, campaign);
    std::error_code error;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            return false;
        }
    }

    const std::span<const StoryMission> missions = storyMissions(campaign);
    const int clamped = std::clamp(completedCount, 0, static_cast<int>(missions.size()));
    std::ofstream stream(path, std::ios::trunc);
    stream << "GLOOMTHORN_STORY_PROGRESS_V2\n";
    for (int index = 0; index < clamped; ++index)
    {
        stream << "completed=" << missions[static_cast<std::size_t>(index)].id << '\n';
    }
    return static_cast<bool>(stream);
}

} // namespace bayou::client
