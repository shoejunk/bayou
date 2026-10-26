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
sf::Texture* registeredCornerThornTexture = nullptr;
sf::Texture* registeredTitleFinialTexture = nullptr;
sf::Texture* registeredRivetTexture = nullptr;

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

void drawOrnament(
    sf::RenderWindow& window,
    sf::Texture* texture,
    sf::FloatRect target,
    bool flipX = false,
    bool flipY = false,
    sf::Color tint = sf::Color::White)
{
    if (texture == nullptr || target.size.x <= 0.0f || target.size.y <= 0.0f)
    {
        return;
    }

    const sf::Vector2u imageSize = texture->getSize();
    if (imageSize.x == 0 || imageSize.y == 0)
    {
        return;
    }

    const float sourceWidth = static_cast<float>(imageSize.x);
    const float sourceHeight = static_cast<float>(imageSize.y);
    const float scale = std::min(target.size.x / sourceWidth, target.size.y / sourceHeight);
    sf::Sprite sprite(*texture);
    sprite.setScale({flipX ? -scale : scale, flipY ? -scale : scale});
    sprite.setColor(tint);
    sprite.setPosition({
        target.position.x +
            (target.size.x + (flipX ? sourceWidth * scale : -sourceWidth * scale)) * 0.5f,
        target.position.y +
            (target.size.y + (flipY ? sourceHeight * scale : -sourceHeight * scale)) * 0.5f});
    window.draw(sprite);
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
    if (registeredRivetTexture != nullptr)
    {
        const float extent = radius * 4.35f;
        sf::Color tint = mix(color, sf::Color::White, 0.58f);
        tint.a = color.a;
        drawOrnament(
            window,
            registeredRivetTexture,
            {{center.x - extent * 0.5f, center.y - extent * 0.5f}, {extent, extent}},
            false,
            false,
            tint);
        return;
    }

    const float socketRadius = radius * 1.34f;
    sf::CircleShape socket(socketRadius, 6);
    socket.setOrigin({socketRadius, socketRadius});
    socket.setPosition(center + sf::Vector2f(0.7f, 1.1f));
    socket.setRotation(sf::degrees(30.0f));
    socket.setFillColor(withAlpha(palette::BrassShadow, 180));
    socket.setOutlineThickness(std::max(0.7f, radius * 0.18f));
    socket.setOutlineColor(withAlpha(palette::BrassEdge, 190));
    window.draw(socket);

    sf::CircleShape shadow(radius, 6);
    shadow.setOrigin({radius, radius});
    shadow.setPosition(center + sf::Vector2f(1.0f, 1.0f));
    shadow.setRotation(sf::degrees(30.0f));
    shadow.setFillColor(sf::Color(0, 0, 0, 105));
    window.draw(shadow);

    sf::CircleShape rivet(radius, 6);
    rivet.setOrigin({radius, radius});
    rivet.setPosition(center);
    rivet.setRotation(sf::degrees(30.0f));
    rivet.setFillColor(color);
    rivet.setOutlineThickness(std::max(1.0f, radius * 0.28f));
    rivet.setOutlineColor(withAlpha(palette::BrassDeep, 220));
    window.draw(rivet);

    sf::CircleShape shine(radius * 0.38f, 6);
    shine.setOrigin({radius * 0.38f, radius * 0.38f});
    shine.setPosition(center + sf::Vector2f(-radius * 0.22f, -radius * 0.22f));
    shine.setRotation(sf::degrees(30.0f));
    shine.setFillColor(withAlpha(palette::BrassPale, 140));
    window.draw(shine);
}

} // namespace

sf::Color withAlphaForControls(sf::Color color, std::uint8_t alpha)
{
    color.a = alpha;
    return color;
}

void drawDiamond(
    sf::RenderWindow& window,
    sf::Vector2f center,
    float width,
    float height,
    sf::Color fill,
    sf::Color outline)
{
    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    sf::ConvexShape diamond(4);
    diamond.setPoint(0, {center.x, center.y - height * 0.5f});
    diamond.setPoint(1, {center.x + width * 0.5f, center.y});
    diamond.setPoint(2, {center.x, center.y + height * 0.5f});
    diamond.setPoint(3, {center.x - width * 0.5f, center.y});
    diamond.setFillColor(fill);
    if (outline.a > 0)
    {
        diamond.setOutlineThickness(1.0f);
        diamond.setOutlineColor(outline);
    }
    window.draw(diamond);
}

