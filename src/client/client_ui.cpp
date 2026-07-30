#include "client_ui.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace bayou::client
{
namespace
{
const sf::Color Brass = palette::Brass;
const sf::Color BrassBright = palette::BrassBright;
const sf::Color BrassDim = palette::BrassDim;
const sf::Color Ink = palette::Ink;

sf::Font* registeredDisplayFont = nullptr;

sf::Color mix(sf::Color from, sf::Color to, float amount)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto lerp = [amount](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * amount));
    };
    return {lerp(from.r, to.r), lerp(from.g, to.g), lerp(from.b, to.b), lerp(from.a, to.a)};
}

sf::Color withAlpha(sf::Color color, std::uint8_t alpha)
{
    color.a = alpha;
    return color;
}

// Scales a colour's channels without touching alpha: the cheap way to get a
// lit or shadowed variant of a metal that stays on-hue.
sf::Color shade(sf::Color color, float factor)
{
    const auto apply = [factor](std::uint8_t channel) {
        return static_cast<std::uint8_t>(
            std::clamp(std::lround(static_cast<float>(channel) * factor), 0L, 255L));
    };
    return {apply(color.r), apply(color.g), apply(color.b), color.a};
}

sf::ConvexShape makeCutRect(sf::Vector2f position, sf::Vector2f size, float cut)
{
    cut = std::max(0.0f, std::min(cut, std::min(size.x, size.y) * 0.45f));
    sf::ConvexShape shape(8);
    shape.setPoint(0, {position.x + cut, position.y});
    shape.setPoint(1, {position.x + size.x - cut, position.y});
    shape.setPoint(2, {position.x + size.x, position.y + cut});
    shape.setPoint(3, {position.x + size.x, position.y + size.y - cut});
    shape.setPoint(4, {position.x + size.x - cut, position.y + size.y});
    shape.setPoint(5, {position.x + cut, position.y + size.y});
    shape.setPoint(6, {position.x, position.y + size.y - cut});
    shape.setPoint(7, {position.x, position.y + cut});
    return shape;
}

void drawLine(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size, sf::Color color)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    sf::RectangleShape line(size);
    line.setPosition(position);
    line.setFillColor(color);
    window.draw(line);
}

// Fills the eight-sided plate outline with a top-to-bottom gradient. A flat
// fill plus a bevel outline always reads as a rectangle with a border drawn on
// it; a graded body is what makes the surface look like a lit material.
void fillCutRectGraded(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    sf::Color top,
    sf::Color bottom)
{
    const sf::ConvexShape outline = makeCutRect(position, size, cut);
    const std::size_t points = outline.getPointCount();
    const float minY = position.y;
    const float span = std::max(1.0f, size.y);

    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, points + 2);
    const sf::Vector2f center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
    fan[0].position = center;
    fan[0].color = mix(top, bottom, 0.5f);
    for (std::size_t i = 0; i <= points; ++i)
    {
        const sf::Vector2f point = outline.getPoint(i % points);
        fan[i + 1].position = point;
        fan[i + 1].color = mix(top, bottom, (point.y - minY) / span);
    }
    window.draw(fan);
}

void drawRivet(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color)
{
    sf::CircleShape shadow(radius);
    shadow.setOrigin({radius, radius});
    shadow.setPosition(center + sf::Vector2f(1.0f, 1.0f));
    shadow.setFillColor(sf::Color(0, 0, 0, 105));
    window.draw(shadow);

    sf::CircleShape rivet(radius);
    rivet.setOrigin({radius, radius});
    rivet.setPosition(center);
    rivet.setFillColor(color);
    rivet.setOutlineThickness(std::max(1.0f, radius * 0.28f));
    rivet.setOutlineColor(sf::Color(64, 38, 20, 210));
    window.draw(rivet);

    sf::CircleShape shine(radius * 0.38f);
    shine.setOrigin({radius * 0.38f, radius * 0.38f});
    shine.setPosition(center + sf::Vector2f(-radius * 0.22f, -radius * 0.22f));
    shine.setFillColor(sf::Color(255, 231, 169, 130));
    window.draw(shine);
}

