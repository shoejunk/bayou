#include "deck_collection.hpp"

#include "../shared/game_data.hpp"
#include "../shared/starter_decks.hpp"

#include "client_card_text.hpp"
#include "client_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>

namespace bayou::client
{
std::vector<card_data::Card> resolveDeckCards(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library)
{
    std::vector<card_data::Card> resolved;
    resolved.reserve(deck.cardTitles.size());
    for (const std::string& title : deck.cardTitles)
    {
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        if (found != library.end())
        {
            resolved.push_back(*found);
        }
    }
    return resolved;
}

int collectionCopiesFor(const std::vector<account_data::CollectionCard>& collection, const std::string& title)
{
    const auto found = std::find_if(collection.begin(), collection.end(), [&](const account_data::CollectionCard& card) {
        return card.title == title;
    });
    return found == collection.end() ? 0 : found->copies;
}

std::vector<card_data::Card> ownedCardsFromCollection(
    const std::vector<card_data::Card>& library,
    const std::vector<account_data::CollectionCard>& collection)
{
    std::vector<card_data::Card> ownedCards;
    for (const card_data::Card& card : library)
    {
        if (collectionCopiesFor(collection, card.title) > 0)
        {
            ownedCards.push_back(card);
        }
    }
    return ownedCards;
}

int countHeroes(const std::vector<card_data::Card>& cards)
{
    return static_cast<int>(std::count_if(cards.begin(), cards.end(), [](const card_data::Card& card) {
        return game_data::isHeroCard(card);
    }));
}

// ---------------------------------------------------------------------------
// Collection presentation kit
// ---------------------------------------------------------------------------

// withAlpha lives in client_board_layout.cpp; the board and the collection
// screens share the one definition rather than each linking their own.

sf::Color mix(sf::Color from, sf::Color to, float amount)
{
    const float t = std::clamp(amount, 0.0f, 1.0f);
    const auto blend = [&](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t));
    };
    return sf::Color(blend(from.r, to.r), blend(from.g, to.g), blend(from.b, to.b), blend(from.a, to.a));
}

namespace
{

void fillRect(sf::RenderWindow& window, sf::FloatRect rect, sf::Color color)
{
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f)
    {
        return;
    }
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setFillColor(color);
    window.draw(shape);
}

void strokeRect(sf::RenderWindow& window, sf::FloatRect rect, sf::Color color, float thickness = 1.0f)
{
    sf::RectangleShape shape({rect.size.x - thickness, rect.size.y - thickness});
    shape.setPosition({rect.position.x + thickness * 0.5f, rect.position.y + thickness * 0.5f});
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineThickness(thickness);
    shape.setOutlineColor(color);
    window.draw(shape);
}

// A gradient needs per-vertex colour; SFML has no gradient primitive.
void gradientRect(sf::RenderWindow& window, sf::FloatRect rect, sf::Color top, sf::Color bottom)
{
    if (rect.size.x <= 0.0f || rect.size.y <= 0.0f)
    {
        return;
    }
    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0] = {{rect.position.x, rect.position.y}, top};
    quad[1] = {{rect.position.x + rect.size.x, rect.position.y}, top};
    quad[2] = {{rect.position.x, rect.position.y + rect.size.y}, bottom};
    quad[3] = {{rect.position.x + rect.size.x, rect.position.y + rect.size.y}, bottom};
    window.draw(quad);
}

// A soft radial falloff, built as a fan so it does not read as the hard
// concentric rings the shop used to stack.
void radialGlow(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color, float squash = 1.0f)
{
    constexpr int Segments = 40;
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, Segments + 2);
    fan[0] = {center, color};
    for (int i = 0; i <= Segments; ++i)
    {
        const float angle = static_cast<float>(i) / static_cast<float>(Segments) * 6.2831853f;
        fan[static_cast<std::size_t>(i) + 1] = {
            {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius * squash},
            withAlpha(color, 0)};
    }
    window.draw(fan);
}

// Centre-cropped art that stays inside its rect. drawCoverSprite scales to fill
// but overflows the target, which is fine for a full-screen backdrop and wrong
// for a framed window; cropping in texture space instead keeps it contained.
void drawCroppedCover(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::FloatRect target,
    sf::Color tint,
    float verticalBias = 0.5f)
{
    const sf::Vector2u size = texture.getSize();
    if (size.x == 0 || size.y == 0 || target.size.x <= 0.0f || target.size.y <= 0.0f)
    {
        return;
    }

    const float targetAspect = target.size.x / target.size.y;
    float cropWidth = static_cast<float>(size.x);
    float cropHeight = cropWidth / targetAspect;
    if (cropHeight > static_cast<float>(size.y))
    {
        cropHeight = static_cast<float>(size.y);
        cropWidth = cropHeight * targetAspect;
    }

    const float left = (static_cast<float>(size.x) - cropWidth) * 0.5f;
    const float top = (static_cast<float>(size.y) - cropHeight) * std::clamp(verticalBias, 0.0f, 1.0f);
    drawTextureRectContain(
        window,
        texture,
        sf::IntRect(
            {static_cast<int>(std::lround(left)), static_cast<int>(std::lround(top))},
            {static_cast<int>(std::lround(cropWidth)), static_cast<int>(std::lround(cropHeight))}),
        target,
        tint);
}

sf::ConvexShape cutRect(sf::FloatRect rect, float cut)
{
    cut = std::max(0.0f, std::min(cut, std::min(rect.size.x, rect.size.y) * 0.45f));
    const sf::Vector2f p = rect.position;
    const sf::Vector2f s = rect.size;
    sf::ConvexShape shape(8);
    shape.setPoint(0, {p.x + cut, p.y});
    shape.setPoint(1, {p.x + s.x - cut, p.y});
    shape.setPoint(2, {p.x + s.x, p.y + cut});
    shape.setPoint(3, {p.x + s.x, p.y + s.y - cut});
    shape.setPoint(4, {p.x + s.x - cut, p.y + s.y});
    shape.setPoint(5, {p.x + cut, p.y + s.y});
    shape.setPoint(6, {p.x, p.y + s.y - cut});
    shape.setPoint(7, {p.x, p.y + cut});
    return shape;
}

// A quiet plate for list rows and chips. drawBeveledPlate hangs corner brackets
// and edge rivets on everything it draws; at 20-40px those brackets read as stray
// glyphs beside the label and the rivets collide with the row's own content.
void rowPlate(sf::RenderWindow& window, sf::FloatRect rect, sf::Color fill, sf::Color outline, bool lit, float cut)
{
    sf::ConvexShape shadow = cutRect({rect.position + sf::Vector2f(0.0f, 2.0f), rect.size}, cut);
    shadow.setFillColor(sf::Color(0, 0, 0, lit ? 140 : 96));
    window.draw(shadow);

    sf::ConvexShape plate = cutRect(rect, cut);
    plate.setFillColor(fill);
    plate.setOutlineThickness(1.4f);
    plate.setOutlineColor(outline);
    window.draw(plate);

    // Lit top edge, shaded bottom edge: enough to read as a raised plate.
    fillRect(
        window,
        {{rect.position.x + cut + 3.0f, rect.position.y + 1.5f}, {std::max(0.0f, rect.size.x - (cut + 3.0f) * 2.0f), 1.0f}},
        sf::Color(255, 224, 154, lit ? 96 : 44));
    fillRect(
        window,
        {{rect.position.x + cut + 3.0f, rect.position.y + rect.size.y - 2.5f}, {std::max(0.0f, rect.size.x - (cut + 3.0f) * 2.0f), 1.0f}},
        sf::Color(38, 24, 13, 150));
}