void drawMenuFrameLegacy(sf::RenderWindow& window, sf::Texture* borderTexture)
{
    if (borderTexture)
    {
        const sf::Vector2u textureSize = borderTexture->getSize();
        if (textureSize.x > 0 && textureSize.y > 0)
        {
            sf::Sprite border(*borderTexture);
            border.setPosition({ui_canvas::Left, 0.0f});
            border.setScale({
                ui_canvas::Width / static_cast<float>(textureSize.x),
                ui_canvas::Height / static_cast<float>(textureSize.y)});
            window.draw(border);
            return;
        }
    }

    const float left = ui_canvas::Left + 10.0f;
    const float right = ui_canvas::Right - 10.0f;
    const float top = 10.0f;
    const float bottom = ui_canvas::Height - 10.0f;
    const sf::Vector2f framePosition{left, top};
    const sf::Vector2f frameSize{right - left, bottom - top};

    // The outer keyline and inner rail give the background a manufactured,
    // silver-edged container before the living vine work is layered on top.
    drawBrassFrame(
        window,
        framePosition,
        frameSize,
        18.0f,
        withAlpha(palette::SteelDeep, 250),
        7.0f);
    drawBrassFrame(
        window,
        framePosition + sf::Vector2f(5.0f, 5.0f),
        frameSize - sf::Vector2f(10.0f, 10.0f),
        14.0f,
        withAlpha(palette::SteelEdge, 245),
        4.0f);
    drawBrassFrame(
        window,
        framePosition + sf::Vector2f(10.0f, 10.0f),
        frameSize - sf::Vector2f(20.0f, 20.0f),
        10.0f,
        withAlpha(palette::SteelBright, 205),
        2.0f);

    const float railLeft = left + 42.0f;
    const float railWidth = std::max(0.0f, frameSize.x - 84.0f);
    drawLine(window, {railLeft, top + 7.0f}, {railWidth, 2.0f}, withAlpha(palette::SteelBright, 190));
    drawLine(window, {railLeft, top + 11.0f}, {railWidth, 2.0f}, withAlpha(palette::SteelEdge, 230));
    drawLine(window, {railLeft, bottom - 8.0f}, {railWidth, 2.0f}, withAlpha(palette::SteelEdge, 235));
    drawLine(window, {railLeft, bottom - 12.0f}, {railWidth, 2.0f}, withAlpha(palette::SteelBright, 132));

    const auto normalize = [](sf::Vector2f value) {
        const float length = std::hypot(value.x, value.y);
        return length > 0.01f ? value / length : sf::Vector2f{};
    };

    const auto drawSegment = [&](sf::Vector2f start, sf::Vector2f end, sf::Color color, float thickness) {
        const sf::Vector2f delta = end - start;
        const float length = std::hypot(delta.x, delta.y);
        if (length <= 0.01f)
        {
            return;
        }

        sf::RectangleShape branch({length, thickness});
        branch.setOrigin({0.0f, thickness * 0.5f});
        branch.setPosition(start);
        branch.setRotation(sf::degrees(std::atan2(delta.y, delta.x) * 180.0f / 3.14159265f));
        branch.setFillColor(color);
        window.draw(branch);
    };

    const auto drawLeaf = [&](sf::Vector2f root, sf::Vector2f direction, float side, float scale) {
        const sf::Vector2f normal{-direction.y * side, direction.x * side};
        const sf::Vector2f leafDirection = normalize(direction + normal * 0.82f);
        const float length = 17.0f * scale;
        const float width = 4.4f * scale;
        const sf::Vector2f tip = root + leafDirection * length;
        const sf::Vector2f shoulder = root + leafDirection * (length * 0.47f);

        sf::ConvexShape leaf(4);
        leaf.setPoint(0, root);
        leaf.setPoint(1, shoulder + normal * width);
        leaf.setPoint(2, tip);
        leaf.setPoint(3, shoulder - normal * width);
        leaf.setFillColor(withAlpha(palette::VineBright, 228));
        leaf.setOutlineThickness(1.0f);
        leaf.setOutlineColor(withAlpha(palette::Steel, 175));
        window.draw(leaf);
    };

    const auto drawVine = [&](const std::vector<sf::Vector2f>& points, float thornSide) {
        if (points.size() < 2)
        {
            return;
        }

        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            const sf::Vector2f start = points[index];
            const sf::Vector2f end = points[index + 1];
            const sf::Vector2f delta = end - start;
            const float length = std::hypot(delta.x, delta.y);
            if (length <= 0.01f)
            {
                continue;
            }

            drawSegment(start + sf::Vector2f(2.0f, 3.0f), end + sf::Vector2f(2.0f, 3.0f), withAlpha(palette::VineShadow, 245), 12.0f);
            drawSegment(start, end, withAlpha(palette::SteelDeep, 250), 9.0f);
            drawSegment(start, end, withAlpha(palette::Steel, 238), 6.0f);
            drawSegment(start, end, withAlpha(palette::Vine, 250), 4.0f);
            drawSegment(start, end, withAlpha(palette::VineBright, 205), 1.8f);

            if ((index % 2) != 0)
            {
                continue;
            }

            const sf::Vector2f direction = normalize(delta);
            const sf::Vector2f normal{-direction.y * thornSide, direction.x * thornSide};
            const sf::Vector2f root = start + delta * 0.58f;
            const sf::Vector2f tip = root + normal * 19.0f;
            const sf::Vector2f base = root - normal * 2.5f;

            sf::ConvexShape thorn(3);
            thorn.setPoint(0, tip);
            thorn.setPoint(1, base + direction * 6.0f);
            thorn.setPoint(2, base - direction * 6.0f);
            thorn.setFillColor(withAlpha(palette::SteelBright, 220));
            thorn.setOutlineThickness(1.2f);
            thorn.setOutlineColor(withAlpha(palette::VineShadow, 240));
            window.draw(thorn);

            drawLeaf(root, direction, thornSide, 0.92f);
        }
    };

    // Corner growths are deliberately asymmetric: the frame feels cultivated,
    // but not like a repeated HUD texture stamped four times.
    drawVine(
        {{left + 9.0f, bottom - 12.0f},
         {left + 26.0f, bottom - 78.0f},
         {left + 13.0f, bottom - 145.0f},
         {left + 29.0f, bottom - 214.0f},
         {left + 15.0f, top + 150.0f},
         {left + 31.0f, top + 79.0f},
         {left + 18.0f, top + 10.0f}},
        -1.0f);
    drawVine(
        {{right - 9.0f, top + 12.0f},
         {right - 27.0f, top + 77.0f},
         {right - 13.0f, top + 146.0f},
         {right - 30.0f, top + 218.0f},
         {right - 16.0f, bottom - 150.0f},
         {right - 32.0f, bottom - 79.0f},
         {right - 18.0f, bottom - 10.0f}},
        1.0f);
    drawVine(
        {{left + 18.0f, top + 21.0f},
         {left + 112.0f, top + 9.0f},
         {left + 214.0f, top + 24.0f},
         {left + 322.0f, top + 10.0f},
         {left + 432.0f, top + 23.0f},
         {left + 548.0f, top + 8.0f},
         {right - 18.0f, top + 20.0f}},
        -1.0f);
    drawVine(
        {{left + 18.0f, bottom - 21.0f},
         {left + 124.0f, bottom - 8.0f},
         {left + 235.0f, bottom - 24.0f},
         {left + 348.0f, bottom - 10.0f},
         {left + 462.0f, bottom - 23.0f},
         {left + 574.0f, bottom - 9.0f},
         {right - 18.0f, bottom - 20.0f}},
        1.0f);

    const auto drawJewel = [&](sf::Vector2f center, float scale) {
        drawRadialGlow(window, center, 30.0f * scale, withAlpha(palette::Jewel, 58), 22);
        drawDiamond(
            window,
            center,
            22.0f * scale,
            34.0f * scale,
            withAlpha(palette::Jewel, 230),
            withAlpha(palette::SteelBright, 235));
        drawDiamond(
            window,
            center - sf::Vector2f(1.5f * scale, 3.0f * scale),
            7.0f * scale,
            14.0f * scale,
            withAlpha(palette::JewelBright, 235),
            withAlpha(palette::JewelDeep, 220));
    };

    drawJewel({left + 24.0f, top + 54.0f}, 1.0f);
    drawJewel({right - 24.0f, top + 54.0f}, 1.0f);
    drawJewel({left + 24.0f, bottom - 54.0f}, 1.0f);
    drawJewel({right - 24.0f, bottom - 54.0f}, 1.0f);
    drawJewel({(left + right) * 0.5f, top + 7.0f}, 0.78f);
    drawJewel({(left + right) * 0.5f, bottom - 7.0f}, 0.78f);

    drawRivet(window, {left + 35.0f, top + 31.0f}, 5.5f, withAlpha(palette::SteelBright, 225));
    drawRivet(window, {right - 35.0f, top + 31.0f}, 5.5f, withAlpha(palette::SteelBright, 225));
    drawRivet(window, {left + 35.0f, bottom - 31.0f}, 5.5f, withAlpha(palette::SteelBright, 225));
    drawRivet(window, {right - 35.0f, bottom - 31.0f}, 5.5f, withAlpha(palette::SteelBright, 225));
}