void drawCornerBrackets(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size, float inset, sf::Color color)
{
    const float bracket = std::min(24.0f, std::min(size.x, size.y) * 0.28f);
    const float thickness = 1.5f;
    const float left = position.x + inset;
    const float top = position.y + inset;
    const float right = position.x + size.x - inset;
    const float bottom = position.y + size.y - inset;

    drawLine(window, {left, top}, {bracket, thickness}, color);
    drawLine(window, {left, top}, {thickness, bracket}, color);
    drawLine(window, {right - bracket, top}, {bracket, thickness}, color);
    drawLine(window, {right - thickness, top}, {thickness, bracket}, color);
    drawLine(window, {left, bottom - thickness}, {bracket, thickness}, color);
    drawLine(window, {left, bottom - bracket}, {thickness, bracket}, color);
    drawLine(window, {right - bracket, bottom - thickness}, {bracket, thickness}, color);
    drawLine(window, {right - thickness, bottom - bracket}, {thickness, bracket}, color);
}
}

void setDisplayFont(sf::Font* font)
{
    registeredDisplayFont = font;
}

sf::Font* displayFont()
{
    return registeredDisplayFont;
}

sf::Font& displayFontOr(sf::Font& fallback)
{
    return registeredDisplayFont ? *registeredDisplayFont : fallback;
}

void centerText(sf::Text& text, float x)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, text.getOrigin().y});
    text.setPosition({x, text.getPosition().y});
}

void centerText(sf::Text& text, sf::Vector2f center)
{
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({
        bounds.position.x + bounds.size.x / 2.0f,
        bounds.position.y + bounds.size.y / 2.0f});
    text.setPosition(center);
}

void centerButtonText(sf::Text& text, sf::Vector2f center)
{
    const float opticalOffset = std::clamp(static_cast<float>(text.getCharacterSize()) * 0.15f, 3.0f, 8.0f);
    centerText(text, {center.x, center.y + opticalOffset});
}

void setMessage(sf::Text& text, const std::string& message, const sf::Color& color)
{
    text.setString(message);
    text.setFillColor(color);
    centerText(text, 400.0f);
}

void setMessageY(sf::Text& text, float y)
{
    text.setPosition({text.getPosition().x, y});
    centerText(text, 400.0f);
}

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    sf::Text text(font, value, size);
    if (text.getLocalBounds().size.x <= maxWidth)
    {
        return value;
    }

    std::string display = value;
    while (!display.empty())
    {
        display.pop_back();
        text.setString(display + "...");
        if (text.getLocalBounds().size.x <= maxWidth)
        {
            return display + "...";
        }
    }

    return "...";
}

float logicalRenderScale(const sf::RenderTarget& target)
{
    const sf::View& view = target.getView();
    const sf::Vector2u targetSize = target.getSize();
    if (view.getSize().y <= 0.0f || targetSize.y == 0)
    {
        return 1.0f;
    }

    // How many device pixels one logical unit covers, taking the letterboxed
    // viewport into account.
    const float viewportPixels =
        static_cast<float>(targetSize.y) * view.getViewport().size.y;
    const float scale = viewportPixels / view.getSize().y;
    return std::clamp(scale, 1.0f, 8.0f);
}

void drawCrispText(sf::RenderWindow& window, sf::Text& text)
{
    const float scale = logicalRenderScale(window);
    if (scale <= 1.01f)
    {
        window.draw(text);
        return;
    }

    // Glyphs are rasterized at the logical character size and then magnified by
    // the view transform, which softens every edge. Rasterizing at the device
    // size and scaling back down instead keeps text sharp at the size the
    // layout already reserved for it.
    const unsigned int logicalSize = text.getCharacterSize();
    const auto rasterSize = static_cast<unsigned int>(
        std::lround(static_cast<float>(logicalSize) * scale));
    if (rasterSize == 0 || rasterSize == logicalSize)
    {
        window.draw(text);
        return;
    }

    const float inverse = static_cast<float>(logicalSize) / static_cast<float>(rasterSize);
    const sf::Vector2f originalScale = text.getScale();
    const sf::Vector2f originalOrigin = text.getOrigin();
    const float originalOutline = text.getOutlineThickness();
    const float originalLetterSpacing = text.getLetterSpacing();

    text.setCharacterSize(rasterSize);
    // Origin and outline were expressed in logical units, so undo the raster
    // magnification on both before the scale-down puts everything back.
    text.setOrigin({originalOrigin.x / inverse, originalOrigin.y / inverse});
    if (originalOutline != 0.0f)
    {
        text.setOutlineThickness(originalOutline / inverse);
    }
    text.setLetterSpacing(originalLetterSpacing);
    text.setScale({originalScale.x * inverse, originalScale.y * inverse});

    window.draw(text);

    text.setScale(originalScale);
    text.setCharacterSize(logicalSize);
    text.setOrigin(originalOrigin);
    text.setOutlineThickness(originalOutline);
}