// A regular polygon used for gems and pips.
sf::ConvexShape makeGem(sf::Vector2f center, float radius, int points, float rotation)
{
    sf::ConvexShape shape(static_cast<std::size_t>(points));
    for (int i = 0; i < points; ++i)
    {
        const float angle = rotation + static_cast<float>(i) / static_cast<float>(points) * 6.2831853f;
        shape.setPoint(
            static_cast<std::size_t>(i),
            {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
    }
    return shape;
}

const std::array<FactionStyle, 4>& styles()
{
    static const std::array<FactionStyle, 4> value = {
        FactionStyle{
            "Blackthorn",
            "The Blackthorns",
            "Blackthorns",
            "Toll roads and hired steel. Grinds out every advantage.",
            sf::Color(198, 116, 72),
            sf::Color(38, 22, 17),
            "cards/blackthornForeman.png",
            Sigil::Cog},
        FactionStyle{
            "Mirewatch",
            "The Mirewatch Resistance",
            "Mirewatch",
            "Dug in and patient. Outlasts you in the mire.",
            sf::Color(104, 174, 162),
            sf::Color(14, 32, 32),
            "cards/marshlandVeteran.png",
            Sigil::Reed},
        FactionStyle{
            "Seelie",
            "The Seelie Court",
            "Seelie Court",
            "Bright and fast. Strikes before you are set.",
            sf::Color(214, 186, 104),
            sf::Color(30, 28, 16),
            "cards/sylvanChampion.png",
            Sigil::Leaf},
        FactionStyle{
            "Unseelie",
            "The Unseelie Court",
            "Unseelie Court",
            "Curses and ambush. Wins from the shadows.",
            sf::Color(160, 116, 206),
            sf::Color(26, 18, 38),
            "cards/thaeronBaelstone.png",
            Sigil::Thorn}};
    return value;
}

// sf::Text converts std::string through the classic locale, so a byte sequence is
// read as Latin-1 rather than UTF-8. A UTF-8 middot therefore renders as "Â·";
// the single Latin-1 byte is the one that comes out as a middot.
constexpr const char* Middot = "  \xb7  ";

// A plain tapering rule. drawSeparatorRule puts a rivet at its midpoint, which
// reads as a stray dot when the rule is only dividing content inside a panel.
void hairline(sf::RenderWindow& window, sf::Vector2f position, float width)
{
    const float half = width * 0.5f;
    sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, 6);
    const sf::Color edge = withAlpha(sf::Color(174, 117, 54), 0);
    const sf::Color core = withAlpha(sf::Color(174, 117, 54), 165);
    strip[0] = {{position.x, position.y}, edge};
    strip[1] = {{position.x, position.y + 1.0f}, edge};
    strip[2] = {{position.x + half, position.y}, core};
    strip[3] = {{position.x + half, position.y + 1.0f}, core};
    strip[4] = {{position.x + width, position.y}, edge};
    strip[5] = {{position.x + width, position.y + 1.0f}, edge};
    window.draw(strip);
}

// Legality as a glyph badge: a tick when the deck is playable, an exclamation
// when it is not. A bare coloured diamond read as a stray dot.
void statusBadge(sf::RenderWindow& window, sf::Vector2f center, float radius, bool ok)
{
    const sf::Color accent = ok ? sf::Color(138, 198, 142) : sf::Color(226, 170, 88);

    sf::CircleShape disc(radius);
    disc.setOrigin({radius, radius});
    disc.setPosition(center);
    disc.setFillColor(withAlpha(mix(accent, sf::Color(10, 15, 16), 0.72f), 246));
    disc.setOutlineThickness(1.2f);
    disc.setOutlineColor(withAlpha(accent, 235));
    window.draw(disc);

    if (ok)
    {
        sf::ConvexShape tick(3);
        tick.setPoint(0, {center.x - radius * 0.44f, center.y + radius * 0.04f});
        tick.setPoint(1, {center.x - radius * 0.10f, center.y + radius * 0.38f});
        tick.setPoint(2, {center.x + radius * 0.48f, center.y - radius * 0.40f});
        tick.setFillColor(sf::Color::Transparent);
        tick.setOutlineThickness(1.5f);
        tick.setOutlineColor(mix(accent, sf::Color::White, 0.4f));
        window.draw(tick);
    }
    else
    {
        fillRect(window, {{center.x - 0.9f, center.y - radius * 0.52f}, {1.8f, radius * 0.72f}}, accent);
        fillRect(window, {{center.x - 0.9f, center.y + radius * 0.34f}, {1.8f, 1.8f}}, accent);
    }
}

std::string lowered(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

} // namespace

const std::array<FactionStyle, 4>& factionStyles()
{
    return styles();
}

const FactionStyle& unalignedStyle()
{
    // Player-named decks get a neutral brass identity rather than being forced
    // into one of the four courts.
    static const FactionStyle value{
        "Unaligned",
        "Unaligned",
        "Unaligned",
        "A deck of your own making.",
        sf::Color(186, 148, 96),
        sf::Color(24, 22, 18),
        std::string(),
        Sigil::Leaf};
    return value;
}

const FactionStyle& factionStyleForDeckName(const std::string& deckName)
{
    const std::string needle = lowered(deckName);

    // Longest key first: "Unseelie" contains "Seelie", so a first-match scan gave
    // the Unseelie Court the Seelie Court's crest, art and blurb.
    std::array<const FactionStyle*, 4> byKeyLength{};
    for (std::size_t i = 0; i < styles().size(); ++i)
    {
        byKeyLength[i] = &styles()[i];
    }
    std::sort(byKeyLength.begin(), byKeyLength.end(), [](const FactionStyle* left, const FactionStyle* right) {
        return left->key.size() > right->key.size();
    });

    for (const FactionStyle* style : byKeyLength)
    {
        if (needle.find(lowered(style->key)) != std::string::npos)
        {
            return *style;
        }
    }
    return unalignedStyle();
}

sf::Color traitAccent(const std::string& trait)
{
    // Derived from the nine canonical trait labels, all inside the warm/violet
    // palette so a row's accent never introduces a foreign hue.
    static const std::map<std::string, sf::Color> accents = {
        {"corrupt", sf::Color(152, 104, 176)},
        {"fey", sf::Color(148, 196, 138)},
        {"civilized", sf::Color(210, 176, 108)},
        {"wild", sf::Color(184, 138, 78)},
        {"honorable", sf::Color(228, 200, 138)},
        {"arcane", sf::Color(146, 126, 214)},
        {"mechanical", sf::Color(158, 164, 172)},
        {"undead", sf::Color(130, 160, 150)},
        {"ancient", sf::Color(190, 152, 98)}};

    const auto found = accents.find(game_data::normalizedTrait(trait));
    return found == accents.end() ? palette::Brass : found->second;
}

sf::Color cardAccent(const card_data::Card& card)
{
    for (const std::string& trait : card.traits)
    {
        if (!trait.empty())
        {
            return traitAccent(trait);
        }
    }
    return palette::Brass;
}

DeckSummary summarizeDeck(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library)
{
    DeckSummary summary;
    summary.name = deck.name;
    summary.faction = &factionStyleForDeckName(deck.name);

    std::vector<std::string> seen;
    std::map<std::string, int> traitCounts;
    int bestHeroCost = -1;
    int bestCardCost = -1;
    std::string fallbackArt;
    std::string fallbackName;

    for (const std::string& title : deck.cardTitles)
    {
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        if (found == library.end())
        {
            continue;
        }
        const card_data::Card& card = *found;

        if (std::find(seen.begin(), seen.end(), title) == seen.end())
        {
            seen.push_back(title);
        }

        for (const std::string& trait : card.traits)
        {
            if (!trait.empty())
            {
                ++traitCounts[trait];
            }
        }

        if (game_data::isHeroCard(card))
        {
            ++summary.heroCount;
            const int heroCost = game_data::cardInt(card, "heroCost", 0);
            summary.heroCost += heroCost;
            // The deck's face is its most expensive hero.
            if (heroCost > bestHeroCost)
            {
                bestHeroCost = heroCost;
                summary.heroName = card.title;
                summary.heroArt = card.imagePath;
            }
        }
        else
        {
            ++summary.cardCount;
            const int cost = game_data::cardInt(card, "cost", 0);
            const std::size_t bucket = static_cast<std::size_t>(
                std::clamp(cost - 1, 0, static_cast<int>(ManaCurveBuckets) - 1));
            ++summary.curve[bucket];
            if (cost > bestCardCost)
            {
                bestCardCost = cost;
                fallbackName = card.title;
                fallbackArt = card.imagePath;
            }
        }
    }

    summary.uniqueTitles = static_cast<int>(seen.size());
    // A heroless deck still needs a face, so borrow its costliest card.
    if (summary.heroArt.empty())
    {
        summary.heroArt = fallbackArt;
        summary.heroName = fallbackName;
    }

    summary.legal = summary.cardCount == game_data::DeckCardCount &&
        summary.heroCount >= game_data::MinHeroes && summary.heroCount <= game_data::MaxHeroes &&
        summary.heroCost <= game_data::HeroCostLimit;

    summary.traits.assign(traitCounts.begin(), traitCounts.end());
    std::sort(summary.traits.begin(), summary.traits.end(), [](const auto& left, const auto& right) {
        return left.second != right.second ? left.second > right.second : left.first < right.first;
    });
    return summary;
}

// --- chrome ----------------------------------------------------------------

void drawScreenHeader(
    const UiContext& ui,
    const std::string& title,
    const std::string& account,
    int coins,
    float titleWidth)
{
    // A band anchors the header so the plaque, identity and wallet each own a
    // column instead of being placed by eye and overlapping.
    gradientRect(ui.window, {{0.0f, 0.0f}, {800.0f, 84.0f}}, sf::Color(6, 10, 11, 214), sf::Color(6, 10, 11, 0));
    fillRect(ui.window, {{0.0f, 71.0f}, {800.0f, 1.0f}}, withAlpha(palette::Brass, 110));
    fillRect(ui.window, {{0.0f, 72.0f}, {800.0f, 1.0f}}, sf::Color(0, 0, 0, 120));

    // A left-anchored plaque, drawn here rather than through drawTitlePlaque: that
    // helper hangs a symmetric pipe and rivet off both ends, and once the plaque
    // stopped being centred the right one floated unattached in open space.
    const sf::FloatRect plaque{{24.0f, 19.0f}, {titleWidth, 50.0f}};

    drawBeveledPlate(
        ui.window,
        plaque.position,
        plaque.size,
        sf::Color(23, 21, 18, 246),
        palette::BrassBright,
        true,
        16.0f);

    const unsigned int titleSize = static_cast<unsigned int>(std::clamp(plaque.size.y * 0.54f, 22.0f, 30.0f));
    const sf::Vector2f titleCenter{
        plaque.position.x + plaque.size.x * 0.5f,
        plaque.position.y + plaque.size.y * 0.5f};
    sf::Text titleShadow(ui.displayFont, title, titleSize);
    centerText(titleShadow, titleCenter + sf::Vector2f(1.5f, 2.5f));
    titleShadow.setFillColor(sf::Color(0, 0, 0, 205));
    drawCrispText(ui.window, titleShadow);

    sf::Text titleText(ui.displayFont, title, titleSize);
    centerText(titleText, titleCenter);
    titleText.setFillColor(palette::Ink);
    titleText.setOutlineThickness(0.8f);
    titleText.setOutlineColor(sf::Color(70, 43, 25, 190));
    drawCrispText(ui.window, titleText);

    // The wallet: struck coin in a framed pill, right-aligned before the Back
    // button's column.
    const float walletRight = 646.0f;
    sf::Text amount(ui.font, std::to_string(coins), 17);
    const float amountWidth = amount.getLocalBounds().size.x;
    const float walletWidth = std::max(88.0f, amountWidth + 58.0f);
    const float walletX = walletRight - walletWidth;
    drawBeveledPlate(
        ui.window,
        {walletX, 28.0f},
        {walletWidth, 32.0f},
        sf::Color(10, 15, 16, 232),
        withAlpha(palette::Brass, 210),
        false,
        7.0f);
    drawCoin(ui, {walletX + 19.0f, 44.0f}, 9.5f);
    drawText(
        ui.window,
        ui.font,
        std::to_string(coins),
        17,
        {walletX + 33.0f, 34.0f},
        palette::BrassPale,
        walletWidth - 42.0f);

    if (!account.empty())
    {
        // Right-aligned into the gap between plaque and wallet, so a long name
        // is elided rather than allowed to run under either.
        const float left = plaque.position.x + plaque.size.x + 68.0f;
        const float available = walletX - 14.0f - left;
        if (available > 60.0f)
        {
            const std::string label = elideToWidth(ui.font, account, 13, available);
            sf::Text text(ui.font, label, 13);
            drawText(
                ui.window,
                ui.font,
                label,
                13,
                {walletX - 14.0f - text.getLocalBounds().size.x, 38.0f},
                palette::MutedDim);
        }
    }
}

void drawSectionHeading(const UiContext& ui, sf::Vector2f position, const std::string& text, float rule)
{
    // The display face carries headings; Roboto carries everything smaller. That
    // contrast is the type hierarchy the screens were missing.
    sf::Text heading(ui.displayFont, text, 21);
    heading.setPosition(position);
    heading.setFillColor(palette::Ink);
    sf::Text shadow(heading);
    shadow.setPosition(position + sf::Vector2f(1.0f, 2.0f));
    shadow.setFillColor(sf::Color(0, 0, 0, 190));
    drawCrispText(ui.window, shadow);
    drawCrispText(ui.window, heading);

    if (rule > 0.0f)
    {
        const float y = position.y + 27.0f;
        gradientRect(
            ui.window,
            {{position.x, y}, {rule, 1.0f}},
            withAlpha(palette::Brass, 190),
            withAlpha(palette::Brass, 0));
    }
}

void drawInnerRule(const UiContext& ui, sf::Vector2f position, float width)
{
    hairline(ui.window, position, width);
}

void drawCaption(const UiContext& ui, sf::Vector2f position, const std::string& text, float maxWidth)
{
    drawText(ui.window, ui.font, text, 12, position, palette::MutedDim, maxWidth);
}

void drawVerticalScrim(const UiContext& ui, sf::FloatRect rect, sf::Color top, sf::Color bottom)
{
    gradientRect(ui.window, rect, top, bottom);
}

// --- parts -----------------------------------------------------------------

void drawCoin(const UiContext& ui, sf::Vector2f center, float radius)
{
    sf::CircleShape shadow(radius);
    shadow.setOrigin({radius, radius});
    shadow.setPosition(center + sf::Vector2f(0.0f, 1.5f));
    shadow.setFillColor(sf::Color(0, 0, 0, 130));
    ui.window.draw(shadow);

    sf::CircleShape rim(radius);
    rim.setOrigin({radius, radius});
    rim.setPosition(center);
    rim.setFillColor(sf::Color(168, 112, 36));
    rim.setOutlineThickness(std::max(1.0f, radius * 0.16f));
    rim.setOutlineColor(sf::Color(96, 58, 20, 230));
    ui.window.draw(rim);

    sf::CircleShape face(radius * 0.74f);
    face.setOrigin({radius * 0.74f, radius * 0.74f});
    face.setPosition(center);
    face.setFillColor(sf::Color(226, 172, 62));
    ui.window.draw(face);

    // Struck detail: a slim bar so the coin reads as minted, not as a dot.
    fillRect(
        ui.window,
        {{center.x - radius * 0.30f, center.y - radius * 0.10f}, {radius * 0.60f, radius * 0.20f}},
        sf::Color(142, 88, 24, 200));

    sf::CircleShape shine(radius * 0.30f);
    shine.setOrigin({radius * 0.30f, radius * 0.30f});
    shine.setPosition(center + sf::Vector2f(-radius * 0.30f, -radius * 0.34f));
    shine.setFillColor(sf::Color(255, 232, 168, 150));
    ui.window.draw(shine);
}

void drawCostGem(const UiContext& ui, sf::Vector2f center, float radius, int value, bool hero)
{
    const sf::Color deep = hero ? sf::Color(88, 54, 20) : sf::Color(48, 28, 74);
    const sf::Color bright = hero ? sf::Color(244, 202, 112) : sf::Color(176, 138, 224);
    const sf::Color mid = hero ? sf::Color(186, 128, 48) : sf::Color(112, 76, 168);

    radialGlow(ui.window, center, radius * 1.9f, withAlpha(bright, 46));

    // A hero's cost sits on a brass shield, a resource cost on a violet gem, so
    // the two costs are told apart by shape rather than by a text prefix.
    const int points = hero ? 6 : 8;
    sf::ConvexShape shadow = makeGem(center + sf::Vector2f(0.0f, 1.6f), radius, points, hero ? 1.5708f : 0.3927f);
    shadow.setFillColor(sf::Color(0, 0, 0, 150));
    ui.window.draw(shadow);

    sf::ConvexShape gem = makeGem(center, radius, points, hero ? 1.5708f : 0.3927f);
    gem.setFillColor(mid);
    gem.setOutlineThickness(1.4f);
    gem.setOutlineColor(deep);
    ui.window.draw(gem);

    // Top facet catches the light.
    sf::ConvexShape facet = makeGem(center - sf::Vector2f(0.0f, radius * 0.20f), radius * 0.58f, points, hero ? 1.5708f : 0.3927f);
    facet.setFillColor(withAlpha(bright, 150));
    ui.window.draw(facet);

    sf::Text label(ui.font, std::to_string(value), static_cast<unsigned int>(std::max(9.0f, radius * 1.25f)));
    label.setFillColor(sf::Color(255, 248, 232));
    label.setOutlineThickness(1.2f);
    label.setOutlineColor(sf::Color(20, 10, 30, 220));
    centerText(label, center);
    drawCrispText(ui.window, label);
}

void drawRarityGem(const UiContext& ui, sf::Vector2f center, float radius, sf::Color color)
{
    sf::ConvexShape shadow = makeGem(center + sf::Vector2f(0.0f, 1.0f), radius, 4, 0.7854f);
    shadow.setFillColor(sf::Color(0, 0, 0, 150));
    ui.window.draw(shadow);

    sf::ConvexShape gem = makeGem(center, radius, 4, 0.7854f);
    gem.setFillColor(color);
    gem.setOutlineThickness(1.0f);
    gem.setOutlineColor(sf::Color(18, 14, 10, 220));
    ui.window.draw(gem);

    sf::ConvexShape facet = makeGem(center - sf::Vector2f(0.0f, radius * 0.26f), radius * 0.46f, 4, 0.7854f);
    facet.setFillColor(sf::Color(255, 255, 255, 120));
    ui.window.draw(facet);
}

void drawFactionCrest(
    const UiContext& ui,
    sf::Vector2f center,
    float radius,
    const FactionStyle& style,
    bool bright)
{
    const sf::Color accent = bright ? style.accent : mix(style.accent, sf::Color(70, 66, 60), 0.45f);
    radialGlow(ui.window, center, radius * 2.1f, withAlpha(accent, bright ? 62 : 30));

    // Shield: a hexagon flattened at the top with a point at the bottom.
    sf::ConvexShape shield(6);
    shield.setPoint(0, {center.x - radius * 0.82f, center.y - radius * 0.86f});
    shield.setPoint(1, {center.x + radius * 0.82f, center.y - radius * 0.86f});
    shield.setPoint(2, {center.x + radius * 0.82f, center.y + radius * 0.22f});
    shield.setPoint(3, {center.x, center.y + radius});
    shield.setPoint(4, {center.x - radius * 0.82f, center.y + radius * 0.22f});
    shield.setPoint(5, {center.x - radius * 0.82f, center.y - radius * 0.86f});

    sf::ConvexShape shadow(shield);
    shadow.move({0.0f, 2.0f});
    shadow.setFillColor(sf::Color(0, 0, 0, 160));
    ui.window.draw(shadow);

    shield.setFillColor(mix(style.deep, sf::Color::Black, 0.25f));
    shield.setOutlineThickness(1.6f);
    shield.setOutlineColor(accent);
    ui.window.draw(shield);

    const sf::Color markColor = bright ? mix(accent, sf::Color::White, 0.30f) : accent;
    switch (style.sigil)
    {
    case Sigil::Leaf:
    {
        // A leaf: pointed tip, shouldered flanks, visible midrib and stem. The
        // plain lozenge it replaces read as a generic UI diamond.
        sf::ConvexShape leaf(6);
        leaf.setPoint(0, {center.x, center.y - radius * 0.66f});
        leaf.setPoint(1, {center.x + radius * 0.30f, center.y - radius * 0.22f});
        leaf.setPoint(2, {center.x + radius * 0.34f, center.y + radius * 0.16f});
        leaf.setPoint(3, {center.x, center.y + radius * 0.44f});
        leaf.setPoint(4, {center.x - radius * 0.34f, center.y + radius * 0.16f});
        leaf.setPoint(5, {center.x - radius * 0.30f, center.y - radius * 0.22f});
        leaf.setFillColor(markColor);
        ui.window.draw(leaf);
        // Midrib and stem, in the shield's own dark so they read as cut lines.
        const sf::Color rib = mix(style.deep, sf::Color::Black, 0.5f);
        fillRect(
            ui.window,
            {{center.x - radius * 0.045f, center.y - radius * 0.56f}, {radius * 0.09f, radius * 1.10f}},
            rib);
        for (int i = 0; i < 2; ++i)
        {
            const float y = center.y - radius * (0.24f - static_cast<float>(i) * 0.30f);
            fillRect(ui.window, {{center.x - radius * 0.24f, y}, {radius * 0.20f, radius * 0.055f}}, rib);
            fillRect(ui.window, {{center.x + radius * 0.04f, y}, {radius * 0.20f, radius * 0.055f}}, rib);
        }
        break;
    }
    case Sigil::Thorn:
    {
        // A curved thorn: a tapering claw with two barbs off its inner edge. The
        // pair of triangles it replaces read as a paper aeroplane.
        constexpr int Steps = 7;
        for (int i = 0; i < Steps; ++i)
        {
            const float t0 = static_cast<float>(i) / static_cast<float>(Steps);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(Steps);
            const auto spine = [&](float t) {
                // Sweeps from the lower left up to a point at the upper right.
                const float angle = -2.2f + t * 1.9f;
                return sf::Vector2f{
                    center.x + std::cos(angle) * radius * 0.62f + radius * 0.30f,
                    center.y + std::sin(angle) * radius * 0.62f + radius * 0.30f};
            };
            const sf::Vector2f a = spine(t0);
            const sf::Vector2f b = spine(t1);
            const float w0 = radius * 0.20f * (1.0f - t0);
            const float w1 = radius * 0.20f * (1.0f - t1);
            sf::ConvexShape segment(4);
            segment.setPoint(0, {a.x - w0, a.y - w0});
            segment.setPoint(1, {b.x - w1, b.y - w1});
            segment.setPoint(2, {b.x + w1, b.y + w1});
            segment.setPoint(3, {a.x + w0, a.y + w0});
            segment.setFillColor(markColor);
            ui.window.draw(segment);

            // Two barbs branching off the outer edge.
            if (i == 2 || i == 4)
            {
                sf::ConvexShape barb(3);
                barb.setPoint(0, {a.x - w0, a.y - w0});
                barb.setPoint(1, {a.x - radius * 0.30f, a.y - radius * 0.14f});
                barb.setPoint(2, {a.x + w0 * 0.4f, a.y + w0});
                barb.setFillColor(withAlpha(markColor, 225));
                ui.window.draw(barb);
            }
        }
        break;
    }
    case Sigil::Reed:
    {
        // Three reeds of different heights.
        for (int i = 0; i < 3; ++i)
        {
            const float offset = (static_cast<float>(i) - 1.0f) * radius * 0.30f;
            const float height = radius * (i == 1 ? 1.06f : 0.80f);
            fillRect(
                ui.window,
                {{center.x + offset - radius * 0.055f, center.y + radius * 0.50f - height},
                 {radius * 0.11f, height}},
                i == 1 ? markColor : withAlpha(markColor, 210));
        }
        break;
    }
    case Sigil::Cog:
    {
        // A toothed wheel with a bored centre.
        sf::ConvexShape teeth = makeGem(center, radius * 0.52f, 8, 0.3927f);
        teeth.setFillColor(markColor);
        ui.window.draw(teeth);
        sf::CircleShape hub(radius * 0.34f);
        hub.setOrigin({radius * 0.34f, radius * 0.34f});
        hub.setPosition(center);
        hub.setFillColor(markColor);
        ui.window.draw(hub);
        sf::CircleShape bore(radius * 0.15f);
        bore.setOrigin({radius * 0.15f, radius * 0.15f});
        bore.setPosition(center);
        bore.setFillColor(mix(style.deep, sf::Color::Black, 0.4f));
        ui.window.draw(bore);
        break;
    }
    }

    // Inner hairline, the same detail the menu frames carry.
    sf::ConvexShape inner(shield);
    inner.setScale({0.82f, 0.82f});
    inner.setOrigin({center.x, center.y});
    inner.setPosition(center);
    inner.setFillColor(sf::Color::Transparent);
    inner.setOutlineThickness(0.8f);
    inner.setOutlineColor(withAlpha(accent, 130));
    ui.window.draw(inner);
}

void drawArtWindow(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::string& artPath,
    sf::Color edge,
    float cut,
    bool cover,
    float verticalBias)
{
    // Recess the art: black bed, art, then a lit top edge and dark bottom edge
    // so the window looks cut into the plate rather than pasted on.
    fillRect(ui.window, rect, sf::Color(4, 7, 8, 255));

    sf::Texture* art = artPath.empty() ? nullptr : ui.textures.load(artPath);
    if (art)
    {
        if (cover)
        {
            drawCroppedCover(ui.window, *art, rect, sf::Color::White, verticalBias);
        }
        else
        {
            drawContainSprite(ui.window, *art, rect);
        }
    }
    else
    {
        gradientRect(ui.window, rect, withAlpha(edge, 46), sf::Color(4, 7, 8, 255));
    }

    fillRect(ui.window, {{rect.position.x, rect.position.y}, {rect.size.x, 1.0f}}, sf::Color(0, 0, 0, 170));
    strokeRect(ui.window, rect, withAlpha(edge, 210), 1.0f);
    static_cast<void>(cut);
}

void drawCopyPips(const UiContext& ui, sf::Vector2f left, int filled, int total, sf::Color color)
{
    // Copies held out of copies allowed. Shape carries the meaning, so no "2/2"
    // debug string is needed.
    const int shown = std::clamp(total, 0, 4);
    for (int i = 0; i < shown; ++i)
    {
        const sf::Vector2f center{left.x + 3.4f + static_cast<float>(i) * 9.0f, left.y};
        if (i < filled)
        {
            sf::ConvexShape pip = makeGem(center, 3.4f, 4, 0.7854f);
            pip.setFillColor(color);
            ui.window.draw(pip);
        }
        else
        {
            sf::ConvexShape pip = makeGem(center, 3.4f, 4, 0.7854f);
            pip.setFillColor(sf::Color::Transparent);
            pip.setOutlineThickness(1.0f);
            pip.setOutlineColor(withAlpha(color, 130));
            ui.window.draw(pip);
        }
    }
}

void drawMeter(
    const UiContext& ui,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& label,
    const std::string& value,
    float fill,
    sf::Color color)
{
    drawText(ui.window, ui.font, label, 10, position, palette::MutedDim, size.x - 44.0f);

    sf::Text valueText(ui.font, value, 12);
    drawText(
        ui.window,
        ui.font,
        value,
        12,
        {position.x + size.x - valueText.getLocalBounds().size.x, position.y - 1.0f},
        color);

    const sf::FloatRect track{{position.x, position.y + 15.0f}, {size.x, 4.0f}};
    fillRect(ui.window, track, sf::Color(0, 0, 0, 170));
    strokeRect(ui.window, track, withAlpha(palette::BrassDim, 200), 1.0f);
    const float amount = std::clamp(fill, 0.0f, 1.0f);
    if (amount > 0.0f)
    {
        gradientRect(
            ui.window,
            {{track.position.x + 1.0f, track.position.y + 1.0f}, {(track.size.x - 2.0f) * amount, track.size.y - 2.0f}},
            mix(color, sf::Color::White, 0.25f),
            color);
    }
}

void drawManaCurve(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::array<int, ManaCurveBuckets>& curve,
    sf::Color color,
    bool labelled)
{
    const int peak = std::max(1, *std::max_element(curve.begin(), curve.end()));
    const float slot = rect.size.x / static_cast<float>(ManaCurveBuckets);
    const float barWidth = std::max(3.0f, slot - 3.0f);
    const float axisY = rect.position.y + rect.size.y - (labelled ? 11.0f : 0.0f);
    const float plotHeight = axisY - rect.position.y;

    for (std::size_t i = 0; i < ManaCurveBuckets; ++i)
    {
        const float x = rect.position.x + static_cast<float>(i) * slot + (slot - barWidth) * 0.5f;
        // An empty bucket still shows its socket, so the curve reads as a chart.
        fillRect(ui.window, {{x, axisY - 2.0f}, {barWidth, 2.0f}}, sf::Color(0, 0, 0, 140));
        if (curve[i] > 0)
        {
            const float height = std::max(3.0f, plotHeight * static_cast<float>(curve[i]) / static_cast<float>(peak));
            gradientRect(
                ui.window,
                {{x, axisY - height}, {barWidth, height}},
                mix(color, sf::Color::White, 0.34f),
                withAlpha(color, 205));
            fillRect(ui.window, {{x, axisY - height}, {barWidth, 1.0f}}, mix(color, sf::Color::White, 0.65f));
        }
        if (labelled)
        {
            const std::string tick = i + 1 == ManaCurveBuckets
                ? std::to_string(ManaCurveBuckets) + "+"
                : std::to_string(i + 1);
            sf::Text text(ui.font, tick, 9);
            drawText(
                ui.window,
                ui.font,
                tick,
                9,
                {x + (barWidth - text.getLocalBounds().size.x) * 0.5f, axisY + 1.0f},
                palette::MutedDim);
        }
    }

    fillRect(ui.window, {{rect.position.x, axisY}, {rect.size.x, 1.0f}}, withAlpha(palette::Brass, 120));
}

void drawValidationSlot(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::vector<std::string>& warnings,
    bool legal)
{
    const bool ok = warnings.empty() && legal;
    const sf::Color accent = ok ? palette::Good : palette::Warn;

    drawBeveledPlate(
        ui.window,
        rect.position,
        rect.size,
        ok ? sf::Color(14, 26, 20, 232) : sf::Color(32, 22, 14, 236),
        withAlpha(accent, 190),
        false,
        6.0f);

    // Accent spine, so the slot's state is legible before the text is read.
    fillRect(ui.window, {{rect.position.x + 2.0f, rect.position.y + 5.0f}, {2.5f, rect.size.y - 10.0f}}, accent);

    const sf::Vector2f iconCenter{rect.position.x + 22.0f, rect.position.y + rect.size.y * 0.5f};
    if (ok)
    {
        // Tick.
        sf::ConvexShape mark(3);
        mark.setPoint(0, {iconCenter.x - 6.0f, iconCenter.y});
        mark.setPoint(1, {iconCenter.x - 2.0f, iconCenter.y + 5.0f});
        mark.setPoint(2, {iconCenter.x + 7.0f, iconCenter.y - 6.0f});
        mark.setFillColor(sf::Color::Transparent);
        mark.setOutlineThickness(2.0f);
        mark.setOutlineColor(accent);
        ui.window.draw(mark);
    }
    else
    {
        sf::ConvexShape triangle(3);
        triangle.setPoint(0, {iconCenter.x, iconCenter.y - 7.5f});
        triangle.setPoint(1, {iconCenter.x + 7.0f, iconCenter.y + 5.5f});
        triangle.setPoint(2, {iconCenter.x - 7.0f, iconCenter.y + 5.5f});
        triangle.setFillColor(withAlpha(accent, 70));
        triangle.setOutlineThickness(1.4f);
        triangle.setOutlineColor(accent);
        ui.window.draw(triangle);
        fillRect(ui.window, {{iconCenter.x - 0.9f, iconCenter.y - 3.6f}, {1.8f, 5.4f}}, accent);
        fillRect(ui.window, {{iconCenter.x - 0.9f, iconCenter.y + 3.0f}, {1.8f, 1.8f}}, accent);
    }

    const float textX = rect.position.x + 40.0f;
    const float textWidth = rect.size.x - 52.0f;
    if (ok)
    {
        drawText(ui.window, ui.font, "Deck is legal", 14, {textX, rect.position.y + 8.0f}, accent, textWidth);
        drawText(
            ui.window,
            ui.font,
            "Ready to save and take into a match.",
            11,
            {textX, rect.position.y + 26.0f},
            palette::MutedDim,
            textWidth);
        return;
    }

    drawText(ui.window, ui.font, warnings.front(), 14, {textX, rect.position.y + 8.0f}, accent, textWidth);
    if (warnings.size() > 1)
    {
        const std::string more = warnings.size() == 2
            ? warnings[1]
            : warnings[1] + "  (+" + std::to_string(warnings.size() - 2) + " more)";
        drawText(ui.window, ui.font, more, 11, {textX, rect.position.y + 26.0f}, palette::MutedDim, textWidth);
    }
}

void drawEmptyState(
    const UiContext& ui,
    sf::FloatRect rect,
    const std::string& heading,
    const std::string& hint)
{
    const sf::Vector2f center{rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.42f};

    // A ghost card back rather than a bare sentence.
    const sf::Vector2f cardSize{74.0f, 104.0f};
    const sf::FloatRect card{{center.x - cardSize.x * 0.5f, center.y - cardSize.y * 0.72f}, cardSize};
    fillRect(ui.window, card, sf::Color(8, 13, 14, 190));

    // Dashed border, drawn as ticks so it reads as a placeholder slot.
    const float dash = 6.0f;
    for (float x = card.position.x; x < card.position.x + card.size.x; x += dash * 2.0f)
    {
        const float width = std::min(dash, card.position.x + card.size.x - x);
        fillRect(ui.window, {{x, card.position.y}, {width, 1.0f}}, withAlpha(palette::Brass, 120));
        fillRect(ui.window, {{x, card.position.y + card.size.y - 1.0f}, {width, 1.0f}}, withAlpha(palette::Brass, 120));
    }
    for (float y = card.position.y; y < card.position.y + card.size.y; y += dash * 2.0f)
    {
        const float height = std::min(dash, card.position.y + card.size.y - y);
        fillRect(ui.window, {{card.position.x, y}, {1.0f, height}}, withAlpha(palette::Brass, 120));
        fillRect(ui.window, {{card.position.x + card.size.x - 1.0f, y}, {1.0f, height}}, withAlpha(palette::Brass, 120));
    }

    // A plus inside the ghost: an octagon with a single bar read as a "no entry"
    // sign, which is the opposite of the invitation this state is making.
    const sf::Vector2f markCenter{center.x, card.position.y + card.size.y * 0.5f};
    sf::ConvexShape ring = makeGem(markCenter, 16.0f, 8, 0.3927f);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(1.2f);
    ring.setOutlineColor(withAlpha(palette::Brass, 120));
    ui.window.draw(ring);
    fillRect(ui.window, {{markCenter.x - 7.0f, markCenter.y - 1.2f}, {14.0f, 2.4f}}, withAlpha(palette::Brass, 150));
    fillRect(ui.window, {{markCenter.x - 1.2f, markCenter.y - 7.0f}, {2.4f, 14.0f}}, withAlpha(palette::Brass, 150));

    sf::Text headingText(ui.displayFont, heading, 19);
    centerText(headingText, {center.x, card.position.y + card.size.y + 24.0f});
    headingText.setFillColor(palette::Muted);
    drawCrispText(ui.window, headingText);

    const std::vector<std::string> lines = wrapText(ui.font, hint, 12, rect.size.x - 60.0f);
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        sf::Text line(ui.font, lines[i], 12);
        centerText(line, {center.x, card.position.y + card.size.y + 50.0f + static_cast<float>(i) * 16.0f});
        line.setFillColor(palette::MutedDim);
        drawCrispText(ui.window, line);
    }
}