void drawMenuFrame(sf::RenderWindow& window)
{
    // Frame the active view, not the maximum 16:9 canvas. On a 4:3 window the
    // widescreen gutters are cropped, so canvas-relative corners disappear.
    // View-relative bounds preserve the same inset at every supported aspect.
    const sf::View& view = window.getView();
    const sf::Vector2f viewSize = view.getSize();
    const sf::Vector2f viewCenter = view.getCenter();
    const float left = viewCenter.x - viewSize.x * 0.5f + 9.0f;
    const float right = viewCenter.x + viewSize.x * 0.5f - 9.0f;
    const float top = viewCenter.y - viewSize.y * 0.5f + 9.0f;
    const float bottom = viewCenter.y + viewSize.y * 0.5f - 9.0f;
    const sf::Vector2f framePosition{left, top};
    const sf::Vector2f frameSize{right - left, bottom - top};

    // Two narrow steel passes are enough to read as forged structure. A third
    // bright rail made the perimeter look like layered sci-fi chrome.
    drawBrassFrame(window, framePosition, frameSize, 12.0f, withAlpha(palette::SteelDeep, 230), 2.5f);
    drawBrassFrame(
        window,
        framePosition + sf::Vector2f(3.0f, 3.0f),
        frameSize - sf::Vector2f(6.0f, 6.0f),
        10.0f,
        withAlpha(palette::SteelBright, 148),
        1.0f);

    const float railLeft = left + 54.0f;
    const float railWidth = std::max(0.0f, frameSize.x - 108.0f);
    drawLine(window, {railLeft, top + 7.0f}, {railWidth, 1.0f}, withAlpha(palette::SteelBright, 112));
    drawLine(window, {railLeft, bottom - 8.0f}, {railWidth, 1.0f}, withAlpha(palette::SteelEdge, 150));

    // The authored thorn corners keep the actual frame narrow while making the
    // organic silhouette readable at a glance. One transparent source is
    // mirrored into all four corners, so material and thorn scale stay exact.
    if (registeredCornerThornTexture != nullptr)
    {
        constexpr sf::Vector2f OrnamentSize{96.0f, 80.0f};
        const sf::Color ornamentTint(238, 244, 246, 238);
        drawOrnament(
            window,
            registeredCornerThornTexture,
            {{left - 6.0f, top - 6.0f}, OrnamentSize},
            false,
            false,
            ornamentTint);
        drawOrnament(
            window,
            registeredCornerThornTexture,
            {{right - OrnamentSize.x + 6.0f, top - 6.0f}, OrnamentSize},
            true,
            false,
            ornamentTint);
        drawOrnament(
            window,
            registeredCornerThornTexture,
            {{left - 6.0f, bottom - OrnamentSize.y + 6.0f}, OrnamentSize},
            false,
            true,
            ornamentTint);
        drawOrnament(
            window,
            registeredCornerThornTexture,
            {{right - OrnamentSize.x + 6.0f, bottom - OrnamentSize.y + 6.0f}, OrnamentSize},
            true,
            true,
            ornamentTint);
    }

    // Real fastener art replaces the floating diamond glyphs that previously
    // sat at the rail joins.
    drawRivet(window, {(left + right) * 0.5f, top + 7.0f}, 2.7f, withAlpha(palette::SteelBright, 220));
    drawRivet(window, {(left + right) * 0.5f, bottom - 7.0f}, 2.7f, withAlpha(palette::SteelBright, 220));
}