void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth)
{
    sf::Text text(font, maxWidth > 0.0f ? elideToWidth(font, value, size, maxWidth) : value, size);
    text.setFillColor(color);
    text.setPosition(position);
    drawCrispText(window, text);
}

float drawLabelText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float tracking)
{
    // Field labels and column headers are set as tracked caps. The extra
    // letter-spacing is what separates a label from a sentence at a glance, and
    // it stops small type from looking like cramped body copy.
    std::string upper = value;
    for (char& character : upper)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }

    sf::Text text(font, upper, size);
    text.setLetterSpacing(1.0f + tracking / static_cast<float>(std::max(1u, size)) * 6.0f);
    text.setFillColor(color);
    text.setPosition(position);
    drawCrispText(window, text);
    return text.getLocalBounds().size.x;
}

void drawCenteredText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f center,
    sf::Color color)
{
    sf::Text text(font, value, size);
    text.setFillColor(color);
    centerText(text, center);
    drawCrispText(window, text);
}

void drawTextShadowed(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    sf::Color shadowColor,
    sf::Vector2f shadowOffset)
{
    sf::Text shadowText(font, value, size);
    shadowText.setFillColor(shadowColor);
    shadowText.setPosition(position + shadowOffset);
    drawCrispText(window, shadowText);

    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setPosition(position);
    drawCrispText(window, text);
}

void drawVerticalGradient(sf::RenderWindow& window, sf::FloatRect rect, sf::Color top, sf::Color bottom)
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

void drawStud(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color)
{
    drawRivet(window, center, radius, color);
}

void drawInnerShadow(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    float depth,
    std::uint8_t strength)
{
    const float inset = std::max(1.0f, cut * 0.5f);
    const float bandWidth = std::max(0.0f, size.x - inset * 2.0f);
    if (bandWidth <= 0.0f)
    {
        return;
    }

    // A cast shadow on the top inner edge is what tells the eye a surface is
    // below the frame rather than flush with it.
    const float topBand = std::clamp(size.y * 0.42f, 4.0f, 22.0f) * depth;
    drawVerticalGradient(
        window,
        {{position.x + inset, position.y + 1.0f}, {bandWidth, topBand}},
        sf::Color(0, 0, 0, strength),
        sf::Color(0, 0, 0, 0));

    // Weaker bounce on the bottom inner edge closes the recess.
    const float bottomBand = topBand * 0.55f;
    drawVerticalGradient(
        window,
        {{position.x + inset, position.y + size.y - 1.0f - bottomBand}, {bandWidth, bottomBand}},
        sf::Color(0, 0, 0, 0),
        sf::Color(0, 0, 0, static_cast<std::uint8_t>(strength * 0.6f)));
}

void drawBrassFrame(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    sf::Color color,
    float thickness)
{
    sf::ConvexShape frame = makeCutRect(position, size, cut);
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineThickness(thickness);
    frame.setOutlineColor(color);
    window.draw(frame);
}

void drawFocusRing(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size, float cut, float phase)
{
    // Two rings: a wide soft halo plus a tight bright line, so the ring reads on
    // both dark plate and bright art. The pulse keeps it alive without motion.
    const float pulse = 0.72f + 0.28f * std::sin(phase * 4.2f);
    const auto haloAlpha = static_cast<std::uint8_t>(std::lround(74.0f * pulse));
    const auto lineAlpha = static_cast<std::uint8_t>(std::lround(232.0f * pulse));

    drawBrassFrame(
        window,
        position - sf::Vector2f(5.0f, 5.0f),
        size + sf::Vector2f(10.0f, 10.0f),
        cut + 4.0f,
        withAlpha(palette::BrassPale, haloAlpha),
        3.0f);
    drawBrassFrame(
        window,
        position - sf::Vector2f(2.5f, 2.5f),
        size + sf::Vector2f(5.0f, 5.0f),
        cut + 2.0f,
        withAlpha(palette::BrassPale, lineAlpha),
        1.5f);
}

void drawMaterialPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    const PlateStyle& style)
{
    const bool lit = style.state == PlateState::Hover || style.state == PlateState::Selected;
    const bool pressed = style.state == PlateState::Pressed;
    const bool disabled = style.state == PlateState::Disabled;
    const float cut = std::max(0.0f, style.cut);

    sf::Color fill = style.fill;
    sf::Color frame = style.frame;
    if (disabled)
    {
        // Desaturate towards the plate colour rather than just dimming alpha, so
        // a disabled control looks deliberately switched off, not half-drawn.
        fill = mix(fill, sf::Color(20, 22, 23, fill.a), 0.55f);
        frame = mix(frame, sf::Color(96, 88, 76), 0.62f);
    }

    // Pressed surfaces sink: the cast shadow tightens and the whole plate
    // shifts down a pixel, which is what makes a click feel physical.
    const sf::Vector2f offset = pressed ? sf::Vector2f(0.0f, 1.0f) : sf::Vector2f(0.0f, 0.0f);
    const sf::Vector2f plateOrigin = position + offset;

    if (!disabled || style.castShadow)
    {
        const float dropX = pressed ? 1.5f : 4.0f;
        const float dropY = pressed ? 2.0f : 5.0f;
        const auto nearAlpha = static_cast<std::uint8_t>(lit ? 150 : 112);
        sf::ConvexShape wide = makeCutRect(
            plateOrigin + sf::Vector2f(dropX * 0.6f, dropY * 0.8f) - sf::Vector2f(2.0f, 1.0f),
            size + sf::Vector2f(4.0f, 4.0f),
            cut + 1.0f);
        wide.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(nearAlpha * 0.45f)));
        window.draw(wide);

        sf::ConvexShape near = makeCutRect(plateOrigin + sf::Vector2f(dropX, dropY), size, cut);
        near.setFillColor(sf::Color(0, 0, 0, nearAlpha));
        window.draw(near);
    }

    // Graded body. Pressed inverts the gradient so the surface reads concave.
    const float topFactor = pressed ? 0.82f : (lit ? 1.34f : 1.20f);
    const float bottomFactor = pressed ? 1.08f : 0.68f;
    fillCutRectGraded(
        window,
        plateOrigin,
        size,
        cut,
        shade(fill, topFactor),
        shade(fill, bottomFactor));

    if (style.recessed || pressed)
    {
        drawInnerShadow(window, plateOrigin, size, cut, 1.0f, lit ? 128 : 150);
    }
    else
    {
        // Even a raised plate wants a whisper of shadow under its frame,
        // otherwise the body looks pasted onto the border.
        drawInnerShadow(window, plateOrigin, size, cut, 0.62f, 66);
    }

    // Frame: a dark containment line outside, the brass proper, then a lit
    // hairline inside. Three values is the minimum that reads as cast metal.
    drawBrassFrame(
        window,
        plateOrigin - sf::Vector2f(2.0f, 2.0f),
        size + sf::Vector2f(4.0f, 4.0f),
        cut + 1.5f,
        sf::Color(0, 0, 0, disabled ? 90 : 150),
        1.0f);
    drawBrassFrame(window, plateOrigin, size, cut, frame, 2.0f);
    // Mixed towards the pale highlight rather than multiplied up: scaling all
    // three channels of an already-bright brass drives it to yellow.
    drawBrassFrame(
        window,
        plateOrigin + sf::Vector2f(1.0f, 1.0f),
        size - sf::Vector2f(2.0f, 2.0f),
        std::max(0.0f, cut - 1.0f),
        withAlpha(mix(frame, palette::BrassPale, 0.42f), lit ? 150 : 74),
        1.0f);

    const float innerCut = std::max(0.0f, cut - 3.0f);
    if (size.x > 12.0f && size.y > 12.0f)
    {
        drawBrassFrame(
            window,
            plateOrigin + sf::Vector2f(5.0f, 5.0f),
            size - sf::Vector2f(10.0f, 10.0f),
            innerCut,
            disabled ? sf::Color(92, 86, 76, 120)
                     : (lit ? sf::Color(255, 205, 114, 185) : sf::Color(117, 78, 39, 170)),
            1.0f);
    }

    if (style.sheen > 0.0f && !pressed)
    {
        const float run = std::max(0.0f, size.x - (cut + 8.0f) * 2.0f);
        drawLine(
            window,
            {plateOrigin.x + cut + 8.0f, plateOrigin.y + 7.0f},
            {run, 1.0f},
            withAlpha(
                palette::BrassPale,
                static_cast<std::uint8_t>(std::lround((lit ? 132.0f : 62.0f) * style.sheen))));
        drawLine(
            window,
            {plateOrigin.x + cut + 8.0f, plateOrigin.y + size.y - 8.0f},
            {run, 1.0f},
            sf::Color(48, 30, 17, 160));
    }

    if (style.brackets)
    {
        drawCornerBrackets(
            window,
            plateOrigin,
            size,
            8.0f,
            disabled ? sf::Color(96, 90, 80, 110)
                     : (lit ? sf::Color(250, 190, 91, 165) : sf::Color(109, 72, 35, 145)));
    }

    if (style.rivets && size.x >= 54.0f && size.y >= 26.0f)
    {
        const float radius = std::clamp(size.y * 0.065f, 1.7f, 3.0f);
        const sf::Color studColor = disabled
            ? sf::Color(118, 111, 99, 190)
            : (lit ? sf::Color(240, 186, 96, 225) : sf::Color(199, 139, 61, 210));
        drawRivet(window, {plateOrigin.x + 13.0f, plateOrigin.y + size.y * 0.5f}, radius, studColor);
        drawRivet(
            window,
            {plateOrigin.x + size.x - 13.0f, plateOrigin.y + size.y * 0.5f},
            radius,
            studColor);
    }

    if (style.focused)
    {
        drawFocusRing(window, plateOrigin, size, cut, style.focusPhase);
    }
}

void drawBeveledPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    sf::Color fill,
    sf::Color outline,
    bool highlighted,
    float cut)
{
    // Kept for every existing call site: same footprint, same shadow offset and
    // rivet placement, but routed through the richer material renderer.
    PlateStyle style;
    style.fill = fill;
    style.frame = outline;
    style.cut = cut;
    style.state = highlighted ? PlateState::Hover : PlateState::Normal;
    drawMaterialPlate(window, position, size, style);
}

void drawInsetSlot(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    sf::Color fill,
    sf::Color frame,
    bool active,
    bool error)
{
    PlateStyle style;
    style.fill = fill;
    style.frame = error ? palette::Danger : frame;
    style.cut = cut;
    style.state = active ? PlateState::Hover : PlateState::Normal;
    style.recessed = true;
    style.rivets = false;
    style.brackets = false;
    style.sheen = 0.35f;
    drawMaterialPlate(window, position, size, style);

    if (error)
    {
        // A second warm-red ring so an invalid field is unmistakable at a glance
        // without relying on the message text alone.
        drawBrassFrame(
            window,
            position - sf::Vector2f(2.5f, 2.5f),
            size + sf::Vector2f(5.0f, 5.0f),
            cut + 2.0f,
            withAlpha(palette::Danger, 150),
            1.5f);
    }
}

void drawRadialGlow(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color, int segments)
{
    if (radius <= 0.0f || segments < 3)
    {
        return;
    }

    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, static_cast<std::size_t>(segments) + 2);
    fan[0] = {center, color};
    const sf::Color edge = withAlpha(color, 0);
    for (int i = 0; i <= segments; ++i)
    {
        const float angle = static_cast<float>(i) / static_cast<float>(segments) * 6.2831853f;
        fan[static_cast<std::size_t>(i) + 1] = {
            {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius},
            edge};
    }
    window.draw(fan);
}