void drawDisabledButton(
    const UiContext& ui,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& label)
{
    // Unavailable actions were drawn as live buttons, so the screen offered verbs
    // that did nothing. One treatment for all of them.
    drawBeveledPlate(
        ui.window,
        position,
        size,
        sf::Color(28, 28, 26, 208),
        sf::Color(84, 79, 69, 190),
        false,
        std::clamp(size.y * 0.20f, 5.0f, 11.0f));

    sf::Text text(ui.font, label, 20);
    centerButtonText(text, position + size * 0.5f);
    text.setFillColor(sf::Color(146, 148, 146, 205));
    drawCrispText(ui.window, text);
}

void drawFactionRoster(const UiContext& ui, sf::FloatRect rect, const std::string& hint)
{
    // Shown where a deck portrait would go when the player has no decks at all.
    // A second dashed ghost card beside the roster's read as a rendering fault.
    sf::Text heading(ui.displayFont, "Four Courts Await", 21);
    centerText(heading, {rect.position.x + rect.size.x * 0.5f, rect.position.y + 16.0f});
    heading.setFillColor(palette::Muted);
    drawCrispText(ui.window, heading);

    float y = rect.position.y + 50.0f;
    for (const FactionStyle& style : styles())
    {
        drawFactionCrest(ui, {rect.position.x + 34.0f, y + 22.0f}, 17.0f, style, true);
        sf::Text name(ui.displayFont, style.label, 16);
        name.setPosition({rect.position.x + 62.0f, y + 3.0f});
        name.setFillColor(withAlpha(style.accent, 246));
        drawCrispText(ui.window, name);
        // Wrapped, not elided: a truncated strategy line tells a player nothing.
        drawWrappedText(
            ui.window,
            ui.font,
            style.blurb,
            11,
            {rect.position.x + 62.0f, y + 24.0f},
            palette::MutedDim,
            rect.size.x - 78.0f,
            2.0f);
        y += 62.0f;
        if (&style != &styles().back())
        {
            drawInnerRule(ui, {rect.position.x + 24.0f, y - 12.0f}, rect.size.x - 48.0f);
        }
    }

    const std::vector<std::string> lines = wrapText(ui.font, hint, 12, rect.size.x - 56.0f);
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        sf::Text line(ui.font, lines[i], 12);
        centerText(line, {rect.position.x + rect.size.x * 0.5f, y + 16.0f + static_cast<float>(i) * 16.0f});
        line.setFillColor(palette::MutedDim);
        drawCrispText(ui.window, line);
    }
}

