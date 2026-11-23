#pragma once

#include "../client/client_story.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

// Export the assembled mission catalog so a reader sees the active revision,
// including any guides inherited from the original scenario.
namespace story_review
{
inline void quote(std::ostream& out, std::string_view value)
{
    out << '"';
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char ch : value)
    {
        if (ch == '"' || ch == '\\')
            out << '\\' << ch;
        else if (ch < 32)
            out << "\\u00" << hex[ch >> 4] << hex[ch & 15];
        else
            out << ch;
    }
    out << '"';
}

inline void panels(std::ostream& out, const std::vector<bayou::client::StoryPanel>& entries)
{
    out << '[';
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (i) out << ',';
        out << "{\"speaker\":";
        quote(out, entries[i].speaker);
        out << ",\"text\":";
        quote(out, entries[i].text);
        out << '}';
    }
    out << ']';
}

inline void exportCatalog(const std::filesystem::path& folder)
{
    using namespace bayou::client;
    std::filesystem::create_directories(folder);
    for (auto [campaign, file] : {std::pair{StoryCampaign::Mirewatch, "mirewatch.json"},
                                 std::pair{StoryCampaign::Blackthorn, "blackthorn.json"},
                                 std::pair{StoryCampaign::Seelie, "seelie.json"}})
    {
        std::ofstream out(folder / file, std::ios::binary);
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out << std::boolalpha << '[';
        const auto missions = storyMissions(campaign);
        for (std::size_t i = 0; i < missions.size(); ++i)
        {
            const auto& m = missions[i];
            if (i) out << ",\n";
            out << "{\"id\":";
            quote(out, m.id);
            out << ",\"title\":";
            quote(out, m.title);
            out << ",\"act\":";
            quote(out, storyActName(campaign, static_cast<int>(i)));
            out << ",\"shortGoal\":";
            quote(out, storyActShortGoal(campaign, static_cast<int>(i)));
            out << ",\"actGoal\":";
            quote(out, storyActGoal(campaign, static_cast<int>(i)));
            out << ",\"survivors\":";
            quote(out, storyRequiredSurvivorNames(m));
            out << ",\"lesson\":";
            quote(out, m.lesson);
            out << ",\"objective\":";
            quote(out, m.objective);
            out << ",\"hint\":";
            quote(out, m.hint);
            out << ",\"extraPractice\":" << (static_cast<int>(i) >= storyNarrativeEntryCount(campaign))
                << ",\"optional\":" << m.optionalRehearsal
                << ",\"storyOnly\":" << (m.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
                << ",\"briefing\":";
            panels(out, m.briefing);
            out << ",\"steps\":[";
            for (std::size_t j = 0; j < m.script.size(); ++j)
            {
                const auto& step = m.script[j];
                if (j) out << ',';
                out << "{\"panelsBefore\":";
                panels(out, step.panelsBefore);
                out << ",\"heading\":";
                quote(out, step.heading);
                out << ",\"instruction\":";
                quote(out, step.instruction);
                out << ",\"correction\":";
                quote(out, step.correction);
                out << ",\"requiresInput\":" << storyScriptActionRequiresPlayerInput(m, step) << '}';
            }
            out << "],\"aftermath\":";
            panels(out, m.aftermath);
            out << '}';
        }
        out << "]\n";
        out.close();
    }
}
} // namespace story_review