void drawTitlePlaque(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    sf::Vector2f size)
{
    if (value.empty())
    {
        return;
    }

    // Screen titles belong to the display face; body copy stays Roboto. The
    // plaque still measures and grows to whatever face is in play, so registering
    // a display font cannot clip an existing title.
    sf::Font& titleFont = displayFontOr(font);
    unsigned int titleSize = static_cast<unsigned int>(std::clamp(size.y * 0.52f, 22.0f, 44.0f));
    sf::Text measuring(titleFont, value, titleSize);
    const float titleBudget = std::max(size.x, 620.0f) - 132.0f;
    while (titleSize > 16 && measuring.getLocalBounds().size.x > titleBudget)
    {
        measuring.setCharacterSize(--titleSize);
    }

    const float measuredWidth = measuring.getLocalBounds().size.x + 132.0f;
    size.x = std::max(size.x, measuredWidth);
    const sf::Vector2f position{center.x - size.x * 0.5f, center.y - size.y * 0.5f};

    const float pipeY = center.y - 4.0f;
    sf::RectangleShape leftPipe({56.0f, 10.0f});
    leftPipe.setOrigin({56.0f, 5.0f});
    leftPipe.setPosition({position.x + 8.0f, pipeY});
    leftPipe.setFillColor(sf::Color(74, 44, 22, 230));
    leftPipe.setOutlineThickness(1.0f);
    leftPipe.setOutlineColor(BrassDim);
    window.draw(leftPipe);

    sf::RectangleShape rightPipe(leftPipe);
    rightPipe.setOrigin({0.0f, 5.0f});
    rightPipe.setPosition({position.x + size.x - 8.0f, pipeY});
    window.draw(rightPipe);

    drawRivet(window, {position.x - 50.0f, pipeY}, 10.0f, sf::Color(148, 91, 38, 220));
    drawRivet(window, {position.x + size.x + 50.0f, pipeY}, 10.0f, sf::Color(148, 91, 38, 220));

    drawBeveledPlate(
        window,
        position,
        size,
        sf::Color(23, 21, 18, 246),
        BrassBright,
        true,
        17.0f);

    sf::Text shadow(titleFont, value, titleSize);
    shadow.setFillColor(sf::Color(0, 0, 0, 190));
    centerButtonText(shadow, center + sf::Vector2f(2.0f, 3.0f));
    drawCrispText(window, shadow);

    sf::Text text(titleFont, value, titleSize);
    text.setFillColor(Ink);
    text.setOutlineThickness(1.0f);
    text.setOutlineColor(sf::Color(70, 43, 25, 210));
    centerButtonText(text, center);
    drawCrispText(window, text);
}

float drawPanelWithHeader(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& heading)
{
    drawPanel(window, position, size);
    if (heading.empty())
    {
        return position.y + 18.0f;
    }

    constexpr float BandHeight = 38.0f;
    // A darker band behind the heading separates chrome from content, so a panel
    // stops reading as one undifferentiated box of text.
    drawVerticalGradient(
        window,
        {{position.x + 6.0f, position.y + 6.0f}, {size.x - 12.0f, BandHeight}},
        sf::Color(0, 0, 0, 96),
        sf::Color(0, 0, 0, 18));

    sf::Font& headingFont = displayFontOr(font);
    sf::Text text(headingFont, heading, type::Heading);
    text.setFillColor(palette::Ink);
    centerText(text, {position.x + size.x * 0.5f, position.y + 6.0f + BandHeight * 0.48f});
    drawCrispText(window, text);

    drawSeparatorRule(window, {position.x + 22.0f, position.y + 6.0f + BandHeight}, size.x - 44.0f);
    return position.y + BandHeight + 22.0f;
}

void drawValuePill(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& value,
    sf::Color accent,
    float leadingInset)
{
    PlateStyle style;
    style.fill = sf::Color(10, 14, 15, 232);
    style.frame = withAlpha(accent, 205);
    style.cut = std::clamp(size.y * 0.42f, 4.0f, 12.0f);
    style.recessed = true;
    style.rivets = false;
    style.brackets = false;
    style.sheen = 0.4f;
    drawMaterialPlate(window, position, size, style);

    // Numerals sit on the display face: a plaque number that matches body copy
    // is the single clearest tell of an unstyled interface.
    sf::Font& numeralFont = displayFontOr(font);
    unsigned int numeralSize = static_cast<unsigned int>(std::clamp(size.y * 0.60f, 10.0f, 26.0f));
    sf::Text text(numeralFont, value, numeralSize);
    const float available = size.x - leadingInset - 10.0f;
    while (numeralSize > 8 && text.getLocalBounds().size.x > available)
    {
        text.setCharacterSize(--numeralSize);
    }
    text.setFillColor(accent);
    centerText(
        text,
        {position.x + leadingInset + (size.x - leadingInset) * 0.5f, position.y + size.y * 0.5f});
    drawCrispText(window, text);
}

