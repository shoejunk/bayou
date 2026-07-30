#include "client_textures.hpp"

#include "client_config.hpp"
#include "client_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

namespace bayou::client
{

sf::Texture* TextureStore::load(const std::string& assetPath)
{
    const std::string key = assetRelativePath(assetPath);
    if (key.empty())
    {
        return nullptr;
    }

    if (const auto found = cache.find(key); found != cache.end())
    {
        return found->second.get();
    }

    const std::optional<std::filesystem::path> resolvedPath = resolveAssetPath(key);
    auto texture = std::make_shared<sf::Texture>();
    if (!resolvedPath || !texture->loadFromFile(*resolvedPath))
    {
        cache.emplace(key, nullptr);
        return nullptr;
    }

    texture->setSmooth(true);
    sf::Texture* loaded = texture.get();
    cache.emplace(key, std::move(texture));
    return loaded;
}

void drawCoverSprite(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::FloatRect target,
    sf::Color color)
{
    sf::Sprite sprite(texture);
    const sf::Vector2u imageSize = texture.getSize();
    const float scale = std::max(target.size.x / static_cast<float>(imageSize.x),
                                 target.size.y / static_cast<float>(imageSize.y));
    sprite.setScale({scale, scale});
    sprite.setColor(color);
    sprite.setPosition({
        target.position.x + (target.size.x - static_cast<float>(imageSize.x) * scale) * 0.5f,
        target.position.y + (target.size.y - static_cast<float>(imageSize.y) * scale) * 0.5f});
    window.draw(sprite);
}

void drawContainSprite(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::FloatRect target,
    sf::Color color,
    bool flipX)
{
    sf::Sprite sprite(texture);
    const sf::Vector2u imageSize = texture.getSize();
    const float sourceWidth = static_cast<float>(imageSize.x);
    const float sourceHeight = static_cast<float>(imageSize.y);
    const float scale = std::min(target.size.x / static_cast<float>(imageSize.x),
                                 target.size.y / static_cast<float>(imageSize.y));
    sprite.setScale({flipX ? -scale : scale, scale});
    sprite.setColor(color);
    sprite.setPosition({
        target.position.x + (target.size.x + (flipX ? sourceWidth * scale : -sourceWidth * scale)) * 0.5f,
        target.position.y + (target.size.y - sourceHeight * scale) * 0.5f});
    window.draw(sprite);
}

void drawTextureRectContain(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::IntRect textureRect,
    sf::FloatRect target,
    sf::Color color,
    bool flipX)
{
    sf::Sprite sprite(texture);
    sprite.setTextureRect(textureRect);
    const float sourceWidth = static_cast<float>(textureRect.size.x);
    const float sourceHeight = static_cast<float>(textureRect.size.y);
    const float scale = std::min(target.size.x / sourceWidth, target.size.y / sourceHeight);
    sprite.setScale({flipX ? -scale : scale, scale});
    sprite.setColor(color);
    sprite.setPosition({
        target.position.x + (target.size.x + (flipX ? sourceWidth * scale : -sourceWidth * scale)) * 0.5f,
        target.position.y + (target.size.y - sourceHeight * scale) * 0.5f});
    window.draw(sprite);
}

namespace
{

// Deterministic scatter. A real RNG would make every captured frame differ and
// every mote pop in at the origin on the first frame; hashing the index instead
// gives a stable, well-spread field that is already correct at t = 0.
float hashUnit(unsigned int seed)
{
    seed = seed * 1664525u + 1013904223u;
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    return static_cast<float>(seed & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

void runFullWindowView(sf::RenderWindow& window, const std::function<void(sf::Vector2f)>& body)
{
    const sf::View logicalView = window.getView();
    const sf::Vector2u windowSize = window.getSize();
    const sf::Vector2f fullSize{
        static_cast<float>(std::max(windowSize.x, 1u)),
        static_cast<float>(std::max(windowSize.y, 1u))};
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, fullSize)));
    body(fullSize);
    window.setView(logicalView);
}

} // namespace