// --- rows ------------------------------------------------------------------

void drawCardRow(const UiContext& ui, const CardRow& row)
{
    if (!row.card)
    {
        return;
    }

    const card_data::Card& card = *row.card;
    const bool hero = game_data::isHeroCard(card);
    // Rarity, not trait, drives the row's colour: it is what a player scans a
    // collection for, and it is the convention every shipping CCG uses.
    const sf::Color rarity = cardRarityColor(card);
    const sf::Color accent = row.traitMismatch ? palette::Warn : rarity;
    const sf::FloatRect rect = row.rect;

    rowPlate(
        ui.window,
        rect,
        row.selected ? sf::Color(58, 40, 22, 240)
                     : (row.hovered ? sf::Color(26, 34, 35, 234) : sf::Color(15, 22, 23, 226)),
        row.selected ? palette::BrassBright : (row.hovered ? withAlpha(palette::Brass, 225) : withAlpha(palette::Brass, 140)),
        row.selected,
        4.0f);

    // Rarity spine down the leading edge.
    fillRect(ui.window, {{rect.position.x + 2.0f, rect.position.y + 4.0f}, {2.5f, rect.size.y - 8.0f}}, accent);

    const float midY = rect.position.y + rect.size.y * 0.5f;
    drawCostGem(
        ui,
        {rect.position.x + 22.0f, midY},
        10.5f,
        hero ? game_data::cardInt(card, "heroCost", 0) : game_data::cardInt(card, "cost", 0),
        hero);

    // A portrait window, not a square one: card art is a standing figure, and a
    // square crop of it cuts the head off.
    const float thumbHeight = rect.size.y - 10.0f;
    const sf::FloatRect art{
        {rect.position.x + 39.0f, rect.position.y + 5.0f},
        {thumbHeight * 0.78f, thumbHeight}};
    drawArtWindow(ui, art, card.imagePath, accent, 3.0f, true, 0.10f);

    const float textX = art.position.x + art.size.x + 10.0f;
    // Reserve the right cluster so a long title elides instead of running under it.
    const float rightCluster = 58.0f;
    const float textWidth = rect.position.x + rect.size.x - rightCluster - textX;

    drawText(ui.window, ui.font, card.title, 15, {textX, rect.position.y + 5.0f}, palette::Ink, textWidth);

    std::string subline = hero ? std::string("Hero") : card.type;
    if (!card.traits.empty() && !card.traits.front().empty())
    {
        subline += Middot + card.traits.front();
    }
    drawText(ui.window, ui.font, subline, 11, {textX, rect.position.y + rect.size.y - 17.0f}, palette::MutedDim, textWidth);

    // Holdings as a single struck count. Pips and an "x2" were two encodings of
    // the same fact, and at row height neither was legible; the count going gold
    // when no more copies may be added carries the limit on its own.
    const float clusterRight = rect.position.x + rect.size.x - 12.0f;
    const int shownCount = row.copyLimit > 0 ? row.copies : row.owned;
    const int shownLimit = row.copyLimit;
    if (row.copyLimit > 0 || row.showOwned)
    {
        const bool atLimit = shownLimit > 0 && shownCount >= shownLimit;
        const sf::Color countColor = shownCount <= 0
            ? palette::MutedDim
            : (atLimit ? palette::BrassPale : palette::Muted);
        const std::string count = "x" + std::to_string(std::max(0, shownCount));
        sf::Text countText(ui.font, count, 15);
        drawText(
            ui.window,
            ui.font,
            count,
            15,
            {clusterRight - countText.getLocalBounds().size.x, rect.position.y + rect.size.y * 0.5f - 10.0f},
            countColor);
    }

    if (row.traitMismatch)
    {
        // The unit shares no trait with any hero in the deck.
        strokeRect(ui.window, rect, withAlpha(palette::Warn, 190), 1.0f);
    }
    if (row.showOwned && row.owned <= 0)
    {
        // Unowned cards stay visible but read as unavailable.
        fillRect(ui.window, rect, sf::Color(6, 9, 10, 120));
    }
}

