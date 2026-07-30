#pragma once

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include "client_textures.hpp"

#include <SFML/Graphics.hpp>

#include <array>
#include <string>
#include <vector>

namespace bayou::client
{
std::vector<card_data::Card> resolveDeckCards(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library);
int collectionCopiesFor(const std::vector<account_data::CollectionCard>& collection, const std::string& title);
std::vector<card_data::Card> ownedCardsFromCollection(
    const std::vector<card_data::Card>& library,
    const std::vector<account_data::CollectionCard>& collection);
int countHeroes(const std::vector<card_data::Card>& cards);

// ---------------------------------------------------------------------------
// Collection presentation kit
//
// The deck, shop and starter-deck screens all present the same nouns: a card as
// a physical object, a deck as a faction's roster, a price as struck coin. They
// used to each improvise those in local lambdas, which is why no two rows or
// badges matched. Everything below is the single vocabulary they now share, so
// a cost gem is the same gem everywhere and a faction keeps one identity across
// the shop, the picker and the editor.
// ---------------------------------------------------------------------------

namespace palette
{
// The main menu's warm brass over near-black plate, with violet as the cool
// arcane counterpoint. Nothing in the collection screens should introduce a hue
// that is not derived from these.
inline const sf::Color Brass(174, 117, 54);
inline const sf::Color BrassBright(239, 190, 98);
inline const sf::Color BrassPale(248, 214, 112);
inline const sf::Color BrassDim(83, 54, 29);
inline const sf::Color Ink(246, 232, 200);
inline const sf::Color Muted(181, 166, 137);
inline const sf::Color MutedDim(139, 128, 108);
inline const sf::Color Plate(12, 17, 18);
inline const sf::Color PlateLift(24, 31, 32);
inline const sf::Color Arcane(123, 79, 168);
inline const sf::Color ArcaneBright(182, 142, 226);
inline const sf::Color Good(138, 198, 142);
inline const sf::Color Warn(226, 170, 88);
inline const sf::Color Bad(206, 106, 90);
} // namespace palette

sf::Color withAlpha(sf::Color color, int alpha);
sf::Color mix(sf::Color from, sf::Color to, float amount);

// Everything the kit needs to draw: the target, both faces, and the art cache.
struct UiContext
{
    sf::RenderWindow& window;
    sf::Font& font;
    sf::Font& displayFont;
    TextureStore& textures;
};

// Which procedural mark a crest carries. Drawn in code rather than shipped as
// art, since we cannot author new PNGs here.
enum class Sigil
{
    Leaf,     // Seelie: growth
    Thorn,    // Unseelie: barb
    Reed,     // Mirewatch: three reeds
    Cog       // Blackthorn: industry
};

struct FactionStyle
{
    std::string key;
    std::string label;
    std::string shortLabel;
    std::string blurb;
    sf::Color accent;
    sf::Color deep;
    std::string art;
    Sigil sigil = Sigil::Leaf;
};

// The four starter factions, indexed to match starter_decks::Names.
const std::array<FactionStyle, 4>& factionStyles();
// Resolves a deck to a faction by name; falls back to the unaligned style so
// player-authored deck names still get a coherent identity.
const FactionStyle& factionStyleForDeckName(const std::string& deckName);
const FactionStyle& unalignedStyle();
// A card's accent comes from its first canonical trait, which is the only
// faction-ish signal real catalogue cards carry.
sf::Color traitAccent(const std::string& trait);
sf::Color cardAccent(const card_data::Card& card);

inline constexpr std::size_t ManaCurveBuckets = 7;

struct DeckSummary
{
    std::string name;
    int cardCount = 0; // non-hero
    int heroCount = 0;
    int heroCost = 0;
    int uniqueTitles = 0;
    bool legal = false;
    std::string heroName;
    std::string heroArt;
    std::array<int, ManaCurveBuckets> curve{};
    // Trait spread, most common first, for the identity line.
    std::vector<std::pair<std::string, int>> traits;
    const FactionStyle* faction = nullptr;
};

DeckSummary summarizeDeck(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library);

// --- chrome ----------------------------------------------------------------

// One header for every collection screen: title plaque hard left, account and
// struck-coin wallet hard right, both inside reserved columns so they cannot
// collide the way the hand-placed strings used to.
void drawScreenHeader(
    const UiContext& ui,
    const std::string& title,
    const std::string& account,
    int coins,
    float titleWidth = 236.0f);

void drawSectionHeading(const UiContext& ui, sf::Vector2f position, const std::string& text, float rule = 0.0f);
void drawCaption(const UiContext& ui, sf::Vector2f position, const std::string& text, float maxWidth = 0.0f);
// A tapering hairline for dividing content inside a panel. drawSeparatorRule's
// centre rivet reads as a stray dot at this scale.
void drawInnerRule(const UiContext& ui, sf::Vector2f position, float width);

// Vertical wash used to sink art into a plate so text over it stays readable.
void drawVerticalScrim(
    const UiContext& ui,
    sf::FloatRect rect,
    sf::Color top,
    sf::Color bottom);

// --- parts -----------------------------------------------------------------

// Resource cost reads as a faceted violet gem; hero cost as a brass shield, so
// the two never have to be told apart by a text prefix.
void drawCostGem(const UiContext& ui, sf::Vector2f center, float radius, int value, bool hero);
void drawRarityGem(const UiContext& ui, sf::Vector2f center, float radius, sf::Color color);
void drawCoin(const UiContext& ui, sf::Vector2f center, float radius);
void drawFactionCrest(
    const UiContext& ui,
    sf::Vector2f center,
    float radius,
    const FactionStyle& style,
    bool bright);
// Framed art window. Falls back to a faction-tinted void so a missing image
// still reads as a card, not as a hole.
// verticalBias picks where a cover crop takes its slice: 0 is the top of the
// source art, which is where a card illustration keeps the face.
void drawArtWindow(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::string& artPath,
    sf::Color edge,
    float cut = 4.0f,
    bool cover = false,
    float verticalBias = 0.32f);
// Copies held versus copies allowed, as filled/hollow lozenges.
void drawCopyPips(const UiContext& ui, sf::Vector2f left, int filled, int total, sf::Color color);
void drawMeter(
    const UiContext& ui,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& label,
    const std::string& value,
    float fill,
    sf::Color color);
void drawManaCurve(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::array<int, ManaCurveBuckets>& curve,
    sf::Color color,
    bool labelled);
// Warnings live in a reserved slot with an icon, never thrown across a panel.
void drawValidationSlot(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::vector<std::string>& warnings,
    bool legal);
// Designed empty state: a dashed card-back ghost plus heading and hint.
void drawEmptyState(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::string& heading,
    const std::string& hint);
// One treatment for every action a screen cannot currently perform.
void drawDisabledButton(
    const UiContext& ui,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& label);
// Faction crests and blurbs, for the deck portrait slot when there is no deck to
// portray. Two identical ghost cards side by side read as a rendering fault.
void drawFactionRoster(const UiContext& ui, sf::FloatRect rect, const std::string& hint);

// --- rows ------------------------------------------------------------------

struct CardRow
{
    const card_data::Card* card = nullptr;
    sf::FloatRect rect;
    bool selected = false;
    bool hovered = false;
    // Copies of this card in the deck, and the most it may hold.
    int copies = 0;
    int copyLimit = 0;
    int owned = 0;
    bool showOwned = false;
    // Units whose traits no hero in the deck shares.
    bool traitMismatch = false;
};

void drawCardRow(const UiContext& ui, const CardRow& row);

void drawDeckRosterRow(
    const UiContext& ui,
    sf::FloatRect rect,
    const DeckSummary& summary,
    bool selected,
    bool hovered);

// The right-hand deck portrait: crest, hero art, counts, curve, traits.
void drawDeckDetailPanel(const UiContext& ui, sf::FloatRect rect, const DeckSummary& summary);

// --- filter chips ----------------------------------------------------------

// Checkbox rows were laid out on a fixed grid and overran their panel. Chips are
// measured from the font and wrapped, so a longer label can never clip.
struct FilterChip
{
    sf::FloatRect rect;
    std::string label;
};

std::vector<FilterChip> layoutFilterChips(
    sf::Font& font,
    const std::vector<std::string>& labels,
    sf::Vector2f origin,
    float maxWidth,
    unsigned int textSize,
    float height,
    float gap);

void drawFilterChip(
    const UiContext& ui,
    const FilterChip& chip,
    bool checked,
    bool hovered,
    sf::Color accent,
    unsigned int textSize);

// Trait chips, wrapped to maxWidth. A count of 0 prints the trait on its own.
// Returns the y below the last row. Used by both the deck portrait and the card
// inspector so a trait reads identically in each.
float drawTraitChips(
    const UiContext& ui,
    sf::Vector2f origin,
    float maxWidth,
    const std::vector<std::pair<std::string, int>>& traits,
    int maxRows = 16);

// --- shop ------------------------------------------------------------------

// The pack as a jewel: stacked card backs, brass filigree, a faceted arcane
// gem and a slow breathing glow.
void drawPackObject(const UiContext& ui, sf::FloatRect rect, float time, bool hovered);
void drawOddsTable(const UiContext& ui, sf::FloatRect rect);
// Rarity-coloured burst behind a revealed card.
void drawRevealBurst(const UiContext& ui, sf::Vector2f center, float time, sf::Color color);
void drawRarityRibbon(const UiContext& ui, sf::FloatRect rect, const std::string& label, sf::Color color);

// A full card face: framed art, cost gem, rarity gem, name plate, stat block.
void drawCardFace(
    const UiContext& ui,
    sf::FloatRect rect,
    const card_data::Card& card,
    const std::string& rarityLabel,
    sf::Color rarityColor,
    const std::string& footer);

// --- faction tile ----------------------------------------------------------

void drawFactionTile(
    const UiContext& ui,
    sf::FloatRect rect,
    const FactionStyle& style,
    const std::string& name,
    int cardCount,
    const std::string& status,
    sf::Color statusColor,
    bool owned,
    bool selected,
    bool affordable);
}
