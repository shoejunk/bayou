#pragma once

#include <SFML/Graphics.hpp>

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bayou::client::ui_capture
{

// Offline screenshot harness. The client normally needs the accounts,
// matchmaking and card services to reach anything past the login screen, which
// makes it awkward to review a screen's presentation. In capture mode the
// client instead fabricates plausible account state, walks a list of screens,
// and writes one PNG per screen before exiting.
//
// Enable it with:
//   SteamTactics.exe --ui-capture=<directory> [--ui-capture-screens=a,b,c]
//                    [--ui-capture-size=1920x1080]

struct Request
{
    std::filesystem::path outputDirectory;
    // Screen keys to visit, in order. Empty means "every known screen".
    std::vector<std::string> screens;
    unsigned int width = 1920;
    unsigned int height = 1080;
    // Frames to render before grabbing, so texture loads and the first steps of
    // any idle animation have settled.
    int warmupFrames = 6;
};

// Returns nullopt when the command line does not ask for a capture run.
std::optional<Request> parseCommandLine(int argc, char** argv);

// Every screen key the harness understands, in capture order.
const std::vector<std::string>& knownScreens();

bool saveWindow(const sf::RenderWindow& window, const std::filesystem::path& path);

// Fabricated data for the screens that would otherwise be empty offline.
std::vector<card_data::Card> sampleCardLibrary();
std::vector<deck_data::Deck> sampleDecks(const std::vector<card_data::Card>& library);
std::vector<account_data::CollectionCard> sampleCollection(
    const std::vector<card_data::Card>& library);

} // namespace bayou::client::ui_capture