void drawDeckRosterRow(
    const UiContext& ui,
    sf::FloatRect rect,
    const DeckSummary& summary,
    bool selected,
    bool hovered)
{
    const FactionStyle& style = summary.faction ? *summary.faction : unalignedStyle();

    drawBeveledPlate(
        ui.window,
        rect.position,
        rect.size,
        selected ? mix(style.deep, sf::Color(64, 44, 22), 0.55f)
                 : (hovered ? sf::Color(24, 32, 33, 236) : sf::Color(14, 21, 22, 228)),
        selected ? palette::BrassBright : (hovered ? withAlpha(palette::Brass, 230) : withAlpha(palette::Brass, 145)),
        selected,
        5.0f);

    // A faction wash on the selected row ties it to its crest colour.
    if (selected)
    {
        gradientRect(
            ui.window,
            {{rect.position.x + 4.0f, rect.position.y + 4.0f}, {rect.size.x * 0.55f, rect.size.y - 8.0f}},
            withAlpha(style.accent, 44),
            withAlpha(style.accent, 0));
    }
    fillRect(ui.window, {{rect.position.x + 2.0f, rect.position.y + 5.0f}, {2.5f, rect.size.y - 10.0f}}, style.accent);

    // Hero portrait, cropped to the head, in a portrait window.
    const sf::FloatRect portrait{
        {rect.position.x + 11.0f, rect.position.y + 6.0f},
        {(rect.size.y - 12.0f) * 0.74f, rect.size.y - 12.0f}};
    drawArtWindow(ui, portrait, summary.heroArt, style.accent, 3.0f, true, 0.06f);
    drawFactionCrest(
        ui,
        {portrait.position.x + portrait.size.x + 1.0f, portrait.position.y + portrait.size.y - 6.0f},
        10.0f,
        style,
        selected);

    const float textX = portrait.position.x + portrait.size.x + 20.0f;
    const float curveWidth = 52.0f;
    const float textWidth = rect.position.x + rect.size.x - curveWidth - 34.0f - textX;

    sf::Text name(ui.displayFont, elideToWidth(ui.displayFont, summary.name, 17, textWidth), 17);
    name.setPosition({textX, rect.position.y + 8.0f});
    name.setFillColor(selected ? sf::Color(255, 246, 224) : palette::Ink);
    drawCrispText(ui.window, name);

    // Court and card count only. Adding the hero count pushed the line past the
    // width the curve leaves it, so it always elided to "2...".
    const std::string meta = style.shortLabel + Middot +
        std::to_string(summary.cardCount) + "/" + std::to_string(game_data::DeckCardCount) + " cards";
    drawText(ui.window, ui.font, meta, 11, {textX, rect.position.y + rect.size.y - 20.0f}, palette::MutedDim, textWidth);

    // Legality as a glyph badge rather than an unlabelled coloured diamond.
    statusBadge(
        ui.window,
        {rect.position.x + rect.size.x - 18.0f, rect.position.y + 17.0f},
        8.0f,
        summary.legal);

    drawManaCurve(
        ui,
        {{rect.position.x + rect.size.x - curveWidth - 12.0f, rect.position.y + rect.size.y - 24.0f}, {curveWidth, 16.0f}},
        summary.curve,
        style.accent,
        false);
}

