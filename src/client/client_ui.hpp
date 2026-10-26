#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bayou::client
{

// ---- logical canvas ------------------------------------------------------
// Existing screen compositions were authored around x = 0..800 and remain
// centred inside the fixed 16:9 canvas. New edge chrome and full-screen
// overlays should use Left/Right/Width so they include the widescreen gutters.
namespace ui_canvas
{
inline constexpr float LegacyWidth = 800.0f;
inline constexpr float Height = 600.0f;
inline constexpr float Aspect = 16.0f / 9.0f;
inline constexpr float Width = Height * Aspect;
inline constexpr float Left = (LegacyWidth - Width) * 0.5f;
inline constexpr float Right = Left + Width;
} // namespace ui_canvas

// ---- shared palette -------------------------------------------------------
// Draw from these rather than raw sf::Color literals so a retune of the game's
// look happens in one place. The Brass names are retained as source-compatible
// material aliases, but the shipped Gloomthorn chrome is cold forged steel.
namespace palette
{
inline const sf::Color Brass{112, 132, 141};        // primary steel (legacy name)
inline const sf::Color BrassBright{202, 220, 226};  // lit steel, focus, selection
inline const sf::Color BrassPale{231, 241, 244};    // steel specular highlight
inline const sf::Color BrassGlow{170, 200, 215};    // controlled cool bloom
inline const sf::Color BrassEdge{67, 84, 92};       // recessed edge / socket metal
inline const sf::Color BrassDim{49, 65, 72};        // steel in shadow
inline const sf::Color BrassDeep{27, 38, 43};       // metal core shadow
inline const sf::Color BrassShadow{15, 23, 27};     // blackened edge under steel

// Gloomthorn's cooler perimeter material: steel rails, living thornwork, and
// the violet stones that echo the supplied logo banner.
inline const sf::Color Steel{104, 124, 132};
inline const sf::Color SteelBright{198, 215, 220};
inline const sf::Color SteelDim{49, 65, 72};
inline const sf::Color SteelEdge{61, 78, 85};
inline const sf::Color SteelDeep{28, 39, 45};
inline const sf::Color Vine{31, 53, 42};
inline const sf::Color VineBright{78, 121, 84};
inline const sf::Color VineShadow{17, 31, 25};
inline const sf::Color Jewel{114, 62, 171};
inline const sf::Color JewelBright{212, 157, 255};
inline const sf::Color JewelDeep{48, 24, 74};

inline const sf::Color FocusKeyline{4, 8, 9};      // contrast under keyboard focus
inline const sf::Color FocusBright{238, 248, 252}; // accessible focus highlight
inline const sf::Color Ink{246, 232, 200};      // primary text
inline const sf::Color InkBright{255, 246, 222}; // text on hover / active
inline const sf::Color InkMuted{181, 166, 137};  // secondary text
inline const sf::Color InkFaint{124, 114, 96};   // disabled text

inline const sf::Color Plate{12, 17, 18};      // panel body
inline const sf::Color PlateDeep{6, 10, 11};   // recessed slots
inline const sf::Color PlateWarm{24, 30, 34};  // interactive plate body
inline const sf::Color SurfaceTop{23, 31, 31}; // cool top plane on raised surfaces
inline const sf::Color SurfaceBottom{7, 13, 15}; // deep lower plane on raised surfaces
inline const sf::Color SurfaceWarm{38, 31, 49}; // selected plane with restrained violet

inline const sf::Color ArcaneDeep{28, 22, 39};      // violet-black in recesses
inline const sf::Color Arcane{123, 79, 168};        // violet counterpoint
inline const sf::Color ArcaneBright{176, 132, 224}; // lit violet
inline const sf::Color Patina{77, 126, 116};        // rare cool signal for state accents
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

// Generated Gloomthorn hardware shared by every menu surface. Register once
// after TextureStore loads the assets; all plate, title, and perimeter helpers
// then use the same authored thorn-and-steel language automatically.
void setUiOrnamentTextures(
    sf::Texture* cornerThorn,
    sf::Texture* titleFinial,
    sf::Texture* rivet);

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

// Binary form control: a steel slider with a violet on state and a readable
// state label. The entire bounds are the click target, including the label.
void drawToggleSwitch(
    sf::RenderWindow& window, const sf::Font& font, sf::FloatRect bounds,
    bool on, bool hovered, bool focused);

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
// A compact recessed command shelf for keyboard guidance. Keeping these hints
// on a shared material strip prevents them from drifting into the outer frame.
void drawKeyboardHintBar(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    float width);
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
    sf::Color fill{17, 24, 27, 244};
    sf::Color frame = palette::Brass;
    float cut = 8.0f;
    PlateState state = PlateState::Normal;
    float sheen = 1.0f;
    // Recessed surfaces (input fields, tracks, wells) take the inner shadow on
    // the top edge; raised surfaces take only a hint of it.
    bool rivets = true;
    bool brackets = true;
    bool recessed = false;
    bool focused = false;
    // Keep a dark keyline under the bright focus ring when the plate sits over
    // artwork or a light panel. Callers may disable it for already-dark wells.
    bool focusKeyline = true;
    // Seconds; drives the focus ring pulse. Pass 0 for a still ring.
    float focusPhase = 0.0f;
    bool castShadow = true;
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
// Shared thin steel perimeter used by every non-title menu surface, with small
// thorn/branch and violet-jewel accents at deliberate anchor points.
void drawMenuFrame(sf::RenderWindow& window);
// Keyboard focus indicator. Draw it around whatever currently owns the caret or
// the arrow keys; nothing else in the palette uses this pale brass ring.
void drawFocusRing(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    float phase = 0.0f,
    sf::Color color = palette::FocusBright);
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
// Alpha-adjusted palette color for module-owned controls. Kept tiny and
// side-effect free so controls do not need to duplicate the palette helper.
sf::Color withAlphaForControls(sf::Color color, std::uint8_t alpha);
void drawDiamond(
    sf::RenderWindow& window,
    sf::Vector2f center,
    float width,
    float height,
    sf::Color fill,
    sf::Color outline = sf::Color::Transparent);
// A single brass fastener, matching the ones drawMaterialPlate places.
void drawStud(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color);
void drawTitlePlaque(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    sf::Vector2f size);
void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width);
// `ornament = false` drops the centre lozenge. Use it for row dividers inside a
// list or a settings panel; keep the ornament for section breaks.
void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width, bool ornament);
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