namespace
{
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

void setUiOrnamentTextures(
    sf::Texture* cornerThorn,
    sf::Texture* titleFinial,
    sf::Texture* rivet)
{
    registeredCornerThornTexture = cornerThorn;
    registeredTitleFinialTexture = titleFinial;
    registeredRivetTexture = rivet;
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
    // Symmetrical plates use the visible glyph bounds directly. Callers with
    // asymmetric artwork can pass the centre of that style's inset face.
    centerText(text, center);
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
    // Large standalone labels are titles even when an older call site still
    // passes the body font. Keeping that decision here prevents modal titles
    // and other shared overlays from dropping back to generic Roboto while
    // the rest of the surface uses the Gloomthorn face.
    sf::Font& renderFont = size >= type::Hero ? displayFontOr(font) : font;
    const std::string displayValue =
        maxWidth > 0.0f ? elideToWidth(renderFont, value, size, maxWidth) : value;
    sf::Text text(renderFont, displayValue, size);
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

void drawKeyboardHintBar(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    float width)
{
    constexpr float Height = 18.0f;
    PlateStyle style;
    style.fill = sf::Color(7, 13, 16, 236);
    style.frame = withAlpha(palette::SteelBright, 142);
    style.cut = 5.0f;
    style.sheen = 0.2f;
    style.rivets = false;
    style.brackets = false;
    style.recessed = true;
    style.castShadow = false;
    drawMaterialPlate(
        window,
        {center.x - width * 0.5f, center.y - Height * 0.5f},
        {width, Height},
        style);
    drawCenteredText(window, font, value, 11, center, palette::Ink);
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

void drawFocusRing(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    float cut,
    float phase,
    sf::Color color)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    // Three values keep keyboard focus legible over both the dark plate and the
    // pale artwork in the supplied menu reference: a dark keyline, a soft halo,
    // and a crisp brass line. The pulse is deliberately restrained so focus
    // feels alive without turning into a notification effect.
    const float pulse = 0.72f + 0.28f * std::sin(phase * 4.2f);
    const auto haloAlpha = static_cast<std::uint8_t>(std::lround(82.0f * pulse));
    const auto lineAlpha = static_cast<std::uint8_t>(std::lround(242.0f * pulse));

    drawBrassFrame(
        window,
        position - sf::Vector2f(6.0f, 6.0f),
        size + sf::Vector2f(12.0f, 12.0f),
        cut + 5.0f,
        withAlpha(palette::FocusKeyline, 220),
        2.0f);

    drawBrassFrame(
        window,
        position - sf::Vector2f(5.0f, 5.0f),
        size + sf::Vector2f(10.0f, 10.0f),
        cut + 4.0f,
        withAlpha(color, haloAlpha),
        3.0f);
    drawBrassFrame(
        window,
        position - sf::Vector2f(2.5f, 2.5f),
        size + sf::Vector2f(5.0f, 5.0f),
        cut + 2.0f,
        withAlpha(color, lineAlpha),
        1.5f);
}

void drawMaterialPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    const PlateStyle& style)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

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
        frame = mix(frame, sf::Color(78, 91, 96), 0.62f);
    }

    // Pressed surfaces sink: the cast shadow tightens and the whole plate
    // shifts down a pixel, which is what makes a click feel physical. The
    // The additional wide shadow is intentionally blue-black rather than
    // neutral black; it keeps the steel from reading like generic web chrome.
    const sf::Vector2f offset = pressed ? sf::Vector2f(0.0f, 1.0f) : sf::Vector2f(0.0f, 0.0f);
    const sf::Vector2f plateOrigin = position + offset;

    if (style.castShadow)
    {
        const float dropX = pressed ? 1.5f : 4.0f;
        const float dropY = pressed ? 2.0f : 5.0f;
        const auto nearAlpha = static_cast<std::uint8_t>(disabled ? 48 : (lit ? 158 : 122));
        sf::ConvexShape wide = makeCutRect(
            plateOrigin + sf::Vector2f(dropX * 0.6f, dropY * 0.8f) - sf::Vector2f(2.0f, 1.0f),
            size + sf::Vector2f(4.0f, 4.0f),
            cut + 1.0f);
        wide.setFillColor(withAlpha(palette::BrassShadow, static_cast<std::uint8_t>(nearAlpha * 0.42f)));
        window.draw(wide);

        sf::ConvexShape near = makeCutRect(plateOrigin + sf::Vector2f(dropX, dropY), size, cut);
        near.setFillColor(withAlpha(palette::BrassShadow, nearAlpha));
        window.draw(near);
    }

    // Pull every caller a little towards the same ink-blue base before applying
    // the state tint. This is the shared material decision that makes buttons,
    // rows, tabs and fields feel like one manufactured system even when older
    // screens pass slightly different fill colours.
    sf::Color body = mix(fill, style.recessed ? palette::PlateDeep : palette::Plate, style.recessed ? 0.12f : 0.08f);
    // A trace of violet in the black is the common Gloomthorn signature. It
    // should be felt in the depth of a surface, never read as a second theme.
    body = mix(body, palette::ArcaneDeep, style.recessed ? 0.10f : 0.035f);
    if (style.state == PlateState::Selected)
    {
        body = mix(body, palette::SurfaceWarm, 0.26f);
    }

    // Graded body. Pressed inverts the gradient so the surface reads concave.
    const float topFactor = pressed ? 0.84f : (lit ? 1.23f : 1.12f);
    const float bottomFactor = pressed ? 1.04f : (lit ? 0.62f : 0.72f);
    fillCutRectGraded(
        window,
        plateOrigin,
        size,
        cut,
        shade(body, topFactor),
        shade(body, bottomFactor));

    // Every plate gets a controlled top lip and a darker heel. These two
    // hairlines are deliberately inside the frame: at the 800px reference
    // scale they read as manufactured depth instead of extra decoration.
    if (size.x > 24.0f && size.y > 16.0f)
    {
        const float railInset = std::clamp(cut + 7.0f, 9.0f, size.x * 0.30f);
        const float railWidth = std::max(0.0f, size.x - railInset * 2.0f);
        drawLine(
            window,
            {plateOrigin.x + railInset, plateOrigin.y + 5.0f},
            {railWidth, 1.0f},
            withAlpha(
                style.recessed ? palette::BrassEdge : palette::BrassPale,
                static_cast<std::uint8_t>(style.recessed ? 92 : (lit ? 146 : 72))));
        drawLine(
            window,
            {plateOrigin.x + railInset, plateOrigin.y + size.y - 6.0f},
            {railWidth, 1.0f},
            withAlpha(palette::BrassDeep, static_cast<std::uint8_t>(pressed ? 188 : 132)));
    }

    if (style.recessed || pressed)
    {
        drawInnerShadow(window, plateOrigin, size, cut, pressed ? 1.05f : 1.22f, lit ? 132 : 156);
    }
    else
    {
        // Even a raised plate wants a whisper of shadow under its frame,
        // otherwise the body looks pasted onto the border.
        drawInnerShadow(window, plateOrigin, size, cut, 0.58f, 72);
    }

    // A dark containment line, the steel proper, and one restrained inner
    // highlight are sufficient. Additional nested rails made dense screens
    // look like utility software rather than a forged game interface.
    drawBrassFrame(
        window,
        plateOrigin - sf::Vector2f(2.0f, 2.0f),
        size + sf::Vector2f(4.0f, 4.0f),
        cut + 1.5f,
        withAlpha(palette::BrassShadow, disabled ? 105 : 180),
        1.0f);
    drawBrassFrame(window, plateOrigin, size, cut, withAlpha(frame, disabled ? 135 : (lit ? 230 : 205)), 1.5f);
    // Mixed towards the pale highlight rather than multiplied up: scaling all
    // three channels of an already-bright brass drives it to yellow.
    drawBrassFrame(
        window,
        plateOrigin + sf::Vector2f(1.0f, 1.0f),
        size - sf::Vector2f(2.0f, 2.0f),
        std::max(0.0f, cut - 1.0f),
        withAlpha(mix(frame, palette::BrassPale, lit ? 0.45f : 0.28f), disabled ? 45 : (lit ? 140 : 70)),
        1.0f);

    if (style.sheen > 0.0f && !pressed)
    {
        const float run = std::max(0.0f, size.x - (cut + 8.0f) * 2.0f);
        drawLine(
            window,
            {plateOrigin.x + cut + 8.0f, plateOrigin.y + 7.0f},
            {run, 1.0f},
            withAlpha(
                palette::BrassGlow,
                static_cast<std::uint8_t>(std::lround(
                    (lit ? 132.0f : 62.0f) * style.sheen * (disabled ? 0.35f : 1.0f)))));
        drawLine(
            window,
            {plateOrigin.x + cut + 8.0f, plateOrigin.y + size.y - 8.0f},
            {run, 1.0f},
            withAlpha(palette::BrassDeep, 170));
    }

    // Corner brackets need room to read as brackets; on a checkbox-sized plate
    // they fill the whole face and look like noise.
    if (style.brackets && !disabled && size.x >= 34.0f && size.y >= 26.0f)
    {
        drawCornerBrackets(
            window,
            plateOrigin,
            size,
            8.0f,
            disabled ? sf::Color(96, 90, 80, 110)
                     : (lit ? sf::Color(198, 215, 220, 165) : sf::Color(67, 84, 92, 145)));
    }

    if (style.rivets && !disabled && size.x >= 54.0f && size.y >= 26.0f)
    {
        const float radius = std::clamp(size.y * 0.065f, 1.7f, 3.0f);
        const sf::Color studColor = disabled
            ? sf::Color(118, 111, 99, 190)
            : (lit ? sf::Color(208, 226, 232, 225) : sf::Color(137, 159, 168, 210));
        drawRivet(window, {plateOrigin.x + 13.0f, plateOrigin.y + size.y * 0.5f}, radius, studColor);
        drawRivet(
            window,
            {plateOrigin.x + size.x - 13.0f, plateOrigin.y + size.y * 0.5f},
            radius,
            studColor);
    }

    // A disabled control must never advertise keyboard focus: the bright ring
    // would contradict the switched-off material and suggest an available
    // action.
    if (style.focused && !disabled)
    {
        if (style.focusKeyline)
        {
            drawBrassFrame(
                window,
                plateOrigin - sf::Vector2f(4.0f, 4.0f),
                size + sf::Vector2f(8.0f, 8.0f),
                cut + 3.0f,
                withAlpha(palette::FocusKeyline, 240),
                3.5f);
        }
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

    constexpr float FullFinialWidth = 86.0f;
    constexpr float CompactFinialWidth = 54.0f;
    constexpr float FinialHeight = 30.0f;
    const float finialY = center.y - FinialHeight * 0.5f - 2.0f;
    const sf::View& activeView = window.getView();
    const float visibleLeft = activeView.getCenter().x - activeView.getSize().x * 0.5f;
    const float visibleRight = activeView.getCenter().x + activeView.getSize().x * 0.5f;
    const auto finialBounds = [&](float width, bool right) {
        return sf::FloatRect{
            {right ? position.x + size.x - 11.0f : position.x - width + 11.0f,
             finialY},
            {width, FinialHeight}};
    };
    float finialWidth = FullFinialWidth;
    sf::FloatRect leftFinial = finialBounds(finialWidth, false);
    sf::FloatRect rightFinial = finialBounds(finialWidth, true);
    const bool fullPairFits =
        leftFinial.position.x >= visibleLeft + 3.0f &&
        rightFinial.position.x + rightFinial.size.x <= visibleRight - 3.0f;
    if (!fullPairFits)
    {
        // Left-anchored utility headers (notably Card Editor) still deserve a
        // balanced pair. Compress both authored finials together rather than
        // silently dropping one and making the plaque look clipped.
        finialWidth = CompactFinialWidth;
        leftFinial = finialBounds(finialWidth, false);
        rightFinial = finialBounds(finialWidth, true);
    }
    const bool showLeftFinial = leftFinial.position.x >= visibleLeft + 3.0f;
    const bool showRightFinial = rightFinial.position.x + rightFinial.size.x <= visibleRight - 3.0f;
    if (registeredTitleFinialTexture != nullptr)
    {
        const sf::Color finialTint(238, 244, 246, 242);
        if (showLeftFinial)
        {
            drawOrnament(
                window, registeredTitleFinialTexture, leftFinial, false, false, finialTint);
        }
        if (showRightFinial)
        {
            drawOrnament(
                window, registeredTitleFinialTexture, rightFinial, true, false, finialTint);
        }
    }
    else
    {
        if (showLeftFinial)
        {
            drawLine(
                window,
                {position.x - 48.0f, center.y - 1.0f},
                {48.0f, 2.0f},
                withAlpha(palette::Steel, 210));
        }
        if (showRightFinial)
        {
            drawLine(
                window,
                {position.x + size.x, center.y - 1.0f},
                {48.0f, 2.0f},
                withAlpha(palette::Steel, 210));
        }
    }

    PlateStyle plaque;
    plaque.fill = sf::Color(15, 20, 23, 246);
    plaque.frame = BrassBright;
    plaque.cut = 17.0f;
    plaque.state = PlateState::Normal;
    plaque.rivets = false;
    plaque.brackets = false;
    plaque.sheen = 0.62f;
    drawMaterialPlate(window, position, size, plaque);

    // The logo banner uses a strong horizontal lock-up. These inset rails and
    // side diamonds give text-only plaques the same deliberate silhouette,
    // while remaining quiet enough behind the supplied banner artwork.
    drawLine(
        window,
        {position.x + 28.0f, position.y + 8.0f},
        {std::max(0.0f, size.x - 56.0f), 1.0f},
        withAlpha(palette::BrassPale, 132));
    drawLine(
        window,
        {position.x + 28.0f, position.y + size.y - 9.0f},
        {std::max(0.0f, size.x - 56.0f), 1.0f},
        withAlpha(palette::BrassDeep, 198));
    drawRivet(window, {position.x + 17.0f, center.y}, 2.8f, withAlpha(palette::SteelBright, 220));
    drawRivet(
        window,
        {position.x + size.x - 17.0f, center.y},
        2.8f,
        withAlpha(palette::SteelBright, 220));

    sf::Text shadow(titleFont, value, titleSize);
    shadow.setFillColor(sf::Color(0, 0, 0, 190));
    centerButtonText(shadow, center + sf::Vector2f(2.0f, 3.0f));
    drawCrispText(window, shadow);

    sf::Text text(titleFont, value, titleSize);
    text.setFillColor(palette::InkBright);
    text.setLetterSpacing(1.025f);
    text.setOutlineThickness(1.25f);
    text.setOutlineColor(withAlpha(palette::BrassDeep, 220));
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

    constexpr float BandHeight = 40.0f;
    // A darker band behind the heading separates chrome from content, so a panel
    // stops reading as one undifferentiated box of text.
    fillCutRectGraded(
        window,
        {position.x + 7.0f, position.y + 7.0f},
        {std::max(0.0f, size.x - 14.0f), BandHeight},
        8.0f,
        withAlpha(palette::BrassDeep, 118),
        withAlpha(palette::PlateDeep, 42));

    sf::Font& headingFont = displayFontOr(font);
    sf::Text text(headingFont, heading, type::Heading);
    text.setFillColor(palette::Ink);
    text.setLetterSpacing(1.015f);
    centerText(text, {position.x + size.x * 0.5f, position.y + 6.0f + BandHeight * 0.48f});
    drawCrispText(window, text);

    drawSeparatorRule(window, {position.x + 24.0f, position.y + 7.0f + BandHeight}, size.x - 48.0f, false);
    drawStud(
        window,
        {position.x + size.x * 0.5f, position.y + 7.0f + BandHeight},
        2.7f,
        withAlpha(palette::SteelBright, 205));
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
    style.fill = palette::PlateDeep;
    // The value may be gold, green, or violet, but the hardware holding it is
    // always the same forged steel used by the surrounding interface.
    style.frame = withAlpha(palette::Steel, 215);
    style.cut = std::clamp(size.y * 0.42f, 4.0f, 12.0f);
    style.recessed = true;
    style.rivets = false;
    style.brackets = false;
    style.sheen = 0.4f;
    drawMaterialPlate(window, position, size, style);

    if (leadingInset > 0.0f && leadingInset < size.x - 8.0f)
    {
        drawLine(
            window,
            {position.x + leadingInset, position.y + 5.0f},
            {1.0f, std::max(0.0f, size.y - 10.0f)},
            withAlpha(palette::SteelEdge, 132));
    }

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
    drawSeparatorRule(window, position, width, true);
}