void drawDeckDetailPanel(const UiContext& ui, sf::FloatRect rect, const DeckSummary& summary)
{
    const FactionStyle& style = summary.faction ? *summary.faction : unalignedStyle();

    drawPanel(ui.window, rect.position, rect.size);

    // Hero art fills the head of the panel and sinks into it under a scrim, the
    // way a deck box carries its champion.
    // The panel is used at two heights (the editor's roster and the pre-match
    // picker), so the art scales and the trait row anchors to the foot rather than
    // everything being laid out from the top at fixed offsets and overflowing.
    const sf::FloatRect art{
        {rect.position.x + 13.0f, rect.position.y + 13.0f},
        {rect.size.x - 26.0f, std::clamp(rect.size.y * 0.38f, 118.0f, 156.0f)}};
    fillRect(ui.window, art, sf::Color(4, 7, 8));
    if (sf::Texture* texture = summary.heroArt.empty() ? nullptr : ui.textures.load(summary.heroArt))
    {
        drawCroppedCover(ui.window, *texture, art, sf::Color::White, 0.18f);
    }
    else
    {
        gradientRect(ui.window, art, withAlpha(style.accent, 60), sf::Color(4, 7, 8));
    }
    // Faction tint plus a bottom scrim so the name reads over any art.
    gradientRect(ui.window, art, withAlpha(style.accent, 34), withAlpha(style.deep, 130));
    gradientRect(
        ui.window,
        {{art.position.x, art.position.y + art.size.y * 0.45f}, {art.size.x, art.size.y * 0.55f}},
        sf::Color(6, 9, 10, 0),
        sf::Color(6, 9, 10, 246));
    strokeRect(ui.window, art, withAlpha(style.accent, 170), 1.0f);

    drawFactionCrest(ui, {art.position.x + 30.0f, art.position.y + 30.0f}, 17.0f, style, true);

    sf::Text name(
        ui.displayFont,
        elideToWidth(ui.displayFont, summary.name, 23, art.size.x - 24.0f),
        23);
    name.setPosition({art.position.x + 13.0f, art.position.y + art.size.y - 52.0f});
    sf::Text nameShadow(name);
    nameShadow.move({1.0f, 2.0f});
    nameShadow.setFillColor(sf::Color(0, 0, 0, 210));
    drawCrispText(ui.window, nameShadow);
    name.setFillColor(sf::Color(255, 246, 226));
    drawCrispText(ui.window, name);

    std::string identity = style.label;
    if (!summary.heroName.empty())
    {
        identity += Middot + summary.heroName;
    }
    drawText(
        ui.window,
        ui.font,
        identity,
        11,
        {art.position.x + 14.0f, art.position.y + art.size.y - 22.0f},
        withAlpha(style.accent, 240),
        art.size.x - 26.0f);

    // Counters as labelled meters rather than bare ratios.
    const float meterX = rect.position.x + 22.0f;
    const float meterWidth = (rect.size.x - 44.0f - 16.0f) * 0.5f;
    float y = art.position.y + art.size.y + 20.0f;
    drawMeter(
        ui,
        {meterX, y},
        {meterWidth, 20.0f},
        "CARDS",
        std::to_string(summary.cardCount) + "/" + std::to_string(game_data::DeckCardCount),
        static_cast<float>(summary.cardCount) / static_cast<float>(game_data::DeckCardCount),
        summary.cardCount == game_data::DeckCardCount ? palette::Good : palette::Warn);
    drawMeter(
        ui,
        {meterX + meterWidth + 16.0f, y},
        {meterWidth, 20.0f},
        "HEROES",
        std::to_string(summary.heroCount) + "/" + std::to_string(game_data::MaxHeroes),
        static_cast<float>(summary.heroCount) / static_cast<float>(game_data::MaxHeroes),
        (summary.heroCount >= game_data::MinHeroes && summary.heroCount <= game_data::MaxHeroes)
            ? palette::Good
            : palette::Warn);

    y += 34.0f;
    drawMeter(
        ui,
        {meterX, y},
        {rect.size.x - 44.0f, 20.0f},
        "HERO COST",
        std::to_string(summary.heroCost) + "/" + std::to_string(game_data::HeroCostLimit),
        static_cast<float>(summary.heroCost) / static_cast<float>(game_data::HeroCostLimit),
        summary.heroCost <= game_data::HeroCostLimit ? palette::BrassPale : palette::Bad);

    y += 36.0f;
    drawInnerRule(ui, {rect.position.x + 22.0f, y}, rect.size.x - 44.0f);

    // Trait row sits a fixed distance off the panel's bottom edge.
    const float traitsLabelY = rect.position.y + rect.size.y - 52.0f;

    y += 12.0f;
    drawText(ui.window, ui.font, "RESOURCE CURVE", 10, {meterX, y}, palette::MutedDim);
    const float curveHeight = std::clamp(traitsLabelY - (y + 14.0f) - 12.0f, 30.0f, 54.0f);
    drawManaCurve(
        ui,
        {{meterX, y + 14.0f}, {rect.size.x - 44.0f, curveHeight}},
        summary.curve,
        style.accent,
        true);

    drawText(ui.window, ui.font, "TRAITS", 10, {meterX, traitsLabelY}, palette::MutedDim);
    if (summary.traits.empty())
    {
        drawText(ui.window, ui.font, "None yet", 12, {meterX, traitsLabelY + 14.0f}, palette::MutedDim);
        return;
    }

    // Only what fits on one row; the full spread is not worth overflowing for.
    drawTraitChips(ui, {meterX, traitsLabelY + 14.0f}, rect.size.x - 44.0f, summary.traits, 1);
}

float drawTraitChips(
    const UiContext& ui,
    sf::Vector2f origin,
    float maxWidth,
    const std::vector<std::pair<std::string, int>>& traits,
    int maxRows)
{
    float x = origin.x;
    float y = origin.y;
    int row = 1;
    for (const auto& [trait, count] : traits)
    {
        if (trait.empty())
        {
            continue;
        }
        const std::string label = count > 0 ? trait + " " + std::to_string(count) : trait;
        sf::Text text(ui.font, label, 11);
        const float width = text.getLocalBounds().size.x + 16.0f;
        if (x > origin.x && x + width > origin.x + maxWidth)
        {
            if (row >= maxRows)
            {
                // Anything past the allowed rows is dropped rather than pushed out
                // of the panel that owns this row.
                break;
            }
            ++row;
            x = origin.x;
            y += 23.0f;
        }
        const sf::Color accent = traitAccent(trait);
        rowPlate(
            ui.window,
            {{x, y}, {width, 19.0f}},
            withAlpha(mix(accent, sf::Color(10, 14, 15), 0.78f), 240),
            withAlpha(accent, 190),
            false,
            4.0f);
        drawText(ui.window, ui.font, label, 11, {x + 8.0f, y + 3.0f}, mix(accent, palette::Ink, 0.55f));
        x += width + 5.0f;
    }
    return y + 23.0f;
}

// --- filter chips ----------------------------------------------------------

std::vector<FilterChip> layoutFilterChips(
    sf::Font& font,
    const std::vector<std::string>& labels,
    sf::Vector2f origin,
    float maxWidth,
    unsigned int textSize,
    float height,
    float gap)
{
    std::vector<FilterChip> chips;
    chips.reserve(labels.size());

    float x = origin.x;
    float y = origin.y;
    for (const std::string& label : labels)
    {
        sf::Text text(font, label, textSize);
        // Room for the tick well plus symmetric padding.
        const float width = text.getLocalBounds().size.x + height + 12.0f;
        if (x > origin.x && x + width > origin.x + maxWidth)
        {
            x = origin.x;
            y += height + gap;
        }
        chips.push_back({{{x, y}, {width, height}}, label});
        x += width + gap;
    }
    return chips;
}

void drawFilterChip(
    const UiContext& ui,
    const FilterChip& chip,
    bool checked,
    bool hovered,
    sf::Color accent,
    unsigned int textSize)
{
    const sf::FloatRect rect = chip.rect;
    // Every chip frames in brass whatever it filters. Tinting the whole chip by
    // its trait turned nine enabled filters into a paint chart; the trait colour
    // survives as the gem in the well, which is where the eye needs it.
    rowPlate(
        ui.window,
        rect,
        checked ? sf::Color(52, 36, 19, 244)
                : (hovered ? sf::Color(24, 31, 32, 236) : sf::Color(11, 16, 17, 228)),
        checked ? palette::BrassBright
                : (hovered ? withAlpha(palette::Brass, 220) : withAlpha(palette::BrassDim, 220)),
        checked,
        4.0f);

    // The well doubles as the state: a lit gem when on, an empty socket when off.
    const sf::Vector2f well{rect.position.x + 11.0f, rect.position.y + rect.size.y * 0.5f};
    if (checked)
    {
        drawRarityGem(ui, well, 4.4f, accent);
    }
    else
    {
        sf::ConvexShape socket = makeGem(well, 4.0f, 4, 0.7854f);
        socket.setFillColor(sf::Color(0, 0, 0, 130));
        socket.setOutlineThickness(1.0f);
        socket.setOutlineColor(withAlpha(palette::MutedDim, 170));
        ui.window.draw(socket);
    }

    sf::Text text(ui.font, chip.label, textSize);
    centerText(
        text,
        {rect.position.x + rect.size.y * 0.5f + 10.0f + (rect.size.x - rect.size.y * 0.5f - 16.0f) * 0.5f,
         rect.position.y + rect.size.y * 0.5f});
    text.setFillColor(checked ? palette::Ink : (hovered ? palette::Muted : palette::MutedDim));
    drawCrispText(ui.window, text);
}

// --- shop ------------------------------------------------------------------

