#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include "client_board_layout.hpp"
#include "client_card_text.hpp"
#include "client_config.hpp"
#include "client_display.hpp"
#include "client_sandbox.hpp"
#include "client_string.hpp"
#include "client_textures.hpp"
#include "client_ui.hpp"
#include "deck_collection.hpp"

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/game_data.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

import button;
import card_editor_screen;
import client_controls;
import client_services;
import inputbox;
import network;

namespace
{
using namespace bayou::client;

enum class AudioCue
{
    ButtonClick,
    PiecePlace,
    UnitMove,
    UnitAttack,
    UnitDeath,
    Dematerialize,
    Victory,
    Defeat
};

class AudioSystem
{
public:
    AudioSystem()
    {
        makeEffects();
        startMusic();
    }

    void play(AudioCue cue, float volumeScale = 1.0f)
    {
        if (allMuted || soundEffectsMuted)
        {
            return;
        }

        trimStoppedSounds();
        sf::SoundBuffer& buffer = effectBuffers[static_cast<std::size_t>(cue)];
        sf::Sound& sound = activeSounds.emplace_back(buffer);
        sound.setVolume(effectVolume(cue) * volumeScale * allVolume * soundEffectsVolume);
        sound.play();
    }

    void update()
    {
        trimStoppedSounds();
        updateMusicVolume();
        if (music && music->getStatus() != sf::SoundSource::Status::Playing)
        {
            music->play();
        }
    }

    void setAllVolume(float volume)
    {
        allVolume = std::clamp(volume, 0.0f, 1.0f);
        updateMusicVolume();
    }

    float getAllVolume() const
    {
        return allVolume;
    }

    void setMusicVolume(float volume)
    {
        musicVolume = std::clamp(volume, 0.0f, 1.0f);
        updateMusicVolume();
    }

    float getMusicVolume() const
    {
        return musicVolume;
    }

    void setSoundEffectsVolume(float volume)
    {
        soundEffectsVolume = std::clamp(volume, 0.0f, 1.0f);
    }

    float getSoundEffectsVolume() const
    {
        return soundEffectsVolume;
    }

    void setAllMuted(bool muted)
    {
        allMuted = muted;
        if (allMuted)
        {
            stopActiveSounds();
        }
        updateMusicVolume();
    }

    bool isAllMuted() const
    {
        return allMuted;
    }

    void setMusicMuted(bool muted)
    {
        musicMuted = muted;
        updateMusicVolume();
    }

    bool isMusicMuted() const
    {
        return musicMuted;
    }

    void setSoundEffectsMuted(bool muted)
    {
        soundEffectsMuted = muted;
        if (soundEffectsMuted)
        {
            stopActiveSounds();
        }
    }

    bool isSoundEffectsMuted() const
    {
        return soundEffectsMuted;
    }

private:
    static constexpr unsigned int SampleRate = 44100;
    static constexpr int EffectCount = 8;
    std::array<sf::SoundBuffer, EffectCount> effectBuffers;
    std::unique_ptr<sf::Music> music;
    std::list<sf::Sound> activeSounds;
    float allVolume = 1.0f;
    float musicVolume = 1.0f;
    float soundEffectsVolume = 1.0f;
    bool allMuted = false;
    bool musicMuted = false;
    bool soundEffectsMuted = false;

    static float envelope(float t, float duration, float attack, float release)
    {
        if (t < attack)
        {
            return t / attack;
        }
        if (t > duration - release)
        {
            return std::max(0.0f, (duration - t) / release);
        }
        return 1.0f;
    }

    static sf::SoundBuffer bufferFromSamples(const std::vector<std::int16_t>& samples)
    {
        sf::SoundBuffer buffer;
        const bool loaded = buffer.loadFromSamples(samples.data(), samples.size(), 1, SampleRate, {sf::SoundChannel::Mono});
        (void)loaded;
        return buffer;
    }

    static std::vector<std::int16_t> makeTone(
        float duration,
        float startFrequency,
        float endFrequency,
        float volume,
        float noiseAmount = 0.0f)
    {
        const int sampleCount = static_cast<int>(duration * static_cast<float>(SampleRate));
        std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
        std::uint32_t noise = 0x9e3779b9u;
        float phase = 0.0f;

        for (int i = 0; i < sampleCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(SampleRate);
            const float p = duration > 0.0f ? t / duration : 0.0f;
            const float frequency = startFrequency + (endFrequency - startFrequency) * p;
            phase += 2.0f * Pi * frequency / static_cast<float>(SampleRate);
            noise = noise * 1664525u + 1013904223u;
            const float noiseSample = (static_cast<float>((noise >> 16) & 0xffffu) / 32767.5f) - 1.0f;
            const float wave = std::sin(phase) * (1.0f - noiseAmount) + noiseSample * noiseAmount;
            const float amp = wave * volume * envelope(t, duration, 0.008f, std::min(0.12f, duration * 0.42f));
            samples[static_cast<std::size_t>(i)] =
                static_cast<std::int16_t>(std::clamp(amp, -1.0f, 1.0f) * 32767.0f);
        }
        return samples;
    }

    static std::vector<std::int16_t> makeMoveSamples()
    {
        const float duration = 0.34f;
        const int sampleCount = static_cast<int>(duration * static_cast<float>(SampleRate));
        std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
        for (int i = 0; i < sampleCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(SampleRate);
            const float stepPulse = std::sin(2.0f * Pi * 7.0f * t);
            const float body = std::sin(2.0f * Pi * (92.0f + 34.0f * t) * t);
            const float clank = std::sin(2.0f * Pi * 680.0f * t) * std::max(0.0f, stepPulse);
            const float amp = (body * 0.42f + clank * 0.18f) * envelope(t, duration, 0.012f, 0.12f) * 0.38f;
            samples[static_cast<std::size_t>(i)] =
                static_cast<std::int16_t>(std::clamp(amp, -1.0f, 1.0f) * 32767.0f);
        }
        return samples;
    }

    static std::vector<std::int16_t> makePlaceSamples()
    {
        const float duration = 0.26f;
        const int sampleCount = static_cast<int>(duration * static_cast<float>(SampleRate));
        std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
        std::uint32_t noise = 0x85ebca6bu;

        for (int i = 0; i < sampleCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(SampleRate);
            const float p = duration > 0.0f ? t / duration : 0.0f;
            noise = noise * 1664525u + 1013904223u;
            const float noiseSample = (static_cast<float>((noise >> 16) & 0xffffu) / 32767.5f) - 1.0f;
            const float thud = std::sin(2.0f * Pi * (92.0f - 22.0f * p) * t);
            const float clack = std::sin(2.0f * Pi * 520.0f * t) * std::max(0.0f, 1.0f - p * 5.5f);
            const float dust = noiseSample * std::max(0.0f, 1.0f - p * 3.2f);
            const float amp = (thud * 0.54f + clack * 0.24f + dust * 0.12f) *
                envelope(t, duration, 0.004f, 0.15f) * 0.46f;
            samples[static_cast<std::size_t>(i)] =
                static_cast<std::int16_t>(std::clamp(amp, -1.0f, 1.0f) * 32767.0f);
        }

        return samples;
    }

    void makeEffects()
    {
        effectBuffers[static_cast<std::size_t>(AudioCue::ButtonClick)] =
            bufferFromSamples(makeTone(0.075f, 760.0f, 1040.0f, 0.34f));
        sf::SoundBuffer& placeBuffer = effectBuffers[static_cast<std::size_t>(AudioCue::PiecePlace)];
        const std::optional<std::filesystem::path> placePath = resolveAssetPath("audio/place.wav");
        if (!placePath || !placeBuffer.loadFromFile(*placePath))
        {
            placeBuffer = bufferFromSamples(makePlaceSamples());
        }
        effectBuffers[static_cast<std::size_t>(AudioCue::UnitMove)] =
            bufferFromSamples(makeMoveSamples());
        sf::SoundBuffer& attackBuffer = effectBuffers[static_cast<std::size_t>(AudioCue::UnitAttack)];
        const std::optional<std::filesystem::path> attackPath = resolveAssetPath("audio/attack.wav");
        if (!attackPath || !attackBuffer.loadFromFile(*attackPath))
        {
            attackBuffer = bufferFromSamples(makeTone(0.24f, 520.0f, 92.0f, 0.58f, 0.22f));
        }
        sf::SoundBuffer& deathBuffer = effectBuffers[static_cast<std::size_t>(AudioCue::UnitDeath)];
        const std::optional<std::filesystem::path> deathPath = resolveAssetPath("audio/death.wav");
        if (!deathPath || !deathBuffer.loadFromFile(*deathPath))
        {
            deathBuffer = bufferFromSamples(makeTone(0.46f, 180.0f, 42.0f, 0.56f, 0.34f));
        }
        sf::SoundBuffer& dematerializeBuffer =
            effectBuffers[static_cast<std::size_t>(AudioCue::Dematerialize)];
        const std::optional<std::filesystem::path> dematerializePath =
            resolveAssetPath("audio/dematerialize.wav");
        if (!dematerializePath || !dematerializeBuffer.loadFromFile(*dematerializePath))
        {
            // Airy descending shimmer for a piece fading out of sight.
            dematerializeBuffer = bufferFromSamples(makeTone(0.55f, 940.0f, 180.0f, 0.42f, 0.45f));
        }
        sf::SoundBuffer& victoryBuffer = effectBuffers[static_cast<std::size_t>(AudioCue::Victory)];
        const std::optional<std::filesystem::path> victoryPath = resolveAssetPath("audio/victory.wav");
        if (!victoryPath || !victoryBuffer.loadFromFile(*victoryPath))
        {
            victoryBuffer = bufferFromSamples(makeTone(0.6f, 440.0f, 880.0f, 0.4f, 0.2f));
        }
        sf::SoundBuffer& defeatBuffer = effectBuffers[static_cast<std::size_t>(AudioCue::Defeat)];
        const std::optional<std::filesystem::path> defeatPath = resolveAssetPath("audio/defeat.wav");
        if (!defeatPath || !defeatBuffer.loadFromFile(*defeatPath))
        {
            defeatBuffer = bufferFromSamples(makeTone(0.6f, 220.0f, 80.0f, 0.4f, 0.3f));
        }
    }

    void startMusic()
    {
        const std::optional<std::filesystem::path> musicPath = resolveAssetPath("audio/midnight-carnival-veil.wav");
        if (!musicPath)
        {
            return;
        }

        auto loadedMusic = std::make_unique<sf::Music>();
        if (!loadedMusic->openFromFile(*musicPath))
        {
            return;
        }

        loadedMusic->setLooping(true);
        loadedMusic->setVolume(22.0f);
        loadedMusic->play();
        music = std::move(loadedMusic);
    }

    void updateMusicVolume()
    {
        if (music)
        {
            music->setVolume((allMuted || musicMuted) ? 0.0f : 22.0f * allVolume * musicVolume);
        }
    }

    void trimStoppedSounds()
    {
        for (auto sound = activeSounds.begin(); sound != activeSounds.end();)
        {
            if (sound->getStatus() == sf::SoundSource::Status::Stopped)
            {
                sound = activeSounds.erase(sound);
            }
            else
            {
                ++sound;
            }
        }
    }

    void stopActiveSounds()
    {
        for (sf::Sound& sound : activeSounds)
        {
            sound.stop();
        }
        trimStoppedSounds();
    }

    static float effectVolume(AudioCue cue)
    {
        switch (cue)
        {
            case AudioCue::ButtonClick: return 38.0f;
            case AudioCue::PiecePlace: return 52.0f;
            case AudioCue::UnitMove: return 44.0f;
            case AudioCue::UnitAttack: return 78.0f;
            case AudioCue::UnitDeath: return 45.0f;
            case AudioCue::Dematerialize: return 50.0f;
            case AudioCue::Victory: return 35.0f;
            case AudioCue::Defeat: return 35.0f;
        }
        return 50.0f;
    }
};

AudioSystem* activeAudioSystem = nullptr;

void playButtonClickSound()
{
    if (activeAudioSystem)
    {
        activeAudioSystem->play(AudioCue::ButtonClick);
    }
}

constexpr bool EnableCoinPurchases = false;
constexpr const char* CoinPackId = "coins_50";
constexpr int CoinPackCoins = 50;
constexpr float CoinPurchasePollIntervalSeconds = 2.0f;
constexpr float CoinPurchasePollTimeoutSeconds = 300.0f;
#ifdef NDEBUG
constexpr const char* ClientConfigFileName = "client_release.cfg";
#else
constexpr const char* ClientConfigFileName = "client_debug.cfg";
#endif

constexpr float DeckPickerPanelX = 220.0f;
constexpr float DeckPickerPanelY = 96.0f;
constexpr float DeckPickerPanelWidth = 360.0f;
constexpr float DeckPickerPanelHeight = 400.0f;
constexpr float DeckPanelX = 24.0f;
constexpr float CurrentDeckPanelX = 24.0f;
constexpr float CurrentDeckPanelWidth = 360.0f;
constexpr float LibraryPanelX = 410.0f;
constexpr float LibraryPanelWidth = 366.0f;
constexpr float DeckEditorPanelY = 96.0f;
constexpr float DeckEditorPanelHeight = 400.0f;
constexpr float DeckListX = 244.0f;
constexpr float DeckListY = 194.0f;
constexpr float DeckListWidth = 312.0f;
constexpr float DeckRowHeight = 42.0f;
constexpr std::size_t VisibleDeckRows = 7;

constexpr float DeckCardsX = 42.0f;
constexpr float DeckCardsY = 164.0f;
constexpr float DeckCardsWidth = 324.0f;
constexpr float DeckCardRowHeight = 40.0f;
constexpr std::size_t VisibleDeckCardRows = 8;
constexpr float PasswordIconInset = 42.0f;
constexpr std::uint32_t AdminUsersPageSize = 6;
constexpr float AdminUserRowY = 174.0f;
constexpr float AdminUserRowHeight = 43.0f;

constexpr float LibraryX = 430.0f;
constexpr float LibraryY = 276.0f;
constexpr float LibraryWidth = 326.0f;
constexpr float LibraryRowHeight = 40.0f;
constexpr std::size_t VisibleLibraryRows = 5;
constexpr std::array<const char*, 3> CollectionTypeLabels = {"Heroes", "Units", "Spells"};
constexpr std::array<const char*, 4> CollectionKeywordLabels = {"bio", "mechanical", "occult", "riffraff"};

struct PasswordVisibilityIcon
{
    sf::FloatRect fieldBounds;
    sf::Texture* showTexture = nullptr;
    sf::Texture* hideTexture = nullptr;
    bool hovered = false;

    PasswordVisibilityIcon() = default;

    PasswordVisibilityIcon(sf::FloatRect bounds, sf::Texture* showIcon, sf::Texture* hideIcon)
        : fieldBounds(bounds), showTexture(showIcon), hideTexture(hideIcon)
    {
    }

    sf::FloatRect bounds() const
    {
        return {{fieldBounds.position.x + fieldBounds.size.x - 38.0f, fieldBounds.position.y + 4.0f},
                {34.0f, fieldBounds.size.y - 8.0f}};
    }

    void update(sf::Vector2f mousePos)
    {
        hovered = bounds().contains(mousePos);
    }

    bool isClicked(sf::Vector2f mousePos) const
    {
        return bounds().contains(mousePos);
    }

    void draw(sf::RenderWindow& window, bool passwordVisible) const
    {
        const sf::FloatRect hitBounds = bounds();
        if (hovered)
        {
            drawBeveledPlate(
                window,
                hitBounds.position,
                hitBounds.size,
                sf::Color(60, 39, 22, 120),
                sf::Color(239, 190, 98, 180),
                true,
                4.0f);
        }

        sf::RectangleShape divider({1.0f, hitBounds.size.y - 10.0f});
        divider.setPosition({hitBounds.position.x - 4.0f, hitBounds.position.y + 5.0f});
        divider.setFillColor(sf::Color(154, 112, 61, hovered ? 190 : 125));
        window.draw(divider);

        sf::Texture* texture = passwordVisible ? hideTexture : showTexture;
        if (!texture)
        {
            return;
        }

        const sf::FloatRect iconTarget{{hitBounds.position.x + 5.0f, hitBounds.position.y + 4.0f},
                                       {hitBounds.size.x - 10.0f, hitBounds.size.y - 8.0f}};
        drawContainSprite(
            window,
            *texture,
            iconTarget,
            hovered ? sf::Color(255, 244, 215) : sf::Color(238, 212, 159, 232));
    }
};

struct CheckboxControl
{
    sf::RectangleShape box;
    sf::Text label;
    sf::Texture* checkTexture = nullptr;
    bool hovered = false;

    CheckboxControl(
        sf::Vector2f position,
        const std::string& labelText,
        sf::Font& font,
        sf::Texture* checkmarkTexture,
        unsigned int labelSize = 18,
        float boxSize = 24.0f,
        float labelOffset = 36.0f)
        : label(font, labelText, labelSize)
        , checkTexture(checkmarkTexture)
    {
        box.setPosition(position);
        box.setSize({boxSize, boxSize});
        box.setFillColor(sf::Color(8, 13, 14, 236));
        box.setOutlineThickness(2.0f);
        box.setOutlineColor(sf::Color(154, 101, 49));

        label.setFillColor(sf::Color(246, 232, 200));
        label.setPosition({position.x + labelOffset, position.y - 1.0f});
    }

    sf::FloatRect bounds() const
    {
        const sf::FloatRect boxBounds = box.getGlobalBounds();
        const sf::FloatRect labelBounds = label.getGlobalBounds();
        const float left = std::min(boxBounds.position.x, labelBounds.position.x);
        const float top = std::min(boxBounds.position.y, labelBounds.position.y);
        const float right = std::max(boxBounds.position.x + boxBounds.size.x, labelBounds.position.x + labelBounds.size.x);
        const float bottom = std::max(boxBounds.position.y + boxBounds.size.y, labelBounds.position.y + labelBounds.size.y);
        return {{left, top}, {right - left, bottom - top}};
    }

    void update(sf::Vector2f mousePos)
    {
        hovered = bounds().contains(mousePos);
        box.setOutlineColor(hovered ? sf::Color(239, 190, 98) : sf::Color(154, 112, 61));
        label.setFillColor(hovered ? sf::Color(255, 244, 215) : sf::Color(246, 232, 200));
    }

    bool isClicked(sf::Vector2f mousePos) const
    {
        return bounds().contains(mousePos);
    }

    void draw(sf::RenderWindow& window, bool checked) const
    {
        drawBeveledPlate(
            window,
            box.getPosition(),
            box.getSize(),
            checked ? sf::Color(63, 43, 24, 238) : sf::Color(8, 13, 14, 236),
            hovered || checked ? sf::Color(239, 190, 98) : sf::Color(154, 101, 49),
            hovered || checked,
            4.0f);

        if (checked)
        {
            const sf::Vector2f position = box.getPosition();
            if (checkTexture)
            {
                const sf::Vector2f boxSize = box.getSize();
                drawContainSprite(
                    window,
                    *checkTexture,
                    {{position.x - boxSize.x * 0.08f, position.y + boxSize.y * 0.04f},
                     {boxSize.x * 1.17f, boxSize.y * 0.92f}},
                    hovered ? sf::Color(255, 244, 215) : sf::Color::White);
            }
        }

        window.draw(label);
    }
};

enum class GameState
{
    Menu,
    SandboxLoading,
    Options,
    Login,
    CreateAccount,
    ChangePassword,
    Authenticated,
    StoryIntro,
    DeckSelect,
    Matchmaking,
    DeckEditor,
    Shop,
    AdminUsers,
    CardEditor,
    Game
};

enum class OptionsTab
{
    Graphics,
    Audio,
    Account
};

enum class DeckEditorMode
{
    DeckList,
    EditDeck
};

constexpr float GameLabelY = 44.0f;
constexpr float GameActionButtonY = 14.0f;
constexpr float HandY = 512.0f;
constexpr float HandCardWidth = 88.0f;
constexpr float HandCardHeight = 78.0f;
constexpr float HandGap = 6.0f;
constexpr float HandStartX = 28.0f;
constexpr std::size_t VisibleGameHandCards = 7;
constexpr float TrashCanX = 712.0f;
constexpr float TrashCanY = 516.0f;
constexpr float TrashCanSize = 68.0f;
constexpr float TrashCanDropPadding = 14.0f;
constexpr float PiecePopupX = 150.0f;
constexpr float PiecePopupY = 92.0f;
constexpr float PiecePopupWidth = 500.0f;
constexpr float PiecePopupHeight = 416.0f;
constexpr float PiecePopupTextX = PiecePopupX + 24.0f;
constexpr float PiecePopupTextWidth = PiecePopupWidth - 48.0f;
constexpr float PiecePopupActionHeadingY = PiecePopupY + 186.0f;
constexpr float PiecePopupScrollY = PiecePopupActionHeadingY + 26.0f;
constexpr float PiecePopupScrollHeight = PiecePopupHeight - (PiecePopupScrollY - PiecePopupY) - 66.0f;
constexpr float PiecePopupScrollTextXInset = 24.0f;
constexpr float PiecePopupScrollTextYInset = 14.0f;
constexpr float PieceDoubleClickSeconds = 0.38f;
constexpr float DeckCardDoubleClickSeconds = 0.38f;
constexpr float GameDragStartDistanceSquared = 36.0f;
std::string urlEncode(const std::string& value)
{
    static constexpr char Hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);

    for (unsigned char ch : value)
    {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            encoded.push_back(static_cast<char>(ch));
        }
        else
        {
            encoded.push_back('%');
            encoded.push_back(Hex[ch >> 4]);
            encoded.push_back(Hex[ch & 0x0f]);
        }
    }

    return encoded;
}

std::string coinCheckoutUrl(const std::string& username)
{
    const std::string baseUrl = stripTrailingSlashes(clientConfig().paymentServerUrl);
    return baseUrl + "/checkout?username=" + urlEncode(username) + "&pack=" + urlEncode(CoinPackId);
}