void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width, bool ornament)
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
    const sf::Color core = withAlpha(palette::Brass, 190);
    const sf::Color ends = withAlpha(palette::Brass, 0);
    drawLine(
        window,
        {position.x + width * 0.15f, position.y - 1.0f},
        {width * 0.70f, 3.0f},
        withAlpha(palette::BrassGlow, 24));
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
        withAlpha(palette::BrassShadow, 155));

    if (!ornament)
    {
        // Row dividers inside a panel stay plain: a column of centred lozenges
        // down a settings list reads as a seam, not as decoration.
        return;
    }

    drawStud(
        window,
        {centerX, position.y + 1.0f},
        3.0f,
        withAlpha(palette::SteelBright, 220));
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
    style.fill = sf::Color(9, 15, 16, 242);
    style.frame = mix(Brass, BrassDim, 0.18f);
    style.cut = 14.0f;
    style.rivets = false;
    style.brackets = false;
    style.sheen = 0.72f;
    drawMaterialPlate(window, position, size, style);

    // Keep the inner wash cut to the same silhouette as the outer frame. A
    // rectangular overlay here used to square off the panel corners and made
    // the system look like mixed CSS cards at small capture sizes.
    sf::ConvexShape wash = makeCutRect(
        position + sf::Vector2f(9.0f, 9.0f),
        {std::max(0.0f, size.x - 18.0f), std::max(0.0f, size.y - 18.0f)},
        9.0f);
    wash.setFillColor(withAlpha(mix(palette::PlateDeep, palette::ArcaneDeep, 0.34f), 68));
    window.draw(wash);

    // A deeper inner shadow all round: a large panel needs more depth than a
    // button before it stops looking like a flat sheet of colour.
    drawInnerShadow(window, position, size, 14.0f, 1.42f, 126);

    // Studs at the four corners instead of the mid-edge pair a plate gets: on a
    // panel the corner is where a real frame would be fastened.
    if (size.x >= 120.0f && size.y >= 90.0f)
    {
        constexpr float StudInset = 15.0f;
        const sf::Color studColor = withAlpha(palette::Brass, 205);
        drawRivet(window, {position.x + StudInset, position.y + StudInset}, 3.1f, studColor);
        drawRivet(window, {position.x + size.x - StudInset, position.y + StudInset}, 3.1f, studColor);
        drawRivet(window, {position.x + StudInset, position.y + size.y - StudInset}, 3.1f, studColor);
        drawRivet(
            window,
            {position.x + size.x - StudInset, position.y + size.y - StudInset},
            3.1f,
            studColor);
    }

    const float frameRailWidth = std::max(0.0f, size.x - 56.0f);
    drawLine(
        window,
        {position.x + 28.0f, position.y + 7.0f},
        {frameRailWidth, 1.0f},
        withAlpha(palette::BrassPale, 84));
    drawLine(
        window,
        {position.x + 28.0f, position.y + size.y - 15.0f},
        {frameRailWidth, 1.0f},
        withAlpha(palette::BrassBright, 92));
    if (size.x >= 170.0f)
    {
        drawStud(
            window,
            {position.x + size.x * 0.5f, position.y + 7.0f},
            2.5f,
            withAlpha(palette::SteelBright, 180));
    }
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
    style.fill = selected ? palette::SurfaceWarm
                          : (hovered ? palette::SurfaceTop : palette::Plate);
    style.frame = selected ? BrassBright : (hovered ? Brass : BrassDim);
    style.cut = 5.0f;
    style.state = selected ? PlateState::Selected : (hovered ? PlateState::Hover : PlateState::Normal);
    style.rivets = false;
    style.brackets = false;
    style.sheen = hovered || selected ? 0.82f : 0.42f;
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
    else if (hovered)
    {
        drawVerticalGradient(
            window,
            {{position.x + 3.0f, position.y + 6.0f}, {2.0f, std::max(0.0f, size.y - 12.0f)}},
            withAlpha(palette::BrassBright, 178),
            withAlpha(palette::Brass, 84));
    }

    if ((selected || hovered) && size.x >= 96.0f)
    {
        drawDiamond(
            window,
            {position.x + size.x - 14.0f, position.y + size.y * 0.5f},
            6.0f,
            10.0f,
            withAlpha(selected ? palette::BrassGlow : palette::Brass, selected ? 220 : 150),
            withAlpha(palette::BrassShadow, 220));
    }

    constexpr float RowTextInset = 24.0f;
    drawText(
        window,
        font,
        primary,
        type::Body,
        {position.x + RowTextInset, position.y + 7.0f},
        selected || hovered ? palette::InkBright : palette::Ink,
        size.x - RowTextInset - ((selected || hovered) ? 32.0f : 16.0f));
    if (!secondary.empty())
    {
        drawText(
            window,
            font,
            secondary,
            type::Caption,
            {position.x + RowTextInset, position.y + 25.0f},
            selected ? palette::BrassPale : palette::InkMuted,
            size.x - RowTextInset - ((selected || hovered) ? 32.0f : 16.0f));
    }

    if (size.y >= 32.0f)
    {
        drawLine(
            window,
            {position.x + 18.0f, position.y + size.y - 1.0f},
            {std::max(0.0f, size.x - 36.0f), 1.0f},
            withAlpha(selected ? palette::Brass : palette::BrassDim, selected ? 118 : 72));
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

void drawToggleSwitch(
    sf::RenderWindow& window, const sf::Font& font, sf::FloatRect bounds,
    bool on, bool hovered, bool focused)
{
    const sf::Vector2f position = bounds.position;
    const sf::Vector2f size = bounds.size;
    drawMaterialPlate(window, position, size, PlateStyle{
        .fill = on ? palette::ArcaneDeep : palette::PlateDeep,
        .frame = hovered || focused ? palette::SteelBright : palette::SteelDim,
        .cut = 5.0f,
        .state = hovered ? PlateState::Hover : PlateState::Normal,
        .sheen = 0.25f,
        .rivets = false,
        .brackets = false,
        .recessed = true,
        .focused = focused});

    const sf::Vector2f trackPosition = position + sf::Vector2f(8.0f, (size.y - 20.0f) * 0.5f);
    const sf::Vector2f trackSize(42.0f, 20.0f);
    drawInsetSlot(window, trackPosition, trackSize, 5.0f,
        on ? palette::JewelDeep : palette::PlateDeep,
        on ? palette::ArcaneBright : palette::SteelEdge, false, false);
    const sf::Vector2f thumbPosition = trackPosition + sf::Vector2f(on ? 24.0f : 2.0f, 2.0f);
    drawBeveledPlate(window, thumbPosition, {16.0f, 16.0f},
        on ? palette::Arcane : palette::SteelDim,
        on ? palette::JewelBright : palette::SteelBright, hovered || focused, 3.0f);
    sf::Text stateLabel(font, on ? "ON" : "OFF", 13);
    stateLabel.setFillColor(on ? palette::JewelBright : palette::SteelBright);
    centerButtonText(stateLabel, {position.x + 72.0f, position.y + size.y * 0.5f});
    drawCrispText(window, stateLabel);
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