void drawPackObject(const UiContext& ui, sf::FloatRect rect, float time, bool hovered)
{
    const sf::Vector2f center{rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f};
    const float breathe = 0.5f + 0.5f * std::sin(time * 1.35f);

    // Arcane bloom behind the pack. One soft falloff, not stacked rings.
    radialGlow(ui.window, center, rect.size.x * (1.02f + breathe * 0.06f), withAlpha(palette::Arcane, hovered ? 84 : 62), 1.06f);
    radialGlow(ui.window, center, rect.size.x * 0.62f, withAlpha(sf::Color(214, 168, 92), 44), 1.06f);

    // Two backs peeking out behind the front card, so it reads as a sealed pack.
    for (int i = 2; i >= 1; --i)
    {
        const float offset = static_cast<float>(i) * 7.0f;
        drawBeveledPlate(
            ui.window,
            {rect.position.x + offset, rect.position.y - offset * 0.55f},
            rect.size,
            sf::Color(11, 14, 18, 244),
            withAlpha(palette::Brass, 150),
            false,
            11.0f);
    }

    drawBeveledPlate(
        ui.window,
        rect.position,
        rect.size,
        sf::Color(14, 13, 22, 250),
        hovered ? palette::BrassBright : sf::Color(206, 152, 74),
        true,
        12.0f);

    // The back's field: a violet well that deepens towards the edges.
    const sf::FloatRect field{
        {rect.position.x + 13.0f, rect.position.y + 13.0f},
        {rect.size.x - 26.0f, rect.size.y - 26.0f}};
    gradientRect(ui.window, field, sf::Color(34, 22, 54, 240), sf::Color(10, 8, 16, 245));
    radialGlow(ui.window, center, field.size.x * 0.72f, withAlpha(palette::Arcane, 96));
    strokeRect(ui.window, field, withAlpha(sf::Color(150, 110, 60), 200), 1.0f);

    // A lattice of fine diagonals so the field has woven texture rather than being
    // a flat violet plane.
    for (float offset = -field.size.y; offset < field.size.x; offset += 13.0f)
    {
        for (int direction = 0; direction < 2; ++direction)
        {
            const float slope = direction == 0 ? 1.0f : -1.0f;
            const float startX = direction == 0 ? offset : field.size.x - offset;
            sf::VertexArray line(sf::PrimitiveType::Lines, 2);
            const sf::Color tint(150, 112, 178, 40);
            // Clamped to the field so the lattice cannot bleed past the frame.
            const float x0 = std::clamp(field.position.x + startX, field.position.x, field.position.x + field.size.x);
            const float x1 = std::clamp(
                field.position.x + startX + slope * field.size.y,
                field.position.x,
                field.position.x + field.size.x);
            line[0] = {{x0, field.position.y}, tint};
            line[1] = {{x1, field.position.y + std::abs(x1 - x0)}, tint};
            ui.window.draw(line);
        }
    }

    // Filigree: eight thorned vines curling out from the centre gem in mirrored
    // pairs, so the back reads as an authored engraving rather than as scratches.
    for (int arm = 0; arm < 8; ++arm)
    {
        const float base = static_cast<float>(arm) * 0.7854f + 0.3927f;
        // Mirror the curl direction across the vertical axis for symmetry.
        const float curl = std::cos(base) >= 0.0f ? 0.26f : -0.26f;
        sf::Vector2f cursor{
            center.x + std::cos(base) * 26.0f,
            center.y + std::sin(base) * 26.0f};
        float angle = base;
        for (int segment = 0; segment < 8; ++segment)
        {
            const float length = 12.0f - static_cast<float>(segment) * 0.9f;
            const sf::Vector2f next{
                cursor.x + std::cos(angle) * length,
                cursor.y + std::sin(angle) * length * 1.12f};
            const float thickness = std::max(1.0f, 3.4f - static_cast<float>(segment) * 0.34f);
            sf::RectangleShape stroke({length + 1.2f, thickness});
            stroke.setOrigin({0.0f, thickness * 0.5f});
            stroke.setPosition(cursor);
            stroke.setRotation(sf::radians(std::atan2(next.y - cursor.y, next.x - cursor.x)));
            stroke.setFillColor(withAlpha(sf::Color(214, 166, 92), static_cast<int>(238 - segment * 16)));
            ui.window.draw(stroke);

            // A barb every other segment, on the outside of the curl.
            if (segment % 2 == 1)
            {
                const float barbAngle = angle - (curl > 0.0f ? 1.85f : -1.85f);
                sf::ConvexShape barb(3);
                barb.setPoint(0, next);
                barb.setPoint(1, {next.x + std::cos(barbAngle) * 6.4f, next.y + std::sin(barbAngle) * 6.4f});
                barb.setPoint(2, {next.x + std::cos(angle) * 3.4f, next.y + std::sin(angle) * 3.4f});
                barb.setFillColor(withAlpha(sf::Color(228, 180, 102), 215));
                ui.window.draw(barb);
            }
            cursor = next;
            angle += curl;
        }
    }

    // An inner keyline, the double-rule the menu frames carry.
    strokeRect(
        ui.window,
        {{field.position.x + 5.0f, field.position.y + 5.0f}, {field.size.x - 10.0f, field.size.y - 10.0f}},
        withAlpha(sf::Color(206, 158, 86), 120),
        1.0f);

    // Centre gem: faceted, breathing, the thing you want to click.
    const float gemRadius = 21.0f + breathe * 1.6f;
    radialGlow(ui.window, center, gemRadius * 2.5f, withAlpha(palette::ArcaneBright, 78));

    sf::ConvexShape setting = makeGem(center, gemRadius + 5.0f, 8, 0.3927f);
    setting.setFillColor(sf::Color(46, 30, 16, 240));
    setting.setOutlineThickness(1.4f);
    setting.setOutlineColor(sf::Color(214, 164, 84));
    ui.window.draw(setting);

    sf::ConvexShape gem = makeGem(center, gemRadius, 6, 1.5708f);
    gem.setFillColor(sf::Color(96, 58, 156, 250));
    gem.setOutlineThickness(1.2f);
    gem.setOutlineColor(sf::Color(46, 26, 76));
    ui.window.draw(gem);

    // Facets: alternating light and dark wedges from the gem's centre.
    for (int i = 0; i < 6; ++i)
    {
        const float a0 = 1.5708f + static_cast<float>(i) / 6.0f * 6.2831853f;
        const float a1 = 1.5708f + static_cast<float>(i + 1) / 6.0f * 6.2831853f;
        sf::ConvexShape facet(3);
        facet.setPoint(0, center);
        facet.setPoint(1, {center.x + std::cos(a0) * gemRadius, center.y + std::sin(a0) * gemRadius});
        facet.setPoint(2, {center.x + std::cos(a1) * gemRadius, center.y + std::sin(a1) * gemRadius});
        facet.setFillColor(i % 2 == 0
            ? sf::Color(150, 106, 216, 150)
            : sf::Color(58, 32, 100, 130));
        ui.window.draw(facet);
    }

    sf::ConvexShape highlight = makeGem(center - sf::Vector2f(0.0f, gemRadius * 0.34f), gemRadius * 0.40f, 6, 1.5708f);
    highlight.setFillColor(sf::Color(232, 214, 255, 170));
    ui.window.draw(highlight);

    // Corner flourishes on the back's frame.
    for (int corner = 0; corner < 4; ++corner)
    {
        const float cx = corner % 2 == 0 ? field.position.x + 7.0f : field.position.x + field.size.x - 7.0f;
        const float cy = corner / 2 == 0 ? field.position.y + 7.0f : field.position.y + field.size.y - 7.0f;
        const float sx = corner % 2 == 0 ? 1.0f : -1.0f;
        const float sy = corner / 2 == 0 ? 1.0f : -1.0f;
        fillRect(ui.window, {{cx, cy}, {13.0f * sx, 1.2f * sy}}, withAlpha(sf::Color(216, 168, 90), 210));
        fillRect(ui.window, {{cx, cy}, {1.2f * sx, 13.0f * sy}}, withAlpha(sf::Color(216, 168, 90), 210));
        sf::ConvexShape pip = makeGem({cx + 4.0f * sx, cy + 4.0f * sy}, 2.0f, 4, 0.7854f);
        pip.setFillColor(withAlpha(palette::BrassPale, 190));
        ui.window.draw(pip);
    }
}

void drawOddsTable(const UiContext& ui, sf::FloatRect rect)
{
    struct Entry
    {
        const char* label;
        int percent;
        sf::Color color;
    };
    // Matches the server's pack weighting; the colours are the rarity gems used
    // on every card row, so the table reads as a key as well as odds.
    static const Entry Entries[] = {
        {"Common", 70, sf::Color(190, 198, 214)},
        {"Rare", 25, sf::Color(151, 192, 255)},
        {"Legendary", 5, sf::Color(248, 214, 112)}};

    float y = rect.position.y;
    for (const Entry& entry : Entries)
    {
        drawRarityGem(ui, {rect.position.x + 6.0f, y + 7.0f}, 5.0f, entry.color);
        drawText(ui.window, ui.font, entry.label, 12, {rect.position.x + 18.0f, y}, entry.color, 76.0f);

        const std::string percent = std::to_string(entry.percent) + "%";
        sf::Text percentText(ui.font, percent, 12);
        drawText(
            ui.window,
            ui.font,
            percent,
            12,
            {rect.position.x + rect.size.x - percentText.getLocalBounds().size.x, y},
            palette::Ink);

        const sf::FloatRect track{{rect.position.x + 96.0f, y + 5.0f}, {rect.size.x - 96.0f - 38.0f, 5.0f}};
        fillRect(ui.window, track, sf::Color(0, 0, 0, 170));
        strokeRect(ui.window, track, withAlpha(palette::BrassDim, 210), 1.0f);
        gradientRect(
            ui.window,
            {{track.position.x + 1.0f, track.position.y + 1.0f},
             {(track.size.x - 2.0f) * static_cast<float>(entry.percent) / 100.0f, track.size.y - 2.0f}},
            mix(entry.color, sf::Color::White, 0.3f),
            entry.color);
        y += 22.0f;
    }
}

void drawRevealBurst(const UiContext& ui, sf::Vector2f center, float time, sf::Color color)
{
    const float settle = std::clamp(time / 0.55f, 0.0f, 1.0f);
    // Ease out, so the burst lands rather than creeping.
    const float eased = 1.0f - (1.0f - settle) * (1.0f - settle);

    radialGlow(ui.window, center, 232.0f + eased * 96.0f, withAlpha(color, 58), 0.94f);
    radialGlow(ui.window, center, 132.0f * eased, withAlpha(mix(color, sf::Color::White, 0.4f), 46));

    // Rays: many thin ones that fade to nothing along their length. A dozen wide
    // flat wedges at a flat alpha read as cardboard blades rather than as light.
    constexpr int RayCount = 30;
    for (int i = 0; i < RayCount; ++i)
    {
        const float angle = static_cast<float>(i) / static_cast<float>(RayCount) * 6.2831853f + time * 0.16f;
        // Vary the length per ray so the fan does not look mechanical.
        const float wobble = std::sin(static_cast<float>(i) * 2.399f) * 0.5f + 0.5f;
        const float length = (128.0f + wobble * 128.0f + std::sin(time * 1.4f + static_cast<float>(i)) * 14.0f) * eased;
        const float spread = 0.013f + wobble * 0.012f;
        const float inner = 40.0f;

        // Alpha ramps to zero at the tip, which is what makes it read as a beam.
        const sf::Color hot = withAlpha(mix(color, sf::Color::White, 0.35f), 62);
        const sf::Color cold = withAlpha(color, 0);
        sf::VertexArray ray(sf::PrimitiveType::TriangleStrip, 4);
        ray[0] = {{center.x + std::cos(angle - spread) * inner, center.y + std::sin(angle - spread) * inner}, hot};
        ray[1] = {{center.x + std::cos(angle + spread) * inner, center.y + std::sin(angle + spread) * inner}, hot};
        ray[2] = {{center.x + std::cos(angle - spread * 0.4f) * length, center.y + std::sin(angle - spread * 0.4f) * length}, cold};
        ray[3] = {{center.x + std::cos(angle + spread * 0.4f) * length, center.y + std::sin(angle + spread * 0.4f) * length}, cold};
        ui.window.draw(ray);
    }

    // Motes drifting outward, bright enough to register.
    for (int i = 0; i < 26; ++i)
    {
        const float seed = static_cast<float>(i) * 0.9163f;
        const float angle = seed + time * 0.5f;
        const float drift = std::fmod(time * 30.0f + static_cast<float>(i) * 17.0f, 190.0f);
        const float radius = (52.0f + drift) * eased;
        const float fade = 1.0f - drift / 190.0f;
        sf::ConvexShape mote = makeGem(
            {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius * 0.88f},
            1.5f + static_cast<float>(i % 3) * 0.8f,
            4,
            0.7854f);
        mote.setFillColor(withAlpha(mix(color, sf::Color::White, 0.6f), static_cast<int>(235.0f * fade)));
        ui.window.draw(mote);
    }
}

void drawRarityRibbon(const UiContext& ui, sf::FloatRect rect, const std::string& label, sf::Color color)
{
    // A banner with notched ends, so the rarity announcement is an object.
    sf::ConvexShape banner(6);
    banner.setPoint(0, {rect.position.x, rect.position.y});
    banner.setPoint(1, {rect.position.x + rect.size.x, rect.position.y});
    banner.setPoint(2, {rect.position.x + rect.size.x - 9.0f, rect.position.y + rect.size.y * 0.5f});
    banner.setPoint(3, {rect.position.x + rect.size.x, rect.position.y + rect.size.y});
    banner.setPoint(4, {rect.position.x, rect.position.y + rect.size.y});
    banner.setPoint(5, {rect.position.x + 9.0f, rect.position.y + rect.size.y * 0.5f});

    sf::ConvexShape shadow(banner);
    shadow.move({0.0f, 2.0f});
    shadow.setFillColor(sf::Color(0, 0, 0, 160));
    ui.window.draw(shadow);

    banner.setFillColor(withAlpha(mix(color, sf::Color(12, 16, 17), 0.66f), 246));
    banner.setOutlineThickness(1.2f);
    banner.setOutlineColor(withAlpha(color, 230));
    ui.window.draw(banner);

    sf::Text text(ui.font, label, 13);
    text.setLetterSpacing(1.7f);
    centerText(text, {rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f});
    text.setFillColor(mix(color, sf::Color::White, 0.45f));
    drawCrispText(ui.window, text);
}

