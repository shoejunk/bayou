#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bayou::client
{

// ---- shared palette -------------------------------------------------------
// Draw from these rather than raw sf::Color literals so a retune of the game's
// look happens in one place. Anything outside this set reads as off-palette.
namespace palette
{
inline const sf::Color Brass{174, 117, 54};        // primary metal
inline const sf::Color BrassBright{239, 190, 98};  // lit metal, focus, selection
inline const sf::Color BrassPale{255, 226, 152};   // specular highlight on metal
inline const sf::Color BrassDim{83, 54, 29};       // metal in shadow
inline const sf::Color BrassDeep{48, 30, 17};      // metal core shadow

inline const sf::Color Ink{246, 232, 200};      // primary text
inline const sf::Color InkBright{255, 246, 222}; // text on hover / active
inline const sf::Color InkMuted{181, 166, 137};  // secondary text
inline const sf::Color InkFaint{124, 114, 96};   // disabled text

inline const sf::Color Plate{12, 17, 18};      // panel body
inline const sf::Color PlateDeep{6, 10, 11};   // recessed slots
inline const sf::Color PlateWarm{31, 25, 20};  // interactive plate body

inline const sf::Color Arcane{123, 79, 168};        // violet counterpoint
inline const sf::Color ArcaneBright{176, 132, 224}; // lit violet
inline const sf::Color Gold{232, 176, 74};          // currency
inline const sf::Color Danger{196, 72, 58};         // destructive / error
inline const sf::Color DangerBright{240, 146, 124}; // error text on dark
inline const sf::Color Success{124, 170, 106};      // confirmation
} // namespace palette

// ---- type scale -----------------------------------------------------------
// Logical units in the 800x600 space. Steps are deliberately far apart: a
// hierarchy the eye reads instantly needs ratio, not one or two pixels.
// Display/Hero/Heading belong to the Gloomthorn display face, the rest to
// Roboto. Sizes below Body are for tracked small caps, never sentences.
namespace type
{
// Body/Caption deliberately match the sizes already hardcoded across the
// screens, so adopting the scale never reflows an existing layout.
constexpr unsigned int Display = 34;    // screen title on a plaque
constexpr unsigned int Hero = 26;       // hero numerals, popup titles
constexpr unsigned int Heading = 20;    // panel headings
constexpr unsigned int Subheading = 18; // group headings, primary button labels
constexpr unsigned int Body = 16;       // sentences, row primaries
constexpr unsigned int Label = 13;      // tracked caps field labels
constexpr unsigned int Caption = 12;    // row secondaries, hints
constexpr unsigned int Micro = 10;      // version stamps, tick marks
} // namespace type

// The ornate Gloomthorn display face. Register it once at startup and the
// helpers that draw titles pick it up automatically; screens reach it through
// displayFontOr() for their own headings. Roboto remains the body face, and
// everything keeps working when no display face is registered.
void setDisplayFont(sf::Font* font);
sf::Font* displayFont();
sf::Font& displayFontOr(sf::Font& fallback);

void centerText(sf::Text& text, float x);
void centerText(sf::Text& text, sf::Vector2f center);
void centerButtonText(sf::Text& text, sf::Vector2f center);
void setMessage(sf::Text& text, const std::string& message, const sf::Color& color);
void setMessageY(sf::Text& text, float y);

// Device pixels per logical unit for the target's current view.
float logicalRenderScale(const sf::RenderTarget& target);
// Draws text rasterized at device resolution instead of letting the view
// transform magnify (and soften) logical-size glyphs. Prefer this over
// window.draw(text) for every sf::Text. The text object is left unchanged.
void drawCrispText(sf::RenderWindow& window, sf::Text& text);

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth);
void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth = 0.0f);
// Tracked small caps, the standard treatment for field labels and column
// headers. Returns the drawn width so callers can lay out beside it.
float drawLabelText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float tracking = 1.6f);
void drawCenteredText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f center,
    sf::Color color);
// For type that sits directly on artwork rather than on a plate.
void drawTextShadowed(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    sf::Color shadowColor = sf::Color(0, 0, 0, 200),
    sf::Vector2f shadowOffset = {1.0f, 2.0f});

// ---- surfaces -------------------------------------------------------------

enum class PlateState
{
    Normal,
    Hover,
    Pressed,
    Disabled,
    Selected,
};

struct PlateStyle
{
    sf::Color fill{31, 25, 20, 244};
    sf::Color frame = palette::Brass;
    float cut = 8.0f;
    PlateState state = PlateState::Normal;
    // Recessed surfaces (input fields, tracks, wells) take the inner shadow on
    // the top edge; raised surfaces take only a hint of it.
    bool recessed = false;
    bool rivets = true;
    bool brackets = true;
    bool castShadow = true;
    float sheen = 1.0f;
    bool focused = false;
    // Seconds; drives the focus ring pulse. Pass 0 for a still ring.
    float focusPhase = 0.0f;
};

// The material renderer behind every plate in the game: graded body, layered
// brass frame, inner shadow, and hover/press/disabled/selected states.
void drawMaterialPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    const PlateStyle& style);
// Unchanged footprint; now routed through drawMaterialPlate.
void drawBeveledPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    sf::Color fill,
    sf::Color outline,
    bool highlighted = false,
    float cut = 8.0f);
// A recessed well: what a text field or a progress track sits in.
void drawInsetSlot(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    sf::Color fill,
    sf::Color frame,
    bool active,
    bool error = false);
void drawInnerShadow(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    float depth = 1.0f,
    std::uint8_t strength = 120);
void drawBrassFrame(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    sf::Color color,
    float thickness = 1.0f);
// Keyboard focus indicator. Draw it around whatever currently owns the caret or
// the arrow keys; nothing else in the palette uses this pale brass ring.
void drawFocusRing(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    float phase = 0.0f);
void drawVerticalGradient(
    sf::RenderWindow& window,
    sf::FloatRect rect,
    sf::Color top,
    sf::Color bottom);
void drawRadialGlow(
    sf::RenderWindow& window,
    sf::Vector2f center,
    float radius,
    sf::Color color,
    int segments = 28);
// A single brass fastener, matching the ones drawMaterialPlate places.
void drawStud(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color);
void drawTitlePlaque(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    sf::Vector2f size);
void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width);
std::vector<std::string> wrapText(sf::Font& font, const std::string& value, unsigned int size, float maxWidth);
float drawWrappedText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth,
    float lineGap = 4.0f);
void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size);
// drawPanel plus a heading band across the top. Returns the y of the first
// content row, so callers never have to guess the band height.
float drawPanelWithHeader(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& heading);
// A recessed brass pill for a count, a currency total or a rank. `leadingInset`
// reserves room on the left for an icon drawn by the caller.
void drawValuePill(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& value,
    sf::Color accent,
    float leadingInset = 0.0f);
void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected);
void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected,
    bool hovered);

std::optional<std::size_t> rowIndexAt(
    sf::Vector2f mouse,
    float x,
    float y,
    float width,
    float rowHeight,
    std::size_t visibleRows,
    std::size_t offset,
    std::size_t totalRows);
bool isInsideRect(sf::Vector2f mouse, float x, float y, float width, float height);
void scrollList(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows, float delta);
void clampListOffset(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows);
}
