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

} // namespace bayou::client