void drawCardFace(
    const UiContext& ui,
    sf::FloatRect rect,
    const card_data::Card& card,
    const std::string& rarityLabel,
    sf::Color rarityColor,
    const std::string& footer)
{
    const bool hero = game_data::isHeroCard(card);
    const sf::Color accent = cardAccent(card);

    // Card stock: rarity decides the frame's warmth, as it does in any CCG.
    drawBeveledPlate(
        ui.window,
        rect.position,
        rect.size,
        sf::Color(16, 20, 21, 250),
        mix(rarityColor, palette::Brass, 0.45f),
        hero,
        11.0f);
    gradientRect(
        ui.window,
        {{rect.position.x + 5.0f, rect.position.y + 5.0f}, {rect.size.x - 10.0f, rect.size.y - 10.0f}},
        withAlpha(accent, 30),
        sf::Color(0, 0, 0, 0));

    // Art window across the top two fifths.
    const sf::FloatRect art{
        {rect.position.x + 14.0f, rect.position.y + 14.0f},
        {rect.size.x - 28.0f, rect.size.y * 0.47f}};
    drawArtWindow(ui, art, card.imagePath, mix(rarityColor, palette::Brass, 0.4f), 4.0f, true);
    gradientRect(
        ui.window,
        {{art.position.x, art.position.y + art.size.y * 0.55f}, {art.size.x, art.size.y * 0.45f}},
        sf::Color(6, 9, 10, 0),
        sf::Color(6, 9, 10, 220));

    // Cost gem overlaps the art's top-left corner, the way a mana gem does.
    drawCostGem(
        ui,
        {art.position.x + 2.0f, art.position.y + 2.0f},
        14.0f,
        hero ? game_data::cardInt(card, "heroCost", 0) : game_data::cardInt(card, "cost", 0),
        hero);
    // Inside the art's corner rather than straddling it, so the gem does not read
    // as a clipped artifact.
    drawRarityGem(ui, {art.position.x + art.size.x - 11.0f, art.position.y + 11.0f}, 6.5f, rarityColor);

    // Name plate straddling the art's lower edge.
    const sf::FloatRect plate{
        {rect.position.x + 10.0f, art.position.y + art.size.y - 15.0f},
        {rect.size.x - 20.0f, 30.0f}};
    drawBeveledPlate(
        ui.window,
        plate.position,
        plate.size,
        sf::Color(22, 18, 14, 246),
        withAlpha(mix(rarityColor, palette::Brass, 0.4f), 220),
        false,
        6.0f);
    // Shrink the name to fit rather than eliding it: "Crystal Unico..." on a card
    // face is worse than the same name a point or two smaller.
    unsigned int nameSize = 18;
    sf::Text name(ui.displayFont, card.title, nameSize);
    while (nameSize > 12 && name.getLocalBounds().size.x > plate.size.x - 24.0f)
    {
        name.setCharacterSize(--nameSize);
    }
    if (name.getLocalBounds().size.x > plate.size.x - 24.0f)
    {
        name.setString(elideToWidth(ui.displayFont, card.title, nameSize, plate.size.x - 24.0f));
    }
    centerText(name, {plate.position.x + plate.size.x * 0.5f, plate.position.y + plate.size.y * 0.5f});
    name.setFillColor(palette::Ink);
    drawCrispText(ui.window, name);

    float y = plate.position.y + plate.size.y + 10.0f;
    const float textX = rect.position.x + 18.0f;
    const float textWidth = rect.size.x - 36.0f;

    // Type and trait only. Prefixing the rarity as well overran a narrow face and
    // elided; the frame colour and the rarity gem already carry it.
    std::string typeLine = hero ? std::string("Hero") : card.type;
    if (!card.traits.empty() && !card.traits.front().empty())
    {
        typeLine += Middot + card.traits.front();
    }
    else
    {
        typeLine = rarityLabel + Middot + typeLine;
    }
    drawText(ui.window, ui.font, typeLine, 12, {textX, y}, rarityColor, textWidth);
    y += 20.0f;

    // Stat block: health as a struck badge for anything that fights.
    if (hero || card.type == "Unit")
    {
        const int health = game_data::cardInt(card, "health", 0);
        const auto badge = [&](sf::Vector2f position, const std::string& label, int value, sf::Color color) {
            drawBeveledPlate(
                ui.window,
                position,
                {56.0f, 26.0f},
                withAlpha(mix(color, sf::Color(12, 16, 17), 0.76f), 244),
                withAlpha(color, 200),
                false,
                5.0f);
            drawText(ui.window, ui.font, label, 9, {position.x + 8.0f, position.y + 3.0f}, palette::MutedDim);
            drawText(ui.window, ui.font, std::to_string(value), 14, {position.x + 8.0f, position.y + 11.0f}, color);
        };
        badge({textX + (textWidth - 56.0f) * 0.5f, y}, "HEALTH", health, sf::Color(150, 206, 156));
        y += 34.0f;
    }

    // Rules text on a recessed panel, the way a card's text box is inset.
    const std::string body = game_data::cardStr(card, "description", "");
    const float bodyBottom = rect.position.y + rect.size.y - (footer.empty() ? 16.0f : 32.0f);
    // Below about three lines the box only shows a clipped fragment, which reads
    // worse than leaving the rules to the panel that has room for them.
    if (!body.empty() && bodyBottom - y > 46.0f)
    {
        const sf::FloatRect box{{textX - 5.0f, y - 3.0f}, {textWidth + 10.0f, bodyBottom - y + 4.0f}};
        fillRect(ui.window, box, sf::Color(6, 10, 11, 190));
        strokeRect(ui.window, box, withAlpha(palette::BrassDim, 200), 1.0f);
        drawWrappedText(ui.window, ui.font, body, 12, {textX, y + 2.0f}, palette::Muted, textWidth, 3.0f);
    }

    if (!footer.empty())
    {
        drawText(
            ui.window,
            ui.font,
            footer,
            11,
            {textX, rect.position.y + rect.size.y - 24.0f},
            palette::BrassPale,
            textWidth);
    }
}

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
    bool affordable)
{
    // Unselected tiles recede so the choice between four factions reads as a
    // choice rather than as four identical boxes.
    const float presence = selected ? 1.0f : 0.62f;
    const sf::Color accent = selected ? style.accent : mix(style.accent, sf::Color(74, 70, 64), 0.42f);

    if (selected)
    {
        radialGlow(
            ui.window,
            {rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.42f},
            rect.size.x * 0.95f,
            withAlpha(style.accent, 58));
    }

    drawBeveledPlate(
        ui.window,
        rect.position,
        rect.size,
        selected ? mix(style.deep, sf::Color(30, 24, 16), 0.35f) : sf::Color(11, 15, 16, 244),
        selected ? palette::BrassBright : withAlpha(palette::Brass, 165),
        selected,
        12.0f);

    // Faction art fills the top of the tile and dissolves into the plate.
    const sf::FloatRect art{
        {rect.position.x + 11.0f, rect.position.y + 11.0f},
        {rect.size.x - 22.0f, rect.size.y * 0.54f}};
    fillRect(ui.window, art, sf::Color(4, 7, 8));
    if (sf::Texture* texture = style.art.empty() ? nullptr : ui.textures.load(style.art))
    {
        drawCroppedCover(
            ui.window,
            *texture,
            art,
            selected ? sf::Color::White : sf::Color(178, 176, 172),
            0.10f);
    }
    else
    {
        gradientRect(ui.window, art, withAlpha(accent, 70), sf::Color(4, 7, 8));
    }
    gradientRect(ui.window, art, withAlpha(style.deep, selected ? 70 : 130), withAlpha(style.deep, 40));
    // Dissolve the art's bottom edge into the tile instead of ending on a line.
    gradientRect(
        ui.window,
        {{art.position.x, art.position.y + art.size.y * 0.44f}, {art.size.x, art.size.y * 0.56f}},
        sf::Color(8, 12, 13, 0),
        sf::Color(8, 12, 13, 252));
    // Only the art's top and sides are framed; its foot dissolves into the tile,
    // so a stroke across the middle of the illustration is wrong.
    fillRect(ui.window, {{art.position.x, art.position.y}, {art.size.x, 1.0f}}, withAlpha(accent, 150));
    fillRect(ui.window, {{art.position.x, art.position.y}, {1.0f, art.size.y * 0.62f}}, withAlpha(accent, 110));
    fillRect(ui.window, {{art.position.x + art.size.x - 1.0f, art.position.y}, {1.0f, art.size.y * 0.62f}}, withAlpha(accent, 110));

    // Crest straddles the art/text boundary.
    const sf::Vector2f crest{rect.position.x + rect.size.x * 0.5f, art.position.y + art.size.y - 12.0f};
    drawFactionCrest(ui, crest, 19.0f, style, selected);

    float y = crest.y + 26.0f;
    const float textWidth = rect.size.x - 28.0f;
    const float textX = rect.position.x + 14.0f;

    // Faction name in the display face, centred, wrapped to the tile.
    for (const std::string& line : wrapText(ui.displayFont, name, 17, textWidth))
    {
        sf::Text text(ui.displayFont, line, 17);
        centerText(text, {rect.position.x + rect.size.x * 0.5f, y + 8.0f});
        sf::Text shadow(text);
        shadow.move({1.0f, 2.0f});
        shadow.setFillColor(sf::Color(0, 0, 0, 200));
        drawCrispText(ui.window, shadow);
        text.setFillColor(selected ? sf::Color(255, 246, 226) : palette::Muted);
        drawCrispText(ui.window, text);
        y += 20.0f;
    }

    y += 4.0f;
    sf::Text count(ui.font, std::to_string(cardCount) + " CARDS", 10);
    count.setLetterSpacing(1.5f);
    centerText(count, {rect.position.x + rect.size.x * 0.5f, y + 5.0f});
    count.setFillColor(withAlpha(accent, 240));
    drawCrispText(ui.window, count);

    y += 16.0f;
    drawInnerRule(ui, {textX, y}, textWidth);

    // Strategy blurb, so a tile is not two thirds dead space.
    y += 10.0f;
    for (const std::string& line : wrapText(ui.font, style.blurb, 11, textWidth))
    {
        sf::Text text(ui.font, line, 11);
        centerText(text, {rect.position.x + rect.size.x * 0.5f, y + 6.0f});
        text.setFillColor(selected ? palette::Muted : palette::MutedDim);
        drawCrispText(ui.window, text);
        y += 15.0f;
    }

    // Status foot: an owned ribbon, or a coin-struck price.
    const sf::FloatRect foot{
        {rect.position.x + 12.0f, rect.position.y + rect.size.y - 42.0f},
        {rect.size.x - 24.0f, 30.0f}};
    if (owned)
    {
        drawRarityRibbon(ui, foot, "OWNED", palette::Good);
    }
    else
    {
        drawBeveledPlate(
            ui.window,
            foot.position,
            foot.size,
            affordable ? sf::Color(30, 24, 14, 246) : sf::Color(28, 16, 14, 240),
            withAlpha(statusColor, affordable ? 220 : 170),
            selected && affordable,
            6.0f);
        // Centre the coin and amount as a pair.
        sf::Text amount(ui.font, status, 14);
        const float amountWidth = amount.getLocalBounds().size.x;
        const float groupLeft = foot.position.x + (foot.size.x - (amountWidth + 22.0f)) * 0.5f;
        drawCoin(ui, {groupLeft + 8.0f, foot.position.y + foot.size.y * 0.5f}, 8.0f);
        drawText(
            ui.window,
            ui.font,
            status,
            14,
            {groupLeft + 22.0f, foot.position.y + foot.size.y * 0.5f - 9.0f},
            statusColor,
            foot.size.x - 30.0f);
    }

    if (!selected)
    {
        // A faint veil rather than a colour change, so unselected tiles keep
        // their art readable while clearly sitting behind the choice.
        fillRect(ui.window, rect, sf::Color(6, 9, 10, static_cast<std::uint8_t>((1.0f - presence) * 62.0f)));
    }
}
}
