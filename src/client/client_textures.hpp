#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace bayou::client
{

class TextureStore
{
public:
    sf::Texture* load(const std::string& assetPath);

private:
    std::unordered_map<std::string, std::shared_ptr<sf::Texture>> cache;
};

void drawCoverSprite(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::FloatRect target,
    sf::Color color = sf::Color::White);
void drawContainSprite(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::FloatRect target,
    sf::Color color = sf::Color::White,
    bool flipX = false);
void drawTextureRectContain(
    sf::RenderWindow& window,
    sf::Texture& texture,
    sf::IntRect textureRect,
    sf::FloatRect target,
    sf::Color color = sf::Color::White,
    bool flipX = false);
void drawBackdrop(sf::RenderWindow& window, sf::Texture* backdropTexture);
// Darkens the window edges so a full-bleed backdrop stops competing with the
// interface. Draw it after drawBackdrop and before any UI.
void drawVignette(sf::RenderWindow& window, float strength = 1.0f);
// Drifting arcane motes over the backdrop: the cheapest way to stop a static
// screen looking like a screenshot. Deterministic in `timeSeconds`, so a capture
// of the same moment always matches.
void drawAmbientMotes(
    sf::RenderWindow& window,
    float timeSeconds,
    int count = 34,
    sf::Color color = sf::Color(186, 142, 228, 150),
    float scale = 1.0f);

} // namespace bayou::client
