#include "client_textures.hpp"

#include "client_config.hpp"

#include <algorithm>
#include <filesystem>
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
}

} // namespace bayou::client