void drawVignette(sf::RenderWindow& window, float strength)
{
    runFullWindowView(window, [&](sf::Vector2f fullSize) {
        // Four edge gradients rather than a radial texture: darkening the frame
        // is what stops a full-bleed backdrop from competing with the UI, and
        // the corners double up naturally where the bands overlap.
        const auto band = static_cast<std::uint8_t>(
            std::clamp(std::lround(150.0f * strength), 0L, 255L));
        const sf::Color dark(0, 0, 0, band);
        const sf::Color clear(0, 0, 0, 0);

        const float vertical = fullSize.y * 0.30f;
        const float horizontal = fullSize.x * 0.24f;

        auto gradient = [&](sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d,
                            sf::Color edge) {
            sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
            quad[0] = {a, edge};
            quad[1] = {b, edge};
            quad[2] = {c, clear};
            quad[3] = {d, clear};
            window.draw(quad);
        };

        gradient({0.0f, 0.0f}, {fullSize.x, 0.0f}, {0.0f, vertical}, {fullSize.x, vertical}, dark);
        gradient(
            {0.0f, fullSize.y},
            {fullSize.x, fullSize.y},
            {0.0f, fullSize.y - vertical},
            {fullSize.x, fullSize.y - vertical},
            dark);
        gradient({0.0f, 0.0f}, {0.0f, fullSize.y}, {horizontal, 0.0f}, {horizontal, fullSize.y}, dark);
        gradient(
            {fullSize.x, 0.0f},
            {fullSize.x, fullSize.y},
            {fullSize.x - horizontal, 0.0f},
            {fullSize.x - horizontal, fullSize.y},
            dark);
    });
}

void drawAmbientMotes(
    sf::RenderWindow& window,
    float timeSeconds,
    int count,
    sf::Color color,
    float scale)
{
    if (count <= 0)
    {
        return;
    }

    runFullWindowView(window, [&](sf::Vector2f fullSize) {
        for (int i = 0; i < count; ++i)
        {
            const auto seed = static_cast<unsigned int>(i);
            const float baseX = hashUnit(seed * 3u + 1u);
            const float baseY = hashUnit(seed * 7u + 13u);
            const float speed = 0.010f + hashUnit(seed * 11u + 29u) * 0.028f;
            const float sway = hashUnit(seed * 17u + 41u);
            const float radius = (1.4f + hashUnit(seed * 23u + 53u) * 3.1f) * scale;

            // Drift upward and wrap; the sway keeps the columns from marching.
            float y = baseY - timeSeconds * speed;
            y -= std::floor(y);
            const float x = baseX + std::sin(timeSeconds * (0.32f + sway * 0.5f) + sway * 6.28f) * 0.012f;

            const sf::Vector2f center{
                (x - std::floor(x)) * fullSize.x,
                y * fullSize.y};

            // Twinkle so the field has life even in a still frame.
            const float twinkle =
                0.45f + 0.55f * (0.5f + 0.5f * std::sin(timeSeconds * (1.1f + sway * 1.7f) + sway * 9.4f));
            const auto coreAlpha =
                static_cast<std::uint8_t>(std::clamp(std::lround(color.a * twinkle), 0L, 255L));

            sf::Color halo = color;
            halo.a = static_cast<std::uint8_t>(coreAlpha / 5);
            drawRadialGlow(window, center, radius * 4.2f, halo, 12);

            sf::CircleShape mote(radius, 10);
            mote.setOrigin({radius, radius});
            mote.setPosition(center);
            mote.setFillColor(sf::Color(color.r, color.g, color.b, coreAlpha));
            window.draw(mote);
        }
    });
}

void drawBackdrop(sf::RenderWindow& window, sf::Texture* backdropTexture)
{
    // The interface is laid out in a 4:3 logical space that gets letterboxed on
    // a wider display. Painting the backdrop through a full-window view instead
    // fills those margins, so the art bleeds to the screen edges rather than
    // leaving black bars beside the UI.
    const sf::View logicalView = window.getView();
    const sf::Vector2u windowSize = window.getSize();
    const sf::Vector2f fullSize{
        static_cast<float>(std::max(windowSize.x, 1u)),
        static_cast<float>(std::max(windowSize.y, 1u))};

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, fullSize)));

    if (backdropTexture)
    {
        drawCoverSprite(window, *backdropTexture, {{0.0f, 0.0f}, fullSize});
    }
    else
    {
        window.clear(sf::Color(9, 17, 19));
    }

    sf::RectangleShape wash(fullSize);
    wash.setFillColor(sf::Color(3, 8, 10, 145));
    window.draw(wash);

    sf::RectangleShape bottomShade({fullSize.x, fullSize.y * 0.16f});
    bottomShade.setPosition({0.0f, fullSize.y * 0.84f});
    bottomShade.setFillColor(sf::Color(2, 5, 6, 92));
    window.draw(bottomShade);

    window.setView(logicalView);

    // Every screen wants the edges pulled down: without it a full-bleed
    // backdrop fights the interface for attention at the frame's corners.
    drawVignette(window, 0.85f);
}

} // namespace bayou::client
