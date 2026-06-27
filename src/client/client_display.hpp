#pragma once

#include "client_config.hpp"

#include <SFML/Graphics.hpp>

#include <cstddef>
#include <vector>

namespace bayou::client
{

std::vector<sf::Vector2u> availableDisplayResolutions(
    const sf::VideoMode& desktopMode,
    const std::vector<sf::VideoMode>& fullscreenModes);
void normalizeDisplaySettings(
    DisplaySettings& settings,
    sf::Vector2u desktopSize,
    const std::vector<sf::Vector2u>& resolutions);
std::size_t displayResolutionIndex(
    const std::vector<sf::Vector2u>& resolutions,
    sf::Vector2u size);
void applyLogicalView(sf::RenderWindow& window);
void createDisplayWindow(
    sf::RenderWindow& window,
    DisplaySettings& settings,
    const sf::VideoMode& desktopMode,
    const std::vector<sf::VideoMode>& fullscreenModes);

} // namespace bayou::client