#ifndef _WIN32
std::string shellQuote(const std::string& value)
{
    std::string quoted = "'";
    for (char ch : value)
    {
        if (ch == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}
#endif

bool openExternalUrl(const std::string& url)
{
#ifdef _WIN32
    const auto result = reinterpret_cast<std::intptr_t>(
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
#elif defined(__APPLE__)
    return std::system(("open " + shellQuote(url) + " >/dev/null 2>&1 &").c_str()) == 0;
#else
    return std::system(("xdg-open " + shellQuote(url) + " >/dev/null 2>&1 &").c_str()) == 0;
#endif
}

void resetForm(InputBox& usernameInput, InputBox& passwordInput, InputBox& confirmInput, sf::Text& messageText)
{
    usernameInput.clear();
    passwordInput.clear();
    confirmInput.clear();
    setMessage(messageText, "", sf::Color::Red);
}
}

int main(int argc, char** argv)
{
    setExecutableDirectory(argc > 0 ? argv[0] : nullptr);

    const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    const std::vector<sf::VideoMode>& fullscreenModes = sf::VideoMode::getFullscreenModes();
    std::vector<sf::Vector2u> displayResolutions =
        availableDisplayResolutions(desktopMode, fullscreenModes);

    DisplaySettings displaySettings = loadDisplaySettings();
    normalizeDisplaySettings(displaySettings, desktopMode.size, displayResolutions);

    sf::RenderWindow window;
    createDisplayWindow(window, displaySettings, desktopMode, fullscreenModes);

    sf::Font font;
    const std::optional<std::filesystem::path> fontPath = resolveAssetPath("Roboto.ttf");
    if (!fontPath || !font.openFromFile(*fontPath))
    {
        return 1;
    }

    TextureStore textures;
    sf::Texture* backdropTexture = textures.load("ui/steampunk-bayou-backdrop.png");
    sf::Texture* showPasswordTexture = textures.load("ui/password-eye-open.png");
    sf::Texture* hidePasswordTexture = textures.load("ui/password-eye-off.png");
    sf::Texture* rememberCheckTexture = textures.load("ui/remember-checkmark.png");

    sf::Text title(font, "Steam Tactics", 48);
    title.setFillColor(sf::Color(248, 224, 172));
    title.setPosition({400.0f, 45.0f});
    centerText(title, 400.0f);

    Button loginButton({300.0f, 200.0f}, {200.0f, 60.0f}, "Login", font);
    Button createButton({300.0f, 300.0f}, {200.0f, 60.0f}, "Create Account", font);
    Button menuOptionsButton({300.0f, 400.0f}, {200.0f, 60.0f}, "Options", font);

    InputBox usernameInput({300.0f, 140.0f}, {200.0f, 40.0f}, "Username", font);
    InputBox passwordInput({300.0f, 220.0f}, {200.0f, 40.0f}, "Password", font, true);
    InputBox confirmInput({300.0f, 300.0f}, {200.0f, 40.0f}, "Confirm Password", font, true);
    InputBox currentPasswordInput({300.0f, 150.0f}, {200.0f, 40.0f}, "Current Password", font, true);
    InputBox newPasswordInput({300.0f, 230.0f}, {200.0f, 40.0f}, "New Password", font, true);
    InputBox confirmNewPasswordInput({300.0f, 310.0f}, {200.0f, 40.0f}, "Confirm New Password", font, true);
    passwordInput.setRightContentInset(PasswordIconInset);
    confirmInput.setRightContentInset(PasswordIconInset);
    currentPasswordInput.setRightContentInset(PasswordIconInset);
    newPasswordInput.setRightContentInset(PasswordIconInset);
    confirmNewPasswordInput.setRightContentInset(PasswordIconInset);
    PasswordVisibilityIcon passwordVisibilityIcon(passwordInput.bounds(), showPasswordTexture, hidePasswordTexture);
    PasswordVisibilityIcon confirmVisibilityIcon(confirmInput.bounds(), showPasswordTexture, hidePasswordTexture);
    PasswordVisibilityIcon currentPasswordVisibilityIcon(currentPasswordInput.bounds(), showPasswordTexture, hidePasswordTexture);
    PasswordVisibilityIcon newPasswordVisibilityIcon(newPasswordInput.bounds(), showPasswordTexture, hidePasswordTexture);
    PasswordVisibilityIcon confirmNewPasswordVisibilityIcon(confirmNewPasswordInput.bounds(), showPasswordTexture, hidePasswordTexture);
    InputBox deckNameInput({304.0f, 154.0f}, {210.0f, 40.0f}, "", font);
    InputBox adminSearchInput({120.0f, 94.0f}, {520.0f, 36.0f}, "", font);
    InputBox adminGoldInput({234.0f, 460.0f}, {130.0f, 36.0f}, "Gold amount", font);

    CheckboxControl rememberMeCheckbox({300.0f, 286.0f}, "Remember me", font, rememberCheckTexture);
    Button loginSubmitButton({300.0f, 342.0f}, {200.0f, 50.0f}, "Login", font);
    Button createSubmitButton({300.0f, 380.0f}, {200.0f, 50.0f}, "Create Account", font);
    Button backButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Back", font);
    Button exitDesktopButton({20.0f, 520.0f}, {200.0f, 45.0f}, "Exit to Desktop", font);
    Button cancelMatchmakingButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Cancel", font);
    Button playAiButton({150.0f, 520.0f}, {160.0f, 45.0f}, "Play vs AI", font);
    Button storyButton({300.0f, 136.0f}, {200.0f, 40.0f}, "Story", font);
    Button playButton({300.0f, 184.0f}, {200.0f, 40.0f}, "Play", font);
    Button sandboxButton({300.0f, 184.0f}, {200.0f, 40.0f}, "Sandbox", font);
    Button deckEditorButton({300.0f, 232.0f}, {200.0f, 40.0f}, "Deck Editor", font);
    Button shopButton({300.0f, 280.0f}, {200.0f, 40.0f}, "Shop", font);
    Button adminCardEditorButton({300.0f, 328.0f}, {200.0f, 40.0f}, "Card Editor", font);
    Button adminUsersButton({300.0f, 376.0f}, {200.0f, 40.0f}, "Admin", font);
    Button authenticatedOptionsButton({300.0f, 424.0f}, {200.0f, 40.0f}, "Options", font);
    Button logoutButton({300.0f, 472.0f}, {200.0f, 40.0f}, "Log Out", font);

    TabStrip optionsTabs({128.0f, 116.0f}, {180.0f, 48.0f}, {"Graphics", "Audio", "Account"}, font);
    Button displayModeButton({270.0f, 210.0f}, {260.0f, 54.0f}, "", font);
    Button previousResolutionButton({210.0f, 316.0f}, {64.0f, 54.0f}, "<", font);
    Button resolutionButton({290.0f, 316.0f}, {220.0f, 54.0f}, "", font);
    Button nextResolutionButton({526.0f, 316.0f}, {64.0f, 54.0f}, ">", font);
    Button applyOptionsButton({300.0f, 410.0f}, {200.0f, 54.0f}, "Apply", font);
    SliderControl allAudioSlider({230.0f, 190.0f}, {340.0f, 58.0f}, "All Audio", font);
    SliderControl musicAudioSlider({230.0f, 290.0f}, {340.0f, 58.0f}, "Music", font);
    SliderControl soundFxAudioSlider({230.0f, 390.0f}, {340.0f, 58.0f}, "Sound FX", font);
    CheckboxControl muteAllAudioCheckbox({604.0f, 226.0f}, "Mute", font, rememberCheckTexture, 16, 20.0f, 30.0f);
    CheckboxControl muteMusicCheckbox({604.0f, 326.0f}, "Mute", font, rememberCheckTexture, 16, 20.0f, 30.0f);
    CheckboxControl muteSoundFxCheckbox({604.0f, 426.0f}, "Mute", font, rememberCheckTexture, 16, 20.0f, 30.0f);
    Button changePasswordOptionButton({300.0f, 250.0f}, {200.0f, 54.0f}, "Change Password", font);
    Button optionsBackButton({300.0f, 478.0f}, {200.0f, 54.0f}, "Back", font);
    Button changePasswordSubmitButton({300.0f, 390.0f}, {200.0f, 50.0f}, "Change Password", font);
    Button changePasswordBackButton({300.0f, 470.0f}, {200.0f, 50.0f}, "Back", font);
    Button dismissPasswordChangedButton({320.0f, 344.0f}, {160.0f, 46.0f}, "OK", font);

    Button deckBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button newDeckButton({34.0f, 140.0f}, {102.0f, 38.0f}, "New", font);
    Button refreshDeckButton({146.0f, 140.0f}, {110.0f, 38.0f}, "Refresh", font);
    Button editDeckButton({244.0f, 508.0f}, {110.0f, 38.0f}, "Edit", font);
    Button deleteDeckButton({34.0f, 508.0f}, {110.0f, 38.0f}, "Delete", font);
    Button removeCardButton({304.0f, 508.0f}, {110.0f, 38.0f}, "Remove", font);
    CheckboxControl collectionHeroFilterCheckbox({430.0f, 172.0f}, "Heroes", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    CheckboxControl collectionUnitFilterCheckbox({560.0f, 172.0f}, "Units", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    CheckboxControl collectionSpellFilterCheckbox({690.0f, 172.0f}, "Spells", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    CheckboxControl collectionBioFilterCheckbox({430.0f, 217.0f}, "bio", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    CheckboxControl collectionMechanicalFilterCheckbox({560.0f, 217.0f}, "mechanical", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    CheckboxControl collectionOccultFilterCheckbox({430.0f, 241.0f}, "occult", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    CheckboxControl collectionRiffraffFilterCheckbox({560.0f, 241.0f}, "riffraff", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    Button addCardButton({574.0f, 508.0f}, {88.0f, 38.0f}, "Add", font);
    Button saveDeckButton({668.0f, 508.0f}, {108.0f, 38.0f}, "Save", font);
    Button shopBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button buyCoinPackButton({94.0f, 492.0f}, {180.0f, 46.0f}, "Buy " + std::to_string(CoinPackCoins) + " Coins", font);
    Button refreshShopButton({310.0f, 492.0f}, {180.0f, 46.0f}, "Refresh", font);
    Button buyCardButton(
        {EnableCoinPurchases ? 526.0f : 300.0f, 492.0f},
        {EnableCoinPurchases ? 180.0f : 200.0f, 46.0f},
        "Buy Card",
        font);
    Button dismissRevealedCardButton({300.0f, 492.0f}, {200.0f, 46.0f}, "Dismiss", font);
    TabStrip adminTabs({24.0f, 22.0f}, {150.0f, 38.0f}, {"Users", "Starter Deck"}, font);
    Button adminBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button adminPrevPageButton({270.0f, 516.0f}, {52.0f, 38.0f}, "<", font);
    Button adminNextPageButton({478.0f, 516.0f}, {52.0f, 38.0f}, ">", font);
    Button adminRefreshButton({332.0f, 516.0f}, {104.0f, 38.0f}, "Refresh", font);
    Button adminGrantButton({42.0f, 458.0f}, {150.0f, 42.0f}, "Grant Admin", font);
    Button adminRevokeButton({42.0f, 458.0f}, {150.0f, 42.0f}, "Revoke Admin", font);
    Button adminGrantGoldButton({378.0f, 458.0f}, {150.0f, 42.0f}, "Grant Gold", font);
    Button adminRemoveGoldButton({542.0f, 458.0f}, {150.0f, 42.0f}, "Remove Gold", font);
    Button adminDeleteButton({600.0f, 514.0f}, {176.0f, 40.0f}, "Delete User", font);
    Button cancelDeleteUserButton({250.0f, 366.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmDeleteUserButton({420.0f, 366.0f}, {130.0f, 42.0f}, "Delete", font);
    Button cancelExitDesktopButton({250.0f, 356.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmExitDesktopButton({420.0f, 356.0f}, {130.0f, 42.0f}, "Exit", font);
    Button keepEditingDeckButton({232.0f, 356.0f}, {160.0f, 42.0f}, "Keep Editing", font);
    Button discardDeckChangesButton({420.0f, 356.0f}, {148.0f, 42.0f}, "Discard", font);
    Button closeDeckCardPopupButton({PiecePopupX + 190.0f, PiecePopupY + PiecePopupHeight - 54.0f}, {120.0f, 38.0f}, "Close", font);

    sf::Text messageText(font, "", 20);
    messageText.setFillColor(sf::Color::Red);
    messageText.setPosition({400.0f, 450.0f});
    CardEditorScreen cardEditorScreen(
        font,
        {clientConfig().card.host, clientConfig().card.port},
        fontPath->parent_path());
    AudioSystem audioSystem;
    activeAudioSystem = &audioSystem;
    setButtonClickHandler(playButtonClickSound);

    sf::Clock clock;
    float animationTime = 0.0f;
    GameState currentState = GameState::Menu;
    GameState optionsReturnState = GameState::Menu;
    OptionsTab activeOptionsTab = OptionsTab::Graphics;
    DisplaySettings pendingDisplaySettings = displaySettings;
    std::size_t selectedResolution = displayResolutionIndex(
        displayResolutions,
        {displaySettings.width, displaySettings.height});
    std::optional<std::future<ServerResult>> pendingRequest;
    std::optional<std::future<ServerResult>> pendingMatchmaking;
    std::optional<std::future<CardListResult>> pendingSandboxLoad;
    std::shared_ptr<MatchmakingCancelState> activeMatchmakingCancel;
    bool matchmakingCancelRequested = false;
    std::optional<std::future<void>> pendingLogout;
    std::optional<std::future<DeckEditorLoadResult>> pendingDeckEditorLoad;
    std::optional<std::future<StarterDeckLoadResult>> pendingStarterDeckLoad;
    std::optional<std::future<DeckCommandResult>> pendingDeckSave;
    std::optional<std::future<DeckCommandResult>> pendingDeckDelete;
    std::optional<std::future<AccountStateResult>> pendingAccountState;
    std::optional<std::future<ShopLoadResult>> pendingShopLoad;
    std::optional<std::future<AccountCommandResult>> pendingShopPurchase;
    std::optional<std::future<AdminUsersLoadResult>> pendingAdminUsersLoad;
    std::optional<std::future<AdminUserPrivilegeResult>> pendingAdminPrivilege;
    std::optional<std::future<AdminUserGoldResult>> pendingAdminGold;
    std::optional<std::future<AdminUserDeleteResult>> pendingAdminUserDelete;
    std::optional<std::future<AccountCommandResult>> pendingPasswordChange;
    bool coinPurchasePolling = false;
    int coinPurchaseStartingCoins = 0;
    float nextCoinPurchasePollAt = 0.0f;
    float coinPurchasePollDeadline = 0.0f;
    std::shared_ptr<sf::TcpSocket> activeGameSocket;
    std::string loggedInUsername;
    std::string activeAccessToken;
    std::string activeRememberToken;
    bool rememberMeChecked = false;
    bool passwordVisible = false;
    bool changePasswordsVisible = false;
    bool passwordChangedPopupVisible = false;
    bool exitDesktopPopupVisible = false;
    bool deckUnsavedChangesPopupVisible = false;
    bool exitDesktopCloseHovered = false;
    bool pendingAutoLogin = false;
    bool pendingRememberRequested = false;
    DeckEditorMode deckEditorMode = DeckEditorMode::DeckList;
    // Deck editor repurposed by admins to edit the account-creation starter deck:
    // the library shows every card (copy limits instead of owned copies) and
    // saves go to the admin starter deck endpoint.
    bool starterDeckMode = false;
    std::vector<card_data::Card> cardLibrary;
    std::vector<card_data::Card> filteredCardLibrary;
    std::vector<card_data::Card> allCardLibrary;
    std::array<bool, CollectionTypeLabels.size()> collectionTypeFilterChecked = {true, true, true};
    std::array<bool, CollectionKeywordLabels.size()> collectionKeywordFilterChecked = {true, true, true, true};
    std::vector<deck_data::Deck> playerDecks;
    std::vector<account_data::CollectionCard> playerCollection;
    std::vector<network::AdminUserSummary> adminUsers;
    deck_data::Deck editingDeck;
    std::string activeDeckOriginalName;
    int playerCoins = 0;
    int playerRating = 0;
    bool loggedInIsAdmin = false;
    std::string adminSearchQuery;
    std::uint32_t adminUsersPage = 0;
    std::uint32_t adminUsersPageSize = AdminUsersPageSize;
    std::uint32_t adminUsersTotalCount = 0;
    std::optional<std::size_t> selectedAdminUser;
    bool deleteUserPopupVisible = false;
    std::string adminUserDeleteTarget;
    std::optional<std::size_t> selectedDeck;
    std::optional<std::size_t> selectedDeckCard;
    std::optional<std::size_t> selectedLibraryCard;
    std::optional<std::string> inspectedDeckEditorCardTitle;
    std::optional<std::string> lastDeckEditorClickedCardTitle;
    sf::Vector2f lastDeckEditorCardClickPosition;
    float lastDeckEditorCardClickTime = -10.0f;
    float inspectedDeckEditorCardScroll = 0.0f;
    std::optional<std::string> revealedCardTitle;
    float revealStartedAt = 0.0f;
    bool gameResultReceived = false;
    bool gameResultSuccess = false;
    bool gameOverSoundPlayed = false;
    int gameRatingChange = 0;
    std::string gameRewardText;
    std::optional<std::size_t> draggingLibraryCard;
    std::optional<std::size_t> draggingDeckCard;
    sf::Vector2f dragStartPos;
    sf::Vector2f dragCurrentPos;
    bool dragActive = false;
    std::size_t deckListOffset = 0;
    std::size_t deckCardListOffset = 0;
    std::size_t libraryOffset = 0;
    int focusedInput = 0;

    // Play / in-game state.
    std::optional<std::future<DeckEditorLoadResult>> pendingPlayLoad;
    std::vector<card_data::Card> matchDeck;     // resolved deck submitted to the game
    std::vector<card_data::Card> matchHeroes;   // hero cards in placement order
    game_data::Snapshot gameSnapshot;
    bool haveSnapshot = false;
    bool sandboxMode = false;
    bool storyMode = false;
    enum class StoryStage
    {
        None,
        MoveTutorial,
        ValveChallenge,
        Complete
    };
    StoryStage storyStage = StoryStage::None;
    int storyComicPage = 0;
    int storyTargetRow = -1;
    int storyTargetColumn = -1;
    int sandboxPlacementPlayer = 1;
    int nextSandboxPieceId = 1;
    std::size_t gameHandOffset = 0;
    std::optional<int> selectedPieceId;
    std::optional<std::size_t> selectedHandIndex;
    std::optional<int> inspectedPieceId;
    std::optional<std::size_t> inspectedHandIndex;
    std::optional<int> lastClickedPieceId;
    sf::Vector2f lastPieceClickPosition;
    float lastPieceClickTime = -10.0f;
    std::optional<std::size_t> pendingHandClickIndex;
    sf::Vector2f pendingHandClickPosition;
    float pendingHandClickTime = -10.0f;
    float inspectedPieceScroll = 0.0f;
    enum class GameDragKind
    {
        None,
        HandCard,
        Piece
    };
    GameDragKind gameDragKind = GameDragKind::None;
    std::optional<std::size_t> draggingHandIndex;
    std::optional<int> draggingPieceId;
    sf::Vector2f gameDragStartPos;
    sf::Vector2f gameDragCurrentPos;
    bool gameDragActive = false;
    struct PieceMoveAnimation
    {
        int fromRow = 0;
        int fromColumn = 0;
        int toRow = 0;
        int toColumn = 0;
        float startTime = 0.0f;
        float duration = 0.95f;
    };
    std::unordered_map<int, PieceMoveAnimation> pieceMoveAnimations;
    struct PieceAttackAnimation
    {
        int targetRow = 0;
        int targetColumn = 0;
        float startTime = 0.0f;
        float duration = AttackAnimationDurationSeconds;
    };
    std::unordered_map<int, PieceAttackAnimation> pieceAttackAnimations;
    struct PieceReactionAnimation
    {
        float startTime = 0.0f;
        float duration = PieceReactionAnimationDurationSeconds;
    };
    std::unordered_map<int, PieceReactionAnimation> pieceDamagedAnimations;
    struct PieceKilledAnimation
    {
        game_data::Piece piece;
        float startTime = 0.0f;
        float duration = PieceReactionAnimationDurationSeconds;
    };
    std::vector<PieceKilledAnimation> pieceKilledAnimations;
    // An opposing piece that just dematerialized: it blinks in place for a few
    // seconds, then is not drawn at all until it materializes again.
    struct DematerializeGhost
    {
        game_data::Piece piece;
        float startTime = 0.0f;
    };
    std::vector<DematerializeGhost> dematerializeGhosts;

    Button findMatchButton({300.0f, 458.0f}, {200.0f, 52.0f}, "Find Match", font);
    Button abilityButton({392.0f, GameActionButtonY}, {138.0f, 36.0f}, "Use Ability", font);
    Button endTurnButton({540.0f, GameActionButtonY}, {132.0f, 36.0f}, "Pass Turn", font);
    Button sandboxPlayerButton({532.0f, GameActionButtonY}, {48.0f, 36.0f}, "P1", font);
    Button sandboxAdvanceTurnButton({588.0f, GameActionButtonY}, {88.0f, 36.0f}, "Advance", font);
    Button leaveGameButton({684.0f, 14.0f}, {100.0f, 36.0f}, "Leave", font);
    Button closePiecePopupButton({PiecePopupX + 358.0f, PiecePopupY + PiecePopupHeight - 54.0f}, {120.0f, 38.0f}, "Close", font);
    Button discardCardButton({PiecePopupX + 22.0f, PiecePopupY + PiecePopupHeight - 54.0f}, {220.0f, 38.0f},
                             "Discard to deck bottom", font);

    auto clearFocus = [&]() {
        usernameInput.setActive(false);
        passwordInput.setActive(false);
        confirmInput.setActive(false);
        currentPasswordInput.setActive(false);
        newPasswordInput.setActive(false);
        confirmNewPasswordInput.setActive(false);
        deckNameInput.setActive(false);
        adminSearchInput.setActive(false);
        adminGoldInput.setActive(false);
    };

    auto focusLoginInput = [&](int index) {
        focusedInput = (index + 2) % 2;
        usernameInput.setActive(focusedInput == 0);
        passwordInput.setActive(focusedInput == 1);
        confirmInput.setActive(false);
        deckNameInput.setActive(false);
        adminSearchInput.setActive(false);
    };

    auto focusCreateInput = [&](int index) {
        focusedInput = (index + 3) % 3;
        usernameInput.setActive(focusedInput == 0);
        passwordInput.setActive(focusedInput == 1);
        confirmInput.setActive(focusedInput == 2);
        deckNameInput.setActive(false);
    };

    auto focusChangePasswordInput = [&](int index) {
        focusedInput = (index + 3) % 3;
        usernameInput.setActive(false);
        passwordInput.setActive(false);
        confirmInput.setActive(false);
        currentPasswordInput.setActive(focusedInput == 0);
        newPasswordInput.setActive(focusedInput == 1);
        confirmNewPasswordInput.setActive(focusedInput == 2);
        deckNameInput.setActive(false);
        adminSearchInput.setActive(false);
    };

    auto sortDecks = [&]() {
        std::sort(playerDecks.begin(), playerDecks.end(), [](const deck_data::Deck& left, const deck_data::Deck& right) {
            return lowerKey(left.name) < lowerKey(right.name);
        });
    };

    auto signedInLabel = [&]() {
        return loggedInUsername + (loggedInIsAdmin ? " [Admin]" : "");
    };

    auto layoutDeckEditorControls = [&]() {
        if (deckEditorMode == DeckEditorMode::DeckList)
        {
            newDeckButton.setPosition({244.0f, 140.0f});
            refreshDeckButton.setPosition({376.0f, 140.0f});
            editDeckButton.setPosition({244.0f, 508.0f});
            deleteDeckButton.setPosition({446.0f, 508.0f});
        }
        else
        {
            deckNameInput.setPosition({42.0f, 112.0f});
            removeCardButton.setPosition({42.0f, 508.0f});
            addCardButton.setPosition({430.0f, 508.0f});
            saveDeckButton.setPosition({648.0f, 508.0f});
        }
    };

    auto layoutAuthenticatedButtons = [&]() {
        float y = loggedInIsAdmin ? 104.0f : 128.0f;
        constexpr float x = 300.0f;
        constexpr float gap = 10.0f;
        constexpr float height = 40.0f;

        auto place = [&](Button& button) {
            button.setPosition({x, y});
            y += height + gap;
        };

        place(storyButton);
        place(playButton);
        place(sandboxButton);
        place(deckEditorButton);
        place(shopButton);
        if (loggedInIsAdmin)
        {
            place(adminCardEditorButton);
            place(adminUsersButton);
        }
        place(authenticatedOptionsButton);
        place(logoutButton);
    };

    auto drawCoinIcon = [&](sf::Vector2f position, float radius) {
        sf::CircleShape shadow(radius);
        shadow.setPosition(position + sf::Vector2f(2.0f, 3.0f));
        shadow.setFillColor(sf::Color(0, 0, 0, 90));
        window.draw(shadow);

        sf::CircleShape coin(radius);
        coin.setPosition(position);
        coin.setFillColor(sf::Color(214, 158, 48));
        coin.setOutlineThickness(2.0f);
        coin.setOutlineColor(sf::Color(255, 225, 132));
        window.draw(coin);

        sf::CircleShape shine(radius * 0.48f);
        shine.setPosition(position + sf::Vector2f(radius * 0.34f, radius * 0.28f));
        shine.setFillColor(sf::Color(255, 225, 132, 105));
        window.draw(shine);

        sf::CircleShape center(radius * 0.55f);
        center.setPosition(position + sf::Vector2f(radius * 0.45f, radius * 0.45f));
        center.setFillColor(sf::Color::Transparent);
        center.setOutlineThickness(1.5f);
        center.setOutlineColor(sf::Color(142, 92, 28, 150));
        window.draw(center);
    };

    auto drawExitDesktopCloseButton = [&]() {
        const sf::Vector2f position{724.0f, 18.0f};
        const sf::Vector2f size{52.0f, 52.0f};

        drawBeveledPlate(
            window,
            position,
            size,
            exitDesktopCloseHovered ? sf::Color(134, 38, 28, 248) : sf::Color(75, 31, 25, 244),
            exitDesktopCloseHovered ? sf::Color(255, 178, 120) : sf::Color(176, 92, 59),
            exitDesktopCloseHovered,
            10.0f);

        sf::RectangleShape slashA({32.0f, 6.0f});
        slashA.setOrigin({16.0f, 3.0f});
        slashA.setPosition(position + sf::Vector2f(26.0f, 26.0f));
        slashA.setRotation(sf::degrees(45.0f));
        slashA.setFillColor(sf::Color(255, 238, 238));
        window.draw(slashA);

        sf::RectangleShape slashB({32.0f, 6.0f});
        slashB.setOrigin({16.0f, 3.0f});
        slashB.setPosition(position + sf::Vector2f(26.0f, 26.0f));
        slashB.setRotation(sf::degrees(-45.0f));
        slashB.setFillColor(sf::Color(255, 238, 238));
        window.draw(slashB);
    };

    auto exitDesktopCloseButtonClicked = [&](sf::Vector2f point) {
        return isInsideRect(point, 724.0f, 18.0f, 52.0f, 52.0f);
    };

    auto drawExitDesktopPopup = [&]() {
        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 170));
        window.draw(overlay);
        drawPanel(window, {220.0f, 188.0f}, {360.0f, 220.0f});
        drawText(window, font, "Exit to Desktop?", 28, {266.0f, 218.0f}, sf::Color(248, 224, 172), 270.0f);
        drawText(window, font, "Are you sure you want to exit", 16, {260.0f, 276.0f}, sf::Color(220, 224, 230), 280.0f);
        drawText(window, font, "to desktop?", 16, {350.0f, 302.0f}, sf::Color(220, 224, 230), 120.0f);
        cancelExitDesktopButton.draw(window);
        confirmExitDesktopButton.draw(window);
    };

    auto makeNewDeckName = [&]() {
        std::string name = "New Deck";
        int suffix = 2;
        auto exists = [&playerDecks](const std::string& candidate) {
            return std::any_of(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                return deck.name == candidate;
            });
        };

        while (exists(name))
        {
            name = "New Deck " + std::to_string(suffix++);
        }
        return name;
    };

    auto selectDeck = [&](std::size_t index) {
        if (index >= playerDecks.size())
        {
            return;
        }

        selectedDeck = index;
        editingDeck = playerDecks[index];
        activeDeckOriginalName = editingDeck.name;
        deckNameInput.setContent(editingDeck.name);
        selectedDeckCard.reset();
        deckCardListOffset = 0;
        clampListOffset(deckListOffset, playerDecks.size(), VisibleDeckRows);
        clearFocus();
    };

    auto selectDeckByName = [&](const std::string& deckName) {
        const auto found = std::find_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
            return deck.name == deckName;
        });
        if (found != playerDecks.end())
        {
            selectDeck(static_cast<std::size_t>(std::distance(playerDecks.begin(), found)));
        }
    };

    auto createNewDeck = [&]() {
        deckEditorMode = DeckEditorMode::EditDeck;
        deckUnsavedChangesPopupVisible = false;
        layoutDeckEditorControls();
        selectedDeck.reset();
        selectedDeckCard.reset();
        activeDeckOriginalName.clear();
        editingDeck = {makeNewDeckName(), {}};
        deckNameInput.setContent(editingDeck.name);
        deckNameInput.setActive(true);
        usernameInput.setActive(false);
        passwordInput.setActive(false);
        confirmInput.setActive(false);
        deckCardListOffset = 0;
    };

    auto editSelectedDeck = [&]() {
        if (!selectedDeck || *selectedDeck >= playerDecks.size())
        {
            setMessage(messageText, "Select a deck to edit", sf::Color::Red);
            return;
        }

        selectDeck(*selectedDeck);
        deckEditorMode = DeckEditorMode::EditDeck;
        layoutDeckEditorControls();
        setMessage(messageText, "", sf::Color::Yellow);
    };

    auto showDeckEditorDeckList = [&]() {
        deckEditorMode = DeckEditorMode::DeckList;
        deckUnsavedChangesPopupVisible = false;
        layoutDeckEditorControls();
        inspectedDeckEditorCardTitle.reset();
        lastDeckEditorClickedCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        deckNameInput.setActive(false);
        clampListOffset(deckListOffset, playerDecks.size(), VisibleDeckRows);
    };

    auto applyAccountState = [&](const AccountStateResult& result) {
        playerCoins = result.coins;
        playerRating = result.rating;
        loggedInIsAdmin = result.isAdmin;
        playerCollection = result.collection;
    };

    auto incrementCollection = [&](const std::string& title) {
        const auto found = std::find_if(
            playerCollection.begin(),
            playerCollection.end(),
            [&](const account_data::CollectionCard& card) {
                return card.title == title;
            });
        if (found != playerCollection.end())
        {
            ++found->copies;
        }
        else if (!title.empty())
        {
            playerCollection.push_back({title, 1});
        }
    };

    auto ownedCopies = [&](const std::string& title) {
        if (starterDeckMode)
        {
            const auto found = std::find_if(cardLibrary.begin(), cardLibrary.end(), [&](const card_data::Card& card) {
                return card.title == title;
            });
            const bool isHero = found != cardLibrary.end() && game_data::isHeroCard(*found);
            return isHero ? game_data::MaxHeroCopies : game_data::MaxCardCopies;
        }
        return collectionCopiesFor(playerCollection, title);
    };

    auto deckCopies = [&](const std::string& title) {
        return static_cast<int>(std::count(editingDeck.cardTitles.begin(), editingDeck.cardTitles.end(), title));
    };

    // Deck rows show one entry per card title; copies are conveyed by the X/Y count.
    auto deckUniqueTitles = [&]() {
        std::vector<std::string> unique;
        for (const std::string& title : editingDeck.cardTitles)
        {
            if (std::find(unique.begin(), unique.end(), title) == unique.end())
            {
                unique.push_back(title);
            }
        }
        return unique;
    };

    auto cardMatchesCollectionFilters = [&](const card_data::Card& card) {
        if (game_data::isTokenCard(card))
        {
            return false;
        }

        // Hide cards that can no longer be added: the deck already holds either
        // the per-card limit or every owned copy.
        const int copyLimit = game_data::isHeroCard(card) ? game_data::MaxHeroCopies : game_data::MaxCardCopies;
        if (deckCopies(card.title) >= std::min(copyLimit, ownedCopies(card.title)))
        {
            return false;
        }

        bool typeMatches = false;
        if (game_data::isHeroCard(card))
        {
            typeMatches = collectionTypeFilterChecked[0];
        }
        else if (game_data::isUnitCard(card))
        {
            typeMatches = collectionTypeFilterChecked[1];
        }
        else if (card.type == "Spell")
        {
            typeMatches = collectionTypeFilterChecked[2];
        }
        if (!typeMatches)
        {
            return false;
        }

        const bool allKeywordsChecked = std::all_of(
            collectionKeywordFilterChecked.begin(),
            collectionKeywordFilterChecked.end(),
            [](bool checked) {
                return checked;
            });
        if (allKeywordsChecked)
        {
            return true;
        }

        for (const std::string& keyword : card.keywords)
        {
            const std::string normalizedKeyword = lowerKey(keyword);
            for (std::size_t i = 0; i < CollectionKeywordLabels.size(); ++i)
            {
                if (collectionKeywordFilterChecked[i] && normalizedKeyword == CollectionKeywordLabels[i])
                {
                    return true;
                }
            }
        }

        return false;
    };

    auto applyCollectionFilters = [&]() {
        std::optional<std::string> selectedTitle;
        if (selectedLibraryCard && *selectedLibraryCard < filteredCardLibrary.size())
        {
            selectedTitle = filteredCardLibrary[*selectedLibraryCard].title;
        }

        filteredCardLibrary.clear();
        for (const card_data::Card& card : cardLibrary)
        {
            if (cardMatchesCollectionFilters(card))
            {
                filteredCardLibrary.push_back(card);
            }
        }

        if (filteredCardLibrary.empty())
        {
            selectedLibraryCard.reset();
        }
        else if (selectedTitle)
        {
            const auto selected = std::find_if(
                filteredCardLibrary.begin(),
                filteredCardLibrary.end(),
                [&](const card_data::Card& card) {
                    return card.title == *selectedTitle;
                });
            selectedLibraryCard = selected == filteredCardLibrary.end()
                ? std::optional<std::size_t>(0)
                : std::optional<std::size_t>(static_cast<std::size_t>(std::distance(filteredCardLibrary.begin(), selected)));
        }
        else if (!selectedLibraryCard || *selectedLibraryCard >= filteredCardLibrary.size())
        {
            selectedLibraryCard = 0;
        }

        clampListOffset(libraryOffset, filteredCardLibrary.size(), VisibleLibraryRows);
        if (draggingLibraryCard && *draggingLibraryCard >= filteredCardLibrary.size())
        {
            draggingLibraryCard.reset();
            dragActive = false;
        }
    };

    auto toggleCollectionTypeFilter = [&](std::size_t index) {
        if (index >= collectionTypeFilterChecked.size())
        {
            return;
        }
        collectionTypeFilterChecked[index] = !collectionTypeFilterChecked[index];
        libraryOffset = 0;
        applyCollectionFilters();
    };

    auto toggleCollectionKeywordFilter = [&](std::size_t index) {
        if (index >= collectionKeywordFilterChecked.size())
        {
            return;
        }
        collectionKeywordFilterChecked[index] = !collectionKeywordFilterChecked[index];
        libraryOffset = 0;
        applyCollectionFilters();
    };

    auto startRequest = [&](network::MessageType requestType, network::MessageType expectedResponseType) {
        setMessageY(messageText, 450.0f);
        setMessage(messageText, requestType == network::MessageType::Login ? "Logging in..." : "Creating account...", sf::Color::Yellow);
        pendingAutoLogin = false;
        pendingRememberRequested = requestType == network::MessageType::Login && rememberMeChecked;
        pendingRequest = std::async(
            std::launch::async,
            sendAccountRequest,
            requestType,
            expectedResponseType,
            usernameInput.getContent(),
            passwordInput.getContent(),
            pendingRememberRequested);
    };

    auto returnToMenu = [&]() {
        currentState = GameState::Menu;
        if (activeGameSocket)
        {
            activeGameSocket->disconnect();
            activeGameSocket.reset();
        }
        loggedInUsername.clear();
        activeAccessToken.clear();
        cardLibrary.clear();
        filteredCardLibrary.clear();
        allCardLibrary.clear();
        collectionTypeFilterChecked.fill(true);
        collectionKeywordFilterChecked.fill(true);
        deckEditorMode = DeckEditorMode::DeckList;
        starterDeckMode = false;
        adminTabs.setActive(0);
        playerDecks.clear();
        playerCollection.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        playerCoins = 0;
        playerRating = 0;
        loggedInIsAdmin = false;
        adminUsers.clear();
        adminSearchQuery.clear();
        adminUsersPage = 0;
        adminUsersTotalCount = 0;
        selectedAdminUser.reset();
        deleteUserPopupVisible = false;
        exitDesktopPopupVisible = false;
        deckUnsavedChangesPopupVisible = false;
        adminUserDeleteTarget.clear();
        adminSearchInput.clear();
        adminGoldInput.clear();
        coinPurchasePolling = false;
        selectedDeck.reset();
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        inspectedDeckEditorCardTitle.reset();
        lastDeckEditorClickedCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        revealedCardTitle.reset();
        revealStartedAt = 0.0f;
        gameResultReceived = false;
        gameResultSuccess = false;
        gameOverSoundPlayed = false;
        gameRatingChange = 0;
        gameRewardText.clear();
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        sandboxMode = false;
        storyMode = false;
        storyStage = StoryStage::None;
        storyComicPage = 0;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        sandboxPlacementPlayer = 1;
        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        title.setString("Steam Tactics");
        centerText(title, 400.0f);
        setMessageY(messageText, 450.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        deckNameInput.clear();
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        clearFocus();
    };

    auto showAuthenticatedScreen = [&]() {
        currentState = GameState::Authenticated;
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 560.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        deckNameInput.clear();
        inspectedDeckEditorCardTitle.reset();
        lastDeckEditorClickedCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        revealedCardTitle.reset();
        exitDesktopPopupVisible = false;
        deckUnsavedChangesPopupVisible = false;
        coinPurchasePolling = false;
        clearFocus();
        if (!loggedInUsername.empty())
        {
            pendingAccountState = std::async(std::launch::async, fetchAccountState, activeAccessToken);
        }
    };

    auto loadAdminUsersScreen = [&]() {
        if (!loggedInIsAdmin)
        {
            setMessage(messageText, "Admin access required", sf::Color::Red);
            return;
        }
        currentState = GameState::AdminUsers;
        starterDeckMode = false;
        adminTabs.setActive(0);
        title.setString("");
        centerText(title, 400.0f);
        clearFocus();
        adminSearchInput.setContent(adminSearchQuery);
        adminSearchInput.setActive(true);
        adminUsers.clear();
        selectedAdminUser.reset();
        deleteUserPopupVisible = false;
        adminUserDeleteTarget.clear();
        setMessageY(messageText, 566.0f);
        setMessage(messageText, "Loading users...", sf::Color::Yellow);
        pendingAdminUsersLoad = std::async(
            std::launch::async,
            loadAdminUsers,
            activeAccessToken,
            adminSearchQuery,
            adminUsersPage,
            adminUsersPageSize);
    };

    auto searchAdminUsers = [&]() {
        adminSearchQuery = trim(adminSearchInput.getContent());
        adminUsersPage = 0;
        loadAdminUsersScreen();
    };

    auto loadStarterDeckEditor = [&]() {
        if (!loggedInIsAdmin)
        {
            setMessage(messageText, "Admin access required", sf::Color::Red);
            return;
        }
        currentState = GameState::DeckEditor;
        starterDeckMode = true;
        adminTabs.setActive(1);
        deckEditorMode = DeckEditorMode::EditDeck;
        deckUnsavedChangesPopupVisible = false;
        layoutDeckEditorControls();
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "Loading starter deck...", sf::Color::Yellow);
        clearFocus();
        cardLibrary.clear();
        filteredCardLibrary.clear();
        collectionTypeFilterChecked.fill(true);
        collectionKeywordFilterChecked.fill(true);
        playerDecks.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        selectedDeck.reset();
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        inspectedDeckEditorCardTitle.reset();
        lastDeckEditorClickedCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        deckCardListOffset = 0;
        libraryOffset = 0;
        deckNameInput.clear();
        pendingStarterDeckLoad = std::async(std::launch::async, loadStarterDeckEditorData, activeAccessToken);
    };

    auto leaveStarterDeckEditor = [&]() {
        starterDeckMode = false;
        deckUnsavedChangesPopupVisible = false;
        deckEditorMode = DeckEditorMode::DeckList;
        playerDecks.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        inspectedDeckEditorCardTitle.reset();
        lastDeckEditorClickedCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        deckNameInput.clear();
        loadAdminUsersScreen();
    };

    auto changeSelectedUserGold = [&](bool grant) {
        if (!selectedAdminUser || *selectedAdminUser >= adminUsers.size() || pendingAdminGold)
        {
            return;
        }

        const std::string amountText = trim(adminGoldInput.getContent());
        if (amountText.empty() ||
            !std::all_of(amountText.begin(), amountText.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        {
            setMessage(messageText, "Enter a positive whole-number gold amount", sf::Color::Red);
            return;
        }

        try
        {
            const long long parsedAmount = std::stoll(amountText);
            if (parsedAmount <= 0 || parsedAmount > std::numeric_limits<int>::max())
            {
                setMessage(messageText, "Gold amount is out of range", sf::Color::Red);
                return;
            }

            const int amount = static_cast<int>(parsedAmount) * (grant ? 1 : -1);
            const std::string targetUsername = adminUsers[*selectedAdminUser].username;
            pendingAdminGold = std::async(
                std::launch::async,
                updateAdminUserGold,
                activeAccessToken,
                targetUsername,
                amount);
            setMessage(
                messageText,
                grant ? "Granting gold..." : "Removing gold...",
                sf::Color::Yellow);
        }
        catch (const std::exception&)
        {
            setMessage(messageText, "Gold amount is out of range", sf::Color::Red);
        }
    };

    auto openDeleteUserPopup = [&]() {
        if (!selectedAdminUser || *selectedAdminUser >= adminUsers.size())
        {
            return;
        }
        const std::string& targetUsername = adminUsers[*selectedAdminUser].username;
        if (targetUsername == loggedInUsername)
        {
            setMessage(messageText, "You cannot delete your own account", sf::Color::Red);
            return;
        }
        adminUserDeleteTarget = targetUsername;
        deleteUserPopupVisible = true;
        adminSearchInput.setActive(false);
        adminGoldInput.setActive(false);
    };

    auto dismissDeleteUserPopup = [&]() {
        deleteUserPopupVisible = false;
        adminUserDeleteTarget.clear();
    };

    auto confirmUserDeletion = [&]() {
        if (pendingAdminUserDelete || adminUserDeleteTarget.empty())
        {
            return;
        }
        pendingAdminUserDelete = std::async(
            std::launch::async,
            deleteAdminUser,
            activeAccessToken,
            adminUserDeleteTarget);
        setMessage(messageText, "Deleting user...", sf::Color::Yellow);
        deleteUserPopupVisible = false;
    };

    auto showCardEditorScreen = [&]() {
        if (!loggedInIsAdmin)
        {
            setMessage(messageText, "Admin access required", sf::Color::Red);
            return;
        }
        currentState = GameState::CardEditor;
        title.setString("");
        centerText(title, 400.0f);
        clearFocus();
        cardEditorScreen.setEndpoint({clientConfig().card.host, clientConfig().card.port});
        cardEditorScreen.open();
    };

    auto updateOptionsLabels = [&]() {
        displayModeButton.setLabel(pendingDisplaySettings.fullscreen ? "Fullscreen" : "Windowed");
        const sf::Vector2u size = displayResolutions[selectedResolution];
        resolutionButton.setLabel(std::to_string(size.x) + " x " + std::to_string(size.y));
        allAudioSlider.setValue(audioSystem.getAllVolume());
        musicAudioSlider.setValue(audioSystem.getMusicVolume());
        soundFxAudioSlider.setValue(audioSystem.getSoundEffectsVolume());
    };

    auto setActiveOptionsTab = [&](OptionsTab tab) {
        activeOptionsTab = tab;
        optionsTabs.setActive(static_cast<std::size_t>(tab));
    };

    auto showOptionsScreen = [&](GameState returnState) {
        optionsReturnState = returnState;
        currentState = GameState::Options;
        setActiveOptionsTab(OptionsTab::Graphics);
        pendingDisplaySettings = displaySettings;
        const sf::Vector2u activeSize{displaySettings.width, displaySettings.height};
        const auto found = std::find(displayResolutions.begin(), displayResolutions.end(), activeSize);
        selectedResolution = found == displayResolutions.end()
            ? displayResolutions.size() - 1
            : static_cast<std::size_t>(std::distance(displayResolutions.begin(), found));
        title.setString("Options");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "", sf::Color::White);
        clearFocus();
        updateOptionsLabels();
    };

    auto leaveOptionsScreen = [&]() {
        currentState = optionsReturnState;
        title.setString(optionsReturnState == GameState::Authenticated ? "" : "Steam Tactics");
        centerText(title, 400.0f);
        setMessageY(messageText, optionsReturnState == GameState::Authenticated ? 500.0f : 450.0f);
        setMessage(messageText, "", sf::Color::White);
    };

    auto updateChangePasswordVisibility = [&]() {
        currentPasswordInput.setPasswordMode(!changePasswordsVisible);
        newPasswordInput.setPasswordMode(!changePasswordsVisible);
        confirmNewPasswordInput.setPasswordMode(!changePasswordsVisible);
    };

    auto showChangePasswordScreen = [&]() {
        currentState = GameState::ChangePassword;
        title.setString("Change Password");
        centerText(title, 400.0f);
        setMessageY(messageText, 550.0f);
        setMessage(messageText, "", sf::Color::White);
        currentPasswordInput.clear();
        newPasswordInput.clear();
        confirmNewPasswordInput.clear();
        changePasswordsVisible = false;
        passwordChangedPopupVisible = false;
        updateChangePasswordVisibility();
        focusChangePasswordInput(0);
    };

    auto leaveChangePasswordScreen = [&]() {
        showOptionsScreen(GameState::Authenticated);
    };

    auto dismissPasswordChangedPopup = [&]() {
        const std::string accessTokenToRevoke = activeAccessToken;
        passwordChangedPopupVisible = false;
        if (!accessTokenToRevoke.empty())
        {
            pendingLogout = std::async(
                std::launch::async,
                revokeLoginTokens,
                std::string(),
                accessTokenToRevoke);
        }
        returnToMenu();
    };

    auto showGameScreen = [&](std::shared_ptr<sf::TcpSocket> gameSocket) {
        activeGameSocket = std::move(gameSocket);
        currentState = GameState::Game;
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();

        haveSnapshot = false;
        sandboxMode = false;
        storyMode = false;
        storyStage = StoryStage::None;
        storyComicPage = 0;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        sandboxPlacementPlayer = 1;
        gameHandOffset = 0;
        nextSandboxPieceId = 1;
        gameSnapshot = {};
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        gameResultReceived = false;
        gameResultSuccess = false;
        gameOverSoundPlayed = false;
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();
        pieceAttackAnimations.clear();
        pieceDamagedAnimations.clear();
        pieceKilledAnimations.clear();
        dematerializeGhosts.clear();

        // Submit our deck, then switch the socket to non-blocking polling.
        if (activeGameSocket)
        {
            sendSubmitDeck(*activeGameSocket, matchDeck);
            activeGameSocket->setBlocking(false);
        }
    };

    auto startMatchmaking = [&]() {
        currentState = GameState::Matchmaking;
        title.setString("Matchmaking");
        centerText(title, 400.0f);
        setMessageY(messageText, 450.0f);
        setMessage(messageText, "Finding match...", sf::Color::Yellow);
        matchmakingCancelRequested = false;
        cancelMatchmakingButton.setLabel("Cancel");
        playAiButton.setLabel("Play vs AI");
        activeMatchmakingCancel = std::make_shared<MatchmakingCancelState>();
        pendingMatchmaking =
            std::async(std::launch::async, joinMatchmaking, activeAccessToken, activeMatchmakingCancel);
    };

    auto requestMatchmakingCancel = [&]() {
        if (currentState != GameState::Matchmaking ||
            !pendingMatchmaking ||
            !activeMatchmakingCancel ||
            matchmakingCancelRequested)
        {
            return;
        }

        matchmakingCancelRequested = true;
        activeMatchmakingCancel->requested.store(true);
        cancelMatchmakingButton.setLabel("Cancelling");
        setMessage(messageText, "Cancelling matchmaking...", sf::Color::Yellow);
    };

    auto loadDeckEditor = [&]() {
        currentState = GameState::DeckEditor;
        starterDeckMode = false;
        deckEditorMode = DeckEditorMode::DeckList;
        deckUnsavedChangesPopupVisible = false;
        layoutDeckEditorControls();
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "Loading deck editor...", sf::Color::Yellow);
        clearFocus();
        cardLibrary.clear();
        filteredCardLibrary.clear();
        collectionTypeFilterChecked.fill(true);
        collectionKeywordFilterChecked.fill(true);
        playerDecks.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        selectedDeck.reset();
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        inspectedDeckEditorCardTitle.reset();
        lastDeckEditorClickedCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        deckListOffset = 0;
        deckCardListOffset = 0;
        libraryOffset = 0;
        deckNameInput.clear();
        pendingDeckEditorLoad = std::async(std::launch::async, loadDeckEditorData, activeAccessToken);
    };

    auto deckEditorBusy = [&]() {
        return pendingDeckEditorLoad.has_value() || pendingStarterDeckLoad.has_value() ||
            pendingDeckSave.has_value() || pendingDeckDelete.has_value();
    };

    auto loadShop = [&]() {
        currentState = GameState::Shop;
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "Loading shop...", sf::Color::Yellow);
        clearFocus();
        allCardLibrary.clear();
        revealedCardTitle.reset();
        revealStartedAt = 0.0f;
        coinPurchasePolling = false;
        pendingShopLoad = std::async(std::launch::async, loadShopData, activeAccessToken);
    };

    auto shopBusy = [&]() {
        return pendingShopLoad.has_value() || pendingShopPurchase.has_value();
    };

    auto refreshShop = [&]() {
        setMessage(messageText, "Refreshing coins...", sf::Color::Yellow);
        pendingShopLoad = std::async(std::launch::async, loadShopData, activeAccessToken);
    };

    auto submitLogin = [&]() {
        if (usernameInput.getContent().empty() || passwordInput.getContent().empty())
        {
            setMessage(messageText, "Username and password cannot be empty", sf::Color::Red);
        }
        else
        {
            startRequest(network::MessageType::Login, network::MessageType::LoginResponse);
        }
    };

    auto updatePasswordVisibility = [&]() {
        passwordInput.setPasswordMode(!passwordVisible);
        confirmInput.setPasswordMode(!passwordVisible);
    };

    auto submitCreateAccount = [&]() {
        if (usernameInput.getContent().empty() || passwordInput.getContent().empty())
        {
            setMessage(messageText, "Username and password cannot be empty", sf::Color::Red);
        }
        else if (passwordInput.getContent().size() < 15 || passwordInput.getContent().size() > 128)
        {
            setMessage(messageText, "Password must be 15-128 characters", sf::Color::Red);
        }
        else if (passwordInput.getContent() != confirmInput.getContent())
        {
            setMessage(messageText, "Passwords do not match", sf::Color::Red);
        }
        else
        {
            startRequest(network::MessageType::CreateAccount, network::MessageType::CreateAccountResponse);
        }
    };

    auto submitPasswordChange = [&]() {
        if (currentPasswordInput.getContent().empty() || newPasswordInput.getContent().empty())
        {
            setMessage(messageText, "Current and new passwords cannot be empty", sf::Color::Red);
        }
        else if (newPasswordInput.getContent().size() < 15 || newPasswordInput.getContent().size() > 128)
        {
            setMessage(messageText, "New password must be 15-128 characters", sf::Color::Red);
        }
        else if (newPasswordInput.getContent() != confirmNewPasswordInput.getContent())
        {
            setMessage(messageText, "New passwords do not match", sf::Color::Red);
        }
        else if (currentPasswordInput.getContent() == newPasswordInput.getContent())
        {
            setMessage(messageText, "New password must be different", sf::Color::Red);
        }
        else
        {
            setMessage(messageText, "Changing password...", sf::Color::Yellow);
            pendingPasswordChange = std::async(
                std::launch::async,
                changePassword,
                activeAccessToken,
                currentPasswordInput.getContent(),
                newPasswordInput.getContent());
        }
    };

    auto cardByTitle = [&](const std::string& title) -> const card_data::Card* {
        const auto found = std::find_if(cardLibrary.begin(), cardLibrary.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        return found == cardLibrary.end() ? nullptr : &*found;
    };

    struct DeckStats
    {
        int cardCount = 0;   // non-hero cards
        int heroCount = 0;
        int heroCost = 0;
        std::vector<std::string> heroKeywords;
        std::vector<std::string> keywordMismatchTitles;
        std::vector<std::string> warnings;
    };

    auto computeDeckStats = [&]() {
        DeckStats stats;
        auto addHeroKeyword = [&](const std::string& keyword) {
            const std::string normalized = lowerKey(keyword);
            const bool alreadyPresent = std::any_of(
                stats.heroKeywords.begin(),
                stats.heroKeywords.end(),
                [&](const std::string& existing) {
                    return lowerKey(existing) == normalized;
                });
            if (!alreadyPresent)
            {
                stats.heroKeywords.push_back(keyword);
            }
        };
        auto heroHasKeyword = [&](const std::string& keyword) {
            const std::string normalized = lowerKey(keyword);
            return std::any_of(
                stats.heroKeywords.begin(),
                stats.heroKeywords.end(),
                [&](const std::string& heroKeyword) {
                    return lowerKey(heroKeyword) == normalized;
                });
        };

        for (const std::string& title : editingDeck.cardTitles)
        {
            const card_data::Card* card = cardByTitle(title);
            if (card && game_data::isHeroCard(*card))
            {
                ++stats.heroCount;
                stats.heroCost += game_data::cardInt(*card, "heroCost", 0);
                for (const std::string& keyword : card->keywords)
                {
                    if (!keyword.empty())
                    {
                        addHeroKeyword(keyword);
                    }
                }
            }
            else
            {
                ++stats.cardCount;
            }
        }

        for (const std::string& title : editingDeck.cardTitles)
        {
            const card_data::Card* card = cardByTitle(title);
            if (!card || game_data::isHeroCard(*card))
            {
                continue;
            }

            const bool missingHeroKeyword = std::any_of(
                card->keywords.begin(),
                card->keywords.end(),
                [&](const std::string& keyword) {
                    return !keyword.empty() && !heroHasKeyword(keyword);
                });
            if (missingHeroKeyword &&
                std::find(stats.keywordMismatchTitles.begin(), stats.keywordMismatchTitles.end(), title) ==
                    stats.keywordMismatchTitles.end())
            {
                stats.keywordMismatchTitles.push_back(title);
            }
        }

        if (stats.heroCount == 0)
        {
            stats.warnings.push_back("Add at least one hero.");
        }
        if (stats.heroCost > game_data::HeroCostLimit)
        {
            stats.warnings.push_back(
                "Hero cost is " + std::to_string(stats.heroCost) + "/" +
                std::to_string(game_data::HeroCostLimit) + ".");
        }
        if (stats.cardCount != game_data::DeckCardCount)
        {
            stats.warnings.push_back(
                "Use exactly " + std::to_string(game_data::DeckCardCount) +
                " non-hero cards.");
        }
        if (!stats.keywordMismatchTitles.empty())
        {
            stats.warnings.push_back("Highlighted cards lack hero keywords.");
        }
        return stats;
    };

    auto deckValidationError = [&](const deck_data::Deck& deck) -> std::string {
        const std::vector<card_data::Card> resolved = resolveDeckCards(deck, cardLibrary);
        if (resolved.size() != deck.cardTitles.size())
        {
            return "Deck contains a card that is no longer available";
        }
        const std::optional<std::string> error = game_data::deckRulesError(resolved);
        return error.value_or("");
    };

    auto deckCollectionError = [&]() -> std::string {
        std::unordered_map<std::string, int> used;
        for (const std::string& title : editingDeck.cardTitles)
        {
            const int count = ++used[title];
            const int owned = ownedCopies(title);
            if (count > owned)
            {
                return "Only " + std::to_string(owned) + " owned copies of " + title;
            }
        }
        return "";
    };

    auto saveCurrentDeck = [&]() {
        if (deckEditorBusy())
        {
            return;
        }

        deck_data::Deck deck = editingDeck;
        if (starterDeckMode)
        {
            deck.name = activeDeckOriginalName;
            const std::string validationError = deckValidationError(deck);
            if (!validationError.empty())
            {
                setMessage(messageText, validationError, sf::Color::Red);
                return;
            }

            setMessage(messageText, "Saving starter deck...", sf::Color::Yellow);
            pendingDeckSave = std::async(std::launch::async, saveStarterDeckToAccount, activeAccessToken, deck);
            return;
        }

        deck.name = trim(deckNameInput.getContent());
        if (deck.name.empty())
        {
            setMessage(messageText, "Deck name cannot be empty", sf::Color::Red);
            return;
        }

        const std::string collectionError = deckCollectionError();
        if (!collectionError.empty())
        {
            setMessage(messageText, collectionError, sf::Color::Red);
            return;
        }

        setMessage(messageText, "Saving deck...", sf::Color::Yellow);
        pendingDeckSave = std::async(std::launch::async, saveDeckToAccount, activeAccessToken, activeDeckOriginalName, deck);
    };

    auto deleteCurrentDeck = [&]() {
        if (deckEditorBusy())
        {
            return;
        }

        if (activeDeckOriginalName.empty())
        {
            setMessage(messageText, "Select a saved deck to delete", sf::Color::Red);
            return;
        }

        setMessage(messageText, "Deleting deck...", sf::Color::Yellow);
        pendingDeckDelete = std::async(std::launch::async, deleteDeckFromAccount, activeAccessToken, activeDeckOriginalName);
    };

    auto addLibraryCardToDeck = [&](std::size_t libraryIndex, const std::string& message) {
        if (libraryIndex >= filteredCardLibrary.size())
        {
            return;
        }

        // Copy: applyCollectionFilters below rebuilds filteredCardLibrary.
        const std::string title = filteredCardLibrary[libraryIndex].title;
        if (game_data::isTokenCard(filteredCardLibrary[libraryIndex]))
        {
            setMessage(messageText, title + " is a token and cannot be added to a deck", sf::Color::Red);
            return;
        }

        const bool isHero = game_data::isHeroCard(filteredCardLibrary[libraryIndex]);
        const int copyLimit = isHero ? game_data::MaxHeroCopies : game_data::MaxCardCopies;
        if (deckCopies(title) >= copyLimit)
        {
            setMessage(
                messageText,
                "Deck limit is " + std::to_string(copyLimit) + " " +
                    (isHero ? "copy of hero " : "copies of card ") + title,
                sf::Color::Red);
            return;
        }
        if (deckCopies(title) >= ownedCopies(title))
        {
            setMessage(messageText, "No extra owned copies of " + title, sf::Color::Red);
            return;
        }

        editingDeck.cardTitles.push_back(title);
        const std::vector<std::string> deckTitles = deckUniqueTitles();
        const auto added = std::find(deckTitles.begin(), deckTitles.end(), title);
        selectedDeckCard = static_cast<std::size_t>(std::distance(deckTitles.begin(), added));
        clampListOffset(deckCardListOffset, deckTitles.size(), VisibleDeckCardRows);
        if (*selectedDeckCard >= deckCardListOffset + VisibleDeckCardRows)
        {
            deckCardListOffset = *selectedDeckCard - VisibleDeckCardRows + 1;
        }
        else if (*selectedDeckCard < deckCardListOffset)
        {
            deckCardListOffset = *selectedDeckCard;
        }
        applyCollectionFilters();
        setMessage(messageText, message, sf::Color::Yellow);
    };

    auto addSelectedCard = [&]() {
        if (!selectedLibraryCard || *selectedLibraryCard >= filteredCardLibrary.size())
        {
            setMessage(messageText, "Select a card from the library first", sf::Color::Red);
            return;
        }

        addLibraryCardToDeck(*selectedLibraryCard, "Card added. Save to keep changes.");
    };

    auto removeDeckCardAt = [&](std::size_t uniqueIndex, const std::string& message) {
        const std::vector<std::string> deckTitles = deckUniqueTitles();
        if (uniqueIndex >= deckTitles.size())
        {
            return;
        }

        const std::string title = deckTitles[uniqueIndex];
        const auto lastCopy = std::find(editingDeck.cardTitles.rbegin(), editingDeck.cardTitles.rend(), title);
        editingDeck.cardTitles.erase(std::next(lastCopy).base());

        const std::size_t uniqueCount = deckUniqueTitles().size();
        if (uniqueCount == 0)
        {
            selectedDeckCard.reset();
        }
        else if (selectedDeckCard && *selectedDeckCard >= uniqueCount)
        {
            selectedDeckCard = uniqueCount - 1;
        }
        clampListOffset(deckCardListOffset, uniqueCount, VisibleDeckCardRows);
        applyCollectionFilters();
        setMessage(messageText, message, sf::Color::Yellow);
    };

    auto removeSelectedCard = [&]() {
        if (!selectedDeckCard || *selectedDeckCard >= deckUniqueTitles().size())
        {
            setMessage(messageText, "Select a card in the deck first", sf::Color::Red);
            return;
        }

        removeDeckCardAt(*selectedDeckCard, "Card removed. Save to keep changes.");
    };

    auto deckHasUnsavedChanges = [&]() {
        if (deckEditorMode != DeckEditorMode::EditDeck)
        {
            return false;
        }
        if (starterDeckMode && playerDecks.empty())
        {
            // Starter deck never loaded (still loading or load failed) — nothing to lose.
            return false;
        }

        const std::string currentName = trim(deckNameInput.getContent());
        if (activeDeckOriginalName.empty())
        {
            return true;
        }

        const auto saved = std::find_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
            return deck.name == activeDeckOriginalName;
        });
        if (saved == playerDecks.end())
        {
            return true;
        }

        return currentName != saved->name || editingDeck.cardTitles != saved->cardTitles;
    };

    auto requestLeaveDeckEdit = [&]() {
        if (deckHasUnsavedChanges())
        {
            deckUnsavedChangesPopupVisible = true;
            deckNameInput.setActive(false);
            clearFocus();
            return;
        }

        if (starterDeckMode)
        {
            leaveStarterDeckEditor();
            return;
        }

        showDeckEditorDeckList();
        setMessage(messageText, "Choose a deck to edit.", sf::Color(120, 220, 150));
    };

    auto discardDeckEditChanges = [&]() {
        deckUnsavedChangesPopupVisible = false;
        if (starterDeckMode)
        {
            leaveStarterDeckEditor();
            setMessage(messageText, "Unsaved starter deck changes discarded.", sf::Color(220, 180, 120));
            return;
        }
        showDeckEditorDeckList();
        setMessage(messageText, "Unsaved deck changes discarded.", sf::Color(220, 180, 120));
    };

    #include "screens/deck_editor_screen.inl"

    auto showDeckSelect = [&]() {
        currentState = GameState::DeckSelect;
        title.setString("Select Deck");
        centerText(title, 400.0f);
        clearFocus();
        playerDecks.clear();
        cardLibrary.clear();
        filteredCardLibrary.clear();
        collectionTypeFilterChecked.fill(true);
        collectionKeywordFilterChecked.fill(true);
        selectedDeck.reset();
        deckListOffset = 0;
        setMessageY(messageText, 524.0f);
        setMessage(messageText, "Loading decks...", sf::Color::Yellow);
        pendingPlayLoad = std::async(std::launch::async, loadDeckEditorData, activeAccessToken);
    };

    auto findMatch = [&]() {
        if (!selectedDeck || *selectedDeck >= playerDecks.size())
        {
            setMessage(messageText, "Select a deck first", sf::Color::Red);
            return;
        }

        const std::string validationError = deckValidationError(playerDecks[*selectedDeck]);
        if (!validationError.empty())
        {
            setMessage(messageText, validationError, sf::Color::Red);
            return;
        }

        matchDeck = resolveDeckCards(playerDecks[*selectedDeck], cardLibrary);
        matchHeroes.clear();
        for (const card_data::Card& card : matchDeck)
        {
            if (game_data::isHeroCard(card) && static_cast<int>(matchHeroes.size()) < game_data::MaxHeroes)
            {
                matchHeroes.push_back(card);
            }
        }

        if (matchHeroes.empty())
        {
            setMessage(messageText, "Deck needs at least one hero card", sf::Color::Red);
            return;
        }

        startMatchmaking();
    };

    // ---- in-game helpers ---------------------------------------------------

    auto boardCellMetrics = [&](int row, int column) {
        return boardCellMetricsForViewer(row, column, gameSnapshot.yourPlayer);
    };

    auto drawQuad = [&](const std::array<sf::Vector2f, 4>& corners,
                        sf::Color fill,
                        float outlineThickness = 0.0f,
                        sf::Color outline = sf::Color::Transparent) {
        sf::ConvexShape quad;
        quad.setPointCount(corners.size());
        for (std::size_t i = 0; i < corners.size(); ++i)
        {
            quad.setPoint(i, corners[i]);
        }
        quad.setFillColor(fill);
        quad.setOutlineThickness(outlineThickness);
        quad.setOutlineColor(outline);
        window.draw(quad);
    };

    auto startPieceAttackAnimation = [&](int pieceId, int targetRow, int targetColumn) {
        pieceAttackAnimations[pieceId] = {
            targetRow,
            targetColumn,
            animationTime,
            AttackAnimationDurationSeconds};
    };

    auto startPieceDamagedAnimation = [&](const game_data::Piece& piece) {
        if (!piece.damagedAnimPath.empty())
        {
            pieceDamagedAnimations[piece.id] = {animationTime, PieceReactionAnimationDurationSeconds};
        }
    };

    auto startPieceKilledAnimation = [&](const game_data::Piece& piece) {
        if (!piece.killedAnimPath.empty())
        {
            pieceKilledAnimations.push_back({piece, animationTime, PieceReactionAnimationDurationSeconds});
        }
    };

    auto updatePieceMoveAnimations = [&](const game_data::Snapshot& nextSnapshot) {
        std::vector<int> staleAnimations;
        for (auto& [pieceId, animation] : pieceMoveAnimations)
        {
            if (!pieceByIdInSnapshot(nextSnapshot, pieceId))
            {
                staleAnimations.push_back(pieceId);
            }
        }
        for (int pieceId : staleAnimations)
        {
            pieceMoveAnimations.erase(pieceId);
        }

        staleAnimations.clear();
        for (auto& [pieceId, animation] : pieceAttackAnimations)
        {
            if (!pieceByIdInSnapshot(nextSnapshot, pieceId))
            {
                staleAnimations.push_back(pieceId);
            }
        }
        for (int pieceId : staleAnimations)
        {
            pieceAttackAnimations.erase(pieceId);
        }

        staleAnimations.clear();
        for (auto& [pieceId, animation] : pieceDamagedAnimations)
        {
            if (!pieceByIdInSnapshot(nextSnapshot, pieceId))
            {
                staleAnimations.push_back(pieceId);
            }
        }
        for (int pieceId : staleAnimations)
        {
            pieceDamagedAnimations.erase(pieceId);
        }

        if (!haveSnapshot)
        {
            return;
        }

        // A ghost is stale once its piece is visible again (it materialized).
        dematerializeGhosts.erase(
            std::remove_if(
                dematerializeGhosts.begin(),
                dematerializeGhosts.end(),
                [&](const DematerializeGhost& ghost) {
                    return pieceByIdInSnapshot(nextSnapshot, ghost.piece.id) != nullptr;
                }),
            dematerializeGhosts.end());

        bool playedMoveSound = false;
        bool playedPlaceSound = false;
        bool playedAttackSound = false;
        bool playedDeathSound = false;
        bool playedDematerializeSound = false;
        for (const game_data::Piece& currentPiece : gameSnapshot.pieces)
        {
            if (pieceByIdInSnapshot(nextSnapshot, currentPiece.id))
            {
                continue;
            }
            // A piece that vanished because it dematerialized (rather than
            // died) blinks in place for a moment before disappearing.
            if (nextSnapshot.status.find(currentPiece.name + " used Dematerialize") !=
                std::string::npos)
            {
                dematerializeGhosts.push_back({currentPiece, animationTime});
                playedDematerializeSound = true;
            }
            else
            {
                playedDeathSound = true;
                startPieceKilledAnimation(currentPiece);
            }
        }

        const bool snapshotDescribesAttack = nextSnapshot.status.find(" hit ") != std::string::npos;
        for (const game_data::Piece& nextPiece : nextSnapshot.pieces)
        {
            const game_data::Piece* currentPiece = pieceByIdInSnapshot(gameSnapshot, nextPiece.id);
            if (!currentPiece)
            {
                playedPlaceSound = true;
                continue;
            }

            if (currentPiece->row != nextPiece.row || currentPiece->column != nextPiece.column)
            {
                pieceMoveAnimations[nextPiece.id] = {
                    currentPiece->row,
                    currentPiece->column,
                    nextPiece.row,
                    nextPiece.column,
                    animationTime,
                    0.95f};
                playedMoveSound = true;
            }
            if (nextPiece.health < currentPiece->health)
            {
                startPieceDamagedAnimation(nextPiece);
            }
        }

        if (snapshotDescribesAttack)
        {
            for (const game_data::Piece& currentPiece : gameSnapshot.pieces)
            {
                const game_data::Piece* nextActor = pieceByIdInSnapshot(nextSnapshot, currentPiece.id);
                if (!nextActor)
                {
                    continue;
                }

                const std::string attackStatusPrefix = currentPiece.name + " hit ";
                if (nextSnapshot.status.rfind(attackStatusPrefix, 0) != 0)
                {
                    continue;
                }

                const bool actorWasUsed = nextActor->hasActed ||
                    currentPiece.row != nextActor->row ||
                    currentPiece.column != nextActor->column ||
                    nextActor->disabledTurns != currentPiece.disabledTurns;
                if (!actorWasUsed)
                {
                    continue;
                }

                for (const game_data::Piece& currentTarget : gameSnapshot.pieces)
                {
                    if (currentTarget.owner == currentPiece.owner)
                    {
                        continue;
                    }

                    const game_data::ActionResolution action = game_data::resolvePieceAction(
                        gameSnapshot.pieces,
                        gameSnapshot.holes,
                        currentPiece,
                        currentTarget.row,
                        currentTarget.column);
                    if (!action.legal || !action.attacks || action.targetId != currentTarget.id)
                    {
                        continue;
                    }

                    const game_data::Piece* nextTarget = pieceByIdInSnapshot(nextSnapshot, currentTarget.id);
                    const bool targetChanged = nextTarget == nullptr ||
                        nextTarget->health < currentTarget.health ||
                        nextTarget->disabledTurns != currentTarget.disabledTurns;
                    if (!targetChanged)
                    {
                        continue;
                    }

                    startPieceAttackAnimation(currentPiece.id, currentTarget.row, currentTarget.column);
                    playedAttackSound = true;
                    break;
                }
            }
        }

        if (playedPlaceSound)
        {
            audioSystem.play(AudioCue::PiecePlace);
        }
        if (playedMoveSound)
        {
            audioSystem.play(AudioCue::UnitMove);
        }
        if (playedAttackSound)
        {
            audioSystem.play(AudioCue::UnitAttack, playedDeathSound ? 0.9f : 1.0f);
        }
        if (playedDeathSound)
        {
            audioSystem.play(AudioCue::UnitDeath, playedAttackSound ? 0.5f : 1.0f);
        }
        if (playedDematerializeSound)
        {
            audioSystem.play(AudioCue::Dematerialize);
        }
    };

    auto squareAtPixel = [&](sf::Vector2f point) -> std::optional<std::pair<int, int>> {
        const int viewer = haveSnapshot ? gameSnapshot.yourPlayer : 1;
        for (int screenRow = game_data::BoardSize - 1; screenRow >= 0; --screenRow)
        {
            const int row = rowForScreenRow(screenRow, viewer);
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const BoardCellMetrics metrics = boardCellMetricsForViewer(row, column, viewer);
                if (pointInConvex(point, metrics.corners))
                {
                    return std::make_pair(row, column);
                }
            }
        }
        return std::nullopt;
    };

    auto gamePieceAt = [&](int row, int column) -> const game_data::Piece* {
        return game_data::findPieceAt(gameSnapshot.pieces, row, column);
    };

    auto gamePieceById = [&](int id) -> const game_data::Piece* {
        for (const game_data::Piece& piece : gameSnapshot.pieces)
        {
            if (piece.id == id)
            {
                return &piece;
            }
        }
        return nullptr;
    };

    auto commitSandboxSnapshot = [&](game_data::Snapshot nextSnapshot) {
        recomputeSandboxControl(nextSnapshot);
        refreshSandboxPlayerSnapshots(nextSnapshot);
        updatePieceMoveAnimations(nextSnapshot);
        gameSnapshot = std::move(nextSnapshot);
        haveSnapshot = true;
        clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
        if (selectedHandIndex && *selectedHandIndex >= gameSnapshot.hand.size())
        {
            selectedHandIndex.reset();
        }
        if (inspectedHandIndex && *inspectedHandIndex >= gameSnapshot.hand.size())
        {
            inspectedHandIndex.reset();
            inspectedPieceScroll = 0.0f;
        }
    };

    auto showStoryIntro = [&]() {
        currentState = GameState::StoryIntro;
        title.setString("Story");
        centerText(title, 400.0f);
        setMessageY(messageText, 560.0f);
        setMessage(messageText, "", sf::Color::White);
        storyComicPage = 0;
        clearFocus();
    };

    auto beginStory = [&]() {
        sandboxMode = true;
        storyMode = true;
        storyStage = StoryStage::MoveTutorial;
        storyTargetRow = 4;
        storyTargetColumn = 1;
        sandboxPlacementPlayer = 1;
        sandboxPlayerButton.setLabel("P1");
        activeGameSocket.reset();
        currentState = GameState::Game;
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();

        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        gameResultReceived = false;
        gameResultSuccess = false;
        gameOverSoundPlayed = false;
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();
        pieceAttackAnimations.clear();
        pieceDamagedAnimations.clear();
        pieceKilledAnimations.clear();
        dematerializeGhosts.clear();

        game_data::Snapshot snapshot;
        snapshot.phase = static_cast<std::uint8_t>(game_data::Phase::Playing);
        snapshot.activePlayer = 1;
        snapshot.yourPlayer = 1;
        snapshot.winner = 0;
        snapshot.control.fill(0);
        snapshot.holes.fill(0);
        for (const auto& [row, column] : game_data::homeSquares(1))
        {
            snapshot.control[static_cast<std::size_t>(game_data::squareIndex(row, column))] = 1;
        }
        spawnSandboxPiece(snapshot, nextSandboxPieceId, 1, makeStoryTomCard(), 4, 0, true);
        snapshot.status = "Story: click Tinkering Tom, then click the glowing square to move him.";

        haveSnapshot = false;
        commitSandboxSnapshot(std::move(snapshot));
    };

    auto beginSandbox = [&](std::vector<card_data::Card> cards) {
        sandboxMode = true;
        storyMode = false;
        storyStage = StoryStage::None;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        sandboxPlacementPlayer = 1;
        sandboxPlayerButton.setLabel("P1");
        activeGameSocket.reset();
        currentState = GameState::Game;
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();

        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        gameResultReceived = false;
        gameResultSuccess = false;
        gameOverSoundPlayed = false;
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();
        pieceAttackAnimations.clear();
        pieceDamagedAnimations.clear();
        pieceKilledAnimations.clear();
        dematerializeGhosts.clear();

        game_data::Snapshot snapshot;
        snapshot.phase = static_cast<std::uint8_t>(game_data::Phase::Playing);
        snapshot.activePlayer = 1;
        snapshot.yourPlayer = 1;
        snapshot.winner = 0;
        snapshot.control.fill(0);
        snapshot.holes.fill(0);
        for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
        {
            for (const auto& [row, column] : game_data::homeSquares(playerNumber))
            {
                snapshot.control[static_cast<std::size_t>(game_data::squareIndex(row, column))] =
                    static_cast<std::uint8_t>(playerNumber);
            }
        }

        std::sort(cards.begin(), cards.end(), [](const card_data::Card& left, const card_data::Card& right) {
            const bool leftHero = game_data::isHeroCard(left);
            const bool rightHero = game_data::isHeroCard(right);
            if (leftHero != rightHero)
            {
                return leftHero;
            }
            if (left.type != right.type)
            {
                return left.type < right.type;
            }
            return lowerKey(left.title) < lowerKey(right.title);
        });

        snapshot.hand.reserve(cards.size());
        for (const card_data::Card& card : cards)
        {
            game_data::GameCard playable = game_data::toGameCard(card);
            playable.cost = 0;
            playable.heroCost = 0;
            snapshot.hand.push_back(std::move(playable));
        }
        snapshot.status = snapshot.hand.empty()
            ? "Sandbox loaded, but the card database is empty."
            : "Sandbox: all database cards are available and free. Placing for Player 1.";

        haveSnapshot = false;
        commitSandboxSnapshot(std::move(snapshot));
    };

    auto loadSandbox = [&]() {
        currentState = GameState::SandboxLoading;
        title.setString("Sandbox");
        centerText(title, 400.0f);
        setMessageY(messageText, 450.0f);
        setMessage(messageText, "Loading card database...", sf::Color::Yellow);
        pendingSandboxLoad = std::async(std::launch::async, fetchCards);
    };

    auto updateSandboxPlayerButton = [&]() {
        sandboxPlayerButton.setLabel("P" + std::to_string(sandboxPlacementPlayer));
    };

    auto toggleSandboxPlacementPlayer = [&]() {
        if (!sandboxMode || !haveSnapshot)
        {
            return;
        }
        sandboxPlacementPlayer = sandboxPlacementPlayer == 1 ? 2 : 1;
        updateSandboxPlayerButton();
        game_data::Snapshot next = gameSnapshot;
        next.activePlayer = sandboxPlacementPlayer;
        next.status = "Sandbox: placing for Player " + std::to_string(sandboxPlacementPlayer) + ".";
        commitSandboxSnapshot(std::move(next));
    };

    auto cardArtTexture = [&](const std::string& imagePath) -> sf::Texture* {
        return textures.load(imagePath);
    };

    auto walkAnimTexture = [&](const std::string& walkAnimPath) -> sf::Texture* {
        return textures.load(walkAnimPath);
    };

    auto pieceTokenPath = [](const game_data::Piece& piece) -> const std::string& {
        if (piece.separateBaseArt)
        {
            return piece.tokenPath;
        }
        return piece.owner == 1 ? piece.blueTokenPath : piece.redTokenPath;
    };

    auto pieceWalkAnimPath = [](const game_data::Piece& piece) -> const std::string& {
        if (piece.separateBaseArt)
        {
            return piece.walkAnimPath;
        }
        return piece.owner == 1 ? piece.blueWalkAnimPath : piece.redWalkAnimPath;
    };

    auto cardTokenPath = [](const game_data::GameCard& card, int owner) -> const std::string& {
        if (card.separateBaseArt)
        {
            return card.tokenPath;
        }
        return owner == 1 ? card.blueTokenPath : card.redTokenPath;
    };

    auto cardWalkAnimPath = [](const game_data::GameCard& card, int owner) -> const std::string& {
        if (card.separateBaseArt)
        {
            return card.walkAnimPath;
        }
        return owner == 1 ? card.blueWalkAnimPath : card.redWalkAnimPath;
    };

    auto pieceBasePath = [](const game_data::Piece& piece) -> const std::string& {
        return piece.owner == 1 ? piece.pieceBaseBluePath : piece.pieceBaseRedPath;
    };

    auto cardBasePath = [](const game_data::GameCard& card, int owner) -> const std::string& {
        return owner == 1 ? card.pieceBaseBluePath : card.pieceBaseRedPath;
    };

    auto drawPieceVisual = [&](
        const std::string& tokenPath,
        const std::string& walkPath,
        const std::string& idlePath,
        const std::string& basePath,
        bool separateBaseArt,
        bool flipX,
        int walkAnimFrames,
        int idleAnimFrames,
        sf::Vector2f anchor,
        float scale,
        sf::Color tint,
        int walkFrame,
        int idleFrame) {
        const sf::FloatRect target = pieceTargetRect(anchor, scale, true);
        if (separateBaseArt)
        {
            if (sf::Texture* base = textures.load(basePath))
            {
                drawContainSprite(window, *base, target, tint);
            }
        }

        auto drawAnimFrame = [&](const std::string& sheetPath, int frameCountValue, int frame) {
            if (sf::Texture* sheet = walkAnimTexture(sheetPath))
            {
                const int frameCount = std::max(1, frameCountValue);
                const sf::Vector2u sheetSize = sheet->getSize();
                const int frameWidth = static_cast<int>(sheetSize.x / static_cast<unsigned int>(frameCount));
                const int frameHeight = static_cast<int>(sheetSize.y);
                if (frameWidth > 0 && frameHeight > 0)
                {
                    const int clampedFrame = std::clamp(frame, 0, frameCount - 1);
                    drawTextureRectContain(window,
                        *sheet,
                        sf::IntRect({clampedFrame * frameWidth, 0}, {frameWidth, frameHeight}),
                        target,
                        tint,
                        flipX);
                    return true;
                }
            }
            return false;
        };

        if (walkFrame >= 0 && !walkPath.empty() && drawAnimFrame(walkPath, walkAnimFrames, walkFrame))
        {
            return true;
        }
        if (idleFrame >= 0 && !idlePath.empty() && drawAnimFrame(idlePath, idleAnimFrames, idleFrame))
        {
            return true;
        }
        if (sf::Texture* token = textures.load(tokenPath))
        {
            drawContainSprite(window, *token, target, tint, flipX);
            return true;
        }
        if (!walkPath.empty() && drawAnimFrame(walkPath, walkAnimFrames, 0))
        {
            return true;
        }
        return false;
    };

    auto drawCardPiecePreview = [&](const game_data::GameCard& card,
                                    int owner,
                                    sf::Vector2f anchor,
                                    float scale,
                                    bool valid) {
        const sf::Color tint = valid ? sf::Color(255, 255, 255, 220) : sf::Color(220, 120, 110, 190);
        const std::string& tokenPath = cardTokenPath(card, owner);
        const std::string& walkPath = cardWalkAnimPath(card, owner);

        bool drewPiece = false;
        drewPiece = drawPieceVisual(
            tokenPath,
            walkPath,
            "",
            cardBasePath(card, owner),
            card.separateBaseArt,
            card.separateBaseArt && owner == 2,
            card.walkAnimFrames,
            1,
            anchor,
            scale,
            tint,
            -1,
            -1);
        if (!drewPiece)
        {
            if (sf::Texture* art = cardArtTexture(card.imagePath))
            {
                drawContainSprite(window, *art, pieceTargetRect(anchor, scale, false), tint);
                drewPiece = true;
            }
        }

        if (!drewPiece)
        {
            const float radius = PieceBaseWidth * 0.28f * scale;
            sf::CircleShape body(radius);
            body.setPosition({anchor.x - radius, anchor.y - radius * 2.0f});
            body.setFillColor(valid ? ownerColor(owner) : sf::Color(180, 75, 65, 210));
            window.draw(body);
        }

        const unsigned int healthSize =
            static_cast<unsigned int>(std::clamp(12.0f * scale, 10.0f, 17.0f));
        drawText(
            window,
            font,
            std::to_string(card.health),
            healthSize,
            {anchor.x - 5.0f * scale, anchor.y - 21.0f * scale},
            sf::Color(248, 239, 216, 220));
    };

    auto cardInAllLibraryByTitle = [&](const std::string& title) -> const card_data::Card* {
        const auto found = std::find_if(allCardLibrary.begin(), allCardLibrary.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        if (found != allCardLibrary.end())
        {
            return &*found;
        }
        return cardByTitle(title);
    };

    auto drawLargeCollectionCard = [&](const card_data::Card& card, sf::Vector2f position, sf::Vector2f size) {
        drawBeveledPlate(
            window,
            position,
            size,
            sf::Color(18, 23, 23, 244),
            game_data::isHeroCard(card) ? sf::Color(232, 187, 83) : sf::Color(176, 123, 59),
            game_data::isHeroCard(card),
            12.0f);

        drawBeveledPlate(
            window,
            {position.x + 15.0f, position.y + 16.0f},
            {size.x - 30.0f, 150.0f},
            sf::Color(8, 14, 15),
            sf::Color(116, 86, 52),
            false,
            7.0f);
        if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawContainSprite(window, *art, {{position.x + 20.0f, position.y + 20.0f}, {size.x - 40.0f, 142.0f}});
        }

        drawText(window, font, card.title, 22, {position.x + 18.0f, position.y + 178.0f}, sf::Color(248, 239, 216), size.x - 36.0f);
        const std::string typeLine = game_data::isHeroCard(card)
            ? "Hero cost " + std::to_string(game_data::cardInt(card, "heroCost", 0))
            : card.type + "  " + std::to_string(game_data::cardInt(card, "cost", 0)) + " steam";
        drawText(window, font, cardRarityLabel(card) + "  " + typeLine, 16, {position.x + 18.0f, position.y + 210.0f}, cardRarityColor(card), size.x - 36.0f);

        std::string statLine;
        if (card.type == "Unit" || game_data::isHeroCard(card))
        {
            statLine = "HP " + std::to_string(game_data::cardInt(card, "health", 0)) +
                "  Actions " + std::to_string(card.actions.size());
        }
        else
        {
            statLine = "Spell  " + game_data::cardStr(card, "effect", "effect") +
                " " + std::to_string(game_data::cardInt(card, "power", 0));
        }
        drawText(window, font, statLine, 15, {position.x + 18.0f, position.y + 236.0f}, sf::Color(224, 210, 176), size.x - 36.0f);
        drawText(
            window,
            font,
            (starterDeckMode ? "Deck limit " : "Owned ") + std::to_string(ownedCopies(card.title)),
            15,
            {position.x + 18.0f, position.y + 264.0f},
            sf::Color(248, 214, 112),
            size.x - 36.0f);
    };

    auto deckEditorCardDetailsHeight = [&](const std::vector<std::pair<std::string, sf::Color>>& details) {
        float height = 0.0f;
        for (const auto& [description, color] : details)
        {
            (void)color;
            height += static_cast<float>(
                wrapText(font, description, 14, PiecePopupTextWidth - PiecePopupScrollTextXInset * 2.0f).size()) * 18.0f;
            height += 8.0f;
        }
        return height + PiecePopupScrollTextYInset;
    };

    auto deckEditorCardDetailsMaxScroll = [&](const std::vector<std::pair<std::string, sf::Color>>& details) {
        return std::max(0.0f, deckEditorCardDetailsHeight(details) - PiecePopupScrollHeight);
    };

    #include "screens/deck_editor_popup.inl"

    auto showDeckEditorCardPopupIfDoubleClick = [&](const std::string& title, sf::Vector2f clickPos) {
        const sf::Vector2f clickDelta = clickPos - lastDeckEditorCardClickPosition;
        const bool closeToLastClick = clickDelta.x * clickDelta.x + clickDelta.y * clickDelta.y <= 144.0f;
        const bool isDoubleClick = lastDeckEditorClickedCardTitle && *lastDeckEditorClickedCardTitle == title &&
            closeToLastClick && animationTime - lastDeckEditorCardClickTime <= DeckCardDoubleClickSeconds;

        lastDeckEditorClickedCardTitle = title;
        lastDeckEditorCardClickPosition = clickPos;
        lastDeckEditorCardClickTime = animationTime;

        if (!isDoubleClick)
        {
            return false;
        }

        inspectedDeckEditorCardTitle = title;
        inspectedDeckEditorCardScroll = 0.0f;
        lastDeckEditorClickedCardTitle.reset();
        draggingLibraryCard.reset();
        draggingDeckCard.reset();
        dragActive = false;
        clearFocus();
        return true;
    };

    #include "screens/shop_screen.inl"

    #include "screens/admin_users_screen.inl"

    auto handCardAtPixel = [&](sf::Vector2f point) -> std::optional<std::size_t> {
        const std::size_t last = std::min(gameSnapshot.hand.size(), gameHandOffset + VisibleGameHandCards);
        for (std::size_t i = gameHandOffset; i < last; ++i)
        {
            const float x = HandStartX + static_cast<float>(i - gameHandOffset) * (HandCardWidth + HandGap);
            if (isInsideRect(point, x, HandY, HandCardWidth, HandCardHeight))
            {
                return i;
            }
        }
        return std::nullopt;
    };

    auto isDiscardTrashCanAtPixel = [&](sf::Vector2f point) {
        return isInsideRect(
            point,
            TrashCanX - TrashCanDropPadding,
            TrashCanY - TrashCanDropPadding,
            TrashCanSize + TrashCanDropPadding * 2.0f,
            TrashCanSize + TrashCanDropPadding * 2.0f);
    };

    auto gamePieceAtPixel = [&](sf::Vector2f point) -> const game_data::Piece* {
        const std::optional<std::pair<int, int>> square = squareAtPixel(point);
        if (!square)
        {
            return nullptr;
        }
        return gamePieceAt(square->first, square->second);
    };

    auto showPiecePopupIfDoubleClick = [&](sf::Vector2f clickPos) {
        const game_data::Piece* clickedPiece = haveSnapshot ? gamePieceAtPixel(clickPos) : nullptr;
        if (!clickedPiece)
        {
            lastClickedPieceId.reset();
            return false;
        }

        const sf::Vector2f clickDelta = clickPos - lastPieceClickPosition;
        const bool closeToLastClick = clickDelta.x * clickDelta.x + clickDelta.y * clickDelta.y <= 144.0f;
        const bool isDoubleClick = lastClickedPieceId && *lastClickedPieceId == clickedPiece->id &&
            closeToLastClick && animationTime - lastPieceClickTime <= PieceDoubleClickSeconds;

        lastClickedPieceId = clickedPiece->id;
        lastPieceClickPosition = clickPos;
        lastPieceClickTime = animationTime;

        if (!isDoubleClick)
        {
            return false;
        }

        inspectedPieceId = clickedPiece->id;
        inspectedHandIndex.reset();
        inspectedPieceScroll = 0.0f;
        pendingHandClickIndex.reset();
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        selectedHandIndex.reset();
        lastClickedPieceId.reset();
        return true;
    };

    auto updateStoryAfterMove = [&](game_data::Snapshot& snapshot, const game_data::Piece& piece) {
        if (!storyMode || piece.name != "Tinkering Tom")
        {
            return;
        }

        if (storyStage == StoryStage::MoveTutorial)
        {
            if (piece.row == storyTargetRow && piece.column == storyTargetColumn)
            {
                storyStage = StoryStage::ValveChallenge;
                storyTargetRow = 2;
                storyTargetColumn = 3;
                snapshot.status =
                    "Good. Now guide Tom to the sparking valve. He can only step one square at a time.";
            }
            else
            {
                snapshot.status = "Try moving Tom onto the glowing square.";
            }
        }
        else if (storyStage == StoryStage::ValveChallenge)
        {
            if (piece.row == storyTargetRow && piece.column == storyTargetColumn)
            {
                storyStage = StoryStage::Complete;
                storyTargetRow = -1;
                storyTargetColumn = -1;
                snapshot.status = "Tom repaired the valve. The first story part is complete.";
            }
            else
            {
                snapshot.status = "Keep stepping Tom toward the sparking valve.";
            }
        }
    };

    auto sandboxPlayCard = [&](int handIndex, int row, int column) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing ||
            handIndex < 0 || handIndex >= static_cast<int>(gameSnapshot.hand.size()))
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        const game_data::GameCard card = next.hand[static_cast<std::size_t>(handIndex)];
        const int actingPlayer = sandboxPlacementPlayer;
        if (card.type == "Unit" || card.type == "Hero")
        {
            if (!game_data::inBounds(row, column) ||
                next.control[static_cast<std::size_t>(game_data::squareIndex(row, column))] != actingPlayer ||
                pieceAtInSnapshot(next, row, column) != nullptr)
            {
                next.status = "Sandbox pieces deploy onto an empty square controlled by the selected player.";
                commitSandboxSnapshot(std::move(next));
                return;
            }

            spawnSandboxPiece(next, nextSandboxPieceId, actingPlayer, card, row, column, card.type == "Hero");
            next.status = "Sandbox played " + card.title + " for Player " + std::to_string(actingPlayer) + ".";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        if (card.effect == "steam")
        {
            next.status = "Sandbox played " + card.title + ".";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        game_data::Piece* target = game_data::inBounds(row, column)
            ? pieceByIdInSnapshotMutable(next, pieceAtInSnapshot(next, row, column) ? pieceAtInSnapshot(next, row, column)->id : 0)
            : nullptr;
        if (card.effect == "damage")
        {
            if (!target || target->owner == actingPlayer)
            {
                next.status = "That spell needs an enemy target.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            target->health -= card.power;
            game_data::applyDamageStatus(*target, card.power, 0);
            if (target->health <= 0)
            {
                removePieceFromSnapshot(next, target->id);
            }
        }
        else if (card.effect == "heal")
        {
            if (!target || target->owner != actingPlayer)
            {
                next.status = "That spell needs a friendly target.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            target->health = std::min(target->maxHealth, target->health + card.power);
        }

        next.status = "Sandbox played " + card.title + ".";
        commitSandboxSnapshot(std::move(next));
    };

    auto sandboxPlaceHero = [&](int heroIndex, int row, int column) {
        sandboxPlayCard(heroIndex, row, column);
    };

    auto sandboxActWithPiece = [&](int pieceId, int row, int column) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing)
        {
            return;
        }
        if (storyMode && storyStage == StoryStage::Complete)
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        game_data::Piece* piece = pieceByIdInSnapshotMutable(next, pieceId);
        if (!piece)
        {
            return;
        }

        const game_data::PieceActionOutcome outcome =
            game_data::resolvePieceActionThroughHidden(next.pieces, next.holes, *piece, row, column);
        const game_data::ActionResolution& action = outcome.action;
        if (!action.legal)
        {
            next.status = "That piece cannot act there.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
        const int destinationRow = outcome.destinationRow;
        const int destinationColumn = outcome.destinationColumn;

        const int attackerId = piece->id;
        const std::string attackerName = piece->name;
        std::string targetName;
        bool targetDestroyed = false;
        bool targetAtDestination = false;
        bool targetWasHidden = false;

        if (action.attacks)
        {
            game_data::Piece* target = pieceByIdInSnapshotMutable(next, action.targetId);
            if (!target)
            {
                return;
            }
            targetName = target->name;
            targetAtDestination = target->row == destinationRow && target->column == destinationColumn;
            targetWasHidden = target->hidden;
            startPieceAttackAnimation(attackerId, target->row, target->column);
            target->health -= action.damage;
            game_data::applyDamageStatus(*target, action.damage, action.statusTurns);
            if (target->health <= 0)
            {
                targetDestroyed = true;
                removePieceFromSnapshot(next, target->id);
            }
        }

        std::string revealedName;
        if (outcome.revealedPieceId != 0)
        {
            if (game_data::Piece* revealed = pieceByIdInSnapshotMutable(next, outcome.revealedPieceId))
            {
                revealedName = revealed->name;
                game_data::materializeRevealedPiece(*revealed);
            }
        }

        game_data::Piece* acting = pieceByIdInSnapshotMutable(next, attackerId);
        if (!acting)
        {
            return;
        }

        if (action.moves)
        {
            if (!action.attacks || !targetAtDestination || targetDestroyed)
            {
                acting->row = destinationRow;
                acting->column = destinationColumn;
            }
            else
            {
                acting->row = action.stagingRow;
                acting->column = action.stagingColumn;
            }
        }
        acting->disabledTurns = std::max(acting->disabledTurns, action.cooldownTurns);
        acting->hasActed = false;

        if (action.attacks)
        {
            const int effectiveDisabledTurns =
                game_data::disabledTurnsForDamage(action.damage, action.statusTurns);
            next.status = attackerName + " hit " + (targetWasHidden ? "a hidden " : "") +
                targetName + " for " + std::to_string(action.damage);
            if (!targetDestroyed && effectiveDisabledTurns > 0)
            {
                next.status += " and disabled it for " + std::to_string(effectiveDisabledTurns) + " turn(s)";
            }
            next.status += targetDestroyed ? " and destroyed it." : ".";
            if (targetWasHidden && !targetDestroyed)
            {
                next.status += " It materialized!";
            }
        }
        else if (!revealedName.empty())
        {
            next.status = attackerName + " bumped into a hidden " + revealedName +
                "! It materialized, stunned.";
        }
        else
        {
            next.status = attackerName + " moved.";
        }
        if (acting)
        {
            updateStoryAfterMove(next, *acting);
        }
        commitSandboxSnapshot(std::move(next));
    };

    auto sandboxUseAbility = [&](int pieceId) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing)
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        game_data::Piece* piece = pieceByIdInSnapshotMutable(next, pieceId);
        if (!piece || !game_data::pieceAbilityAvailable(next.pieces, *piece))
        {
            return;
        }

        const std::string abilityLabel = game_data::pieceAbilityLabel(*piece);
        const std::string pieceName = piece->name;
        const int pieceOwner = piece->owner;
        const int actingPieceId = piece->id;
        if (piece->ability == "dig")
        {
            if (piece->abilityUses == 0)
            {
                next.status = "That piece has already dug its hole.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            next.holes[static_cast<std::size_t>(game_data::squareIndex(piece->row, piece->column))] = 1;
            if (piece->abilityUses > 0)
            {
                --piece->abilityUses;
            }
        }
        else if (piece->ability == "transform" || piece->ability == "dematerialize")
        {
            int stateCount = 1;
            for (const game_data::ActionProfile& action : piece->actions)
            {
                stateCount = std::max(stateCount, action.state + 1);
            }
            piece->actionState = (piece->actionState + 1) % stateCount;
            piece->hidden = piece->ability == "dematerialize" && piece->actionState != 0;
        }
        else if (piece->ability == "summon")
        {
            const auto found = std::find_if(
                next.hand.begin(),
                next.hand.end(),
                [&](const game_data::GameCard& card) {
                    return card.title == piece->summonTitle && card.type == "Unit";
                });
            if (found == next.hand.end())
            {
                next.status = "That summon does not name a valid unit.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            const auto [row, column] = game_data::summonDestination(*piece);
            if (!game_data::pieceSummonDestinationFree(next.pieces, *piece))
            {
                next.status = "That summon needs an empty space in front.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            spawnSandboxPiece(next, nextSandboxPieceId, pieceOwner, *found, row, column, false);
        }
        else
        {
            return;
        }

        if (game_data::Piece* actingPiece = pieceByIdInSnapshotMutable(next, actingPieceId))
        {
            actingPiece->hasActed = false;
        }
        next.status = pieceName + " used " + abilityLabel + ".";
        commitSandboxSnapshot(std::move(next));
    };

    auto sandboxEndTurn = [&]() {
        if (!sandboxMode || !haveSnapshot)
        {
            return;
        }
        game_data::Snapshot next = gameSnapshot;
        const int endingPlayer = std::clamp(next.activePlayer, 1, 2);
        for (game_data::Piece& piece : next.pieces)
        {
            if (piece.owner == endingPlayer && piece.sleepTurnsRemaining > 0)
            {
                --piece.sleepTurnsRemaining;
            }
        }

        next.activePlayer = endingPlayer == 1 ? 2 : 1;
        const int startingPlayer = next.activePlayer;
        for (game_data::Piece& piece : next.pieces)
        {
            if (piece.owner == startingPlayer)
            {
                piece.hasActed = false;
                if (piece.growTurnsRemaining > 0)
                {
                    --piece.growTurnsRemaining;
                    piece.hasActed = piece.growTurnsRemaining > 0;
                }
                if (piece.disabledTurns > 0)
                {
                    --piece.disabledTurns;
                    piece.hasActed = true;
                }
            }
        }
        next.status = "Sandbox advanced timing to Player " + std::to_string(startingPlayer) + ".";
        commitSandboxSnapshot(std::move(next));
    };

    auto sendGamePacket = [&](sf::Packet& packet) {
        if (activeGameSocket)
        {
            [[maybe_unused]] auto result = activeGameSocket->send(packet);
        }
    };

    auto sendPlaceHero = [&](int heroIndex, int row, int column) {
        if (sandboxMode)
        {
            sandboxPlaceHero(heroIndex, row, column);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::PlaceHero) << heroIndex << row << column;
        sendGamePacket(packet);
    };

    auto sendPlayCard = [&](int handIndex, int row, int column) {
        if (sandboxMode)
        {
            sandboxPlayCard(handIndex, row, column);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::PlayCard) << handIndex << row << column;
        sendGamePacket(packet);
    };

    auto sendMovePiece = [&](int pieceId, int row, int column) {
        if (sandboxMode)
        {
            sandboxActWithPiece(pieceId, row, column);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::MovePiece) << pieceId << row << column;
        sendGamePacket(packet);
    };

    auto sendAttackPiece = [&](int attackerId, int row, int column) {
        if (sandboxMode)
        {
            sandboxActWithPiece(attackerId, row, column);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::AttackPiece) << attackerId << row << column;
        sendGamePacket(packet);
    };

    auto sendUseAbility = [&](int pieceId) {
        if (sandboxMode)
        {
            sandboxUseAbility(pieceId);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::UseAbility) << pieceId;
        sendGamePacket(packet);
    };

    auto sendEndTurn = [&]() {
        if (sandboxMode)
        {
            sandboxEndTurn();
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::EndTurn);
        sendGamePacket(packet);
    };

    auto sendDiscardCard = [&](int handIndex) {
        if (sandboxMode)
        {
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::DiscardCard) << handIndex;
        sendGamePacket(packet);
    };

    auto playerCanDiscardThisTurn = [&]() {
        if (!haveSnapshot || sandboxMode ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing)
        {
            return false;
        }
        const int me = gameSnapshot.yourPlayer;
        if (me < 1 || me > 2 || gameSnapshot.activePlayer != me)
        {
            return false;
        }
        const game_data::PlayerSnapshot& mine = gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        return mine.discardsThisTurn < game_data::MaxDiscardsPerTurn && !gameSnapshot.hand.empty();
    };

    auto canDiscardHandCard = [&](std::size_t handIndex) {
        return playerCanDiscardThisTurn() && handIndex < gameSnapshot.hand.size();
    };

    auto canDiscardInspectedHandCard = [&]() {
        return inspectedHandIndex && canDiscardHandCard(*inspectedHandIndex);
    };

    auto handleHandCardClick = [&](std::size_t handIndex) {
        if (handIndex >= gameSnapshot.hand.size())
        {
            return false;
        }

        const game_data::GameCard& card = gameSnapshot.hand[handIndex];
        selectedPieceId.reset();
        if (card.type != "Unit" && card.effect == "steam" &&
            (sandboxMode ||
             game_data::heroKeywordsAllowCard(
                 gameSnapshot.pieces, gameSnapshot.yourPlayer, card)))
        {
            sendPlayCard(static_cast<int>(handIndex), -1, -1);
            selectedHandIndex.reset();
            return true;
        }
        else
        {
            selectedHandIndex = (selectedHandIndex && *selectedHandIndex == handIndex)
                ? std::nullopt
                : std::optional<std::size_t>(handIndex);
        }
        return false;
    };

    auto flushPendingHandClick = [&]() {
        bool sentImmediateAction = false;
        if (pendingHandClickIndex)
        {
            sentImmediateAction = handleHandCardClick(*pendingHandClickIndex);
            pendingHandClickIndex.reset();
        }
        return sentImmediateAction;
    };

    auto handleHandCardClickOrPopup = [&](sf::Vector2f clickPos) {
        const std::optional<std::size_t> handIndex = haveSnapshot ? handCardAtPixel(clickPos) : std::nullopt;
        if (!handIndex)
        {
            return false;
        }

        const sf::Vector2f clickDelta = clickPos - pendingHandClickPosition;
        const bool closeToLastClick = clickDelta.x * clickDelta.x + clickDelta.y * clickDelta.y <= 144.0f;
        const bool isDoubleClick = pendingHandClickIndex && *pendingHandClickIndex == *handIndex &&
            closeToLastClick && animationTime - pendingHandClickTime <= PieceDoubleClickSeconds;

        if (isDoubleClick)
        {
            inspectedHandIndex = *handIndex;
            inspectedPieceId.reset();
            selectedPieceId.reset();
            selectedHandIndex.reset();
            pendingHandClickIndex.reset();
            inspectedPieceScroll = 0.0f;
            gameDragKind = GameDragKind::None;
            draggingHandIndex.reset();
            draggingPieceId.reset();
            gameDragActive = false;
            return true;
        }

        pendingHandClickIndex = *handIndex;
        pendingHandClickPosition = clickPos;
        pendingHandClickTime = animationTime;
        lastClickedPieceId.reset();
        return true;
    };

    auto resetGameDrag = [&]() {
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
    };

    auto beginPotentialGameDrag = [&](sf::Vector2f clickPos) {
        resetGameDrag();
        if (!haveSnapshot || inspectedPieceId || inspectedHandIndex)
        {
            return;
        }

        const int me = gameSnapshot.yourPlayer;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        if (phase == game_data::Phase::HeroPlacement)
        {
            if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
            {
                gameDragKind = GameDragKind::HandCard;
                draggingHandIndex = *handIndex;
                gameDragStartPos = clickPos;
                gameDragCurrentPos = clickPos;
            }
            return;
        }

        if (phase != game_data::Phase::Playing || (!sandboxMode && gameSnapshot.activePlayer != me))
        {
            return;
        }

        if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
        {
            gameDragKind = GameDragKind::HandCard;
            draggingHandIndex = *handIndex;
            gameDragStartPos = clickPos;
            gameDragCurrentPos = clickPos;
            return;
        }

        if (const game_data::Piece* piece = gamePieceAtPixel(clickPos);
            piece && (sandboxMode || (piece->owner == me && !piece->hasActed)))
        {
            gameDragKind = GameDragKind::Piece;
            draggingPieceId = piece->id;
            gameDragStartPos = clickPos;
            gameDragCurrentPos = clickPos;
        }
    };

    auto finishGameDrag = [&](sf::Vector2f releasePos) {
        if (!gameDragActive || !haveSnapshot)
        {
            resetGameDrag();
            return false;
        }

        if (gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
            *draggingHandIndex < gameSnapshot.hand.size() &&
            isDiscardTrashCanAtPixel(releasePos))
        {
            if (canDiscardHandCard(*draggingHandIndex))
            {
                sendDiscardCard(static_cast<int>(*draggingHandIndex));
                selectedHandIndex.reset();
                selectedPieceId.reset();
                inspectedHandIndex.reset();
                inspectedPieceId.reset();
                pendingHandClickIndex.reset();
            }
            resetGameDrag();
            return true;
        }

        const std::optional<std::pair<int, int>> square = squareAtPixel(releasePos);
        if (!square)
        {
            resetGameDrag();
            return true;
        }

        const auto [row, column] = *square;
        if (gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
            *draggingHandIndex < gameSnapshot.hand.size())
        {
            const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
            if (phase == game_data::Phase::HeroPlacement)
            {
                sendPlaceHero(static_cast<int>(*draggingHandIndex), row, column);
            }
            else if (phase == game_data::Phase::Playing)
            {
                sendPlayCard(static_cast<int>(*draggingHandIndex), row, column);
            }
            selectedHandIndex.reset();
            selectedPieceId.reset();
            pendingHandClickIndex.reset();
        }
        else if (gameDragKind == GameDragKind::Piece && draggingPieceId)
        {
            if (const game_data::Piece* piece = gamePieceById(*draggingPieceId))
            {
                const game_data::PieceActionOutcome outcome = game_data::resolvePieceActionThroughHidden(
                    gameSnapshot.pieces, gameSnapshot.holes, *piece, row, column);
                if (outcome.action.legal)
                {
                    sendMovePiece(piece->id, row, column);
                }
            }
            selectedPieceId.reset();
            selectedHandIndex.reset();
        }

        resetGameDrag();
        return true;
    };

    auto pollGameSocket = [&]() {
        if (!activeGameSocket)
        {
            return;
        }
        sf::Packet packet;
        while (activeGameSocket->receive(packet) == sf::Socket::Status::Done)
        {
            std::uint8_t type = 0;
            packet >> type;
            if (static_cast<network::MessageType>(type) == network::MessageType::GameStateUpdate)
            {
                game_data::Snapshot snapshot;
                if (game_data::readSnapshot(packet, snapshot))
                {
                    updatePieceMoveAnimations(snapshot);
                    gameSnapshot = snapshot;
                    haveSnapshot = true;
                    clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
                    if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                            game_data::Phase::GameOver &&
                        !gameResultReceived)
                    {
                        gameRewardText = "Finalizing match rewards...";
                    }
                    if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                            game_data::Phase::GameOver &&
                        !gameOverSoundPlayed)
                    {
                        gameOverSoundPlayed = true;
                        const int me = gameSnapshot.yourPlayer;
                        audioSystem.play(gameSnapshot.winner == me
                            ? AudioCue::Victory
                            : AudioCue::Defeat);
                    }
                }
            }
            else if (static_cast<network::MessageType>(type) == network::MessageType::GameOver)
            {
                bool success = false;
                std::string message;
                int newRating = playerRating;
                int coinsAwarded = 0;
                bool selfMatch = false;
                packet >> success >> message >> gameRatingChange >> newRating
                       >> coinsAwarded >> selfMatch;
                if (packet)
                {
                    gameResultReceived = true;
                    gameResultSuccess = success;
                    if (success)
                    {
                        playerRating = newRating;
                        playerCoins += coinsAwarded;
                        if (selfMatch)
                        {
                            gameRewardText = "Self-match: no gold awarded.";
                        }
                        else if (coinsAwarded > 0)
                        {
                            gameRewardText =
                                "+" + std::to_string(coinsAwarded) + " coins";
                        }
                        else
                        {
                            gameRewardText.clear();
                        }
                    }
                    else
                    {
                        gameRewardText = "Match rewards unavailable: " + message;
                    }
                }
            }
            packet.clear();
        }
    };

    auto leaveGame = [&]() {
        const bool wasSandbox = sandboxMode;
        if (activeGameSocket)
        {
            sendDisconnect(*activeGameSocket);
            activeGameSocket.reset();
        }
        haveSnapshot = false;
        gameSnapshot = {};
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        gameResultReceived = false;
        gameResultSuccess = false;
        gameOverSoundPlayed = false;
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();
        pieceAttackAnimations.clear();
        pieceDamagedAnimations.clear();
        pieceKilledAnimations.clear();
        dematerializeGhosts.clear();
        sandboxMode = false;
        storyMode = false;
        storyStage = StoryStage::None;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        sandboxPlacementPlayer = 1;
        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        if (wasSandbox)
        {
            showAuthenticatedScreen();
        }
        else
        {
            showAuthenticatedScreen();
        }
    };

    auto handleGameClick = [&](sf::Vector2f clickPos) {
        if (!haveSnapshot)
        {
            return;
        }

        const int me = gameSnapshot.yourPlayer;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        if (phase == game_data::Phase::GameOver)
        {
            return;
        }

        const std::optional<std::pair<int, int>> square = squareAtPixel(clickPos);

        if (phase == game_data::Phase::HeroPlacement)
        {
            if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
            {
                handleHandCardClick(*handIndex);
            }
            else if (square && selectedHandIndex &&
                     *selectedHandIndex < gameSnapshot.hand.size())
            {
                sendPlaceHero(static_cast<int>(*selectedHandIndex), square->first, square->second);
                selectedHandIndex.reset();
            }
            return;
        }

        // Playing phase Ã¢â‚¬â€ only the active player may act.
        if (!sandboxMode && gameSnapshot.activePlayer != me)
        {
            return;
        }

        if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
        {
            handleHandCardClick(*handIndex);
            return;
        }

        if (!square)
        {
            selectedPieceId.reset();
            selectedHandIndex.reset();
            return;
        }

        const auto [row, column] = *square;
        const game_data::Piece* clicked = gamePieceAt(row, column);

        if (selectedHandIndex)
        {
            sendPlayCard(static_cast<int>(*selectedHandIndex), row, column);
            selectedHandIndex.reset();
            return;
        }

        if (selectedPieceId)
        {
            const game_data::Piece* selected = gamePieceById(*selectedPieceId);
            if (selected)
            {
                if (clicked && clicked->owner != (sandboxMode ? selected->owner : me))
                {
                    const game_data::PieceActionOutcome outcome = game_data::resolvePieceActionThroughHidden(
                        gameSnapshot.pieces, gameSnapshot.holes, *selected, row, column);
                    if (outcome.action.legal)
                    {
                        sendMovePiece(selected->id, row, column);
                    }
                    selectedPieceId.reset();
                    return;
                }
                if (clicked && (sandboxMode || clicked->owner == me))
                {
                    selectedPieceId = (!sandboxMode && clicked->hasActed) ? std::nullopt : std::optional<int>(clicked->id);
                    return;
                }
                const game_data::PieceActionOutcome outcome = game_data::resolvePieceActionThroughHidden(
                    gameSnapshot.pieces, gameSnapshot.holes, *selected, row, column);
                if (outcome.action.legal)
                {
                    sendMovePiece(selected->id, row, column);
                }
                selectedPieceId.reset();
                return;
            }
            selectedPieceId.reset();
            return;
        }

        if (clicked && (sandboxMode || (clicked->owner == me && !clicked->hasActed)))
        {
            selectedPieceId = clicked->id;
        }
        else
        {
            selectedPieceId.reset();
        }
    };

    #include "screens/game_screen.inl"

    #include "screens/story_intro_screen.inl"

    #include "screens/deck_select_screen.inl"

    if (const std::optional<std::string> savedToken = loadRememberToken())
    {
        activeRememberToken = *savedToken;
        pendingAutoLogin = true;
        pendingRememberRequested = true;
        title.setString("Signing In");
        centerText(title, 400.0f);
        setMessage(messageText, "Restoring saved login...", sf::Color::Yellow);
        pendingRequest = std::async(std::launch::async, sendRememberLogin, activeRememberToken);
    }

    while (window.isOpen())
    {
        const float deltaTime = clock.restart().asSeconds();
        animationTime += deltaTime;
        audioSystem.update();
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (currentState == GameState::Game)
        {
            pollGameSocket();
        }

        if (pendingRequest &&
            pendingRequest->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ServerResult result = pendingRequest->get();
            pendingRequest.reset();
            if (result.success)
            {
                loggedInUsername = result.username.empty() ? usernameInput.getContent() : result.username;
                activeAccessToken = std::move(result.accessToken);
                bool rememberSaveFailed = false;
                if (!result.rememberToken.empty())
                {
                    activeRememberToken = result.rememberToken;
                    rememberSaveFailed = !saveRememberToken(activeRememberToken);
                }
                else if (pendingRememberRequested)
                {
                    activeRememberToken.clear();
                    clearRememberToken();
                }
                showAuthenticatedScreen();
                if (rememberSaveFailed)
                {
                    setMessage(messageText, "Logged in, but the saved login could not be stored.", sf::Color::Red);
                }
            }
            else
            {
                if (pendingAutoLogin)
                {
                    if (result.rejectStoredCredential)
                    {
                        activeRememberToken.clear();
                        clearRememberToken();
                    }
                    currentState = GameState::Menu;
                    title.setString("Steam Tactics");
                    centerText(title, 400.0f);
                }
                setMessage(messageText, result.message, sf::Color::Red);
            }
            pendingAutoLogin = false;
            pendingRememberRequested = false;
        }

        if (pendingAccountState &&
            pendingAccountState->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AccountStateResult result = pendingAccountState->get();
            pendingAccountState.reset();
            if (!loggedInUsername.empty() && result.success)
            {
                applyAccountState(result);
            }
            else if (!loggedInUsername.empty() && currentState == GameState::Authenticated)
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingShopLoad &&
            pendingShopLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ShopLoadResult result = pendingShopLoad->get();
            pendingShopLoad.reset();
            if (!loggedInUsername.empty() && currentState == GameState::Shop)
            {
                allCardLibrary = std::move(result.cards);
                playerCoins = result.coins;
                playerCollection = std::move(result.collection);
                if (coinPurchasePolling && result.success && result.coins > coinPurchaseStartingCoins)
                {
                    const int coinsAdded = result.coins - coinPurchaseStartingCoins;
                    coinPurchasePolling = false;
                    setMessage(
                        messageText,
                        "Payment complete. +" + std::to_string(coinsAdded) + " coins added.",
                        sf::Color(120, 220, 150));
                }
                else if (coinPurchasePolling)
                {
                    setMessage(
                        messageText,
                        result.success ? "Waiting for payment to complete..." : "Could not refresh yet. Retrying...",
                        result.success ? sf::Color::Yellow : sf::Color(240, 170, 90));
                }
                else
                {
                    setMessage(
                        messageText,
                        result.success ? "Spend 5 coins to reveal a random card." : result.message,
                        result.success ? sf::Color(120, 220, 150) : sf::Color::Red);
                }
            }
        }

        if (coinPurchasePolling && currentState == GameState::Shop)
        {
            if (animationTime >= coinPurchasePollDeadline)
            {
                coinPurchasePolling = false;
                setMessage(
                    messageText,
                    "Payment refresh timed out. Use Refresh after checkout completes.",
                    sf::Color(240, 170, 90));
            }
            else if (animationTime >= nextCoinPurchasePollAt && !shopBusy())
            {
                nextCoinPurchasePollAt = animationTime + CoinPurchasePollIntervalSeconds;
                setMessage(messageText, "Checking for completed payment...", sf::Color::Yellow);
                pendingShopLoad = std::async(std::launch::async, loadShopData, activeAccessToken);
            }
        }

        if (pendingShopPurchase &&
            pendingShopPurchase->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AccountCommandResult result = pendingShopPurchase->get();
            pendingShopPurchase.reset();
            if (!loggedInUsername.empty() && result.success)
            {
                playerCoins = result.coins;
                incrementCollection(result.cardTitle);
                revealedCardTitle = result.cardTitle;
                revealStartedAt = animationTime;
                setMessage(messageText, result.message + " Dismiss it before buying another.", sf::Color(120, 220, 150));
            }
            else if (!loggedInUsername.empty())
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingAdminUsersLoad &&
            pendingAdminUsersLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AdminUsersLoadResult result = pendingAdminUsersLoad->get();
            pendingAdminUsersLoad.reset();
            if (!loggedInUsername.empty() && currentState == GameState::AdminUsers)
            {
                if (result.success)
                {
                    adminUsers = std::move(result.users);
                    adminUsersTotalCount = result.totalCount;
                    adminUsersPage = result.page;
                    adminUsersPageSize = result.pageSize == 0 ? adminUsersPageSize : result.pageSize;
                    if (!adminUsers.empty())
                    {
                        if (!selectedAdminUser || *selectedAdminUser >= adminUsers.size())
                        {
                            selectedAdminUser = 0;
                        }
                    }
                    else
                    {
                        selectedAdminUser.reset();
                    }
                    setMessage(messageText, result.message, sf::Color(120, 220, 150));
                }
                else
                {
                    setMessage(messageText, result.message, sf::Color::Red);
                }
            }
        }

        if (pendingAdminPrivilege &&
            pendingAdminPrivilege->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AdminUserPrivilegeResult result = pendingAdminPrivilege->get();
            pendingAdminPrivilege.reset();
            if (!loggedInUsername.empty() && currentState == GameState::AdminUsers)
            {
                if (result.success)
                {
                    if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
                    {
                        adminUsers[*selectedAdminUser].isAdmin = result.targetIsAdmin;
                    }
                    if (!result.targetIsAdmin && selectedAdminUser && *selectedAdminUser < adminUsers.size() &&
                        adminUsers[*selectedAdminUser].username == loggedInUsername)
                    {
                        loggedInIsAdmin = false;
                    }
                    setMessage(messageText, result.message, sf::Color(120, 220, 150));
                }
                else
                {
                    setMessage(messageText, result.message, sf::Color::Red);
                }
            }
        }

        if (pendingAdminGold &&
            pendingAdminGold->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AdminUserGoldResult result = pendingAdminGold->get();
            pendingAdminGold.reset();
            if (!loggedInUsername.empty() && currentState == GameState::AdminUsers)
            {
                const auto target = std::find_if(
                    adminUsers.begin(),
                    adminUsers.end(),
                    [&](const network::AdminUserSummary& user) {
                        return user.username == result.targetUsername;
                    });
                if (result.success && target != adminUsers.end())
                {
                    target->gold = result.targetGold;
                }
                setMessage(
                    messageText,
                    result.message,
                    result.success ? sf::Color(120, 220, 150) : sf::Color::Red);
            }
        }

        if (pendingAdminUserDelete &&
            pendingAdminUserDelete->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AdminUserDeleteResult result = pendingAdminUserDelete->get();
            pendingAdminUserDelete.reset();
            if (!loggedInUsername.empty() && currentState == GameState::AdminUsers)
            {
                if (result.success)
                {
                    const auto target = std::find_if(
                        adminUsers.begin(),
                        adminUsers.end(),
                        [&](const network::AdminUserSummary& user) {
                            return user.username == result.targetUsername;
                        });
                    if (target != adminUsers.end())
                    {
                        adminUsers.erase(target);
                    }
                    if (adminUsersTotalCount > 0)
                    {
                        --adminUsersTotalCount;
                    }
                    selectedAdminUser.reset();
                    adminUserDeleteTarget.clear();
                }
                setMessage(
                    messageText,
                    result.message,
                    result.success ? sf::Color(120, 220, 150) : sf::Color::Red);
            }
        }

        if (pendingPasswordChange &&
            pendingPasswordChange->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AccountCommandResult result = pendingPasswordChange->get();
            pendingPasswordChange.reset();
            if (!loggedInUsername.empty() && currentState == GameState::ChangePassword)
            {
                setMessage(
                    messageText,
                    result.message,
                    result.success ? sf::Color(120, 220, 150) : sf::Color::Red);
                if (result.success)
                {
                    activeRememberToken.clear();
                    clearRememberToken();
                    currentPasswordInput.clear();
                    newPasswordInput.clear();
                    confirmNewPasswordInput.clear();
                    clearFocus();
                    passwordChangedPopupVisible = true;
                }
            }
        }

        if (pendingMatchmaking &&
            pendingMatchmaking->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ServerResult result = pendingMatchmaking->get();
            pendingMatchmaking.reset();
            activeMatchmakingCancel.reset();
            matchmakingCancelRequested = false;
            cancelMatchmakingButton.setLabel("Cancel");
            playAiButton.setLabel("Play vs AI");
            if (result.success)
            {
                showGameScreen(result.gameSocket);
            }
            else
            {
                currentState = GameState::DeckSelect;
                title.setString("Select Deck");
                centerText(title, 400.0f);
                setMessageY(messageText, 524.0f);
                setMessage(
                    messageText,
                    result.message,
                    result.cancelled ? sf::Color(120, 220, 150) : sf::Color::Red);
            }
        }

        if (pendingSandboxLoad &&
            pendingSandboxLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            CardListResult result = pendingSandboxLoad->get();
            pendingSandboxLoad.reset();
            if (currentState == GameState::SandboxLoading)
            {
                if (result.success)
                {
                    beginSandbox(std::move(result.cards));
                }
                else
                {
                    currentState = GameState::Menu;
                    title.setString("Steam Tactics");
                    centerText(title, 400.0f);
                    setMessageY(messageText, 450.0f);
                    setMessage(messageText, result.message, sf::Color::Red);
                }
            }
        }

        if (pendingPlayLoad &&
            pendingPlayLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckEditorLoadResult result = pendingPlayLoad->get();
            pendingPlayLoad.reset();
            cardLibrary = std::move(result.cards);
            playerDecks = std::move(result.decks);
            playerCoins = result.coins;
            playerCollection = std::move(result.collection);
            const bool hadSavedDecks = !playerDecks.empty();
            playerDecks.erase(
                std::remove_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                    return !deckValidationError(deck).empty();
                }),
                playerDecks.end());
            sortDecks();
            deckListOffset = 0;
            selectedDeck = playerDecks.empty() ? std::nullopt : std::optional<std::size_t>(0);
            if (result.success)
            {
                setMessage(messageText,
                           playerDecks.empty()
                               ? (hadSavedDecks ? "No playable decks. Fix one in the Deck Editor."
                                                : "No decks yet. Build one in the Deck Editor.")
                               : "Pick a deck and find a match.",
                           playerDecks.empty() ? sf::Color(220, 180, 120) : sf::Color(120, 220, 150));
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingDeckEditorLoad &&
            pendingDeckEditorLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckEditorLoadResult result = pendingDeckEditorLoad->get();
            pendingDeckEditorLoad.reset();
            if (result.success)
            {
                cardLibrary = std::move(result.cards);
                playerDecks = std::move(result.decks);
                playerCoins = result.coins;
                playerCollection = std::move(result.collection);
                sortDecks();
                applyCollectionFilters();
                if (!playerDecks.empty())
                {
                    selectDeck(0);
                }
                else
                {
                    selectedDeck.reset();
                    editingDeck = {};
                    activeDeckOriginalName.clear();
                    deckNameInput.clear();
                }
                showDeckEditorDeckList();
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                cardLibrary = std::move(result.cards);
                playerCoins = result.coins;
                playerCollection = std::move(result.collection);
                applyCollectionFilters();
                playerDecks.clear();
                selectedDeck.reset();
                editingDeck = {};
                activeDeckOriginalName.clear();
                deckNameInput.clear();
                showDeckEditorDeckList();
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingStarterDeckLoad &&
            pendingStarterDeckLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            StarterDeckLoadResult result = pendingStarterDeckLoad->get();
            pendingStarterDeckLoad.reset();
            if (currentState == GameState::DeckEditor && starterDeckMode)
            {
                if (result.success)
                {
                    cardLibrary = std::move(result.cards);
                    playerDecks = {result.deck};
                    editingDeck = result.deck;
                    applyCollectionFilters();
                    activeDeckOriginalName = result.deck.name;
                    deckNameInput.setContent(result.deck.name);
                    selectedDeckCard.reset();
                    deckCardListOffset = 0;
                    libraryOffset = 0;
                    setMessage(
                        messageText,
                        "Every new account starts with this deck.",
                        sf::Color(120, 220, 150));
                }
                else
                {
                    setMessage(messageText, result.message, sf::Color::Red);
                }
            }
        }

        if (pendingDeckSave &&
            pendingDeckSave->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckCommandResult result = pendingDeckSave->get();
            pendingDeckSave.reset();
            if (result.success)
            {
                const auto existing = std::find_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                    return (!result.originalName.empty() && deck.name == result.originalName) || deck.name == result.deck.name;
                });

                if (existing != playerDecks.end())
                {
                    *existing = result.deck;
                }
                else
                {
                    playerDecks.push_back(result.deck);
                }

                sortDecks();
                selectDeckByName(result.deck.name);
                deckUnsavedChangesPopupVisible = false;
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingDeckDelete &&
            pendingDeckDelete->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckCommandResult result = pendingDeckDelete->get();
            pendingDeckDelete.reset();
            if (result.success)
            {
                playerDecks.erase(
                    std::remove_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                        return deck.name == result.originalName;
                    }),
                    playerDecks.end());
                if (!playerDecks.empty())
                {
                    const std::size_t nextIndex = selectedDeck && *selectedDeck < playerDecks.size()
                        ? *selectedDeck
                        : playerDecks.size() - 1;
                    selectDeck(nextIndex);
                }
                else
                {
                    selectedDeck.reset();
                    editingDeck = {};
                    activeDeckOriginalName.clear();
                    deckNameInput.clear();
                }
                showDeckEditorDeckList();
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>();
                mousePressed && mousePressed->button == sf::Mouse::Button::Left)
            {
                const sf::Vector2f clickPos = window.mapPixelToCoords(mousePressed->position);
                if (exitDesktopPopupVisible)
                {
                    if (confirmExitDesktopButton.isClicked(clickPos))
                    {
                        window.close();
                        break;
                    }
                    if (cancelExitDesktopButton.isClicked(clickPos) ||
                        !isInsideRect(clickPos, 220.0f, 188.0f, 360.0f, 220.0f))
                    {
                        exitDesktopPopupVisible = false;
                    }
                    continue;
                }

                if (deckUnsavedChangesPopupVisible)
                {
                    if (discardDeckChangesButton.isClicked(clickPos))
                    {
                        discardDeckEditChanges();
                    }
                    else if (keepEditingDeckButton.isClicked(clickPos) ||
                             !isInsideRect(clickPos, 220.0f, 188.0f, 360.0f, 220.0f))
                    {
                        deckUnsavedChangesPopupVisible = false;
                    }
                    continue;
                }

                if ((currentState == GameState::Menu || currentState == GameState::Authenticated) &&
                    exitDesktopCloseButtonClicked(clickPos))
                {
                    exitDesktopPopupVisible = true;
                    continue;
                }

                const bool screenHasExitButton =
                    currentState == GameState::SandboxLoading;
                if (screenHasExitButton && exitDesktopButton.isClicked(clickPos))
                {
                    window.close();
                    break;
                }

                if (currentState == GameState::Matchmaking &&
                    cancelMatchmakingButton.isClicked(clickPos))
                {
                    requestMatchmakingCancel();
                }

                if (currentState == GameState::Matchmaking &&
                    playAiButton.isClicked(clickPos) &&
                    activeMatchmakingCancel &&
                    !activeMatchmakingCancel->aiRequested.load())
                {
                    activeMatchmakingCancel->aiRequested.store(true);
                    playAiButton.setLabel("Starting...");
                    setMessage(messageText, "Requesting AI match...", sf::Color::Yellow);
                }
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>();
                mousePressed && mousePressed->button == sf::Mouse::Button::Left &&
                !pendingRequest && !pendingMatchmaking && !pendingSandboxLoad)
            {
                sf::Vector2f clickPos = window.mapPixelToCoords(mousePressed->position);
                if (currentState == GameState::Menu)
                {
                    if (loginButton.isClicked(clickPos))
                    {
                        currentState = GameState::Login;
                        title.setString("Login");
                        centerText(title, 400.0f);
                        setMessageY(messageText, 450.0f);
                        resetForm(usernameInput, passwordInput, confirmInput, messageText);
                        rememberMeChecked = false;
                        passwordVisible = false;
                        updatePasswordVisibility();
                        focusLoginInput(0);
                    }
                    else if (createButton.isClicked(clickPos))
                    {
                        currentState = GameState::CreateAccount;
                        title.setString("Create Account");
                        centerText(title, 400.0f);
                        setMessageY(messageText, 450.0f);
                        resetForm(usernameInput, passwordInput, confirmInput, messageText);
                        passwordVisible = false;
                        updatePasswordVisibility();
                        focusCreateInput(0);
                    }
                    else if (menuOptionsButton.isClicked(clickPos))
                    {
                        showOptionsScreen(GameState::Menu);
                    }
                }
                else if (currentState == GameState::StoryIntro)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (storyComicPage + 1 >= 3)
                    {
                        beginStory();
                    }
                    else
                    {
                        ++storyComicPage;
                    }
                }
                else if (currentState == GameState::Options)
                {
                    if (const std::optional<std::size_t> tabIndex = optionsTabs.clickedIndex(clickPos))
                    {
                        setActiveOptionsTab(static_cast<OptionsTab>(*tabIndex));
                    }
                    else if (activeOptionsTab == OptionsTab::Graphics && displayModeButton.isClicked(clickPos))
                    {
                        pendingDisplaySettings.fullscreen = !pendingDisplaySettings.fullscreen;
                        updateOptionsLabels();
                    }
                    else if (activeOptionsTab == OptionsTab::Graphics && previousResolutionButton.isClicked(clickPos))
                    {
                        selectedResolution = selectedResolution == 0
                            ? displayResolutions.size() - 1
                            : selectedResolution - 1;
                        updateOptionsLabels();
                    }
                    else if (activeOptionsTab == OptionsTab::Graphics && nextResolutionButton.isClicked(clickPos))
                    {
                        selectedResolution = (selectedResolution + 1) % displayResolutions.size();
                        updateOptionsLabels();
                    }
                    else if (activeOptionsTab == OptionsTab::Graphics && applyOptionsButton.isClicked(clickPos))
                    {
                        const sf::Vector2u size = displayResolutions[selectedResolution];
                        pendingDisplaySettings.width = size.x;
                        pendingDisplaySettings.height = size.y;
                        createDisplayWindow(window, pendingDisplaySettings, desktopMode, fullscreenModes);
                        displaySettings = pendingDisplaySettings;
                        selectedResolution = displayResolutionIndex(
                            displayResolutions,
                            {displaySettings.width, displaySettings.height});
                        updateOptionsLabels();
                        const bool saved = saveDisplaySettings(displaySettings);
                        setMessage(
                            messageText,
                            saved ? "Display settings applied and saved." : "Settings applied, but could not be saved.",
                            saved ? sf::Color(120, 220, 150) : sf::Color::Red);
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && allAudioSlider.beginDrag(clickPos))
                    {
                        audioSystem.setAllVolume(allAudioSlider.getValue());
                        updateOptionsLabels();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && musicAudioSlider.beginDrag(clickPos))
                    {
                        audioSystem.setMusicVolume(musicAudioSlider.getValue());
                        updateOptionsLabels();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && soundFxAudioSlider.beginDrag(clickPos))
                    {
                        audioSystem.setSoundEffectsVolume(soundFxAudioSlider.getValue());
                        updateOptionsLabels();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && muteAllAudioCheckbox.isClicked(clickPos))
                    {
                        audioSystem.setAllMuted(!audioSystem.isAllMuted());
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && muteMusicCheckbox.isClicked(clickPos))
                    {
                        audioSystem.setMusicMuted(!audioSystem.isMusicMuted());
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && muteSoundFxCheckbox.isClicked(clickPos))
                    {
                        audioSystem.setSoundEffectsMuted(!audioSystem.isSoundEffectsMuted());
                    }
                    else if (activeOptionsTab == OptionsTab::Account &&
                             optionsReturnState == GameState::Authenticated &&
                             changePasswordOptionButton.isClicked(clickPos))
                    {
                        showChangePasswordScreen();
                    }
                    else if (optionsBackButton.isClicked(clickPos))
                    {
                        leaveOptionsScreen();
                    }
                }
                else if (currentState == GameState::ChangePassword)
                {
                    if (passwordChangedPopupVisible)
                    {
                        if (dismissPasswordChangedButton.isClicked(clickPos))
                        {
                            dismissPasswordChangedPopup();
                        }
                    }
                    else if (changePasswordBackButton.isClicked(clickPos) && !pendingPasswordChange)
                    {
                        leaveChangePasswordScreen();
                    }
                    else if (changePasswordSubmitButton.isClicked(clickPos) && !pendingPasswordChange)
                    {
                        submitPasswordChange();
                    }
                    else if (currentPasswordVisibilityIcon.isClicked(clickPos) ||
                             newPasswordVisibilityIcon.isClicked(clickPos) ||
                             confirmNewPasswordVisibilityIcon.isClicked(clickPos))
                    {
                        changePasswordsVisible = !changePasswordsVisible;
                        updateChangePasswordVisibility();
                    }
                    else if (currentPasswordInput.contains(clickPos))
                    {
                        focusChangePasswordInput(0);
                    }
                    else if (newPasswordInput.contains(clickPos))
                    {
                        focusChangePasswordInput(1);
                    }
                    else if (confirmNewPasswordInput.contains(clickPos))
                    {
                        focusChangePasswordInput(2);
                    }
                    else
                    {
                        clearFocus();
                    }
                }
                else if (currentState == GameState::Login)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        returnToMenu();
                    }
                    else if (loginSubmitButton.isClicked(clickPos))
                    {
                        submitLogin();
                    }
                    else if (rememberMeCheckbox.isClicked(clickPos))
                    {
                        rememberMeChecked = !rememberMeChecked;
                    }
                    else if (passwordVisibilityIcon.isClicked(clickPos))
                    {
                        passwordVisible = !passwordVisible;
                        updatePasswordVisibility();
                    }
                    else if (usernameInput.contains(clickPos))
                    {
                        focusLoginInput(0);
                    }
                    else if (passwordInput.contains(clickPos))
                    {
                        focusLoginInput(1);
                    }
                    else
                    {
                        clearFocus();
                    }
                }
                else if (currentState == GameState::CreateAccount)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        returnToMenu();
                    }
                    else if (createSubmitButton.isClicked(clickPos))
                    {
                        submitCreateAccount();
                    }
                    else if (passwordVisibilityIcon.isClicked(clickPos) || confirmVisibilityIcon.isClicked(clickPos))
                    {
                        passwordVisible = !passwordVisible;
                        updatePasswordVisibility();
                    }
                    else if (usernameInput.contains(clickPos))
                    {
                        focusCreateInput(0);
                    }
                    else if (passwordInput.contains(clickPos))
                    {
                        focusCreateInput(1);
                    }
                    else if (confirmInput.contains(clickPos))
                    {
                        focusCreateInput(2);
                    }
                    else
                    {
                        clearFocus();
                    }
                }
                else if (currentState == GameState::Authenticated)
                {
                    if (storyButton.isClicked(clickPos))
                    {
                        showStoryIntro();
                    }
                    else if (playButton.isClicked(clickPos))
                    {
                        showDeckSelect();
                    }
                    else if (sandboxButton.isClicked(clickPos))
                    {
                        loadSandbox();
                    }
                    else if (deckEditorButton.isClicked(clickPos))
                    {
                        loadDeckEditor();
                    }
                    else if (shopButton.isClicked(clickPos))
                    {
                        loadShop();
                    }
                    else if (loggedInIsAdmin && adminCardEditorButton.isClicked(clickPos))
                    {
                        showCardEditorScreen();
                    }
                    else if (loggedInIsAdmin && adminUsersButton.isClicked(clickPos))
                    {
                        adminUsersPage = 0;
                        loadAdminUsersScreen();
                    }
                    else if (authenticatedOptionsButton.isClicked(clickPos))
                    {
                        showOptionsScreen(GameState::Authenticated);
                    }
                    else if (logoutButton.isClicked(clickPos))
                    {
                        const std::string rememberTokenToRevoke = activeRememberToken;
                        const std::string accessTokenToRevoke = activeAccessToken;
                        activeRememberToken.clear();
                        clearRememberToken();
                        if (!rememberTokenToRevoke.empty() || !accessTokenToRevoke.empty())
                        {
                            pendingLogout = std::async(
                                std::launch::async,
                                revokeLoginTokens,
                                rememberTokenToRevoke,
                                accessTokenToRevoke);
                        }
                        returnToMenu();
                    }
                }
                else if (currentState == GameState::AdminUsers)
                {
                    if (deleteUserPopupVisible)
                    {
                        if (confirmDeleteUserButton.isClicked(clickPos))
                        {
                            confirmUserDeletion();
                        }
                        else if (cancelDeleteUserButton.isClicked(clickPos))
                        {
                            dismissDeleteUserPopup();
                        }
                    }
                    else if (adminBackButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (adminTabs.clickedIndex(clickPos).value_or(0) == 1)
                    {
                        loadStarterDeckEditor();
                    }
                    else if (adminPrevPageButton.isClicked(clickPos) && adminUsersPage > 0)
                    {
                        --adminUsersPage;
                        loadAdminUsersScreen();
                    }
                    else if (adminNextPageButton.isClicked(clickPos) &&
                             (adminUsersPage + 1) * adminUsersPageSize < adminUsersTotalCount)
                    {
                        ++adminUsersPage;
                        loadAdminUsersScreen();
                    }
                    else if (adminRefreshButton.isClicked(clickPos))
                    {
                        searchAdminUsers();
                    }
                    else if (const std::optional<std::size_t> userIndex = rowIndexAt(
                                 clickPos,
                                 38.0f,
                                 AdminUserRowY,
                                 704.0f,
                                 AdminUserRowHeight,
                                 AdminUsersPageSize,
                                 0,
                                 adminUsers.size()))
                    {
                        selectedAdminUser = *userIndex;
                    }
                    else if (adminSearchInput.contains(clickPos))
                    {
                        clearFocus();
                        adminSearchInput.setActive(true);
                    }
                    else if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
                    {
                        const std::string& targetUsername = adminUsers[*selectedAdminUser].username;
                        if (adminGrantGoldButton.isClicked(clickPos))
                        {
                            changeSelectedUserGold(true);
                        }
                        else if (adminRemoveGoldButton.isClicked(clickPos))
                        {
                            changeSelectedUserGold(false);
                        }
                        else if (adminGoldInput.contains(clickPos))
                        {
                            clearFocus();
                            adminGoldInput.setActive(true);
                        }
                        else if (targetUsername != loggedInUsername && adminDeleteButton.isClicked(clickPos))
                        {
                            openDeleteUserPopup();
                        }
                        else if (adminUsers[*selectedAdminUser].isAdmin)
                        {
                            if (adminRevokeButton.isClicked(clickPos))
                            {
                                if (targetUsername == loggedInUsername)
                                {
                                    setMessage(messageText, "You cannot revoke your own admin privilege", sf::Color::Red);
                                }
                                else
                                {
                                    pendingAdminPrivilege = std::async(
                                        std::launch::async,
                                        updateAdminUserPrivilege,
                                        activeAccessToken,
                                        targetUsername,
                                        false);
                                    setMessage(messageText, "Revoking admin privilege...", sf::Color::Yellow);
                                }
                            }
                        }
                        else if (adminGrantButton.isClicked(clickPos))
                        {
                            pendingAdminPrivilege = std::async(
                                std::launch::async,
                                updateAdminUserPrivilege,
                                activeAccessToken,
                                targetUsername,
                                true);
                            setMessage(messageText, "Granting admin privilege...", sf::Color::Yellow);
                        }
                        else
                        {
                            adminSearchInput.setActive(false);
                            adminGoldInput.setActive(false);
                        }
                    }
                    else
                    {
                        adminSearchInput.setActive(false);
                        adminGoldInput.setActive(false);
                    }
                }
                else if (currentState == GameState::DeckSelect)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (findMatchButton.isClicked(clickPos))
                    {
                        findMatch();
                    }
                    else if (const std::optional<std::size_t> deckIndex = rowIndexAt(
                                 clickPos, 266.0f, 172.0f, 268.0f, DeckRowHeight,
                                 VisibleDeckRows, deckListOffset, playerDecks.size()))
                    {
                        selectedDeck = *deckIndex;
                    }
                }
                else if (currentState == GameState::Game)
                {
                    if (inspectedPieceId || inspectedHandIndex)
                    {
                        if (canDiscardInspectedHandCard() && discardCardButton.isClicked(clickPos))
                        {
                            sendDiscardCard(static_cast<int>(*inspectedHandIndex));
                            inspectedPieceId.reset();
                            inspectedHandIndex.reset();
                            inspectedPieceScroll = 0.0f;
                            selectedPieceId.reset();
                            selectedHandIndex.reset();
                            resetGameDrag();
                        }
                        else if (closePiecePopupButton.isClicked(clickPos) ||
                            !isInsideRect(clickPos, PiecePopupX, PiecePopupY, PiecePopupWidth, PiecePopupHeight))
                        {
                            inspectedPieceId.reset();
                            inspectedHandIndex.reset();
                            inspectedPieceScroll = 0.0f;
                            resetGameDrag();
                        }
                    }
                    else if (leaveGameButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        leaveGame();
                    }
                    else if (haveSnapshot && selectedPieceId &&
                             static_cast<game_data::Phase>(gameSnapshot.phase) == game_data::Phase::Playing &&
                             (sandboxMode || gameSnapshot.activePlayer == gameSnapshot.yourPlayer) &&
                             abilityButton.isClicked(clickPos))
                    {
                        if (const game_data::Piece* piece = gamePieceById(*selectedPieceId);
                            piece && (sandboxMode || (piece->owner == gameSnapshot.yourPlayer && !piece->hasActed)) &&
                            game_data::pieceAbilityAvailable(gameSnapshot.pieces, *piece))
                        {
                            pendingHandClickIndex.reset();
                            sendUseAbility(piece->id);
                            selectedPieceId.reset();
                            selectedHandIndex.reset();
                        }
                    }
                    else if (sandboxMode && !storyMode && sandboxPlayerButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        toggleSandboxPlacementPlayer();
                    }
                    else if (sandboxMode && !storyMode && sandboxAdvanceTurnButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        sendEndTurn();
                        selectedPieceId.reset();
                        selectedHandIndex.reset();
                    }
                    else if (haveSnapshot &&
                             static_cast<game_data::Phase>(gameSnapshot.phase) == game_data::Phase::Playing &&
                             !sandboxMode &&
                             gameSnapshot.activePlayer == gameSnapshot.yourPlayer &&
                             endTurnButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        sendEndTurn();
                        selectedPieceId.reset();
                        selectedHandIndex.reset();
                    }
                    else
                    {
                        beginPotentialGameDrag(clickPos);
                        if (handleHandCardClickOrPopup(clickPos))
                        {
                        }
                        else
                        {
                            const bool consumedByPendingCard = flushPendingHandClick();
                            if (!consumedByPendingCard && !showPiecePopupIfDoubleClick(clickPos))
                            {
                                handleGameClick(clickPos);
                            }
                        }
                    }
                }
                else if (currentState == GameState::DeckEditor)
                {
                    draggingLibraryCard.reset();
                    draggingDeckCard.reset();
                    dragActive = false;

                    if (inspectedDeckEditorCardTitle)
                    {
                        if (closeDeckCardPopupButton.isClicked(clickPos) ||
                            !isInsideRect(clickPos, PiecePopupX, PiecePopupY, PiecePopupWidth, PiecePopupHeight))
                        {
                            inspectedDeckEditorCardTitle.reset();
                            lastDeckEditorClickedCardTitle.reset();
                            inspectedDeckEditorCardScroll = 0.0f;
                        }
                    }
                    else if (deckBackButton.isClicked(clickPos) && !deckEditorBusy())
                    {
                        if (deckEditorMode == DeckEditorMode::EditDeck)
                        {
                            requestLeaveDeckEdit();
                        }
                        else
                        {
                            showAuthenticatedScreen();
                        }
                    }
                    else if (starterDeckMode && !deckEditorBusy() &&
                             adminTabs.clickedIndex(clickPos).value_or(1) == 0)
                    {
                        requestLeaveDeckEdit();
                    }
                    else if (!deckEditorBusy())
                    {
                        if (deckEditorMode == DeckEditorMode::DeckList && newDeckButton.isClicked(clickPos))
                        {
                            createNewDeck();
                            applyCollectionFilters();
                            setMessage(messageText, "Editing a new deck. Save to store it.", sf::Color::Yellow);
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList && refreshDeckButton.isClicked(clickPos))
                        {
                            loadDeckEditor();
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList && editDeckButton.isClicked(clickPos))
                        {
                            editSelectedDeck();
                            applyCollectionFilters();
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList && deleteDeckButton.isClicked(clickPos))
                        {
                            deleteCurrentDeck();
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && removeCardButton.isClicked(clickPos))
                        {
                            removeSelectedCard();
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionHeroFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionTypeFilter(0);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionUnitFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionTypeFilter(1);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionSpellFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionTypeFilter(2);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionBioFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionKeywordFilter(0);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionMechanicalFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionKeywordFilter(1);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionOccultFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionKeywordFilter(2);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && collectionRiffraffFilterCheckbox.isClicked(clickPos))
                        {
                            clearFocus();
                            toggleCollectionKeywordFilter(3);
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && addCardButton.isClicked(clickPos))
                        {
                            addSelectedCard();
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck &&
                                 deckHasUnsavedChanges() &&
                                 saveDeckButton.isClicked(clickPos))
                        {
                            saveCurrentDeck();
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck && !starterDeckMode &&
                                 deckNameInput.contains(clickPos))
                        {
                            clearFocus();
                            deckNameInput.setActive(true);
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList)
                        {
                            const std::optional<std::size_t> deckIndex = rowIndexAt(
                                     clickPos,
                                     DeckListX,
                                     DeckListY,
                                     DeckListWidth,
                                     DeckRowHeight,
                                     VisibleDeckRows,
                                     deckListOffset,
                                     playerDecks.size());
                            if (deckIndex)
                            {
                                const std::string deckName = playerDecks[*deckIndex].name;
                                const sf::Vector2f clickDelta = clickPos - lastDeckEditorCardClickPosition;
                                const bool closeToLastClick =
                                    clickDelta.x * clickDelta.x + clickDelta.y * clickDelta.y <= 144.0f;
                                const bool isDoubleClick =
                                    lastDeckEditorClickedCardTitle && *lastDeckEditorClickedCardTitle == deckName &&
                                    closeToLastClick &&
                                    animationTime - lastDeckEditorCardClickTime <= DeckCardDoubleClickSeconds;
                                lastDeckEditorClickedCardTitle = deckName;
                                lastDeckEditorCardClickPosition = clickPos;
                                lastDeckEditorCardClickTime = animationTime;
                                selectDeck(*deckIndex);
                                if (isDoubleClick)
                                {
                                    lastDeckEditorClickedCardTitle.reset();
                                    editSelectedDeck();
                                    applyCollectionFilters();
                                }
                            }
                            else
                            {
                                clearFocus();
                                lastDeckEditorClickedCardTitle.reset();
                            }
                        }
                        else if (deckEditorMode == DeckEditorMode::EditDeck)
                        {
                            const std::vector<std::string> deckTitles = deckUniqueTitles();
                            if (const std::optional<std::size_t> cardIndex = rowIndexAt(
                                     clickPos,
                                     DeckCardsX,
                                     DeckCardsY,
                                     DeckCardsWidth,
                                     DeckCardRowHeight,
                                     VisibleDeckCardRows,
                                     deckCardListOffset,
                                     deckTitles.size()))
                            {
                                clearFocus();
                                selectedDeckCard = *cardIndex;
                                if (!showDeckEditorCardPopupIfDoubleClick(deckTitles[*cardIndex], clickPos))
                                {
                                    draggingDeckCard = *cardIndex;
                                    dragStartPos = clickPos;
                                    dragCurrentPos = clickPos;
                                    dragActive = false;
                                }
                            }
                            else if (const std::optional<std::size_t> libraryIndex = rowIndexAt(
                                     clickPos,
                                     LibraryX,
                                     LibraryY,
                                     LibraryWidth,
                                     LibraryRowHeight,
                                     VisibleLibraryRows,
                                     libraryOffset,
                                     filteredCardLibrary.size()))
                            {
                                clearFocus();
                                selectedLibraryCard = *libraryIndex;
                                if (!showDeckEditorCardPopupIfDoubleClick(filteredCardLibrary[*libraryIndex].title, clickPos))
                                {
                                    draggingLibraryCard = *libraryIndex;
                                    dragStartPos = clickPos;
                                    dragCurrentPos = clickPos;
                                    dragActive = false;
                                }
                            }
                            else
                            {
                                clearFocus();
                                lastDeckEditorClickedCardTitle.reset();
                            }
                        }
                    }
                }
                else if (currentState == GameState::Shop)
                {
                    if (shopBackButton.isClicked(clickPos) && !shopBusy())
                    {
                        showAuthenticatedScreen();
                    }
                    else if (revealedCardTitle && dismissRevealedCardButton.isClicked(clickPos) && !shopBusy())
                    {
                        revealedCardTitle.reset();
                        revealStartedAt = 0.0f;
                        setMessage(messageText, "Revealed card dismissed. You can buy another card.", sf::Color(120, 220, 150));
                    }
                    else if (EnableCoinPurchases &&
                             !revealedCardTitle &&
                             refreshShopButton.isClicked(clickPos) &&
                             !shopBusy())
                    {
                        refreshShop();
                    }
                    else if (EnableCoinPurchases &&
                             !revealedCardTitle &&
                             buyCoinPackButton.isClicked(clickPos) &&
                             !shopBusy())
                    {
                        const std::string checkoutUrl = coinCheckoutUrl(loggedInUsername);
                        if (openExternalUrl(checkoutUrl))
                        {
                            coinPurchasePolling = true;
                            coinPurchaseStartingCoins = playerCoins;
                            nextCoinPurchasePollAt = animationTime + 1.0f;
                            coinPurchasePollDeadline = animationTime + CoinPurchasePollTimeoutSeconds;
                            setMessage(
                                messageText,
                                "Checkout opened. Coins will refresh automatically.",
                                sf::Color(120, 220, 150));
                        }
                        else
                        {
                            setMessage(messageText, "Could not open checkout URL.", sf::Color::Red);
                        }
                    }
                    else if (buyCardButton.isClicked(clickPos) && !shopBusy())
                    {
                        if (revealedCardTitle)
                        {
                            setMessage(messageText, "Dismiss the revealed card before buying another.", sf::Color::Red);
                        }
                        else if (playerCoins < 5)
                        {
                            setMessage(messageText, "Need 5 coins to buy a card", sf::Color::Red);
                        }
                        else
                        {
                            setMessage(messageText, "Opening card...", sf::Color::Yellow);
                            pendingShopPurchase = std::async(std::launch::async, purchaseRandomCard, activeAccessToken);
                        }
                    }
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();
                mouseMoved && currentState == GameState::Options && activeOptionsTab == OptionsTab::Audio)
            {
                const sf::Vector2f dragPos = window.mapPixelToCoords(mouseMoved->position);
                if (allAudioSlider.dragTo(dragPos))
                {
                    audioSystem.setAllVolume(allAudioSlider.getValue());
                    updateOptionsLabels();
                }
                else if (musicAudioSlider.dragTo(dragPos))
                {
                    audioSystem.setMusicVolume(musicAudioSlider.getValue());
                    updateOptionsLabels();
                }
                else if (soundFxAudioSlider.dragTo(dragPos))
                {
                    audioSystem.setSoundEffectsVolume(soundFxAudioSlider.getValue());
                    updateOptionsLabels();
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();
                mouseMoved && currentState == GameState::DeckEditor &&
                deckEditorMode == DeckEditorMode::EditDeck && !deckUnsavedChangesPopupVisible &&
                (draggingLibraryCard || draggingDeckCard))
            {
                dragCurrentPos = window.mapPixelToCoords(mouseMoved->position);
                const sf::Vector2f delta = dragCurrentPos - dragStartPos;
                if (delta.x * delta.x + delta.y * delta.y > 16.0f)
                {
                    dragActive = true;
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();
                mouseMoved && currentState == GameState::Game && gameDragKind != GameDragKind::None)
            {
                gameDragCurrentPos = window.mapPixelToCoords(mouseMoved->position);
                const sf::Vector2f delta = gameDragCurrentPos - gameDragStartPos;
                if (delta.x * delta.x + delta.y * delta.y > GameDragStartDistanceSquared)
                {
                    gameDragActive = true;
                    pendingHandClickIndex.reset();
                    lastClickedPieceId.reset();
                }
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>();
                mouseReleased && mouseReleased->button == sf::Mouse::Button::Left && currentState == GameState::Options)
            {
                allAudioSlider.endDrag();
                musicAudioSlider.endDrag();
                soundFxAudioSlider.endDrag();
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>();
                mouseReleased && mouseReleased->button == sf::Mouse::Button::Left && currentState == GameState::DeckEditor)
            {
                const sf::Vector2f releasePos = window.mapPixelToCoords(mouseReleased->position);
                if (deckEditorMode == DeckEditorMode::EditDeck && !deckUnsavedChangesPopupVisible &&
                    draggingLibraryCard && dragActive &&
                    isInsideRect(releasePos, CurrentDeckPanelX, DeckEditorPanelY, CurrentDeckPanelWidth, DeckEditorPanelHeight))
                {
                    addLibraryCardToDeck(*draggingLibraryCard, "Card dropped into deck. Save to keep changes.");
                }
                else if (deckEditorMode == DeckEditorMode::EditDeck && !deckUnsavedChangesPopupVisible &&
                         draggingDeckCard && dragActive &&
                         isInsideRect(releasePos, LibraryPanelX, DeckEditorPanelY, LibraryPanelWidth, DeckEditorPanelHeight))
                {
                    removeDeckCardAt(*draggingDeckCard, "Card removed. Save to keep changes.");
                }

                draggingLibraryCard.reset();
                draggingDeckCard.reset();
                dragActive = false;
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>();
                mouseReleased && mouseReleased->button == sf::Mouse::Button::Left && currentState == GameState::Game)
            {
                const sf::Vector2f releasePos = window.mapPixelToCoords(mouseReleased->position);
                if (gameDragKind != GameDragKind::None)
                {
                    finishGameDrag(releasePos);
                }
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>();
                wheel && currentState == GameState::DeckEditor && !deckUnsavedChangesPopupVisible)
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                if (inspectedDeckEditorCardTitle &&
                    isInsideRect(wheelPos, PiecePopupTextX, PiecePopupScrollY, PiecePopupTextWidth, PiecePopupScrollHeight))
                {
                    const card_data::Card* card = cardByTitle(*inspectedDeckEditorCardTitle);
                    if (!card)
                    {
                        card = cardInAllLibraryByTitle(*inspectedDeckEditorCardTitle);
                    }
                    if (card)
                    {
                        const std::vector<std::pair<std::string, sf::Color>> details = deckEditorCardDetails(*card);
                        inspectedDeckEditorCardScroll = std::clamp(
                            inspectedDeckEditorCardScroll - wheel->delta * 34.0f,
                            0.0f,
                            deckEditorCardDetailsMaxScroll(details));
                    }
                }
                else if (!inspectedDeckEditorCardTitle &&
                         deckEditorMode == DeckEditorMode::DeckList &&
                         isInsideRect(wheelPos, DeckListX, DeckListY, DeckListWidth, DeckRowHeight * VisibleDeckRows))
                {
                    scrollList(deckListOffset, playerDecks.size(), VisibleDeckRows, wheel->delta);
                }
                else if (!inspectedDeckEditorCardTitle &&
                         deckEditorMode == DeckEditorMode::EditDeck &&
                         isInsideRect(wheelPos, DeckCardsX, DeckCardsY, DeckCardsWidth, DeckCardRowHeight * VisibleDeckCardRows))
                {
                    scrollList(deckCardListOffset, deckUniqueTitles().size(), VisibleDeckCardRows, wheel->delta);
                }
                else if (!inspectedDeckEditorCardTitle &&
                         deckEditorMode == DeckEditorMode::EditDeck &&
                         isInsideRect(wheelPos, LibraryX, LibraryY, LibraryWidth, LibraryRowHeight * VisibleLibraryRows))
                {
                    scrollList(libraryOffset, filteredCardLibrary.size(), VisibleLibraryRows, wheel->delta);
                }
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>();
                wheel && currentState == GameState::Game && (inspectedPieceId || inspectedHandIndex))
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                if (isInsideRect(wheelPos, PiecePopupTextX, PiecePopupScrollY, PiecePopupTextWidth, PiecePopupScrollHeight))
                {
                    std::vector<std::pair<std::string, sf::Color>> actionDescriptions;
                    if (inspectedHandIndex && *inspectedHandIndex < gameSnapshot.hand.size())
                    {
                        actionDescriptions = cardPopupActionDescriptions(gameSnapshot.hand[*inspectedHandIndex]);
                    }
                    else if (inspectedPieceId)
                    {
                        if (const game_data::Piece* piece = gamePieceById(*inspectedPieceId))
                        {
                            actionDescriptions = piecePopupActionDescriptions(*piece);
                        }
                    }

                    if (!actionDescriptions.empty())
                    {
                        inspectedPieceScroll = std::clamp(
                            inspectedPieceScroll - wheel->delta * 34.0f,
                            0.0f,
                            popupMaxScroll(actionDescriptions));
                    }
                }
            }
            else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>();
                     wheel && currentState == GameState::Game && haveSnapshot &&
                     gameSnapshot.hand.size() > VisibleGameHandCards)
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                const float handWidth = static_cast<float>(VisibleGameHandCards) * HandCardWidth +
                    static_cast<float>(VisibleGameHandCards - 1) * HandGap;
                if (isInsideRect(wheelPos, HandStartX, HandY - 22.0f, handWidth, HandCardHeight + 22.0f))
                {
                    scrollList(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards, wheel->delta);
                }
            }

            if (currentState == GameState::Login || currentState == GameState::CreateAccount)
            {
                usernameInput.handleEvent(*event, window);
                passwordInput.handleEvent(*event, window);
            }
            if (currentState == GameState::ChangePassword && !passwordChangedPopupVisible)
            {
                currentPasswordInput.handleEvent(*event, window);
                newPasswordInput.handleEvent(*event, window);
                confirmNewPasswordInput.handleEvent(*event, window);
            }
            if (currentState == GameState::AdminUsers && !deleteUserPopupVisible)
            {
                adminSearchInput.handleEvent(*event, window);
                adminGoldInput.handleEvent(*event, window);
            }

            if (currentState == GameState::CardEditor)
            {
                if (cardEditorScreen.handleEvent(*event, window))
                {
                    showAuthenticatedScreen();
                }
                continue;
            }

            if (currentState == GameState::CreateAccount)
            {
                confirmInput.handleEvent(*event, window);
            }

            if (currentState == GameState::DeckEditor && deckEditorMode == DeckEditorMode::EditDeck &&
                !starterDeckMode && !deckUnsavedChangesPopupVisible &&
                !deckEditorBusy() && !inspectedDeckEditorCardTitle)
            {
                deckNameInput.handleEvent(*event, window);
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (exitDesktopPopupVisible)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Escape)
                    {
                        exitDesktopPopupVisible = false;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        window.close();
                    }
                    continue;
                }

                if (deckUnsavedChangesPopupVisible)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Escape)
                    {
                        deckUnsavedChangesPopupVisible = false;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        discardDeckEditChanges();
                    }
                    continue;
                }

                if (keyPressed->code == sf::Keyboard::Key::Escape)
                {
                    if (currentState == GameState::ChangePassword && passwordChangedPopupVisible)
                    {
                        dismissPasswordChangedPopup();
                    }
                    else if (currentState == GameState::Options)
                    {
                        leaveOptionsScreen();
                    }
                    else if (currentState == GameState::StoryIntro)
                    {
                        showAuthenticatedScreen();
                    }
                    else if (currentState == GameState::ChangePassword && !pendingPasswordChange)
                    {
                        leaveChangePasswordScreen();
                    }
                    else if (currentState == GameState::Game && (inspectedPieceId || inspectedHandIndex))
                    {
                        inspectedPieceId.reset();
                        inspectedHandIndex.reset();
                        inspectedPieceScroll = 0.0f;
                    }
                    else if (currentState == GameState::DeckEditor && inspectedDeckEditorCardTitle)
                    {
                        inspectedDeckEditorCardTitle.reset();
                        lastDeckEditorClickedCardTitle.reset();
                        inspectedDeckEditorCardScroll = 0.0f;
                    }
                    else if (currentState == GameState::AdminUsers && deleteUserPopupVisible)
                    {
                        dismissDeleteUserPopup();
                    }
                    else if (currentState == GameState::AdminUsers)
                    {
                        showAuthenticatedScreen();
                    }
                    else if (currentState == GameState::DeckEditor && !deckEditorBusy())
                    {
                        if (deckEditorMode == DeckEditorMode::EditDeck)
                        {
                            requestLeaveDeckEdit();
                        }
                        else
                        {
                            showAuthenticatedScreen();
                        }
                    }
                    else if (currentState == GameState::Shop && revealedCardTitle && !shopBusy())
                    {
                        revealedCardTitle.reset();
                        revealStartedAt = 0.0f;
                        setMessage(messageText, "Revealed card dismissed. You can buy another card.", sf::Color(120, 220, 150));
                    }
                    else if (currentState == GameState::Shop && !shopBusy())
                    {
                        showAuthenticatedScreen();
                    }
                    else if (currentState == GameState::Game)
                    {
                        leaveGame();
                    }
                    else if (currentState == GameState::DeckSelect)
                    {
                        showAuthenticatedScreen();
                    }
                    else if (currentState == GameState::Matchmaking)
                    {
                        requestMatchmakingCancel();
                    }
                    else if (currentState == GameState::DeckEditor || currentState == GameState::Shop)
                    {
                        // Busy editor/shop requests keep their screen until they finish.
                    }
                    else if (currentState == GameState::ChangePassword && pendingPasswordChange)
                    {
                        // Keep the password form open until the request finishes.
                    }
                    else if (!pendingRequest && !pendingMatchmaking && !pendingSandboxLoad)
                    {
                        returnToMenu();
                    }
                }
                else if (!pendingRequest && !pendingMatchmaking && !pendingSandboxLoad && currentState == GameState::Login)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        focusLoginInput(focusedInput + (keyPressed->shift ? -1 : 1));
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        submitLogin();
                    }
                }
                else if (!pendingRequest && !pendingMatchmaking && !pendingSandboxLoad && currentState == GameState::CreateAccount)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        focusCreateInput(focusedInput + (keyPressed->shift ? -1 : 1));
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        submitCreateAccount();
                    }
                }
                else if (currentState == GameState::ChangePassword &&
                         passwordChangedPopupVisible &&
                         keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    dismissPasswordChangedPopup();
                }
                else if (currentState == GameState::ChangePassword && !pendingPasswordChange)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        focusChangePasswordInput(focusedInput + (keyPressed->shift ? -1 : 1));
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        submitPasswordChange();
                    }
                }
                else if (currentState == GameState::AdminUsers &&
                         deleteUserPopupVisible &&
                         keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    confirmUserDeletion();
                }
                else if (currentState == GameState::AdminUsers && !deleteUserPopupVisible)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        if (adminGoldInput.isActive())
                        {
                            changeSelectedUserGold(true);
                        }
                        else
                        {
                            searchAdminUsers();
                        }
                    }
                }
                else if (currentState == GameState::DeckEditor && !deckEditorBusy())
                {
                    if (deckEditorMode == DeckEditorMode::DeckList &&
                        keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        editSelectedDeck();
                    }
                    else if (deckEditorMode == DeckEditorMode::EditDeck &&
                             deckHasUnsavedChanges() &&
                             keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        saveCurrentDeck();
                    }
                    else if (deckEditorMode == DeckEditorMode::DeckList &&
                             keyPressed->code == sf::Keyboard::Key::Delete)
                    {
                        deleteCurrentDeck();
                    }
                    else if (deckEditorMode == DeckEditorMode::EditDeck &&
                             keyPressed->code == sf::Keyboard::Key::Delete && !deckNameInput.active)
                    {
                        removeSelectedCard();
                    }
                }
            }
        }

        if (!window.isOpen())
        {
            break;
        }

        if (currentState == GameState::Game && pendingHandClickIndex &&
            !(inspectedPieceId || inspectedHandIndex) &&
            animationTime - pendingHandClickTime > PieceDoubleClickSeconds)
        {
            flushPendingHandClick();
        }

        if (currentState == GameState::Menu)
        {
            exitDesktopCloseHovered = exitDesktopCloseButtonClicked(mousePos);
            if (exitDesktopPopupVisible)
            {
                cancelExitDesktopButton.update(mousePos);
                confirmExitDesktopButton.update(mousePos);
            }
            else
            {
                loginButton.update(mousePos);
                createButton.update(mousePos);
                menuOptionsButton.update(mousePos);
            }
        }
        else if (currentState == GameState::SandboxLoading)
        {
            exitDesktopButton.update(mousePos);
        }
        else if (currentState == GameState::Options)
        {
            optionsTabs.update(mousePos);
            if (activeOptionsTab == OptionsTab::Graphics)
            {
                displayModeButton.update(mousePos);
                previousResolutionButton.update(mousePos);
                resolutionButton.update(mousePos);
                nextResolutionButton.update(mousePos);
                applyOptionsButton.update(mousePos);
            }
            else if (activeOptionsTab == OptionsTab::Audio)
            {
                allAudioSlider.update(mousePos);
                musicAudioSlider.update(mousePos);
                soundFxAudioSlider.update(mousePos);
                muteAllAudioCheckbox.update(mousePos);
                muteMusicCheckbox.update(mousePos);
                muteSoundFxCheckbox.update(mousePos);
            }
            else if (optionsReturnState == GameState::Authenticated)
            {
                changePasswordOptionButton.update(mousePos);
            }
            optionsBackButton.update(mousePos);
        }
        else if (currentState == GameState::StoryIntro)
        {
            backButton.update(mousePos);
        }
        else if (currentState == GameState::ChangePassword)
        {
            if (passwordChangedPopupVisible)
            {
                dismissPasswordChangedButton.update(mousePos);
            }
            else
            {
                currentPasswordVisibilityIcon.update(mousePos);
                newPasswordVisibilityIcon.update(mousePos);
                confirmNewPasswordVisibilityIcon.update(mousePos);
                changePasswordSubmitButton.update(mousePos);
                changePasswordBackButton.update(mousePos);
                currentPasswordInput.updateCursor(deltaTime);
                newPasswordInput.updateCursor(deltaTime);
                confirmNewPasswordInput.updateCursor(deltaTime);
            }
        }
        else if (currentState == GameState::Login)
        {
            rememberMeCheckbox.update(mousePos);
            passwordVisibilityIcon.update(mousePos);
            loginSubmitButton.update(mousePos);
            backButton.update(mousePos);
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            passwordVisibilityIcon.update(mousePos);
            confirmVisibilityIcon.update(mousePos);
            createSubmitButton.update(mousePos);
            backButton.update(mousePos);
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
            confirmInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::Authenticated)
        {
            layoutAuthenticatedButtons();
            exitDesktopCloseHovered = exitDesktopCloseButtonClicked(mousePos);
            if (exitDesktopPopupVisible)
            {
                cancelExitDesktopButton.update(mousePos);
                confirmExitDesktopButton.update(mousePos);
            }
            else
            {
                storyButton.update(mousePos);
                playButton.update(mousePos);
                sandboxButton.update(mousePos);
                deckEditorButton.update(mousePos);
                shopButton.update(mousePos);
                if (loggedInIsAdmin)
                {
                    adminCardEditorButton.update(mousePos);
                    adminUsersButton.update(mousePos);
                }
                authenticatedOptionsButton.update(mousePos);
                logoutButton.update(mousePos);
            }
        }
        else if (currentState == GameState::AdminUsers)
        {
            if (deleteUserPopupVisible)
            {
                cancelDeleteUserButton.update(mousePos);
                confirmDeleteUserButton.update(mousePos);
            }
            else
            {
                adminTabs.update(mousePos);
                adminBackButton.update(mousePos);
                adminPrevPageButton.update(mousePos);
                adminRefreshButton.update(mousePos);
                adminNextPageButton.update(mousePos);
                if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
                {
                    adminGrantGoldButton.update(mousePos);
                    adminRemoveGoldButton.update(mousePos);
                    if (adminUsers[*selectedAdminUser].isAdmin)
                    {
                        if (adminUsers[*selectedAdminUser].username != loggedInUsername)
                        {
                            adminRevokeButton.update(mousePos);
                        }
                    }
                    else
                    {
                        adminGrantButton.update(mousePos);
                    }
                    if (adminUsers[*selectedAdminUser].username != loggedInUsername)
                    {
                        adminDeleteButton.update(mousePos);
                    }
                }
            }
            adminSearchInput.updateCursor(deltaTime);
            adminGoldInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::DeckSelect)
        {
            findMatchButton.update(mousePos);
            backButton.update(mousePos);
        }
        else if (currentState == GameState::Matchmaking)
        {
            cancelMatchmakingButton.update(mousePos);
            playAiButton.update(mousePos);
        }
        else if (currentState == GameState::DeckEditor)
        {
            layoutDeckEditorControls();
            deckBackButton.update(mousePos);
            if (starterDeckMode && !deckUnsavedChangesPopupVisible)
            {
                adminTabs.update(mousePos);
            }
            if (deckUnsavedChangesPopupVisible)
            {
                keepEditingDeckButton.update(mousePos);
                discardDeckChangesButton.update(mousePos);
            }
            else if (deckEditorMode == DeckEditorMode::DeckList)
            {
                newDeckButton.update(mousePos);
                refreshDeckButton.update(mousePos);
                editDeckButton.update(mousePos);
                deleteDeckButton.update(mousePos);
            }
            else
            {
                removeCardButton.update(mousePos);
                collectionHeroFilterCheckbox.update(mousePos);
                collectionUnitFilterCheckbox.update(mousePos);
                collectionSpellFilterCheckbox.update(mousePos);
                collectionBioFilterCheckbox.update(mousePos);
                collectionMechanicalFilterCheckbox.update(mousePos);
                collectionOccultFilterCheckbox.update(mousePos);
                collectionRiffraffFilterCheckbox.update(mousePos);
                addCardButton.update(mousePos);
                if (deckHasUnsavedChanges())
                {
                    saveDeckButton.update(mousePos);
                }
                else
                {
                    saveDeckButton.hovered = false;
                }
            }
            if (!deckUnsavedChangesPopupVisible && inspectedDeckEditorCardTitle)
            {
                closeDeckCardPopupButton.update(mousePos);
            }
            if (!deckUnsavedChangesPopupVisible && deckEditorMode == DeckEditorMode::EditDeck)
            {
                deckNameInput.updateCursor(deltaTime);
            }
        }
        else if (currentState == GameState::CardEditor)
        {
            cardEditorScreen.update(window, deltaTime);
        }
        else if (currentState == GameState::Shop)
        {
            shopBackButton.update(mousePos);
            if (revealedCardTitle)
            {
                dismissRevealedCardButton.update(mousePos);
            }
            else
            {
                if (EnableCoinPurchases)
                {
                    buyCoinPackButton.update(mousePos);
                    refreshShopButton.update(mousePos);
                }
                buyCardButton.update(mousePos);
            }
        }
        else if (currentState == GameState::Game)
        {
            if (inspectedPieceId || inspectedHandIndex)
            {
                if (canDiscardInspectedHandCard())
                {
                    discardCardButton.update(mousePos);
                }
                closePiecePopupButton.update(mousePos);
            }
            else
            {
                if (haveSnapshot && selectedPieceId &&
                    static_cast<game_data::Phase>(gameSnapshot.phase) == game_data::Phase::Playing &&
                    (sandboxMode || gameSnapshot.activePlayer == gameSnapshot.yourPlayer))
                {
                    if (const game_data::Piece* piece = gamePieceById(*selectedPieceId);
                        piece && (sandboxMode || (piece->owner == gameSnapshot.yourPlayer && !piece->hasActed)) &&
                        game_data::pieceAbilityAvailable(gameSnapshot.pieces, *piece))
                    {
                        abilityButton.update(mousePos);
                    }
                }
                if (sandboxMode && !storyMode)
                {
                    sandboxPlayerButton.update(mousePos);
                    sandboxAdvanceTurnButton.update(mousePos);
                }
                else
                {
                    endTurnButton.update(mousePos);
                }
                leaveGameButton.update(mousePos);
            }
        }

        window.clear(sf::Color(9, 17, 19));
        drawBackdrop(window, backdropTexture);
        if (currentState != GameState::DeckEditor &&
            currentState != GameState::Shop &&
            currentState != GameState::AdminUsers &&
            currentState != GameState::CardEditor &&
            currentState != GameState::Game)
        {
            drawTitlePlaque(window, font, title.getString().toAnsiString(), {400.0f, 64.0f}, {360.0f, 70.0f});
        }

        if (currentState == GameState::Menu)
        {
            loginButton.draw(window);
            createButton.draw(window);
            menuOptionsButton.draw(window);
            drawExitDesktopCloseButton();
            if (exitDesktopPopupVisible)
            {
                drawExitDesktopPopup();
            }
        }
        else if (currentState == GameState::SandboxLoading)
        {
            window.draw(messageText);
            exitDesktopButton.draw(window);
        }
        else if (currentState == GameState::Options)
        {
            drawPanel(window, {112.0f, 164.0f}, {576.0f, 302.0f});
            optionsTabs.draw(window);
            if (activeOptionsTab == OptionsTab::Graphics)
            {
                drawText(window, font, "Display Mode", 18, {332.0f, 180.0f}, sf::Color(246, 232, 200));
                displayModeButton.draw(window);
                drawText(window, font, "Resolution", 18, {350.0f, 286.0f}, sf::Color(246, 232, 200));
                previousResolutionButton.draw(window);
                resolutionButton.draw(window);
                nextResolutionButton.draw(window);
                applyOptionsButton.draw(window);
            }
            else if (activeOptionsTab == OptionsTab::Audio)
            {
                allAudioSlider.draw(window);
                musicAudioSlider.draw(window);
                soundFxAudioSlider.draw(window);
                muteAllAudioCheckbox.draw(window, audioSystem.isAllMuted());
                muteMusicCheckbox.draw(window, audioSystem.isMusicMuted());
                muteSoundFxCheckbox.draw(window, audioSystem.isSoundEffectsMuted());
            }
            else if (optionsReturnState == GameState::Authenticated)
            {
                changePasswordOptionButton.draw(window);
            }
            else
            {
                drawText(
                    window,
                    font,
                    "Sign in to manage account settings.",
                    18,
                    {246.0f, 248.0f},
                    sf::Color(220, 224, 230),
                    320.0f);
            }
            optionsBackButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::StoryIntro)
        {
            drawStoryIntro();
        }
        else if (currentState == GameState::ChangePassword)
        {
            currentPasswordInput.draw(window);
            newPasswordInput.draw(window);
            confirmNewPasswordInput.draw(window);
            currentPasswordVisibilityIcon.draw(window, changePasswordsVisible);
            newPasswordVisibilityIcon.draw(window, changePasswordsVisible);
            confirmNewPasswordVisibilityIcon.draw(window, changePasswordsVisible);
            changePasswordSubmitButton.draw(window);
            changePasswordBackButton.draw(window);
            window.draw(messageText);
            if (passwordChangedPopupVisible)
            {
                sf::RectangleShape overlay({800.0f, 600.0f});
                overlay.setFillColor(sf::Color(0, 0, 0, 170));
                window.draw(overlay);
                drawPanel(window, {220.0f, 190.0f}, {360.0f, 220.0f});
                drawText(
                    window,
                    font,
                    "Password Changed",
                    28,
                    {270.0f, 225.0f},
                    sf::Color(248, 224, 172),
                    260.0f);
                drawText(
                    window,
                    font,
                    "Your password was changed",
                    18,
                    {280.0f, 280.0f},
                    sf::Color(220, 224, 230),
                    240.0f);
                drawText(
                    window,
                    font,
                    "successfully.",
                    18,
                    {330.0f, 307.0f},
                    sf::Color(220, 224, 230),
                    140.0f);
                dismissPasswordChangedButton.draw(window);
            }
        }
        else if (currentState == GameState::Login)
        {
            usernameInput.draw(window);
            passwordInput.draw(window);
            passwordVisibilityIcon.draw(window, passwordVisible);
            rememberMeCheckbox.draw(window, rememberMeChecked);
            loginSubmitButton.draw(window);
            backButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::CreateAccount)
        {
            usernameInput.draw(window);
            passwordInput.draw(window);
            confirmInput.draw(window);
            passwordVisibilityIcon.draw(window, passwordVisible);
            confirmVisibilityIcon.draw(window, passwordVisible);
            createSubmitButton.draw(window);
            backButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::Authenticated)
        {
            drawTitlePlaque(window, font, "Steam Tactics", {400.0f, 64.0f}, {360.0f, 70.0f});
            drawText(window, font, signedInLabel(), 18, {24.0f, 20.0f}, sf::Color(246, 238, 218), 240.0f);
            drawText(window, font, "Rating: " + std::to_string(playerRating), 16, {24.0f, 48.0f}, sf::Color(151, 192, 255), 180.0f);
            drawCoinIcon({24.0f, 76.0f}, 13.0f);
            drawText(window, font, std::to_string(playerCoins), 18, {58.0f, 75.0f}, sf::Color(248, 239, 216), 120.0f);
            storyButton.draw(window);
            playButton.draw(window);
            sandboxButton.draw(window);
            deckEditorButton.draw(window);
            shopButton.draw(window);
            if (loggedInIsAdmin)
            {
                adminCardEditorButton.draw(window);
                adminUsersButton.draw(window);
            }
            authenticatedOptionsButton.draw(window);
            logoutButton.draw(window);
            drawExitDesktopCloseButton();
            window.draw(messageText);
            if (exitDesktopPopupVisible)
            {
                drawExitDesktopPopup();
            }
        }
        else if (currentState == GameState::AdminUsers)
        {
            drawAdminUsers();
        }
        else if (currentState == GameState::DeckSelect)
        {
            drawDeckSelect();
        }
        else if (currentState == GameState::Matchmaking)
        {
            window.draw(messageText);
            cancelMatchmakingButton.draw(window);
            playAiButton.draw(window);
        }
        else if (currentState == GameState::DeckEditor)
        {
            drawDeckEditor();
            drawDeckEditorCardPopup();
            drawDeckUnsavedChangesPopup();
        }
        else if (currentState == GameState::Shop)
        {
            drawShop();
        }
        else if (currentState == GameState::CardEditor)
        {
            cardEditorScreen.render(window);
        }
        else if (currentState == GameState::Game)
        {
            drawGame();
        }

        window.display();
    }

    return 0;
}