void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width)
{
    if (width <= 0.0f)
    {
        return;
    }

    const float centerX = position.x + width * 0.5f;

    // The rule fades out towards both ends rather than butting into the frame,
    // and carries a small brass lozenge at the centre. A flat full-width line
    // with a blob on it is the giveaway of a rule drawn by a programmer.
    const float half = width * 0.5f;
    const sf::Color core(163, 108, 51, 190);
    const sf::Color ends(163, 108, 51, 0);
    sf::VertexArray fade(sf::PrimitiveType::TriangleStrip, 8);
    fade[0] = {{position.x, position.y}, ends};
    fade[1] = {{position.x, position.y + 1.0f}, ends};
    fade[2] = {{position.x + half * 0.45f, position.y}, core};
    fade[3] = {{position.x + half * 0.45f, position.y + 1.0f}, core};
    fade[4] = {{position.x + width - half * 0.45f, position.y}, core};
    fade[5] = {{position.x + width - half * 0.45f, position.y + 1.0f}, core};
    fade[6] = {{position.x + width, position.y}, ends};
    fade[7] = {{position.x + width, position.y + 1.0f}, ends};
    window.draw(fade);

    drawLine(
        window,
        {position.x + width * 0.14f, position.y + 2.0f},
        {width * 0.72f, 1.0f},
        sf::Color(38, 25, 15, 130));

    constexpr float LozengeHalf = 5.5f;
    sf::ConvexShape lozenge(4);
    lozenge.setPoint(0, {centerX, position.y + 1.0f - LozengeHalf});
    lozenge.setPoint(1, {centerX + LozengeHalf * 0.62f, position.y + 1.0f});
    lozenge.setPoint(2, {centerX, position.y + 1.0f + LozengeHalf});
    lozenge.setPoint(3, {centerX - LozengeHalf * 0.62f, position.y + 1.0f});
    lozenge.setFillColor(sf::Color(196, 138, 62, 235));
    lozenge.setOutlineThickness(1.0f);
    lozenge.setOutlineColor(sf::Color(58, 36, 18, 215));
    window.draw(lozenge);

    sf::CircleShape spark(1.4f, 8);
    spark.setOrigin({1.4f, 1.4f});
    spark.setPosition({centerX - 0.6f, position.y - 0.4f});
    spark.setFillColor(sf::Color(255, 231, 169, 190));
    window.draw(spark);
}

std::vector<std::string> wrapText(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    std::vector<std::string> lines;
    sf::Text measuringText(font, "", size);
    std::string line;
    std::size_t position = 0;

    auto fits = [&](const std::string& text) {
        measuringText.setString(text);
        return measuringText.getLocalBounds().size.x <= maxWidth;
    };

    auto pushLine = [&]() {
        lines.push_back(line);
        line.clear();
    };

    while (position < value.size())
    {
        if (value[position] == '\n')
        {
            pushLine();
            ++position;
            continue;
        }

        while (position < value.size() && value[position] == ' ')
        {
            ++position;
        }
        if (position >= value.size())
        {
            break;
        }

        const std::size_t wordStart = position;
        while (position < value.size() && value[position] != ' ' && value[position] != '\n')
        {
            ++position;
        }

        const std::string word = value.substr(wordStart, position - wordStart);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (line.empty() || fits(candidate))
        {
            line = candidate;
        }
        else
        {
            pushLine();
            line = word;
        }
    }

    if (!line.empty() || lines.empty())
    {
        lines.push_back(line);
    }
    return lines;
}

float drawWrappedText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth,
    float lineGap)
{
    float y = position.y;
    for (const std::string& line : wrapText(font, value, size, maxWidth))
    {
        drawText(window, font, line, size, {position.x, y}, color);
        y += static_cast<float>(size) + lineGap;
    }
    return y;
}

