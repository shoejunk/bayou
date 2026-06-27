#include "client_display.hpp"

#include <algorithm>
#include <cstdint>

namespace bayou::client
{
namespace
{
constexpr float LogicalWidth = 800.0f;
constexpr float LogicalHeight = 600.0f;

void addResolution(
    std::vector<sf::Vector2u>& resolutions,
    sf::Vector2u size,
    sf::Vector2u desktopSize)
{
    if (size.x < 1024u || size.y < 720u || size.x > desktopSize.x || size.y > desktopSize.y)
    {
        return;
    }
    if (std::find(resolutions.begin(), resolutions.end(), size) == resolutions.end())
    {
        resolutions.push_back(size);
    }
}

} // namespace

std::vector<sf::Vector2u> availableDisplayResolutions(
    const sf::VideoMode& desktopMode,
    const std::vector<sf::VideoMode>& fullscreenModes)
{
    std::vector<sf::Vector2u> resolutions;
    addResolution(resolutions, {1024, 768}, desktopMode.size);
    addResolution(resolutions, {1280, 720}, desktopMode.size);
    addResolution(resolutions, {1280, 800}, desktopMode.size);
    addResolution(resolutions, {1366, 768}, desktopMode.size);
    addResolution(resolutions, {1600, 900}, desktopMode.size);
    addResolution(resolutions, {1920, 1080}, desktopMode.size);
    addResolution(resolutions, {2560, 1440}, desktopMode.size);
    addResolution(resolutions, desktopMode.size, desktopMode.size);
    for (const sf::VideoMode& mode : fullscreenModes)
    {
        addResolution(resolutions, mode.size, desktopMode.size);
    }

    std::sort(resolutions.begin(), resolutions.end(), [](sf::Vector2u left, sf::Vector2u right) {
        const std::uint64_t leftPixels = static_cast<std::uint64_t>(left.x) * left.y;
        const std::uint64_t rightPixels = static_cast<std::uint64_t>(right.x) * right.y;
        return leftPixels == rightPixels ? left.x < right.x : leftPixels < rightPixels;
    });
    return resolutions;
}

void normalizeDisplaySettings(
    DisplaySettings& settings,
    sf::Vector2u desktopSize,
    const std::vector<sf::Vector2u>& resolutions)
{
    if (settings.width == 0 || settings.height == 0)
    {
        settings.width = desktopSize.x;
        settings.height = desktopSize.y;
    }

    const sf::Vector2u configuredSize{settings.width, settings.height};
    if (std::find(resolutions.begin(), resolutions.end(), configuredSize) == resolutions.end())
    {
        settings.width = desktopSize.x;
        settings.height = desktopSize.y;
    }
}

std::size_t displayResolutionIndex(
    const std::vector<sf::Vector2u>& resolutions,
    sf::Vector2u size)
{
    const auto found = std::find(resolutions.begin(), resolutions.end(), size);
    return found == resolutions.end()
        ? resolutions.size() - 1
        : static_cast<std::size_t>(std::distance(resolutions.begin(), found));
}

void applyLogicalView(sf::RenderWindow& window)
{
    const sf::Vector2u windowSize = window.getSize();
    if (windowSize.x == 0 || windowSize.y == 0)
    {
        return;
    }

    const float windowAspect = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
    const float logicalAspect = LogicalWidth / LogicalHeight;
    sf::FloatRect viewport({0.0f, 0.0f}, {1.0f, 1.0f});
    if (windowAspect > logicalAspect)
    {
        viewport.size.x = logicalAspect / windowAspect;
        viewport.position.x = (1.0f - viewport.size.x) * 0.5f;
    }
    else if (windowAspect < logicalAspect)
    {
        viewport.size.y = windowAspect / logicalAspect;
        viewport.position.y = (1.0f - viewport.size.y) * 0.5f;
    }

    sf::View view(sf::FloatRect({0.0f, 0.0f}, {LogicalWidth, LogicalHeight}));
    view.setViewport(viewport);
    window.setView(view);
}

void createDisplayWindow(
    sf::RenderWindow& window,
    DisplaySettings& settings,
    const sf::VideoMode& desktopMode,
    const std::vector<sf::VideoMode>& fullscreenModes)
{
    sf::VideoMode mode({settings.width, settings.height});
    if (settings.fullscreen)
    {
        const auto matchingMode = std::find_if(fullscreenModes.begin(), fullscreenModes.end(), [&](const sf::VideoMode& candidate) {
            return candidate.size == mode.size;
        });
        mode = matchingMode != fullscreenModes.end() ? *matchingMode : desktopMode;
        settings.width = mode.size.x;
        settings.height = mode.size.y;
        window.create(mode, "Steam Tactics", sf::State::Fullscreen);
    }
    else
    {
        window.create(mode, "Steam Tactics", sf::Style::Titlebar | sf::Style::Close, sf::State::Windowed);
    }
    window.setFramerateLimit(60);
    applyLogicalView(window);
}

} // namespace bayou::client
