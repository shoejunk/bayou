#pragma once

#include <SFML/Graphics.hpp>

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <filesystem>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bayou::client::ui_capture
{

struct CheckoutIdentity
{
    std::string repositoryRoot;
    std::string reference;
    std::string commit;
    bool dirty = true;
    bool dirtyKnown = false;
};

struct InputSnapshotGroup
{
    std::string name;
    std::string sha256;
    std::uint64_t fileCount = 0;
    std::uint64_t totalBytes = 0;
};

struct InputSnapshot
{
    bool available = false;
    std::string error;
    std::string overallSha256;
    std::uint64_t fileCount = 0;
    std::uint64_t totalBytes = 0;
    InputSnapshotGroup projectInputs;
    InputSnapshotGroup runtimeAssets;
};

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
    // Snapshotted before output-directory admission so capture files cannot
    // make an otherwise-clean checkout appear dirty.
    CheckoutIdentity captureCheckout;
    // Content identity of every project client input and runtime asset, taken
    // before output-directory admission. A second identity is taken when the
    // completion manifest is published so a long capture cannot silently mix
    // source or art revisions.
    InputSnapshot captureStartSnapshot;
};

inline constexpr const char* CompletionManifestFileName = "capture-manifest.json";

// Returns nullopt when the command line does not ask for a capture run.
std::optional<Request> parseCommandLine(int argc, char** argv);

// Every screen key the harness understands, in capture order.
const std::vector<std::string>& knownScreens();

// Admits a capture destination before the window is created. Existing empty
// directories are allowed; any existing entry causes the run to fail closed.
// This never removes or replaces caller-owned output.
bool prepareOutputDirectory(
    Request& request,
    const std::filesystem::path& executablePath,
    std::string& error);

// The deterministic filename for a requested screen at its zero-based index.
std::string captureFileName(std::size_t index, std::string_view screen);

bool saveWindow(const sf::RenderWindow& window, const std::filesystem::path& path);

// Validates that the directory contains exactly the PNGs expected for the
// request and atomically publishes the completion/provenance manifest. The
// manifest is never published when validation or writing fails.
bool writeCompletionManifest(
    const Request& request,
    const std::vector<std::string>& successfulFiles,
    const std::filesystem::path& executablePath,
    std::string& error);

// Fabricated data for the screens that would otherwise be empty offline.
std::vector<card_data::Card> sampleCardLibrary();
std::vector<deck_data::Deck> sampleDecks(const std::vector<card_data::Card>& library);
// A deck that satisfies every deck-building rule, for reviewing the editor's
// populated counters, curve and "deck is legal" state.
deck_data::Deck sampleLegalDeck(const std::vector<card_data::Card>& library);
std::vector<account_data::CollectionCard> sampleCollection(
    const std::vector<card_data::Card>& library);

} // namespace bayou::client::ui_capture