void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size)
{
    PlateStyle style;
    style.fill = sf::Color(9, 15, 16, 238);
    style.frame = Brass;
    style.cut = 14.0f;
    style.rivets = false;
    style.brackets = false;
    drawMaterialPlate(window, position, size, style);

    sf::RectangleShape wash({std::max(0.0f, size.x - 18.0f), std::max(0.0f, size.y - 18.0f)});
    wash.setPosition(position + sf::Vector2f(9.0f, 9.0f));
    wash.setFillColor(sf::Color(0, 0, 0, 54));
    window.draw(wash);

    // A deeper inner shadow all round: a large panel needs more depth than a
    // button before it stops looking like a flat sheet of colour.
    drawInnerShadow(window, position, size, 14.0f, 1.35f, 118);

    // Studs at the four corners instead of the mid-edge pair a plate gets: on a
    // panel the corner is where a real frame would be fastened.
    if (size.x >= 120.0f && size.y >= 90.0f)
    {
        constexpr float StudInset = 15.0f;
        const sf::Color studColor(186, 130, 57, 205);
        drawRivet(window, {position.x + StudInset, position.y + StudInset}, 3.1f, studColor);
        drawRivet(window, {position.x + size.x - StudInset, position.y + StudInset}, 3.1f, studColor);
        drawRivet(window, {position.x + StudInset, position.y + size.y - StudInset}, 3.1f, studColor);
        drawRivet(
            window,
            {position.x + size.x - StudInset, position.y + size.y - StudInset},
            3.1f,
            studColor);
    }

    drawLine(
        window,
        {position.x + 18.0f, position.y + size.y - 15.0f},
        {std::max(0.0f, size.x - 36.0f), 1.0f},
        sf::Color(224, 159, 74, 80));
}

void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected)
{
    drawRow(window, font, position, size, primary, secondary, selected, false);
}

void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected,
    bool hovered)
{
    PlateStyle style;
    style.fill = selected ? sf::Color(76, 49, 25, 238)
                          : (hovered ? sf::Color(32, 44, 45, 232) : sf::Color(19, 29, 30, 226));
    style.frame = selected ? BrassBright : (hovered ? sf::Color(158, 112, 60) : sf::Color(103, 72, 39));
    style.cut = 5.0f;
    style.state = selected ? PlateState::Selected : (hovered ? PlateState::Hover : PlateState::Normal);
    drawMaterialPlate(window, position, size, style);

    // A brass edge marker on the selected row: colour alone is not enough to
    // pick a row out of a long list at a glance.
    if (selected)
    {
        drawVerticalGradient(
            window,
            {{position.x + 3.0f, position.y + 4.0f}, {3.0f, size.y - 8.0f}},
            withAlpha(palette::BrassBright, 235),
            withAlpha(palette::Brass, 150));
    }

    constexpr float RowTextInset = 24.0f;
    drawText(
        window,
        font,
        primary,
        type::Body,
        {position.x + RowTextInset, position.y + 7.0f},
        selected || hovered ? palette::InkBright : sf::Color(246, 238, 218),
        size.x - RowTextInset - 16.0f);
    if (!secondary.empty())
    {
        drawText(
            window,
            font,
            secondary,
            type::Caption,
            {position.x + RowTextInset, position.y + 25.0f},
            selected ? sf::Color(226, 200, 152) : sf::Color(203, 173, 125),
            size.x - RowTextInset - 16.0f);
    }
}

std::optional<std::size_t> rowIndexAt(
    sf::Vector2f mouse,
    float x,
    float y,
    float width,
    float rowHeight,
    std::size_t visibleRows,
    std::size_t offset,
    std::size_t totalRows)
{
    if (mouse.x < x || mouse.x > x + width || mouse.y < y)
    {
        return std::nullopt;
    }

    const std::size_t visibleIndex = static_cast<std::size_t>((mouse.y - y) / rowHeight);
    const std::size_t index = offset + visibleIndex;
    if (visibleIndex < visibleRows && index < totalRows)
    {
        return index;
    }

    return std::nullopt;
}

bool isInsideRect(sf::Vector2f mouse, float x, float y, float width, float height)
{
    return mouse.x >= x && mouse.x <= x + width && mouse.y >= y && mouse.y <= y + height;
}

void scrollList(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows, float delta)
{
    if (delta < 0.0f)
    {
        if (offset + visibleRows < totalRows)
        {
            ++offset;
        }
    }
    else if (offset > 0)
    {
        --offset;
    }
}

void clampListOffset(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows)
{
    if (totalRows <= visibleRows)
    {
        offset = 0;
        return;
    }

    offset = std::min(offset, totalRows - visibleRows);
}
}
