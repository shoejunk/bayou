#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include "tls_socket.hpp"

#include "client_board_layout.hpp"
#include "client_card_text.hpp"
#include "client_clock_warning.hpp"
#include "client_config.hpp"
#include "client_display.hpp"
#include "client_sandbox.hpp"
#include "client_string.hpp"
#include "client_textures.hpp"
#include "client_ui.hpp"
#include "client_ui_capture.hpp"
#include "deck_collection.hpp"

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/game_data.hpp"
#include "../shared/starter_decks.hpp"

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
#include <random>
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
import conquest_screen;
import conquest_services;
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
    Defeat,
    ClockWarning
};

constexpr std::size_t MinimumPasswordLength = 7;
constexpr std::size_t MaximumPasswordLength = 128;
constexpr const char* PasswordRequirementMessage =
    "Password needs 7-128 chars, upper, lower, number, special";
constexpr const char* NewPasswordRequirementMessage =
    "New password needs 7-128 chars, upper, lower, number, special";
constexpr const char* PasswordRequirementHintLineOne =
    "Use a minimum of 7 characters.";
constexpr const char* PasswordRequirementHintLineTwo =
    "Include uppercase, lowercase, number, and special.";

bool isValidNewPassword(const std::string& password)
{
    if (password.size() < MinimumPasswordLength ||
        password.size() > MaximumPasswordLength)
    {
        return false;
    }

    bool hasLowercase = false;
    bool hasUppercase = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (unsigned char ch : password)
    {
        hasLowercase = hasLowercase || std::islower(ch) != 0;
        hasUppercase = hasUppercase || std::isupper(ch) != 0;
        hasDigit = hasDigit || std::isdigit(ch) != 0;
        hasSpecial = hasSpecial || std::ispunct(ch) != 0;
    }

    return hasLowercase && hasUppercase && hasDigit && hasSpecial;
}

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
    static constexpr int EffectCount = 9;
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

    static std::vector<std::int16_t> makeClockWarningSamples()
    {
        constexpr float duration = 0.72f;
        constexpr float pulseDuration = 0.20f;
        const int sampleCount = static_cast<int>(duration * static_cast<float>(SampleRate));
        std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));

        for (int i = 0; i < sampleCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(SampleRate);
            const int pulseIndex = std::min(2, static_cast<int>(t / 0.24f));
            const float pulseTime = t - static_cast<float>(pulseIndex) * 0.24f;
            if (pulseTime < 0.0f || pulseTime >= pulseDuration)
            {
                continue;
            }

            const float frequency = 660.0f + static_cast<float>(pulseIndex) * 110.0f;
            const float tone = std::sin(2.0f * Pi * frequency * pulseTime) * 0.72f +
                std::sin(2.0f * Pi * frequency * 2.0f * pulseTime) * 0.28f;
            const float amp = tone * envelope(pulseTime, pulseDuration, 0.008f, 0.07f) * 0.42f;
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
        effectBuffers[static_cast<std::size_t>(AudioCue::ClockWarning)] =
            bufferFromSamples(makeClockWarningSamples());
    }

    void startMusic()
    {
        const std::optional<std::filesystem::path> musicPath = resolveAssetPath("audio/Gloomthorn Swamp 2.mp3");
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
            case AudioCue::ClockWarning: return 64.0f;
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
constexpr float FidgetDelayMinimumSeconds = 3.0f;
constexpr float FidgetDelayMaximumSeconds = 8.0f;
constexpr float FidgetAnimationDurationSeconds = 0.75f;
constexpr float PieceMoveAnimationDurationSeconds = 0.95f;
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
constexpr float DeckRowHeight = 52.0f;
constexpr std::size_t VisibleDeckRows = 5;

constexpr float DeckCardsX = 42.0f;
constexpr float DeckCardsY = 164.0f;
constexpr float DeckCardsWidth = 324.0f;
constexpr float DeckCardRowHeight = 52.0f;
constexpr std::size_t VisibleDeckCardRows = 6;
constexpr float PasswordIconInset = 42.0f;
constexpr std::uint32_t AdminUsersPageSize = 6;
constexpr float AdminUserRowY = 174.0f;
constexpr float AdminUserRowHeight = 43.0f;
constexpr float AdminCardRowY = 276.0f;
constexpr float AdminCardRowHeight = 36.0f;
constexpr std::size_t VisibleAdminCardRows = 5;
constexpr float AdminStarterDeckRowY = 232.0f;
constexpr float AdminStarterDeckRowHeight = 42.0f;

constexpr float LibraryX = 430.0f;
constexpr float LibraryY = 276.0f;
constexpr float LibraryWidth = 326.0f;
constexpr float LibraryRowHeight = 52.0f;
constexpr std::size_t VisibleLibraryRows = 4;
constexpr std::array<const char*, 3> CollectionTypeLabels = {"Heroes", "Units", "Spells & Enchantments"};

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

    // Lets a screen lay the control out at draw time rather than only at
    // construction, so a form can be reflowed without moving its declaration.
    void setPosition(sf::Vector2f position)
    {
        const sf::Vector2f offset = label.getPosition() - box.getPosition();
        box.setPosition(position);
        label.setPosition(position + offset);
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
    StarterDecks,
    AdminUsers,
    AdminTools,
    CardEditor,
    Conquest,
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
constexpr float GameTurnLabelY = 20.0f;
constexpr float GameTurnReadoutWidth = 350.0f;
constexpr float GamePlayerReadoutGap = 16.0f;
constexpr float GamePlayerReadoutWidth = (BoardBottomWidth - GamePlayerReadoutGap) * 0.5f;
constexpr float HandY = 512.0f;
constexpr float HandCardWidth = 88.0f;
constexpr float HandCardHeight = 78.0f;
constexpr float HandGap = 6.0f;
constexpr float HandStartX = 28.0f;
constexpr std::size_t VisibleGameHandCards = 4;
constexpr float TrashCanSize = 86.0f;
constexpr float TrashCanX = HandStartX + 4.0f * (HandCardWidth + HandGap) + 8.0f;
constexpr float TrashCanY = HandY;
constexpr float TrashCanDropPadding = 14.0f;
constexpr float GameActionButtonGap = 6.0f;
constexpr float GameActionButtonHeight = 30.0f;
constexpr float GameActionButtonX =
    TrashCanX + TrashCanSize + TrashCanDropPadding + GameActionButtonGap;
constexpr float GameActionButtonY =
    TrashCanY + TrashCanSize - GameActionButtonHeight - GameActionButtonGap;
constexpr float GameSandboxAbilityButtonY =
    GameActionButtonY - GameActionButtonHeight - GameActionButtonGap;
constexpr float GameAbilityButtonWidth = 92.0f;
constexpr float GameEndTurnButtonWidth = 90.0f;
constexpr float GameLeaveButtonWidth = 72.0f;
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

    const std::optional<ui_capture::Request> captureRequest =
        ui_capture::parseCommandLine(argc, argv);

    const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    const std::vector<sf::VideoMode>& fullscreenModes = sf::VideoMode::getFullscreenModes();
    std::vector<sf::Vector2u> displayResolutions =
        availableDisplayResolutions(desktopMode, fullscreenModes);

    DisplaySettings displaySettings = loadDisplaySettings();
    normalizeDisplaySettings(displaySettings, desktopMode.size, displayResolutions);
    if (captureRequest)
    {
        // Windowed at exactly the requested size so captures are reproducible
        // regardless of the machine's desktop resolution.
        displaySettings.fullscreen = false;
        displaySettings.width = captureRequest->width;
        displaySettings.height = captureRequest->height;
    }

    sf::RenderWindow window;
    createDisplayWindow(window, displaySettings, desktopMode, fullscreenModes);

    sf::Font font;
    const std::optional<std::filesystem::path> fontPath = resolveAssetPath("Roboto.ttf");
    if (!fontPath || !font.openFromFile(*fontPath))
    {
        return 1;
    }

    sf::Font gloomthornFont;
    const std::optional<std::filesystem::path> gloomthornFontPath =
        resolveAssetPath("fonts/gloomthorn/GloomthornDisplay-Regular.ttf");
    const bool gloomthornFontLoaded =
        gloomthornFontPath && gloomthornFont.openFromFile(*gloomthornFontPath);
    if (gloomthornFontLoaded)
    {
        // Registering the display face lets the shared helpers set titles,
        // headings and plaque numerals in it while body copy stays Roboto.
        setDisplayFont(&gloomthornFont);
    }

    TextureStore textures;
    sf::Texture* backdropTexture = textures.load("ui/gloomthorn-backdrop.png");
    sf::Texture* gloomthornTitleTexture = textures.load("ui/gloomthorn-title.png");
    sf::Texture* showPasswordTexture = textures.load("ui/password-eye-open.png");
    sf::Texture* hidePasswordTexture = textures.load("ui/password-eye-off.png");
    sf::Texture* rememberCheckTexture = textures.load("ui/remember-checkmark.png");
    sf::Texture* mainMenuProfileFrameTexture =
        textures.load("ui/main-menu/account_profile_circle_frame.png");
    sf::Texture* mainMenuButtonTexture = textures.load("ui/main-menu/button_blank.png");
    sf::Texture* mainMenuCoinTexture = textures.load("ui/main-menu/gold_coin.png");
    sf::Texture* mainMenuAdminIconTexture = textures.load("ui/main-menu/icon_admin.png");
    sf::Texture* mainMenuConquestIconTexture = textures.load("ui/main-menu/icon_conquest.png");
    sf::Texture* mainMenuDeckEditorIconTexture =
        textures.load("ui/main-menu/icon_deck_editor.png");
    sf::Texture* mainMenuLogoutIconTexture = textures.load("ui/main-menu/icon_log_out.png");
    sf::Texture* mainMenuPlayIconTexture = textures.load("ui/main-menu/icon_play.png");
    sf::Texture* mainMenuShopIconTexture = textures.load("ui/main-menu/icon_shop.png");
    sf::Texture* mainMenuStoryIconTexture = textures.load("ui/main-menu/icon_story.png");
    sf::Texture* mainMenuExitTexture = textures.load("ui/main-menu/red_banner_x.png");
    sf::Texture* mainMenuSettingsTexture = textures.load("ui/main-menu/settings_gear_icon.png");
    sf::Texture* mainMenuSmallHexTexture = textures.load("ui/main-menu/small_hex_frame.png");
    sf::Texture* mainMenuTitleFrameTexture = textures.load("ui/main-menu/title_frame.png");
    sf::Texture* mainMenuWoodLeagueTexture = textures.load("ui/main-menu/wood_league_leaf.png");
    // Stands in for a player-chosen avatar until portraits are a real feature; an
    // empty portrait frame is the loudest "unfinished" signal on the menu.
    sf::Texture* mainMenuAvatarTexture = textures.load("characters/sylvara.png");

    sf::Text title(font, "Gloomthorn", 48);
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
    InputBox adminSearchInput({120.0f, 94.0f}, {390.0f, 36.0f}, "", font);
    InputBox adminGoldInput({234.0f, 460.0f}, {130.0f, 36.0f}, "Gold amount", font);
    InputBox adminCardInput({240.0f, 224.0f}, {320.0f, 36.0f}, "Card name", font);

    CheckboxControl rememberMeCheckbox({300.0f, 286.0f}, "Remember me", font, rememberCheckTexture);
    Button loginSubmitButton({300.0f, 342.0f}, {200.0f, 50.0f}, "Login", font);
    Button createSubmitButton({300.0f, 410.0f}, {200.0f, 50.0f}, "Create Account", font);
    Button backButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Back", font);
    Button exitDesktopButton({20.0f, 520.0f}, {200.0f, 45.0f}, "Exit to Desktop", font);
    Button cancelMatchmakingButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Cancel", font);
    Button playAiButton({150.0f, 520.0f}, {160.0f, 45.0f}, "Play vs AI", font);
    Button storyButton({300.0f, 152.0f}, {200.0f, 48.0f}, "STORY", font);
    Button playButton({300.0f, 215.0f}, {200.0f, 48.0f}, "PLAY", font);
    Button conquestButton({300.0f, 278.0f}, {200.0f, 48.0f}, "CONQUEST", font);
    Button deckEditorButton({300.0f, 341.0f}, {200.0f, 48.0f}, "DECK EDITOR", font);
    Button shopButton({300.0f, 404.0f}, {200.0f, 48.0f}, "SHOP", font);
    Button adminUsersButton({300.0f, 467.0f}, {200.0f, 48.0f}, "ADMIN", font);
    Button logoutButton({300.0f, 530.0f}, {200.0f, 48.0f}, "LOG OUT", font);

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
    CheckboxControl collectionSpellFilterCheckbox({646.0f, 172.0f}, "Spells & Enchantments", font, rememberCheckTexture, 13, 16.0f, 22.0f);
    std::vector<CheckboxControl> collectionTraitFilterCheckboxes;
    collectionTraitFilterCheckboxes.reserve(game_data::CardTraitLabels.size());
    for (std::size_t i = 0; i < game_data::CardTraitLabels.size(); ++i)
    {
        collectionTraitFilterCheckboxes.emplace_back(
            sf::Vector2f{430.0f + static_cast<float>(i % 3) * 105.0f, 217.0f + static_cast<float>(i / 3) * 24.0f},
            game_data::CardTraitLabels[i],
            font,
            rememberCheckTexture,
            13,
            16.0f,
            22.0f);
    }
    Button addCardButton({574.0f, 508.0f}, {88.0f, 38.0f}, "Add", font);
    Button saveDeckButton({668.0f, 508.0f}, {108.0f, 38.0f}, "Save", font);
    Button shopBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    // Shop action row: coin pack and refresh only exist when coin purchases are
    // enabled, so the remaining buttons spread out to fill the row.
    Button buyCoinPackButton({46.0f, 492.0f}, {168.0f, 46.0f}, "Buy " + std::to_string(CoinPackCoins) + " Coins", font);
    Button refreshShopButton({226.0f, 492.0f}, {168.0f, 46.0f}, "Refresh", font);
    Button shopStarterDecksButton(
        {EnableCoinPurchases ? 406.0f : 190.0f, 492.0f},
        {EnableCoinPurchases ? 168.0f : 200.0f, 46.0f},
        "Starter Decks",
        font);
    Button buyCardButton(
        {EnableCoinPurchases ? 586.0f : 410.0f, 492.0f},
        {EnableCoinPurchases ? 168.0f : 200.0f, 46.0f},
        "Buy Card",
        font);
    Button dismissRevealedCardButton({300.0f, 492.0f}, {200.0f, 46.0f}, "Dismiss", font);
    Button starterDeckBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button claimStarterDeckButton({280.0f, 502.0f}, {240.0f, 46.0f}, "Claim Deck", font);
    // The Tools tab collects the admin-only screens that used to sit on the main
    // menu. Tabs are a little narrower than three at the old width would be so
    // the signed-in text still fits between the strip and the Back button.
    TabStrip adminTabs({24.0f, 22.0f}, {140.0f, 38.0f}, {"Users", "Starter Decks", "Tools"}, font);
    Button adminBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button adminPrevPageButton({530.0f, 93.0f}, {52.0f, 38.0f}, "<", font);
    Button adminNextPageButton({706.0f, 93.0f}, {52.0f, 38.0f}, ">", font);
    Button adminRefreshButton({592.0f, 93.0f}, {104.0f, 38.0f}, "Refresh", font);
    Button adminGrantButton({42.0f, 458.0f}, {150.0f, 42.0f}, "Grant Admin", font);
    Button adminRevokeButton({42.0f, 458.0f}, {150.0f, 42.0f}, "Revoke Admin", font);
    Button adminGrantGoldButton({378.0f, 458.0f}, {150.0f, 42.0f}, "Grant Gold", font);
    Button adminRemoveGoldButton({542.0f, 458.0f}, {150.0f, 42.0f}, "Remove Gold", font);
    Button adminAddCardButton({42.0f, 514.0f}, {176.0f, 40.0f}, "Add Card", font);
    Button adminGiveStarterDeckButton({228.0f, 514.0f}, {176.0f, 40.0f}, "Give Deck", font);
    Button adminSandboxButton({48.0f, 152.0f}, {220.0f, 54.0f}, "Sandbox", font);
    Button adminCardEditorButton({48.0f, 246.0f}, {220.0f, 54.0f}, "Card Editor", font);
    Button adminDeleteButton({600.0f, 514.0f}, {176.0f, 40.0f}, "Delete User", font);
    Button cancelAddCardButton({250.0f, 476.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmAddCardButton({420.0f, 476.0f}, {130.0f, 42.0f}, "Add Card", font);
    Button cancelGiveStarterDeckButton({250.0f, 424.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmGiveStarterDeckButton({420.0f, 424.0f}, {130.0f, 42.0f}, "Give Deck", font);
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
    ConquestScreen conquestScreen(font, textures);
    AudioSystem audioSystem;
    activeAudioSystem = &audioSystem;
    setButtonClickHandler(playButtonClickSound);

    sf::Clock clock;
    float animationTime = 0.0f;
    // Armed the first frame the matchmaking screen is shown, so the elapsed
    // search time counts from when the player actually started queuing.
    float matchmakingSearchStart = 0.0f;
    GameState lastFrameState = GameState::Menu;
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
    std::optional<std::future<AdminUserCardResult>> pendingAdminUserCard;
    std::optional<std::future<AdminUserStarterDeckResult>> pendingAdminUserStarterDeck;
    std::optional<std::future<StarterDeckOffersResult>> pendingStarterDeckOffers;
    std::optional<std::future<StarterDeckClaimResult>> pendingStarterDeckClaim;
    std::optional<std::future<CardListResult>> pendingAdminCardListLoad;
    std::optional<std::future<AdminUserDeleteResult>> pendingAdminUserDelete;
    std::optional<std::future<AccountCommandResult>> pendingPasswordChange;
    std::optional<std::future<ConquestBattleJoinResult>> pendingConquestBattleJoin;
    std::string pendingConquestBattleAccessToken;
    std::string pendingConquestBattleUsername;
    std::uint64_t conquestScreenGeneration = 0;
    std::uint64_t pendingConquestBattleGeneration = 0;
    std::uint64_t pendingConquestBattleEventId = 0;
    bool coinPurchasePolling = false;
    int coinPurchaseStartingCoins = 0;
    float nextCoinPurchasePollAt = 0.0f;
    float coinPurchasePollDeadline = 0.0f;
    std::shared_ptr<bayou::tls::Socket> activeGameSocket;
    bool conquestBattleMode = false;
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
    bool authenticatedSettingsHovered = false;
    bool pendingAutoLogin = false;
    bool pendingRememberRequested = false;
    DeckEditorMode deckEditorMode = DeckEditorMode::DeckList;
    // Deck editor repurposed by admins to edit the four faction starter decks:
    // the deck list shows those decks, the library shows every card (copy limits
    // instead of owned copies), and saves go to the admin starter deck endpoint.
    bool starterDeckMode = false;
    // Records whether confirming the unsaved-changes popup should leave the
    // starter deck editor entirely instead of returning to its deck list.
    bool starterDeckExitRequested = false;
    // Admin tab the starter deck editor should open once it has been left, so a
    // tab click that has to wait on the unsaved-changes popup still lands there.
    std::size_t starterDeckExitTab = 0;
    std::vector<card_data::Card> cardLibrary;
    std::vector<card_data::Card> filteredCardLibrary;
    std::vector<card_data::Card> allCardLibrary;
    std::array<bool, CollectionTypeLabels.size()> collectionTypeFilterChecked = {true, true, true};
    std::array<bool, game_data::CardTraitLabels.size()> collectionTraitFilterChecked = [] {
        std::array<bool, game_data::CardTraitLabels.size()> checked{};
        checked.fill(true);
        return checked;
    }();
    std::vector<deck_data::Deck> playerDecks;
    std::vector<account_data::CollectionCard> playerCollection;
    std::vector<network::AdminUserSummary> adminUsers;
    std::vector<card_data::Card> adminCardLibrary;
    deck_data::Deck editingDeck;
    std::string activeDeckOriginalName;
    int playerCoins = 0;
    int playerRating = 0;
    ranking::League playerLeague = ranking::League::Wood;
    bool loggedInIsAdmin = false;
    std::string adminSearchQuery;
    std::uint32_t adminUsersPage = 0;
    std::uint32_t adminUsersPageSize = AdminUsersPageSize;
    std::uint32_t adminUsersTotalCount = 0;
    std::optional<std::size_t> selectedAdminUser;
    bool addCardPopupVisible = false;
    bool giveStarterDeckPopupVisible = false;
    std::optional<std::size_t> selectedAdminStarterDeck;
    std::string adminCardLoadError;
    std::vector<network::StarterDeckOffer> starterDeckOffers;
    std::optional<std::size_t> selectedStarterDeckOffer;
    // Set while the player still owes their free pick: the screen is mandatory
    // and has no way back to the menu until a deck is claimed.
    bool starterDeckPickRequired = false;
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
    std::chrono::steady_clock::time_point gameSnapshotReceivedAt{};
    bool haveSnapshot = false;
    ClockWarningTracker clockWarningTracker;
    struct DisplayedClockWarning
    {
        int playerNumber = 0;
        std::int64_t thresholdMs = 0;
        std::chrono::steady_clock::time_point visibleUntil{};
    };
    std::optional<DisplayedClockWarning> displayedClockWarning;
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
    int gameDragPieceRowOffset = 0;
    int gameDragPieceColumnOffset = 0;
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
    struct PieceFidgetAnimation
    {
        float nextStartTime = 0.0f;
        float startTime = 0.0f;
        bool playing = false;
    };
    std::unordered_map<int, PieceFidgetAnimation> pieceFidgetAnimations;
    std::mt19937 fidgetRandomEngine(std::random_device{}());
    std::mt19937 sandboxDamageRandomEngine(std::random_device{}());
    struct PieceKilledAnimation
    {
        game_data::Piece piece;
        float startTime = 0.0f;
        float duration = PieceReactionAnimationDurationSeconds;
    };
    std::vector<PieceKilledAnimation> pieceKilledAnimations;
    struct FloatingNumberEffect
    {
        int row = 0;
        int column = 0;
        sf::Vector2f screenPosition;
        bool boardPosition = false;
        std::string text;
        sf::Color color = sf::Color::White;
        float startTime = 0.0f;
        float duration = 1.15f;
    };
    std::vector<FloatingNumberEffect> floatingNumberEffects;
    // An opposing piece that just dematerialized: it blinks in place for a few
    // seconds, then is not drawn at all until it materializes again.
    struct DematerializeGhost
    {
        game_data::Piece piece;
        float startTime = 0.0f;
    };
    std::vector<DematerializeGhost> dematerializeGhosts;

    Button findMatchButton({300.0f, 458.0f}, {200.0f, 52.0f}, "Find Match", font);
    Button abilityButton(
        {GameActionButtonX, GameActionButtonY},
        {GameAbilityButtonWidth, GameActionButtonHeight},
        "Use Ability",
        font);
    Button endTurnButton(
        {GameActionButtonX + GameAbilityButtonWidth + GameActionButtonGap, GameActionButtonY},
        {GameEndTurnButtonWidth, GameActionButtonHeight},
        "Pass Turn",
        font);
    Button sandboxPlayerButton(
        {GameActionButtonX, GameActionButtonY}, {48.0f, GameActionButtonHeight}, "P1", font);
    Button sandboxAdvanceTurnButton(
        {GameActionButtonX + 48.0f + GameActionButtonGap, GameActionButtonY},
        {96.0f, GameActionButtonHeight},
        "Advance",
        font);
    Button leaveGameButton(
        {GameActionButtonX + GameAbilityButtonWidth + GameActionButtonGap +
             GameEndTurnButtonWidth + GameActionButtonGap,
         GameActionButtonY},
        {GameLeaveButtonWidth, GameActionButtonHeight},
        "Leave",
        font);
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
        adminCardInput.setActive(false);
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
            // Starter decks cannot be created or deleted, so Refresh and Edit
            // take the centre of the row on their own.
            newDeckButton.setPosition({244.0f, 140.0f});
            refreshDeckButton.setPosition({starterDeckMode ? 345.0f : 376.0f, 140.0f});
            editDeckButton.setPosition({starterDeckMode ? 345.0f : 244.0f, 508.0f});
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
        // Three tiers instead of seven identical plates. Play is the reason the
        // player opened the game, so it is the only wide plate; the four modes
        // below share one rhythm; Admin and Log Out are shrunk into a footer pair
        // so leaving the game never competes with entering it.
        constexpr float centerX = 400.0f;
        constexpr float primaryWidth = 264.0f;
        constexpr float primaryHeight = 62.0f;
        constexpr float secondaryWidth = 214.0f;
        constexpr float secondaryHeight = 46.0f;
        constexpr float footerWidth = 104.0f;
        constexpr float footerHeight = 32.0f;

        playButton.setVariant(ButtonVariant::Primary);
        playButton.setSize({primaryWidth, primaryHeight});
        playButton.setPosition({centerX - primaryWidth * 0.5f, 168.0f});

        float y = 256.0f;
        auto placeSecondary = [&](Button& button) {
            button.setVariant(ButtonVariant::Secondary);
            button.setSize({secondaryWidth, secondaryHeight});
            button.setPosition({centerX - secondaryWidth * 0.5f, y});
            y += secondaryHeight + 13.0f;
        };

        placeSecondary(storyButton);
        placeSecondary(conquestButton);
        placeSecondary(deckEditorButton);
        placeSecondary(shopButton);

        const float footerY = y + 10.0f;
        const bool showAdmin = loggedInIsAdmin;
        const float footerSpan = showAdmin ? footerWidth * 2.0f + 12.0f : footerWidth;
        float footerX = centerX - footerSpan * 0.5f;
        auto placeFooter = [&](Button& button) {
            button.setVariant(ButtonVariant::Quiet);
            button.setSize({footerWidth, footerHeight});
            button.setLabelSize(type::Caption);
            button.setPosition({footerX, footerY});
            footerX += footerWidth + 12.0f;
        };

        if (showAdmin)
        {
            placeFooter(adminUsersButton);
        }
        placeFooter(logoutButton);
    };

    auto drawMainMenuTextureStretched =
        [&](sf::Texture* texture, sf::Vector2f position, sf::Vector2f size, sf::Color color = sf::Color::White) {
            if (!texture)
            {
                return;
            }

            const sf::Vector2u textureSize = texture->getSize();
            if (textureSize.x == 0 || textureSize.y == 0)
            {
                return;
            }

            sf::Sprite sprite(*texture);
            sprite.setPosition(position);
            sprite.setScale({
                size.x / static_cast<float>(textureSize.x),
                size.y / static_cast<float>(textureSize.y)});
            sprite.setColor(color);
            window.draw(sprite);
        };

    auto drawMainMenuTextureContained =
        [&](sf::Texture* texture, sf::Vector2f position, sf::Vector2f size, sf::Color color = sf::Color::White) {
            if (texture)
            {
                drawContainSprite(window, *texture, {position, size}, color);
            }
        };

    auto drawGloomthornWordmark = [&](sf::Vector2f center, sf::Vector2f size) {
        const sf::Vector2f position{
            center.x - size.x * 0.5f,
            center.y - size.y * 0.5f};
        if (gloomthornTitleTexture)
        {
            drawContainSprite(window, *gloomthornTitleTexture, {position, size});
            return;
        }

        sf::Font& wordmarkFont = gloomthornFontLoaded ? gloomthornFont : font;
        unsigned int characterSize = static_cast<unsigned int>(std::max(18.0f, size.y * 0.82f));
        sf::Text measuring(wordmarkFont, "Gloomthorn", characterSize);
        while (characterSize > 18 &&
               (measuring.getLocalBounds().size.x > size.x ||
                measuring.getLocalBounds().size.y > size.y))
        {
            measuring.setCharacterSize(--characterSize);
        }

        sf::Text glow(wordmarkFont, "Gloomthorn", characterSize);
        glow.setFillColor(sf::Color(214, 139, 48, 70));
        glow.setOutlineThickness(3.0f);
        glow.setOutlineColor(sf::Color(214, 139, 48, 35));
        centerButtonText(glow, center);
        window.draw(glow);

        sf::Text shadow(wordmarkFont, "Gloomthorn", characterSize);
        shadow.setFillColor(sf::Color(18, 8, 3, 235));
        shadow.setOutlineThickness(2.0f);
        shadow.setOutlineColor(sf::Color(0, 0, 0, 210));
        centerButtonText(shadow, center + sf::Vector2f(2.0f, 3.0f));
        window.draw(shadow);

        sf::Text text(wordmarkFont, "Gloomthorn", characterSize);
        text.setFillColor(sf::Color(221, 174, 82));
        text.setOutlineThickness(1.5f);
        text.setOutlineColor(sf::Color(255, 226, 145));
        centerButtonText(text, center);
        window.draw(text);
    };

    auto drawCoinIcon = [&](sf::Vector2f position, float radius) {
        if (mainMenuCoinTexture)
        {
            drawMainMenuTextureContained(
                mainMenuCoinTexture,
                position,
                {radius * 2.0f, radius * 2.0f});
            return;
        }

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

    auto drawAuthenticatedMenuButton = [&](const Button& button, sf::Texture* iconTexture) {
        const sf::Vector2f position = button.shape.getPosition();
        const sf::Vector2f size = button.shape.getSize();
        const sf::Vector2f center{position.x + size.x * 0.5f, position.y + size.y * 0.5f};
        const bool primary = button.getVariant() == ButtonVariant::Primary;
        const bool footer = button.getVariant() == ButtonVariant::Quiet;

        // The footer pair is deliberately plain metal: reusing the ornate
        // button_blank plaque at that size would put Log Out on the same visual
        // footing as Play.
        if (footer || !mainMenuButtonTexture)
        {
            button.draw(window, animationTime);
            return;
        }

        if (primary)
        {
            // Warm bloom under the primary plaque, so Play reads first even
            // before the eye resolves any text.
            drawRadialGlow(
                window,
                center,
                size.x * 0.72f,
                sf::Color(232, 168, 76, button.hovered ? 62 : 34));
        }

        // Scale button_blank slightly past the hitbox so the plaque reads larger.
        const float bgPadX = primary ? 20.0f : 14.0f;
        const float bgPadY = primary ? 11.0f : 8.0f;
        drawTextureRectContain(
            window,
            *mainMenuButtonTexture,
            sf::IntRect({48, 312}, {1435, 306}),
            {
                {position.x - bgPadX, position.y - bgPadY},
                {size.x + bgPadX * 2.0f, size.y + bgPadY * 2.0f}},
            button.hovered ? sf::Color::White : sf::Color(primary ? 244 : 226, primary ? 244 : 226, primary ? 244 : 226));

        const float iconSize = primary ? 30.0f : 23.0f;
        const float iconLeft = position.x + (primary ? 24.0f : 18.0f);
        drawMainMenuTextureContained(
            iconTexture,
            {iconLeft, position.y + (size.y - iconSize) * 0.5f},
            {iconSize, iconSize},
            button.hovered ? sf::Color::White : sf::Color(235, 225, 202));

        // Menu labels take the display face: a main menu is exactly where
        // display type belongs, and it separates navigation from body copy.
        sf::Text label(
            displayFontOr(font),
            button.text.getString(),
            primary ? 27u : 19u);
        label.setFillColor(button.hovered ? palette::InkBright : palette::Ink);
        label.setOutlineThickness(primary ? 1.0f : 0.0f);
        label.setOutlineColor(sf::Color(58, 33, 14, 190));
        // Nudge off the icon so the label optically centres in the clear space.
        centerButtonText(label, {center.x + (primary ? 12.0f : 8.0f), center.y});
        drawCrispText(window, label);
    };

    auto drawAuthenticatedMenuTitle = [&]() {
        if (mainMenuTitleFrameTexture)
        {
            // Stretch the frame to the dest rect (contain letterboxes and won't grow padding).
            const sf::FloatRect frameRect{{190.0f, 26.0f}, {420.0f, 130.0f}};
            const sf::IntRect textureRect({86, 307}, {1368, 403});
            sf::Sprite frame(*mainMenuTitleFrameTexture);
            frame.setTextureRect(textureRect);
            frame.setPosition(frameRect.position);
            frame.setScale({
                frameRect.size.x / static_cast<float>(textureRect.size.x),
                frameRect.size.y / static_cast<float>(textureRect.size.y)});
            window.draw(frame);
        }

        // Slightly smaller than the frame so the wordmark has breathing room inside.
        drawGloomthornWordmark({400.0f, 75.0f}, {310.0f, 54.0f});
    };

    // League identity colours. A rank is the one place a cool accent belongs on
    // this menu, and each tier needs its own so the badge reads at a glance.
    auto leagueAccent = [](ranking::League league) {
        switch (league)
        {
        case ranking::League::Wood: return sf::Color(150, 118, 78);
        case ranking::League::Bronze: return sf::Color(198, 126, 66);
        case ranking::League::Silver: return sf::Color(196, 205, 214);
        case ranking::League::Gold: return sf::Color(238, 194, 96);
        case ranking::League::Diamond: return sf::Color(140, 202, 226);
        case ranking::League::Master: return sf::Color(176, 132, 224);
        case ranking::League::Grandmaster: return sf::Color(236, 148, 116);
        }
        return palette::Brass;
    };

    auto drawLeagueSigil = [&](sf::Vector2f center, float radius, sf::Color accent) {
        // A faceted gem rather than a circle: it has to look struck, not drawn.
        drawRadialGlow(window, center, radius * 2.1f, sf::Color(accent.r, accent.g, accent.b, 58));

        sf::CircleShape ring(radius, 6);
        ring.setOrigin({radius, radius});
        ring.setPosition(center);
        ring.setRotation(sf::degrees(30.0f));
        ring.setFillColor(sf::Color(14, 18, 19, 240));
        ring.setOutlineThickness(1.6f);
        ring.setOutlineColor(accent);
        window.draw(ring);

        sf::CircleShape gem(radius * 0.52f, 6);
        gem.setOrigin({radius * 0.52f, radius * 0.52f});
        gem.setPosition(center);
        gem.setRotation(sf::degrees(30.0f));
        gem.setFillColor(sf::Color(accent.r, accent.g, accent.b, 225));
        window.draw(gem);

        sf::CircleShape glint(radius * 0.2f, 6);
        glint.setOrigin({radius * 0.2f, radius * 0.2f});
        glint.setPosition(center + sf::Vector2f(-radius * 0.18f, -radius * 0.22f));
        glint.setFillColor(sf::Color(255, 250, 236, 205));
        window.draw(glint);
    };

    auto drawPlayerBadge = [&]() {
        // Width is bounded by the title frame, which starts at x = 190 and cannot
        // move: the wordmark is centred on the screen.
        constexpr sf::Vector2f BadgePosition{10.0f, 14.0f};
        constexpr sf::Vector2f BadgeSize{174.0f, 100.0f};

        PlateStyle badge;
        badge.fill = sf::Color(11, 16, 17, 226);
        badge.frame = palette::Brass;
        badge.cut = 13.0f;
        badge.rivets = false;
        drawMaterialPlate(window, BadgePosition, BadgeSize, badge);

        // ---- portrait -----------------------------------------------------
        constexpr sf::Vector2f PortraitCenter{46.0f, 55.0f};
        constexpr float PortraitRadius = 25.0f;
        // account_profile_circle_frame is a filled disc rather than a ring, so it
        // has to go down first as the bezel; drawing it last is what left the
        // portrait looking permanently empty.
        if (mainMenuProfileFrameTexture)
        {
            drawMainMenuTextureContained(
                mainMenuProfileFrameTexture,
                {PortraitCenter.x - PortraitRadius - 4.0f, PortraitCenter.y - PortraitRadius - 4.5f},
                {(PortraitRadius + 4.0f) * 2.0f, (PortraitRadius + 4.5f) * 2.0f});
        }

        // Character art is drawn on transparency, so the well needs its own
        // ground before the figure goes on top.
        constexpr float AvatarRadius = 21.5f;
        sf::CircleShape portraitGround(AvatarRadius, 40);
        portraitGround.setOrigin({AvatarRadius, AvatarRadius});
        portraitGround.setPosition(PortraitCenter);
        portraitGround.setFillColor(sf::Color(17, 25, 27, 255));
        window.draw(portraitGround);

        if (mainMenuAvatarTexture)
        {
            // A textured CircleShape is the only masking SFML offers, and it is
            // exactly what a round portrait wants. The rect crops to head and
            // antlers; the full figure would be illegible at this size.
            sf::CircleShape portrait(AvatarRadius, 40);
            portrait.setOrigin({AvatarRadius, AvatarRadius});
            portrait.setPosition(PortraitCenter);
            portrait.setTexture(mainMenuAvatarTexture);
            portrait.setTextureRect(sf::IntRect({98, 38}, {54, 54}));
            window.draw(portrait);
        }

        // A whisper of shade at the bottom of the well so the bezel reads as
        // sitting over the art rather than beside a flat cut-out.
        sf::CircleShape portraitShade(AvatarRadius, 40);
        portraitShade.setOrigin({AvatarRadius, AvatarRadius});
        portraitShade.setPosition(PortraitCenter + sf::Vector2f(0.0f, AvatarRadius * 0.66f));
        portraitShade.setFillColor(sf::Color(4, 8, 9, 74));
        window.draw(portraitShade);

        sf::CircleShape portraitRing(AvatarRadius + 0.5f, 40);
        portraitRing.setOrigin({AvatarRadius + 0.5f, AvatarRadius + 0.5f});
        portraitRing.setPosition(PortraitCenter);
        portraitRing.setFillColor(sf::Color::Transparent);
        portraitRing.setOutlineThickness(1.5f);
        portraitRing.setOutlineColor(sf::Color(158, 112, 56, 220));
        window.draw(portraitRing);

        // ---- identity -----------------------------------------------------
        constexpr float TextLeft = 80.0f;
        const float textRight = BadgePosition.x + BadgeSize.x - 10.0f;
        const float column = textRight - TextLeft;

        unsigned int nameSize = 17;
        sf::Text name(displayFontOr(font), loggedInUsername, nameSize);
        while (nameSize > 11 && name.getLocalBounds().size.x > column - 6.0f)
        {
            name.setCharacterSize(--nameSize);
        }
        name.setFillColor(palette::Ink);
        name.setPosition({TextLeft, 22.0f});
        drawCrispText(window, name);

        drawSeparatorRule(window, {TextLeft, 45.0f}, column);

        // ---- rank ---------------------------------------------------------
        const sf::Color accent = leagueAccent(playerLeague);
        drawLeagueSigil({TextLeft + 8.0f, 58.0f}, 8.0f, accent);
        const float leagueWidth = drawLabelText(
            window,
            font,
            std::string(ranking::leagueName(playerLeague)) + " league",
            9,
            {TextLeft + 20.0f, 54.0f},
            accent,
            1.5f);

        if (playerLeague == ranking::League::Wood && mainMenuWoodLeagueTexture)
        {
            drawMainMenuTextureContained(
                mainMenuWoodLeagueTexture,
                {TextLeft + 23.0f + leagueWidth, 51.0f},
                {14.0f, 15.0f});
        }

        if (loggedInIsAdmin)
        {
            // A violet tag rather than "[Admin]" welded onto the name: a role is
            // metadata, not part of what the player calls themselves. It hangs
            // below the badge so it can never crowd the rank or the rating.
            const sf::Vector2f tagSize{54.0f, 15.0f};
            const sf::Vector2f tagPosition{
                BadgePosition.x + 8.0f,
                BadgePosition.y + BadgeSize.y + 6.0f};
            PlateStyle tag;
            tag.fill = sf::Color(38, 24, 54, 232);
            tag.frame = palette::Arcane;
            tag.cut = 4.0f;
            tag.rivets = false;
            tag.brackets = false;
            tag.sheen = 0.3f;
            drawMaterialPlate(window, tagPosition, tagSize, tag);
            drawLabelText(
                window,
                font,
                "admin",
                9,
                {tagPosition.x + 9.0f, tagPosition.y + 3.0f},
                palette::ArcaneBright,
                1.4f);
        }

        // Position the numerals off the measured label width rather than a magic
        // offset, so a longer word can never run into the number.
        const float ratingLabelWidth =
            drawLabelText(window, font, "rating", 8, {TextLeft + 20.0f, 69.0f}, palette::InkFaint, 1.4f);
        sf::Text rating(displayFontOr(font), std::to_string(playerRating), 14);
        rating.setFillColor(palette::BrassBright);
        rating.setPosition({TextLeft + 26.0f + ratingLabelWidth, 64.0f});
        drawCrispText(window, rating);

        // ---- currency -----------------------------------------------------
        const sf::Vector2f coinPill{TextLeft, 82.0f};
        const sf::Vector2f pillSize{std::min(78.0f, column), 19.0f};
        drawValuePill(
            window,
            font,
            coinPill,
            pillSize,
            std::to_string(playerCoins),
            palette::Gold,
            20.0f);
        drawCoinIcon({coinPill.x + 2.5f, coinPill.y + 1.5f}, 8.0f);
    };

    // A tracked-caps build mark on a hairline, the way a shipping client marks
    // itself, rather than bare text in the corner.
    auto drawBuildStamp = [&]() {
        constexpr float StampRight = 786.0f;
        constexpr float StampY = 578.0f;
        sf::Text version(font, "BUILD 1.0.0", type::Micro);
        version.setLetterSpacing(1.6f);
        version.setFillColor(sf::Color(156, 140, 112, 215));
        const float versionWidth = version.getLocalBounds().size.x;
        version.setPosition({StampRight - versionWidth, StampY});
        drawCrispText(window, version);

        sf::RectangleShape stampRule({28.0f, 1.0f});
        stampRule.setPosition({StampRight - versionWidth - 34.0f, StampY + 5.0f});
        stampRule.setFillColor(sf::Color(146, 104, 52, 130));
        window.draw(stampRule);
    };

    auto drawAuthenticatedMenuChrome = [&]() {
        // Motes first: ambient life belongs behind the interface, never over it.
        drawAmbientMotes(window, animationTime, 40, sf::Color(178, 138, 224, 138));

        drawPlayerBadge();
        drawAuthenticatedMenuTitle();

        drawMainMenuTextureContained(
            mainMenuSmallHexTexture,
            {683.0f, 14.0f},
            {36.0f, 38.0f},
            authenticatedSettingsHovered ? sf::Color::White : sf::Color(225, 218, 202));
        drawMainMenuTextureContained(
            mainMenuSettingsTexture,
            {691.0f, 22.0f},
            {20.0f, 20.0f},
            authenticatedSettingsHovered ? sf::Color::White : sf::Color(235, 225, 202));

        drawBuildStamp();
    };

    auto authenticatedSettingsButtonClicked = [&](sf::Vector2f point) {
        return isInsideRect(point, 682.0f, 13.0f, 38.0f, 40.0f);
    };

    auto drawExitDesktopCloseButton = [&]() {
        if (currentState == GameState::Authenticated && mainMenuExitTexture)
        {
            drawMainMenuTextureStretched(
                mainMenuExitTexture,
                {730.0f, -2.0f},
                {58.0f, 90.0f},
                exitDesktopCloseHovered ? sf::Color::White : sf::Color(224, 214, 202));
            return;
        }

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
        if (currentState == GameState::Authenticated)
        {
            return isInsideRect(point, 730.0f, 0.0f, 58.0f, 72.0f);
        }
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
        playerLeague = result.league;
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
            return found == cardLibrary.end() ? 0 : game_data::cardDeckLimit(*found);
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
        const int copyLimit = game_data::cardDeckLimit(card);
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
        else if (card.type == "Spell" || card.type == "Enchantment")
        {
            typeMatches = collectionTypeFilterChecked[2];
        }
        if (!typeMatches)
        {
            return false;
        }

        const bool allTraitsChecked = std::all_of(
            collectionTraitFilterChecked.begin(),
            collectionTraitFilterChecked.end(),
            [](bool checked) {
                return checked;
            });
        if (allTraitsChecked)
        {
            return true;
        }

        for (const std::string& trait : card.traits)
        {
            const std::string normalizedCardTrait = game_data::normalizedTrait(trait);
            for (std::size_t i = 0; i < game_data::CardTraitLabels.size(); ++i)
            {
                if (collectionTraitFilterChecked[i] &&
                    normalizedCardTrait == game_data::normalizedTrait(game_data::CardTraitLabels[i]))
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

    auto toggleCollectionTraitFilter = [&](std::size_t index) {
        if (index >= collectionTraitFilterChecked.size())
        {
            return;
        }
        collectionTraitFilterChecked[index] = !collectionTraitFilterChecked[index];
        libraryOffset = 0;
        applyCollectionFilters();
    };

    auto clickCollectionTraitFilter = [&](sf::Vector2f clickPos) {
        for (std::size_t i = 0; i < collectionTraitFilterCheckboxes.size(); ++i)
        {
            if (collectionTraitFilterCheckboxes[i].isClicked(clickPos))
            {
                clearFocus();
                toggleCollectionTraitFilter(i);
                return true;
            }
        }
        return false;
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
        adminCardLibrary.clear();
        collectionTypeFilterChecked.fill(true);
        collectionTraitFilterChecked.fill(true);
        deckEditorMode = DeckEditorMode::DeckList;
        starterDeckMode = false;
        starterDeckExitRequested = false;
        starterDeckExitTab = 0;
        starterDeckOffers.clear();
        selectedStarterDeckOffer.reset();
        starterDeckPickRequired = false;
        giveStarterDeckPopupVisible = false;
        selectedAdminStarterDeck.reset();
        adminTabs.setActive(0);
        playerDecks.clear();
        playerCollection.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        playerCoins = 0;
        playerRating = 0;
        playerLeague = ranking::League::Wood;
        loggedInIsAdmin = false;
        adminUsers.clear();
        adminSearchQuery.clear();
        adminUsersPage = 0;
        adminUsersTotalCount = 0;
        selectedAdminUser.reset();
        addCardPopupVisible = false;
        deleteUserPopupVisible = false;
        exitDesktopPopupVisible = false;
        deckUnsavedChangesPopupVisible = false;
        adminUserDeleteTarget.clear();
        adminSearchInput.clear();
        adminGoldInput.clear();
        adminCardInput.clear();
        adminCardLoadError.clear();
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
        clockWarningTracker.reset();
        displayedClockWarning.reset();
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
        gameDragPieceRowOffset = 0;
        gameDragPieceColumnOffset = 0;
        gameDragActive = false;
        title.setString("Gloomthorn");
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
        addCardPopupVisible = false;
        giveStarterDeckPopupVisible = false;
        selectedAdminStarterDeck.reset();
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
        if (adminCardLibrary.empty() && !pendingAdminCardListLoad)
        {
            adminCardLoadError.clear();
            pendingAdminCardListLoad = std::async(std::launch::async, fetchCards);
        }
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
        starterDeckExitTab = 0;
        adminTabs.setActive(1);
        deckEditorMode = DeckEditorMode::DeckList;
        deckUnsavedChangesPopupVisible = false;
        layoutDeckEditorControls();
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "Loading starter decks...", sf::Color::Yellow);
        clearFocus();
        cardLibrary.clear();
        filteredCardLibrary.clear();
        collectionTypeFilterChecked.fill(true);
        collectionTraitFilterChecked.fill(true);
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
        pendingStarterDeckLoad = std::async(std::launch::async, loadStarterDeckEditorData, activeAccessToken);
    };

    auto loadAdminToolsScreen = [&]() {
        if (!loggedInIsAdmin)
        {
            setMessage(messageText, "Admin access required", sf::Color::Red);
            return;
        }
        currentState = GameState::AdminTools;
        starterDeckMode = false;
        adminTabs.setActive(2);
        title.setString("");
        centerText(title, 400.0f);
        clearFocus();
        setMessageY(messageText, 566.0f);
        setMessage(messageText, "", sf::Color::White);
    };

    // The starter deck editor and the tools live on screens of their own; the
    // admin user list is tab 0.
    auto openAdminTab = [&](std::size_t index) {
        if (index == 1)
        {
            loadStarterDeckEditor();
        }
        else if (index == 2)
        {
            loadAdminToolsScreen();
        }
        else
        {
            loadAdminUsersScreen();
        }
    };

    auto leaveStarterDeckEditor = [&]() {
        starterDeckMode = false;
        starterDeckExitRequested = false;
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
        const std::size_t destination = starterDeckExitTab;
        starterDeckExitTab = 0;
        openAdminTab(destination);
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

    auto visibleAdminCardTitles = [&]() {
        std::vector<std::string> titles;
        const std::string query = game_data::normalizedAbility(adminCardInput.getContent());
        for (const card_data::Card& card : adminCardLibrary)
        {
            if (game_data::isTokenCard(card) ||
                (!query.empty() && game_data::normalizedTrait(card.title).find(query) == std::string::npos))
            {
                continue;
            }
            titles.push_back(card.title);
        }
        std::sort(titles.begin(), titles.end(), [](const std::string& left, const std::string& right) {
            return game_data::normalizedTrait(left) < game_data::normalizedTrait(right);
        });
        if (titles.size() > VisibleAdminCardRows)
        {
            titles.resize(VisibleAdminCardRows);
        }
        return titles;
    };

    auto openAddCardPopup = [&]() {
        if (!selectedAdminUser || *selectedAdminUser >= adminUsers.size())
        {
            return;
        }
        addCardPopupVisible = true;
        adminCardInput.clear();
        clearFocus();
        adminCardInput.setActive(true);
    };

    auto dismissAddCardPopup = [&]() {
        addCardPopupVisible = false;
        adminCardInput.clear();
        adminCardInput.setActive(false);
    };

    auto openGiveStarterDeckPopup = [&]() {
        if (!selectedAdminUser || *selectedAdminUser >= adminUsers.size())
        {
            return;
        }
        giveStarterDeckPopupVisible = true;
        selectedAdminStarterDeck = 0;
        clearFocus();
    };

    auto dismissGiveStarterDeckPopup = [&]() {
        giveStarterDeckPopupVisible = false;
        selectedAdminStarterDeck.reset();
    };

    auto confirmGiveStarterDeck = [&]() {
        if (pendingAdminUserStarterDeck || !selectedAdminStarterDeck ||
            *selectedAdminStarterDeck >= starter_decks::Names.size() ||
            !selectedAdminUser || *selectedAdminUser >= adminUsers.size())
        {
            return;
        }

        const std::string deckName = starter_decks::Names[*selectedAdminStarterDeck];
        const std::string targetUsername = adminUsers[*selectedAdminUser].username;
        setMessage(messageText, "Giving " + deckName + "...", sf::Color::Yellow);
        pendingAdminUserStarterDeck = std::async(
            std::launch::async,
            giveStarterDeckToAdminUser,
            activeAccessToken,
            targetUsername,
            deckName);
    };

    auto confirmAddCard = [&]() {
        if (pendingAdminUserCard || !selectedAdminUser || *selectedAdminUser >= adminUsers.size())
        {
            return;
        }

        const std::string requestedTitle = game_data::normalizedAbility(adminCardInput.getContent());
        const auto card = std::find_if(
            adminCardLibrary.begin(),
            adminCardLibrary.end(),
            [&](const card_data::Card& candidate) {
                return !game_data::isTokenCard(candidate) &&
                    game_data::normalizedTrait(candidate.title) == requestedTitle;
            });
        if (card == adminCardLibrary.end())
        {
            setMessage(messageText, "Choose a card from the list", sf::Color::Red);
            return;
        }

        const std::string targetUsername = adminUsers[*selectedAdminUser].username;
        const std::string cardTitle = card->title;
        pendingAdminUserCard = std::async(
            std::launch::async,
            addCardToAdminUser,
            activeAccessToken,
            targetUsername,
            cardTitle);
        adminCardInput.setActive(false);
        setMessage(messageText, "Adding card...", sf::Color::Yellow);
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
        title.setString(optionsReturnState == GameState::Authenticated ? "" : "Gloomthorn");
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

    auto showGameScreen = [&](std::shared_ptr<bayou::tls::Socket> gameSocket,
                              bool isConquestBattle = false) {
        activeGameSocket = std::move(gameSocket);
        conquestBattleMode = isConquestBattle;
        currentState = GameState::Game;
        abilityButton.setPosition({GameActionButtonX, GameActionButtonY});
        leaveGameButton.setLabel(isConquestBattle ? "Map" : "Leave");
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
        gameSnapshotReceivedAt = {};
        clockWarningTracker.reset();
        displayedClockWarning.reset();
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
        floatingNumberEffects.clear();
        pieceFidgetAnimations.clear();
        pieceKilledAnimations.clear();
        dematerializeGhosts.clear();

        // Ranked games submit the selected deck. A Conquest session restores
        // its frozen army deck on the coordinator before this socket is handed
        // to the client, so submitting again would corrupt the replay.
        if (activeGameSocket)
        {
            if (!isConquestBattle)
            {
                sendSubmitDeck(*activeGameSocket, matchDeck);
            }
            activeGameSocket->setBlocking(false);
        }
    };

    auto handleConquestScreenAction = [&]() {
        const std::optional<ConquestScreenAction> action = conquestScreen.takeAction();
        if (!action)
        {
            return;
        }
        if (action->kind == ConquestScreenAction::Kind::Close)
        {
            ++conquestScreenGeneration;
            showAuthenticatedScreen();
            return;
        }
        if (!pendingConquestBattleJoin)
        {
            conquestScreen.setStatus("Reconnecting to the tactical battle...", true);
            const std::uint64_t battleId = action->battleId;
            pendingConquestBattleAccessToken = activeAccessToken;
            pendingConquestBattleUsername = loggedInUsername;
            pendingConquestBattleGeneration = conquestScreenGeneration;
            pendingConquestBattleEventId = action->eventId;
            pendingConquestBattleJoin.emplace(std::async(
                std::launch::async,
                [token = activeAccessToken, battleId] {
                    return joinConquestBattle(token, battleId);
                }));
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
        collectionTraitFilterChecked.fill(true);
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

    // `required` marks the one-time free pick every account owes before it can
    // reach the menu; otherwise the screen is the shop's starter deck store.
    auto loadStarterDecksScreen = [&](bool required) {
        currentState = GameState::StarterDecks;
        starterDeckPickRequired = required;
        starterDeckOffers.clear();
        selectedStarterDeckOffer.reset();
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 560.0f);
        setMessage(messageText, "Loading starter decks...", sf::Color::Yellow);
        clearFocus();
        pendingStarterDeckOffers = std::async(std::launch::async, fetchStarterDeckOffers, activeAccessToken);
    };

    auto starterDecksBusy = [&]() {
        return pendingStarterDeckOffers.has_value() || pendingStarterDeckClaim.has_value();
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
        else if (!isValidNewPassword(passwordInput.getContent()))
        {
            setMessage(messageText, PasswordRequirementMessage, sf::Color::Red);
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
        else if (!isValidNewPassword(newPasswordInput.getContent()))
        {
            setMessage(messageText, NewPasswordRequirementMessage, sf::Color::Red);
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

    // ---- entry form geometry ----------------------------------------------
    // Both entry forms are laid out from one place: a floating column of fields
    // on the backdrop reads as an unfinished form, and the reserved notice row is
    // what stops the layout jumping when validation fails.
    const sf::Vector2f LoginPanelPosition{252.0f, 116.0f};
    const sf::Vector2f LoginPanelSize{296.0f, 352.0f};
    const sf::Vector2f CreatePanelPosition{252.0f, 108.0f};
    const sf::Vector2f CreatePanelSize{296.0f, 412.0f};
    constexpr float LoginNoticeY = 358.0f;
    constexpr float CreateNoticeY = 406.0f;
    constexpr float FormFieldX = 300.0f;
    constexpr float FormFieldWidth = 200.0f;

    // ---- options screen geometry -------------------------------------------
    // Label on the left, control on the right, separated by rules. Centred
    // labels stacked over centred controls is what made the old screen read as a
    // debug menu.
    const sf::Vector2f OptionsPanelPosition{140.0f, 156.0f};
    const sf::Vector2f OptionsPanelSize{519.0f, 302.0f};

    auto layoutOptionsScreen = [&]() {
        optionsTabs.position = {OptionsPanelPosition.x, 110.0f};
        optionsTabs.tabSize = {OptionsPanelSize.x / 3.0f, 46.0f};

        constexpr float ControlRight = 636.0f;
        displayModeButton.setSize({224.0f, 40.0f});
        displayModeButton.setPosition({ControlRight - 224.0f, 202.0f});
        displayModeButton.setLabelSize(type::Body);

        previousResolutionButton.setSize({38.0f, 40.0f});
        previousResolutionButton.setPosition({ControlRight - 224.0f, 270.0f});
        resolutionButton.setSize({144.0f, 40.0f});
        resolutionButton.setPosition({ControlRight - 182.0f, 270.0f});
        resolutionButton.setLabelSize(type::Body);
        nextResolutionButton.setSize({38.0f, 40.0f});
        nextResolutionButton.setPosition({ControlRight - 38.0f, 270.0f});

        applyOptionsButton.setVariant(ButtonVariant::Primary);
        applyOptionsButton.setSize({170.0f, 42.0f});
        applyOptionsButton.setPosition({ControlRight - 170.0f, 396.0f});
        applyOptionsButton.setLabelSize(type::Subheading);

        changePasswordOptionButton.setSize({224.0f, 40.0f});
        changePasswordOptionButton.setPosition({ControlRight - 224.0f, 270.0f});
        changePasswordOptionButton.setLabelSize(type::Body);

        // Sliders share one column with the mute toggles parked at its right.
        constexpr float SliderX = 168.0f;
        const sf::Vector2f sliderSize{330.0f, 50.0f};
        allAudioSlider.position = {SliderX, 202.0f};
        allAudioSlider.size = sliderSize;
        musicAudioSlider.position = {SliderX, 278.0f};
        musicAudioSlider.size = sliderSize;
        soundFxAudioSlider.position = {SliderX, 354.0f};
        soundFxAudioSlider.size = sliderSize;
        muteAllAudioCheckbox.setPosition({552.0f, 236.0f});
        muteMusicCheckbox.setPosition({552.0f, 312.0f});
        muteSoundFxCheckbox.setPosition({552.0f, 388.0f});

        optionsBackButton.setVariant(ButtonVariant::Quiet);
        optionsBackButton.setSize({170.0f, 40.0f});
        optionsBackButton.setPosition({315.0f, 474.0f});
        optionsBackButton.setLabelSize(type::Body);
    };

    auto layoutEntryForms = [&]() {
        loginSubmitButton.setVariant(ButtonVariant::Primary);
        createSubmitButton.setVariant(ButtonVariant::Primary);
        backButton.setVariant(ButtonVariant::Quiet);

        loginSubmitButton.setSize({FormFieldWidth, 46.0f});
        createSubmitButton.setSize({FormFieldWidth, 46.0f});
        backButton.setSize({132.0f, 34.0f});
        backButton.setLabelSize(type::Body);
    };

    auto layoutLoginForm = [&]() {
        layoutEntryForms();
        usernameInput.setPosition({FormFieldX, 214.0f});
        passwordInput.setPosition({FormFieldX, 286.0f});
        passwordVisibilityIcon.fieldBounds = passwordInput.bounds();
        rememberMeCheckbox.setPosition({FormFieldX, 336.0f});
        loginSubmitButton.setPosition({FormFieldX, 402.0f});
        backButton.setPosition({334.0f, 480.0f});
    };

    auto layoutCreateAccountForm = [&]() {
        layoutEntryForms();
        usernameInput.setPosition({FormFieldX, 186.0f});
        passwordInput.setPosition({FormFieldX, 254.0f});
        confirmInput.setPosition({FormFieldX, 322.0f});
        passwordVisibilityIcon.fieldBounds = passwordInput.bounds();
        confirmVisibilityIcon.fieldBounds = confirmInput.bounds();
        createSubmitButton.setLabelSize(type::Heading);
        createSubmitButton.setPosition({FormFieldX, 456.0f});
        backButton.setPosition({334.0f, 532.0f});
    };

    // A validation notice that belongs to the form rather than floating loose at
    // the bottom of the screen. The row is always reserved, so the button never
    // moves when a message appears.
    auto drawFormNotice = [&](float y) {
        const std::string message = messageText.getString().toAnsiString();
        if (message.empty())
        {
            return;
        }

        const sf::Color messageColor = messageText.getFillColor();
        const bool problem = messageColor.r > messageColor.g + 40 && messageColor.r > messageColor.b + 40;
        const sf::Color accent = problem ? palette::Danger : palette::Brass;
        const sf::Color ink = problem ? palette::DangerBright : palette::Ink;

        // Size the strip to the wrapped copy: a fixed-height notice either clips
        // the second line or leaves a hole under the first.
        constexpr float LineHeight = 15.0f;
        const sf::Vector2f position{264.0f, y};
        const float textWidth = 232.0f;
        const std::size_t lineCount =
            std::max<std::size_t>(1, wrapText(font, message, type::Caption, textWidth).size());
        const sf::Vector2f size{
            272.0f,
            std::max(26.0f, 11.0f + static_cast<float>(lineCount) * LineHeight)};

        drawInsetSlot(
            window,
            position,
            size,
            4.0f,
            problem ? sf::Color(38, 14, 12, 236) : sf::Color(24, 20, 15, 232),
            accent,
            false,
            false);

        // A glyph, not just colour: red text on a dark plate is easy to miss and
        // impossible for a colour-blind player to distinguish from brass.
        const sf::Vector2f markCenter{position.x + 17.0f, position.y + size.y * 0.5f};
        if (problem)
        {
            sf::CircleShape mark(7.5f, 3);
            mark.setOrigin({7.5f, 7.5f});
            mark.setPosition(markCenter + sf::Vector2f(0.0f, -0.5f));
            mark.setFillColor(sf::Color(214, 88, 72, 245));
            window.draw(mark);
            drawCenteredText(window, font, "!", 11, markCenter + sf::Vector2f(0.0f, 1.5f), sf::Color(28, 10, 8));
        }
        else
        {
            drawStud(window, markCenter, 5.0f, palette::Brass);
        }

        drawWrappedText(
            window,
            font,
            message,
            type::Caption,
            {position.x + 32.0f,
             position.y + (size.y - static_cast<float>(lineCount) * LineHeight) * 0.5f},
            ink,
            textWidth,
            3.0f);
    };

    // Heading band shared by both entry forms.
    auto drawEntryFormHeader = [&](sf::Vector2f panelPosition,
                                   sf::Vector2f panelSize,
                                   const std::string& heading,
                                   const std::string& flavour) {
        const float centerX = panelPosition.x + panelSize.x * 0.5f;
        drawCenteredText(
            window,
            displayFontOr(font),
            heading,
            type::Heading,
            {centerX, panelPosition.y + 24.0f},
            palette::Ink);

        if (!flavour.empty())
        {
            sf::Text line(font, flavour, type::Caption);
            line.setStyle(sf::Text::Italic);
            line.setFillColor(palette::InkMuted);
            centerText(line, {centerX, panelPosition.y + 44.0f});
            drawCrispText(window, line);
        }

        drawSeparatorRule(
            window,
            {panelPosition.x + 28.0f, panelPosition.y + (flavour.empty() ? 44.0f : 60.0f)},
            panelSize.x - 56.0f);
    };

    // ---- matchmaking --------------------------------------------------------
    // Two portrait slots and a live search ring, so waiting reads as the game
    // hunting for an opponent rather than a sentence over a static backdrop.
    auto drawSearchRing = [&](sf::Vector2f center, float radius, float phase, sf::Color color) {
        constexpr int Arcs = 3;
        constexpr int SegmentsPerArc = 9;
        for (int arc = 0; arc < Arcs; ++arc)
        {
            const float arcPhase = phase * (arc % 2 == 0 ? 1.0f : -0.72f) +
                static_cast<float>(arc) * 2.094f;
            const float arcRadius = radius + static_cast<float>(arc) * 5.5f;
            for (int segment = 0; segment < SegmentsPerArc; ++segment)
            {
                const float t = static_cast<float>(segment) / static_cast<float>(SegmentsPerArc);
                // Fade each dash along the arc so the ring reads as sweeping
                // rather than merely spinning.
                const float fade = 0.15f + 0.85f * t;
                const float angle = arcPhase + t * 1.35f;
                const sf::Vector2f point{
                    center.x + std::cos(angle) * arcRadius,
                    center.y + std::sin(angle) * arcRadius};

                sf::CircleShape dash(1.9f, 8);
                dash.setOrigin({1.9f, 1.9f});
                dash.setPosition(point);
                dash.setFillColor(sf::Color(
                    color.r,
                    color.g,
                    color.b,
                    static_cast<std::uint8_t>(std::lround(200.0f * fade))));
                window.draw(dash);
            }
        }
    };

    auto drawOpponentSlot = [&](sf::Vector2f center, bool unknown, const std::string& caption) {
        constexpr float Radius = 42.0f;

        drawRadialGlow(
            window,
            center,
            Radius * 1.65f,
            unknown ? sf::Color(123, 79, 168, 62) : sf::Color(196, 138, 62, 52));

        sf::CircleShape well(Radius, 44);
        well.setOrigin({Radius, Radius});
        well.setPosition(center);
        well.setFillColor(sf::Color(10, 15, 16, 246));
        window.draw(well);

        if (!unknown && mainMenuAvatarTexture)
        {
            sf::CircleShape portrait(Radius - 4.0f, 44);
            portrait.setOrigin({Radius - 4.0f, Radius - 4.0f});
            portrait.setPosition(center);
            portrait.setTexture(mainMenuAvatarTexture);
            portrait.setTextureRect(sf::IntRect({98, 38}, {54, 54}));
            window.draw(portrait);
        }
        else if (unknown)
        {
            // A breathing question sigil: the opponent is not a blank, they are
            // being looked for.
            const float pulse = 0.62f + 0.38f * (0.5f + 0.5f * std::sin(animationTime * 2.3f));
            sf::Text mark(displayFontOr(font), "?", 44);
            mark.setFillColor(sf::Color(
                palette::ArcaneBright.r,
                palette::ArcaneBright.g,
                palette::ArcaneBright.b,
                static_cast<std::uint8_t>(std::lround(235.0f * pulse))));
            centerText(mark, center);
            drawCrispText(window, mark);
        }

        sf::CircleShape rim(Radius, 44);
        rim.setOrigin({Radius, Radius});
        rim.setPosition(center);
        rim.setFillColor(sf::Color::Transparent);
        rim.setOutlineThickness(2.0f);
        rim.setOutlineColor(unknown ? sf::Color(112, 84, 138) : palette::Brass);
        window.draw(rim);

        if (unknown)
        {
            drawSearchRing(center, Radius + 9.0f, animationTime * 1.15f, palette::ArcaneBright);
        }

        drawCenteredText(
            window,
            displayFontOr(font),
            caption,
            type::Subheading,
            {center.x, center.y + Radius + 22.0f},
            unknown ? palette::InkMuted : palette::Ink);
    };

    auto drawMatchmakingScreen = [&]() {
        const sf::Vector2f panelPosition{176.0f, 150.0f};
        const sf::Vector2f panelSize{448.0f, 296.0f};

        // Cancel and Play vs AI belong under the panel they act on, not stranded
        // in the bottom-left corner of the screen.
        cancelMatchmakingButton.setVariant(ButtonVariant::Quiet);
        cancelMatchmakingButton.setSize({148.0f, 40.0f});
        cancelMatchmakingButton.setPosition({248.0f, 464.0f});
        cancelMatchmakingButton.setLabelSize(type::Body);
        playAiButton.setSize({148.0f, 40.0f});
        playAiButton.setPosition({404.0f, 464.0f});
        playAiButton.setLabelSize(type::Body);

        // Ambience goes behind the panel, never over it.
        drawAmbientMotes(window, animationTime, 26, sf::Color(178, 138, 224, 118));
        drawPanel(window, panelPosition, panelSize);

        const float centerX = panelPosition.x + panelSize.x * 0.5f;
        const std::string status = messageText.getString().toAnsiString();

        // Animated ellipsis so the screen is visibly working even when the
        // service has nothing new to say.
        const int dots = static_cast<int>(std::fmod(animationTime * 1.6f, 4.0f));
        std::string heading = status.empty() ? "Searching for an opponent" : status;
        while (!heading.empty() && (heading.back() == '.' || heading.back() == ' '))
        {
            heading.pop_back();
        }

        drawLabelText(
            window,
            font,
            heading + std::string(static_cast<std::size_t>(dots), '.'),
            type::Label,
            {panelPosition.x + 30.0f, panelPosition.y + 22.0f},
            palette::Brass,
            2.0f);
        drawSeparatorRule(window, {panelPosition.x + 28.0f, panelPosition.y + 44.0f}, panelSize.x - 56.0f);

        const float slotY = panelPosition.y + 128.0f;
        drawOpponentSlot({centerX - 112.0f, slotY}, false, loggedInUsername);
        drawOpponentSlot({centerX + 112.0f, slotY}, true, "Unknown");

        // The crossed-swords glyph already carries "versus" elsewhere in the
        // menu, so reuse it rather than inventing a second symbol.
        if (mainMenuPlayIconTexture)
        {
            drawMainMenuTextureContained(
                mainMenuPlayIconTexture,
                {centerX - 17.0f, slotY - 17.0f},
                {34.0f, 34.0f},
                sf::Color(232, 198, 140, 210));
        }

        const int elapsedSeconds =
            static_cast<int>(std::max(0.0f, animationTime - matchmakingSearchStart));
        char elapsed[16] = {};
        std::snprintf(elapsed, sizeof(elapsed), "%02d:%02d", elapsedSeconds / 60, elapsedSeconds % 60);

        drawValuePill(
            window,
            font,
            {centerX - 44.0f, panelPosition.y + panelSize.y - 62.0f},
            {88.0f, 24.0f},
            elapsed,
            palette::BrassBright);
        drawCenteredText(
            window,
            font,
            "Matching you against a similar rating.",
            type::Caption,
            {centerX, panelPosition.y + panelSize.y - 24.0f},
            palette::InkMuted);
    };

    auto drawPasswordRequirementHint = [&](float firstLineY) {
        auto drawCenteredHintLine = [&](const char* value, float y) {
            // Muted parchment rather than the old blue-grey, which was the only
            // cool-neutral text anywhere in the interface.
            sf::Text hint(font, value, type::Caption);
            hint.setFillColor(palette::InkMuted);
            hint.setPosition({400.0f, y});
            centerText(hint, 400.0f);
            drawCrispText(window, hint);
        };

        drawCenteredHintLine(PasswordRequirementHintLineOne, firstLineY);
        drawCenteredHintLine(PasswordRequirementHintLineTwo, firstLineY + 17.0f);
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
        std::vector<std::string> heroTraits;
        std::vector<std::string> traitMismatchTitles;
        std::vector<std::string> warnings;
    };

    auto computeDeckStats = [&]() {
        DeckStats stats;
        auto addHeroTrait = [&](const std::string& trait) {
            const std::string normalized = game_data::normalizedTrait(trait);
            const bool alreadyPresent = std::any_of(
                stats.heroTraits.begin(),
                stats.heroTraits.end(),
                [&](const std::string& existing) {
                    return game_data::normalizedTrait(existing) == normalized;
                });
            if (!alreadyPresent)
            {
                stats.heroTraits.push_back(trait);
            }
        };
        auto heroHasTrait = [&](const std::string& trait) {
            const std::string normalized = game_data::normalizedTrait(trait);
            return std::any_of(
                stats.heroTraits.begin(),
                stats.heroTraits.end(),
                [&](const std::string& heroTrait) {
                    return game_data::normalizedTrait(heroTrait) == normalized;
                });
        };

        for (const std::string& title : editingDeck.cardTitles)
        {
            const card_data::Card* card = cardByTitle(title);
            if (card && game_data::isHeroCard(*card))
            {
                ++stats.heroCount;
                stats.heroCost += game_data::cardInt(*card, "heroCost", 0);
                for (const std::string& trait : card->traits)
                {
                    if (!trait.empty())
                    {
                        addHeroTrait(trait);
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

            if (!game_data::isUnitCard(*card))
            {
                continue;
            }

            const bool missingHeroTrait = std::any_of(
                card->traits.begin(),
                card->traits.end(),
                [&](const std::string& trait) {
                    return !trait.empty() && !heroHasTrait(trait);
                });
            if (missingHeroTrait &&
                std::find(stats.traitMismatchTitles.begin(), stats.traitMismatchTitles.end(), title) ==
                    stats.traitMismatchTitles.end())
            {
                stats.traitMismatchTitles.push_back(title);
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
        if (!stats.traitMismatchTitles.empty())
        {
            stats.warnings.push_back("Highlighted units lack matching hero traits.");
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

        const std::string validationError = deckValidationError(deck);
        if (!validationError.empty())
        {
            setMessage(messageText, validationError, sf::Color::Red);
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
        const int copyLimit = game_data::cardDeckLimit(filteredCardLibrary[libraryIndex]);
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

    // `exitEditor` distinguishes leaving the starter deck editor for good (the
    // admin Users tab) from stepping back to its list of starter decks.
    auto requestLeaveDeckEdit = [&](bool exitEditor = false) {
        starterDeckExitRequested = starterDeckMode && exitEditor;
        if (deckHasUnsavedChanges())
        {
            deckUnsavedChangesPopupVisible = true;
            deckNameInput.setActive(false);
            clearFocus();
            return;
        }

        if (starterDeckExitRequested)
        {
            leaveStarterDeckEditor();
            return;
        }

        showDeckEditorDeckList();
        setMessage(
            messageText,
            starterDeckMode ? "Choose a starter deck to edit." : "Choose a deck to edit.",
            sf::Color(120, 220, 150));
    };

    auto discardDeckEditChanges = [&]() {
        deckUnsavedChangesPopupVisible = false;
        if (starterDeckExitRequested)
        {
            leaveStarterDeckEditor();
            setMessage(messageText, "Unsaved starter deck changes discarded.", sf::Color(220, 180, 120));
            return;
        }
        showDeckEditorDeckList();
        setMessage(
            messageText,
            starterDeckMode ? "Unsaved starter deck changes discarded." : "Unsaved deck changes discarded.",
            sf::Color(220, 180, 120));
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
        collectionTraitFilterChecked.fill(true);
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

    auto addFloatingNumber = [&](int value, int row, int column) {
        if (value == 0)
        {
            return;
        }
        floatingNumberEffects.push_back({
            row,
            column,
            {},
            true,
            (value > 0 ? "+" : "") + std::to_string(value),
            value > 0 ? sf::Color(120, 235, 145) : sf::Color(245, 115, 105),
            animationTime,
            1.15f});
    };

    auto addResourceNumber = [&](int playerNumber, int value, int displayedResources) {
        if (value == 0)
        {
            return;
        }
        const std::string effectText = (value > 0 ? "+" : "") + std::to_string(value);
        const std::string prefixText = "Player " + std::to_string(playerNumber) + "  Resources: ";
        const sf::Text prefix(font, prefixText, 16);
        const sf::Text resourceValue(font, std::to_string(displayedResources), 16);
        const sf::Text floatingValue(font, effectText, 20);
        const game_data::PlayerSnapshot& player =
            gameSnapshot.players[static_cast<std::size_t>(playerNumber - 1)];
        const std::string readoutText = elideToWidth(
            font,
            prefixText + std::to_string(displayedResources) +
                "  Control: " + std::to_string(player.controlledSquares),
            16,
            GamePlayerReadoutWidth);
        const sf::Text readout(font, readoutText, 16);
        const sf::FloatRect readoutBounds = readout.getLocalBounds();
        const float readoutX = playerNumber == 1
            ? BoardOriginX
            : BoardOriginX + BoardBottomWidth -
                (readoutBounds.position.x + readoutBounds.size.x);
        const float resourceCenterX = readoutX + prefix.getLocalBounds().size.x +
            resourceValue.getLocalBounds().size.x * 0.5f;
        const float x = resourceCenterX - floatingValue.getLocalBounds().size.x * 0.5f;
        floatingNumberEffects.push_back({
            0,
            0,
            {x, GameLabelY - 22.0f},
            false,
            effectText,
            value > 0 ? sf::Color(120, 235, 145) : sf::Color(245, 115, 105),
            animationTime,
            1.15f});
    };

    auto randomFidgetDelay = [&]() {
        std::uniform_real_distribution<float> distribution(
            FidgetDelayMinimumSeconds,
            FidgetDelayMaximumSeconds);
        return distribution(fidgetRandomEngine);
    };

    auto schedulePieceFidget = [&](const game_data::Piece& piece, float delayAfterSeconds = 0.0f) {
        if (piece.fidgetAnimPath.empty())
        {
            pieceFidgetAnimations.erase(piece.id);
            return;
        }
        pieceFidgetAnimations[piece.id] = {
            animationTime + delayAfterSeconds + randomFidgetDelay(),
            0.0f,
            false};
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

        staleAnimations.clear();
        for (auto& [pieceId, animation] : pieceFidgetAnimations)
        {
            if (!pieceByIdInSnapshot(nextSnapshot, pieceId))
            {
                staleAnimations.push_back(pieceId);
            }
        }
        for (int pieceId : staleAnimations)
        {
            pieceFidgetAnimations.erase(pieceId);
        }

        if (!haveSnapshot)
        {
            for (const game_data::Piece& piece : nextSnapshot.pieces)
            {
                schedulePieceFidget(piece);
            }
            return;
        }

        for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
        {
            const int index = playerNumber - 1;
            const int resourceDelta = nextSnapshot.players[static_cast<std::size_t>(index)].resources -
                gameSnapshot.players[static_cast<std::size_t>(index)].resources;
            addResourceNumber(
                playerNumber,
                resourceDelta,
                nextSnapshot.players[static_cast<std::size_t>(index)].resources);
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
                schedulePieceFidget(nextPiece);
                continue;
            }

            const bool pieceMoved = currentPiece->row != nextPiece.row || currentPiece->column != nextPiece.column;
            if (pieceMoved)
            {
                pieceMoveAnimations[nextPiece.id] = {
                    currentPiece->row,
                    currentPiece->column,
                    nextPiece.row,
                    nextPiece.column,
                    animationTime,
                    PieceMoveAnimationDurationSeconds};
                schedulePieceFidget(nextPiece, PieceMoveAnimationDurationSeconds);
                playedMoveSound = true;
            }
            else if (nextPiece.fidgetAnimPath.empty())
            {
                pieceFidgetAnimations.erase(nextPiece.id);
            }
            else
            {
                const auto fidgetAnimation = pieceFidgetAnimations.find(nextPiece.id);
                if (fidgetAnimation == pieceFidgetAnimations.end() ||
                    currentPiece->fidgetAnimPath != nextPiece.fidgetAnimPath ||
                    currentPiece->fidgetAnimFrames != nextPiece.fidgetAnimFrames)
                {
                    schedulePieceFidget(nextPiece);
                }
            }
            if (nextPiece.health < currentPiece->health)
            {
                addFloatingNumber(nextPiece.health - currentPiece->health, nextPiece.row, nextPiece.column);
                startPieceDamagedAnimation(nextPiece);
            }
        }

        for (const game_data::Piece& currentPiece : gameSnapshot.pieces)
        {
            if (pieceByIdInSnapshot(nextSnapshot, currentPiece.id) != nullptr)
            {
                continue;
            }
            // The final snapshot omits destroyed pieces. Showing their
            // remaining health still communicates the lethal damage at the
            // exact square where the piece was hit.
            addFloatingNumber(-currentPiece.health, currentPiece.row, currentPiece.column);
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
                    nextSnapshot.relentlessPieceId == currentPiece.id ||
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
                    if (!action.legal || !action.attacks ||
                        (action.targetId != currentTarget.id &&
                         std::find(action.targetIds.begin(), action.targetIds.end(), currentTarget.id) == action.targetIds.end()))
                    {
                        continue;
                    }

                    const game_data::Piece* nextTarget = pieceByIdInSnapshot(nextSnapshot, currentTarget.id);
                    const bool targetChanged = nextTarget == nullptr ||
                        nextTarget->health < currentTarget.health ||
                        nextTarget->disabledTurns != currentTarget.disabledTurns ||
                        nextTarget->owner != currentTarget.owner ||
                        nextTarget->controlTurnsRemaining != currentTarget.controlTurnsRemaining;
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

    auto pieceCanTakeTurnAction = [&](const game_data::Piece& piece, int playerNumber) {
        if (!haveSnapshot)
        {
            return false;
        }
        if (gameSnapshot.relentlessPieceId != 0)
        {
            return piece.id == gameSnapshot.relentlessPieceId &&
                (sandboxMode || (piece.owner == playerNumber && !piece.hasActed));
        }
        if (gameSnapshot.commandingPieceId != 0)
        {
            const game_data::Piece* commander = gamePieceById(gameSnapshot.commandingPieceId);
            return commander != nullptr && game_data::pieceCanReceiveCommand(*commander, piece);
        }
        return sandboxMode ||
            (piece.owner == playerNumber && !piece.hasActed);
    };

    auto pieceCanTakeGameAction = [&](const game_data::Piece& piece) {
        return pieceCanTakeTurnAction(piece, gameSnapshot.yourPlayer);
    };

    auto updatePieceFidgetAnimations = [&]() {
        for (auto animation = pieceFidgetAnimations.begin(); animation != pieceFidgetAnimations.end();)
        {
            const game_data::Piece* piece = gamePieceById(animation->first);
            if (!piece || piece->fidgetAnimPath.empty())
            {
                animation = pieceFidgetAnimations.erase(animation);
                continue;
            }

            const auto moveAnimation = pieceMoveAnimations.find(piece->id);
            const bool isMoving = moveAnimation != pieceMoveAnimations.end() &&
                animationTime < moveAnimation->second.startTime + moveAnimation->second.duration;
            if (isMoving)
            {
                animation->second.playing = false;
                ++animation;
                continue;
            }

            if (animation->second.playing)
            {
                if (animationTime >= animation->second.startTime + FidgetAnimationDurationSeconds)
                {
                    animation->second.playing = false;
                    animation->second.nextStartTime = animationTime + randomFidgetDelay();
                }
            }
            else if (animationTime >= animation->second.nextStartTime)
            {
                animation->second.playing = true;
                animation->second.startTime = animationTime;
            }
            ++animation;
        }
    };

    auto commitSandboxSnapshot = [&](game_data::Snapshot nextSnapshot) {
        recomputeSandboxControl(nextSnapshot);
        refreshSandboxPlayerSnapshots(nextSnapshot);
        updatePieceMoveAnimations(nextSnapshot);
        gameSnapshot = std::move(nextSnapshot);
        gameSnapshotReceivedAt = std::chrono::steady_clock::now();
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
        abilityButton.setPosition({GameActionButtonX, GameSandboxAbilityButtonY});
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
        clockWarningTracker.reset();
        displayedClockWarning.reset();
        pieceMoveAnimations.clear();
        pieceAttackAnimations.clear();
        pieceDamagedAnimations.clear();
        floatingNumberEffects.clear();
        pieceFidgetAnimations.clear();
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
        abilityButton.setPosition({GameActionButtonX, GameSandboxAbilityButtonY});
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
        clockWarningTracker.reset();
        displayedClockWarning.reset();
        pieceMoveAnimations.clear();
        pieceAttackAnimations.clear();
        pieceDamagedAnimations.clear();
        pieceFidgetAnimations.clear();
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
        if (!loggedInIsAdmin)
        {
            setMessage(messageText, "Admin access required", sf::Color::Red);
            return;
        }
        currentState = GameState::SandboxLoading;
        title.setString("Sandbox");
        centerText(title, 400.0f);
        clearFocus();
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
        return game_data::pieceTokenPathForState(piece);
    };

    auto pieceWalkAnimPath = [](const game_data::Piece& piece) -> const std::string& {
        return piece.walkAnimPath;
    };

    auto cardTokenPath = [](const game_data::GameCard& card) -> const std::string& {
        return card.tokenPath;
    };

    auto cardWalkAnimPath = [](const game_data::GameCard& card) -> const std::string& {
        return card.walkAnimPath;
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
        bool flipX,
        int walkAnimFrames,
        int idleAnimFrames,
        sf::Vector2f anchor,
        float scale,
        sf::Color tint,
        int walkFrame,
        int idleFrame,
        int footprintWidth = 1,
        int footprintHeight = 1) {
        const sf::FloatRect target = pieceTargetRect(
            anchor, scale, true, footprintWidth, footprintHeight);
        if (sf::Texture* base = textures.load(basePath))
        {
            drawContainSprite(window, *base, target, tint);
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
        const std::string& tokenPath = cardTokenPath(card);
        const std::string& walkPath = cardWalkAnimPath(card);

        bool drewPiece = false;
        drewPiece = drawPieceVisual(
            tokenPath,
            walkPath,
            "",
            cardBasePath(card, owner),
            owner == 2,
            card.walkAnimFrames,
            1,
            anchor,
            scale,
            tint,
            -1,
            -1,
            card.width,
            card.height);
        if (!drewPiece)
        {
            if (sf::Texture* art = cardArtTexture(card.imagePath))
            {
                drawContainSprite(window, *art, pieceTargetRect(
                    anchor, scale, false, card.width, card.height), tint);
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
            : card.type + "  " + std::to_string(game_data::cardInt(card, "cost", 0)) + " Resources";
        drawText(window, font, cardRarityLabel(card) + "  " + typeLine, 16, {position.x + 18.0f, position.y + 210.0f}, cardRarityColor(card), size.x - 36.0f);

        std::string statLine;
        if (card.type == "Unit" || game_data::isHeroCard(card))
        {
            statLine = "HP " + std::to_string(game_data::cardInt(card, "health", 0)) +
                "  Actions " + std::to_string(card.actions.size());
        }
        else
        {
            statLine = card.type + "  " + game_data::cardStr(card, "effect", "effect") +
                " " + std::to_string(game_data::cardInt(card, "power", 0));
        }
        drawText(window, font, statLine, 15, {position.x + 18.0f, position.y + 236.0f}, sf::Color(224, 210, 176), size.x - 36.0f);
        drawText(
            window,
            font,
            starterDeckMode
                ? "Deck limit " + std::to_string(game_data::cardDeckLimit(card))
                : "Owned " + std::to_string(ownedCopies(card.title)) +
                    "  Deck limit " + std::to_string(game_data::cardDeckLimit(card)),
            15,
            {position.x + 18.0f, position.y + 264.0f},
            sf::Color(248, 214, 112),
            size.x - 36.0f);
    };

    auto detailRowsHeight = [&](const DetailRows& details) {
        float height = 0.0f;
        for (const DetailRow& row : details)
        {
            if (row.action)
            {
                height += 54.0f;
                continue;
            }
            height += static_cast<float>(
                wrapText(font, row.text, 14, PiecePopupTextWidth - PiecePopupScrollTextXInset * 2.0f).size()) * 18.0f;
            height += 8.0f;
        }
        return height + PiecePopupScrollTextYInset;
    };

    auto detailRowsMaxScroll = [&](const DetailRows& details) {
        return std::max(0.0f, detailRowsHeight(details) - PiecePopupScrollHeight);
    };

    auto drawDetailRows = [&](const DetailRows& details, float y) {
        const float left = PiecePopupTextX + PiecePopupScrollTextXInset;
        const float width = PiecePopupTextWidth - PiecePopupScrollTextXInset * 2.0f;
        auto measuredTextWidth = [&](const std::string& value, unsigned int size) {
            sf::Text measuring(font, value, size);
            return measuring.getLocalBounds().size.x;
        };
        auto drawInlineIcon = [&](const std::string& path, float x, float iconY) {
            if (sf::Texture* icon = textures.load(path))
            {
                drawContainSprite(window, *icon, {{x, iconY}, {18.0f, 18.0f}});
            }
        };

        for (const DetailRow& row : details)
        {
            if (!row.action)
            {
                y = drawWrappedText(window, font, row.text, 14, {left, y}, row.color, width);
                y += 8.0f;
                continue;
            }

            const ActionDescription& action = *row.action;
            drawText(window, font, action.name, 15, {left, y}, sf::Color(248, 239, 216), width);
            y += 21.0f;

            float x = left;
            drawText(window, font, action.type, 13, {x, y + 1.0f}, row.color, width);
            x += measuredTextWidth(action.type, 13) + 8.0f;

            drawInlineIcon(action.moveIconPath, x, y);
            x += 21.0f;
            drawText(window, font, action.range, 13, {x, y + 1.0f}, row.color);
            x += measuredTextWidth(action.range, 13) + 5.0f;

            auto drawAmount = [&](const std::string& iconPath, int amount) {
                if (amount <= 0)
                {
                    return;
                }
                x += 5.0f;
                drawInlineIcon(iconPath, x, y);
                x += 21.0f;
                const std::string value = std::to_string(amount);
                drawText(window, font, value, 13, {x, y + 1.0f}, row.color);
                x += measuredTextWidth(value, 13);
            };
            drawAmount("ui/damage.png", action.damage);
            drawAmount("ui/heal.png", action.heal);
            drawAmount("ui/stun.png", action.stun);
            drawAmount("ui/cooldown.png", action.cooldown);
            drawAmount("ui/under-control.png", action.control);
            y += 33.0f;
        }
        return y;
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

    #include "screens/starter_decks_screen.inl"

    #include "screens/admin_users_screen.inl"

    #include "screens/admin_tools_screen.inl"

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

    auto playerReadoutAtPixel = [&](sf::Vector2f point) -> std::optional<int> {
        if (isInsideRect(
                point,
                BoardOriginX,
                GameLabelY - 4.0f,
                GamePlayerReadoutWidth,
                26.0f))
        {
            return 1;
        }
        if (isInsideRect(
                point,
                BoardOriginX + BoardBottomWidth - GamePlayerReadoutWidth,
                GameLabelY - 4.0f,
                GamePlayerReadoutWidth,
                26.0f))
        {
            return 2;
        }
        return std::nullopt;
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

    auto destroySandboxPiece = [&](game_data::Snapshot& snapshot, int pieceId) {
        const game_data::Piece* piece = pieceByIdInSnapshot(snapshot, pieceId);
        const game_data::GameCard* rebirthCard = nullptr;
        if (piece != nullptr && !piece->rebirthTitle.empty())
        {
            const auto found = std::find_if(
                snapshot.hand.begin(),
                snapshot.hand.end(),
                [&](const game_data::GameCard& candidate) {
                    return candidate.title == piece->rebirthTitle;
                });
            if (found != snapshot.hand.end())
            {
                rebirthCard = &*found;
            }
        }
        return destroyPieceInSnapshot(
            snapshot,
            nextSandboxPieceId,
            pieceId,
            rebirthCard);
    };

    auto sandboxPlayCard = [&](int handIndex, int row, int column) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing ||
            handIndex < 0 || handIndex >= static_cast<int>(gameSnapshot.hand.size()))
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        if (next.relentlessPieceId != 0)
        {
            next.status = "The Relentless piece must act again or you must advance the turn.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
        const game_data::GameCard card = next.hand[static_cast<std::size_t>(handIndex)];
        const int actingPlayer = sandboxPlacementPlayer;
        if (card.type == "Unit" || card.type == "Hero")
        {
            bool footprintAvailable = row >= 0 && column >= 0 &&
                row + card.height <= game_data::BoardSize &&
                column + card.width <= game_data::BoardSize;
            for (int r = row; footprintAvailable && r < row + card.height; ++r)
                for (int c = column; footprintAvailable && c < column + card.width; ++c)
                    footprintAvailable =
                        next.control[static_cast<std::size_t>(game_data::squareIndex(r, c))] == actingPlayer &&
                        pieceAtInSnapshot(next, r, c) == nullptr;
            if (!footprintAvailable)
            {
                next.status = "Every square under a sandbox piece must be empty and controlled by the selected player.";
                commitSandboxSnapshot(std::move(next));
                return;
            }

            spawnSandboxPiece(next, nextSandboxPieceId, actingPlayer, card, row, column, card.type == "Hero");
            next.status = "Sandbox played " + card.title + " for Player " + std::to_string(actingPlayer) + ".";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        if (card.type == "Enchantment")
        {
            game_data::Enchantment enchantment;
            enchantment.id = 1;
            for (const game_data::Enchantment& existing : next.enchantments)
            {
                enchantment.id = std::max(enchantment.id, existing.id + 1);
            }
            enchantment.owner = actingPlayer;
            enchantment.title = card.title;
            enchantment.imagePath = card.imagePath;
            enchantment.effect = card.effect;
            enchantment.power = std::max(0, card.power);

            if (card.target == "player" && row == -1 &&
                (column == 1 || column == 2) && card.effect == "resourceDrain")
            {
                enchantment.target = static_cast<std::uint8_t>(game_data::EnchantmentTarget::Player);
                enchantment.targetPlayer = column;
            }
            else if (card.target == "square" && game_data::inBounds(row, column) &&
                     next.holes[static_cast<std::size_t>(game_data::squareIndex(row, column))] == 0 &&
                     card.effect == "resources")
            {
                enchantment.target = static_cast<std::uint8_t>(game_data::EnchantmentTarget::Square);
                enchantment.targetRow = row;
                enchantment.targetColumn = column;
            }
            else if (card.target == "piece" && card.effect == "damage")
            {
                const game_data::Piece* targetPiece = game_data::inBounds(row, column)
                    ? pieceAtInSnapshot(next, row, column)
                    : nullptr;
                if (!targetPiece)
                {
                    next.status = "That piece enchantment needs a piece target.";
                    commitSandboxSnapshot(std::move(next));
                    return;
                }
                enchantment.target = static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece);
                enchantment.targetPieceId = targetPiece->id;
                enchantment.targetRow = targetPiece->row;
                enchantment.targetColumn = targetPiece->column;
            }
            else
            {
                next.status = "That enchantment needs a valid player, square, or piece target.";
                commitSandboxSnapshot(std::move(next));
                return;
            }

            next.enchantments.push_back(std::move(enchantment));
            next.status = "Sandbox attached " + card.title + ".";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        if (game_data::isResourcesEffect(card))
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
            const int targetId = target->id;
            const std::vector<game_data::DamageAssignment> damageAssignments =
                game_data::applyDamageWithBodyguards(
                    next.pieces, targetId, card.power, 0, sandboxDamageRandomEngine);
            for (const game_data::DamageAssignment& assignment : damageAssignments)
            {
                game_data::Piece* damagedPiece =
                    pieceByIdInSnapshotMutable(next, assignment.pieceId);
                if (damagedPiece && damagedPiece->health <= 0)
                {
                    destroySandboxPiece(next, damagedPiece->id);
                }
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
        if (next.relentlessPieceId != 0 && piece->id != next.relentlessPieceId)
        {
            next.status = "Only the Relentless piece may take the immediate action.";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        const game_data::Piece* commander = next.commandingPieceId != 0
            ? pieceByIdInSnapshot(next, next.commandingPieceId)
            : nullptr;
        const bool commandedAction = commander != nullptr;
        if (commandedAction && !game_data::pieceCanReceiveCommand(*commander, *piece))
        {
            next.status = "Command must activate a ready adjacent friendly piece.";
            commitSandboxSnapshot(std::move(next));
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
        const int attackerOwner = piece->owner;
        const int originRow = piece->row;
        const int originColumn = piece->column;
        const std::string attackerName = piece->name;
        std::vector<std::string> damagedTargetNames;
        std::vector<std::string> healedTargetNames;
        std::vector<std::string> controlledTargetNames;
        bool anyTargetDestroyed = false;
        bool anyTargetReborn = false;
        bool anyTargetWasHidden = false;
        int pushedSquares = 0;
        int pushCollisionDamage = 0;
        const int attackDamage = action.damage +
            game_data::pieceEnchantmentDamageBonus(next.enchantments, attackerId);

        const std::string commanderName = commandedAction ? commander->name : std::string();
        if (action.attacks)
        {
            const std::vector<int> targetIds = action.targetIds.empty()
                ? std::vector<int>{action.targetId}
                : action.targetIds;
            for (int targetId : targetIds)
            {
                game_data::Piece* target = pieceByIdInSnapshotMutable(next, targetId);
                if (!target) continue;
                const std::string targetName =
                    (target->hidden ? "a hidden " : "") + target->name;
                anyTargetWasHidden = anyTargetWasHidden ||
                    (target->hidden && target->owner != attackerOwner);
                startPieceAttackAnimation(attackerId, target->row, target->column);
                if (target->owner == attackerOwner)
                {
                    healedTargetNames.push_back(targetName);
                    game_data::applyActionHealing(*target, action.heal, action.statusTurns);
                }
                else
                {
                    damagedTargetNames.push_back(targetName);
                    const std::vector<game_data::DamageAssignment> damageAssignments =
                        game_data::applyDamageWithBodyguards(
                            next.pieces,
                            targetId,
                            attackDamage,
                            action.statusTurns,
                            sandboxDamageRandomEngine);
                    for (const game_data::DamageAssignment& assignment : damageAssignments)
                    {
                        game_data::Piece* damagedPiece =
                            pieceByIdInSnapshotMutable(next, assignment.pieceId);
                        if (damagedPiece && damagedPiece->health <= 0)
                        {
                            const bool reborn = destroySandboxPiece(next, damagedPiece->id);
                            anyTargetReborn = anyTargetReborn || reborn;
                            anyTargetDestroyed = anyTargetDestroyed || !reborn;
                        }
                    }
                    const game_data::PushResult pushResult = game_data::applyActionPush(
                        next.pieces,
                        targetId,
                        action.stagingRow,
                        action.stagingColumn,
                        action.push);
                    pushedSquares += pushResult.movedSquares;
                    pushCollisionDamage += pushResult.preventedSquares;
                    if (game_data::Piece* pushedTarget =
                            pieceByIdInSnapshotMutable(next, targetId);
                        pushedTarget && pushedTarget->health <= 0)
                    {
                        const bool reborn = destroySandboxPiece(next, pushedTarget->id);
                        anyTargetReborn = anyTargetReborn || reborn;
                        anyTargetDestroyed = anyTargetDestroyed || !reborn;
                    }
                    if (action.control > 0)
                    {
                        if (game_data::Piece* controllableTarget =
                                pieceByIdInSnapshotMutable(next, targetId);
                            controllableTarget != nullptr && controllableTarget->health > 0)
                        {
                            game_data::applyPieceControl(*controllableTarget, attackerOwner, action.control);
                            if (controllableTarget->owner == attackerOwner)
                            {
                                controlledTargetNames.push_back(targetName);
                            }
                        }
                    }
                }
            }
            if (damagedTargetNames.empty() && healedTargetNames.empty()) return;
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
            if (!anyTargetReborn &&
                game_data::pieceFootprintFree(next.pieces, *acting, destinationRow, destinationColumn))
            {
                acting->row = destinationRow;
                acting->column = destinationColumn;
            }
            else
            {
                if (game_data::pieceFootprintFree(
                        next.pieces,
                        *acting,
                        action.stagingRow,
                        action.stagingColumn))
                {
                    acting->row = action.stagingRow;
                    acting->column = action.stagingColumn;
                }
            }
        }
        acting->disabledTurns = std::max(acting->disabledTurns, action.cooldownTurns);
        game_data::setPieceActionState(*acting, action.nextState);
        acting->hasActed = false;

        if (commandedAction)
        {
            next.commandingPieceId = 0;
            next.status = commanderName + " commanded " + attackerName + " to act.";
        }
        else if (action.attacks)
        {
            next.status.clear();
            const int effectiveDisabledTurns = damagedTargetNames.empty()
                ? std::max(0, action.statusTurns)
                : game_data::disabledTurnsForDamage(attackDamage, action.statusTurns);
            const auto joinTargets = [](const std::vector<std::string>& names) {
                std::string joined;
                for (std::size_t i = 0; i < names.size(); ++i)
                {
                    if (i > 0) joined += i + 1 == names.size() ? " and " : ", ";
                    joined += names[i];
                }
                return joined;
            };
            if (!damagedTargetNames.empty())
            {
                next.status = attackerName + " hit " + joinTargets(damagedTargetNames) +
                    " for " + std::to_string(attackDamage) + " each";
            }
            if (!healedTargetNames.empty())
            {
                const std::string healed = "healed " + joinTargets(healedTargetNames) +
                    " for " + std::to_string(action.heal) + " each";
                next.status += next.status.empty()
                    ? attackerName + " " + healed
                    : " and " + healed;
            }
            if (pushedSquares > 0)
                next.status += " and pushed targets " + std::to_string(pushedSquares) +
                    " square(s)";
            if (pushCollisionDamage > 0)
                next.status += " and dealt " + std::to_string(pushCollisionDamage) +
                    " extra collision damage";
            if (effectiveDisabledTurns > 0)
            {
                next.status += " and disabled surviving targets for " +
                    std::to_string(effectiveDisabledTurns) + " turn(s)";
            }
            if (!controlledTargetNames.empty())
            {
                next.status += " and controlled " + joinTargets(controlledTargetNames) +
                    " for " + std::to_string(action.control) + " turn(s)";
            }
            next.status += anyTargetDestroyed ? "; at least one was destroyed." : ".";
            if (anyTargetReborn)
            {
                next.status += " Rebirth returned a piece to the board!";
            }
            if (anyTargetWasHidden)
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
        if (anyTargetDestroyed && game_data::hasKeyword(acting->keywords, "relentless"))
        {
            next.relentlessPieceId = attackerId;
            next.status += " Relentless: it may act again immediately.";
        }
        else if (next.relentlessPieceId == attackerId)
        {
            next.relentlessPieceId = 0;
        }
        if (acting)
        {
            updateStoryAfterMove(next, *acting);
        }
        const bool leavesTrail = acting &&
            (acting->row != originRow || acting->column != originColumn) &&
            game_data::pieceHasTrailAbility(*acting);
        const std::string trailSummonTitle = leavesTrail ? acting->summonTitle : std::string();
        if (leavesTrail)
        {
            const auto found = std::find_if(
                next.hand.begin(),
                next.hand.end(),
                [&](const game_data::GameCard& card) {
                    return card.title == trailSummonTitle && card.type == "Unit";
                });
            if (found != next.hand.end() &&
                game_data::cardFootprintFree(next.pieces, *found, originRow, originColumn))
            {
                spawnSandboxPiece(
                    next, nextSandboxPieceId, attackerOwner, *found, originRow, originColumn, false);
            }
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
        if (next.relentlessPieceId != 0 && piece->id != next.relentlessPieceId)
        {
            next.status = "Only the Relentless piece may take the immediate action.";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        const game_data::Piece* commander = next.commandingPieceId != 0
            ? pieceByIdInSnapshot(next, next.commandingPieceId)
            : nullptr;
        const bool commandedAction = commander != nullptr;
        if (commandedAction && !game_data::pieceCanReceiveCommand(*commander, *piece))
        {
            next.status = "Command must activate a ready adjacent friendly piece.";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        const std::string abilityLabel = game_data::pieceAbilityLabel(*piece);
        const std::string pieceName = piece->name;
        const std::string commanderName = commandedAction ? commander->name : std::string();
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
                stateCount = std::max(stateCount, game_data::actionNextState(action) + 1);
            }
            game_data::setPieceActionState(
                *piece, (piece->actionState + 1) % stateCount);
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
        else if (piece->ability == "command")
        {
            piece->hasActed = true;
            next.relentlessPieceId = 0;
            next.commandingPieceId = piece->id;
            next.status = pieceName + " used Command. Activate one adjacent friendly piece.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
        else
        {
            return;
        }

        if (game_data::Piece* actingPiece = pieceByIdInSnapshotMutable(next, actingPieceId))
        {
            actingPiece->hasActed = false;
        }
        if (next.relentlessPieceId == actingPieceId)
        {
            next.relentlessPieceId = 0;
        }
        if (commandedAction)
        {
            next.commandingPieceId = 0;
            next.status = commanderName + " commanded " + pieceName + " to use " + abilityLabel + ".";
        }
        else
        {
            next.status = pieceName + " used " + abilityLabel + ".";
        }
        commitSandboxSnapshot(std::move(next));
    };

    auto sandboxEndTurn = [&]() {
        if (!sandboxMode || !haveSnapshot)
        {
            return;
        }
        game_data::Snapshot next = gameSnapshot;
        next.commandingPieceId = 0;
        next.relentlessPieceId = 0;
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
        game_data::updatePieceControlAtTurnStart(next.pieces, startingPlayer);
        for (game_data::Piece& piece : next.pieces)
        {
            if (piece.owner == startingPlayer)
            {
                game_data::beginPieceTurn(piece);
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
        if (gameSnapshot.relentlessPieceId != 0)
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
        if (gameSnapshot.relentlessPieceId != 0)
        {
            return false;
        }

        const game_data::GameCard& card = gameSnapshot.hand[handIndex];
        selectedPieceId.reset();
        if (card.type == "Spell" && game_data::isResourcesEffect(card) &&
            (sandboxMode ||
             game_data::heroTraitsAllowCard(
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
        gameDragPieceRowOffset = 0;
        gameDragPieceColumnOffset = 0;
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
            if (gameSnapshot.relentlessPieceId != 0)
            {
                return;
            }
            gameDragKind = GameDragKind::HandCard;
            draggingHandIndex = *handIndex;
            gameDragStartPos = clickPos;
            gameDragCurrentPos = clickPos;
            return;
        }

        if (const game_data::Piece* piece = gamePieceAtPixel(clickPos);
            piece && pieceCanTakeGameAction(*piece))
        {
            gameDragKind = GameDragKind::Piece;
            draggingPieceId = piece->id;
            gameDragStartPos = clickPos;
            gameDragCurrentPos = clickPos;
            if (const auto grabbedSquare = squareAtPixel(clickPos))
            {
                gameDragPieceRowOffset = grabbedSquare->first - piece->row;
                gameDragPieceColumnOffset = grabbedSquare->second - piece->column;
            }
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

        if (gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
            *draggingHandIndex < gameSnapshot.hand.size())
        {
            const game_data::GameCard& card = gameSnapshot.hand[*draggingHandIndex];
            const std::optional<int> targetPlayer = playerReadoutAtPixel(releasePos);
            if (card.type == "Enchantment" && card.target == "player" && targetPlayer)
            {
                sendPlayCard(static_cast<int>(*draggingHandIndex), -1, *targetPlayer);
                selectedHandIndex.reset();
                selectedPieceId.reset();
                pendingHandClickIndex.reset();
                resetGameDrag();
                return true;
            }
        }

        const std::optional<std::pair<int, int>> square = squareAtPixel(releasePos);
        if (!square)
        {
            resetGameDrag();
            return true;
        }

        int row = square->first;
        int column = square->second;
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
            row -= gameDragPieceRowOffset;
            column -= gameDragPieceColumnOffset;
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
        sf::Socket::Status receiveStatus = activeGameSocket->receive(packet);
        while (receiveStatus == sf::Socket::Status::Done)
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
                    gameSnapshotReceivedAt = std::chrono::steady_clock::now();
                    haveSnapshot = true;
                    if (gameSnapshot.relentlessPieceId != 0 &&
                        gameSnapshot.activePlayer == gameSnapshot.yourPlayer)
                    {
                        selectedPieceId = gameSnapshot.relentlessPieceId;
                        selectedHandIndex.reset();
                    }
                    clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
                    if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                            game_data::Phase::GameOver &&
                        !gameResultReceived)
                    {
                        gameRewardText = conquestBattleMode
                            ? "Battle resolved. Return to the Conquest map."
                            : "Finalizing match rewards...";
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
            receiveStatus = activeGameSocket->receive(packet);
        }

        if (conquestBattleMode &&
            (receiveStatus == sf::Socket::Status::Disconnected ||
             receiveStatus == sf::Socket::Status::Error))
        {
            activeGameSocket->disconnect();
            activeGameSocket.reset();
            conquestBattleMode = false;
            leaveGameButton.setLabel("Leave");
            currentState = GameState::Conquest;
            conquestScreen.setStatus("Battle connection closed; map state refreshed.", true);
            conquestScreen.refresh();
        }
    };

    auto updateClockWarnings = [&]() {
        if (!haveSnapshot || sandboxMode || storyMode || !gameSnapshot.timersEnabled ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const std::int64_t snapshotAgeMs = gameSnapshotReceivedAt ==
                std::chrono::steady_clock::time_point{}
            ? 0
            : std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - gameSnapshotReceivedAt).count();

        std::optional<ClockWarning> warning;
        for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
        {
            const game_data::PlayerSnapshot& player =
                gameSnapshot.players[static_cast<std::size_t>(playerNumber - 1)];
            const bool ticking = playerNumber == gameSnapshot.activePlayer;
            const std::int64_t liveRemainingMs = std::max<std::int64_t>(
                0,
                player.clockRemainingMs - (ticking ? snapshotAgeMs : 0));
            const std::optional<ClockWarning> crossed =
                clockWarningTracker.observe(playerNumber, liveRemainingMs);
            if (crossed && (!warning || crossed->playerNumber == gameSnapshot.yourPlayer))
            {
                warning = crossed;
            }
        }

        if (warning)
        {
            displayedClockWarning = DisplayedClockWarning{
                warning->playerNumber,
                warning->thresholdMs,
                now + std::chrono::seconds(4)};
            audioSystem.play(AudioCue::ClockWarning);
        }
    };

    auto leaveGame = [&]() {
        const bool wasSandbox = sandboxMode;
        const bool wasConquestBattle = conquestBattleMode;
        if (activeGameSocket)
        {
            if (wasConquestBattle)
            {
                leaveConquestBattle(*activeGameSocket);
            }
            else
            {
                sendDisconnect(*activeGameSocket);
            }
            activeGameSocket.reset();
        }
        conquestBattleMode = false;
        leaveGameButton.setLabel("Leave");
        haveSnapshot = false;
        gameSnapshot = {};
        clockWarningTracker.reset();
        displayedClockWarning.reset();
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
        pieceFidgetAnimations.clear();
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
        if (wasConquestBattle)
        {
            currentState = GameState::Conquest;
            conquestScreen.refresh();
        }
        else if (wasSandbox && loggedInIsAdmin)
        {
            loadAdminToolsScreen();
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

        if (phase == game_data::Phase::Playing &&
            (sandboxMode || gameSnapshot.activePlayer == me) &&
            selectedHandIndex && *selectedHandIndex < gameSnapshot.hand.size())
        {
            const game_data::GameCard& selectedCard = gameSnapshot.hand[*selectedHandIndex];
            const std::optional<int> targetPlayer = playerReadoutAtPixel(clickPos);
            if (selectedCard.type == "Enchantment" && selectedCard.target == "player" && targetPlayer)
            {
                sendPlayCard(static_cast<int>(*selectedHandIndex), -1, *targetPlayer);
                selectedHandIndex.reset();
                return;
            }
        }

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
        if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
        {
            if (sandboxMode || gameSnapshot.activePlayer == me)
            {
                handleHandCardClick(*handIndex);
            }
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

        // During the other player's turn, board clicks are previews only.
        // Selecting a piece belonging to the inactive player must never send
        // an action, even if that piece belongs to this client.
        if (!sandboxMode && gameSnapshot.activePlayer != me)
        {
            selectedHandIndex.reset();
            selectedPieceId = clicked
                ? std::optional<int>(clicked->id)
                : std::nullopt;
            return;
        }

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
                if (!sandboxMode && selected->owner != gameSnapshot.activePlayer)
                {
                    selectedPieceId = clicked && clicked->owner != gameSnapshot.activePlayer
                        ? std::optional<int>(clicked->id)
                        : (clicked && pieceCanTakeGameAction(*clicked)
                            ? std::optional<int>(clicked->id)
                            : std::nullopt);
                    return;
                }
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
                    selectedPieceId = pieceCanTakeGameAction(*clicked)
                        ? std::optional<int>(clicked->id)
                        : std::nullopt;
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

        if (clicked && !sandboxMode && clicked->owner != gameSnapshot.activePlayer)
        {
            selectedPieceId = clicked->id;
        }
        else if (clicked && pieceCanTakeGameAction(*clicked))
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

    // ---- offline screenshot harness ---------------------------------------
    // Populates the account state the services would normally supply, then
    // walks the requested screens writing one PNG each. See
    // client_ui_capture.hpp.
    std::size_t captureIndex = 0;
    int captureFramesOnScreen = 0;
    bool captureScreenReady = false;
    // Lets a capture screen pin the pointer somewhere, so hover treatments are
    // reviewable instead of only existing while a human holds the mouse still.
    std::optional<sf::Vector2f> captureHoverPoint;

    auto seedCaptureState = [&]() {
        loggedInUsername = "Thistlewisp";
        activeAccessToken = "ui-capture";
        loggedInIsAdmin = true;
        playerCoins = 1240;
        playerRating = 1780;
        playerLeague = ranking::League::Gold;

        allCardLibrary = ui_capture::sampleCardLibrary();
        cardLibrary = allCardLibrary;
        filteredCardLibrary = allCardLibrary;
        adminCardLibrary = allCardLibrary;
        playerCollection = ui_capture::sampleCollection(allCardLibrary);
        playerDecks = ui_capture::sampleDecks(allCardLibrary);
        editingDeck = playerDecks.empty() ? deck_data::Deck{} : playerDecks.front();
        activeDeckOriginalName = editingDeck.name;

        starterDeckOffers.clear();
        for (std::size_t i = 0; i < starter_decks::Names.size(); ++i)
        {
            network::StarterDeckOffer offer;
            offer.name = starter_decks::Names[i];
            offer.cardCount = 30;
            offer.owned = i == 0;
            offer.price = i == 0 ? 0 : starter_decks::StarterDeckPrice;
            starterDeckOffers.push_back(offer);
        }
        selectedStarterDeckOffer = 0;

        adminUsers.clear();
        static constexpr const char* AdminSampleNames[] = {
            "Thistlewisp", "brackenmoor", "Fenwick", "gallowglass",
            "Mirefoot", "nettlejack", "Rushlight", "sootpetal"};
        for (std::size_t i = 0; i < std::size(AdminSampleNames); ++i)
        {
            network::AdminUserSummary user;
            user.username = AdminSampleNames[i];
            user.isAdmin = i == 0;
            user.gold = 120 + static_cast<int>(i) * 385;
            adminUsers.push_back(user);
        }
        adminUsersTotalCount = static_cast<std::uint32_t>(adminUsers.size());
        adminUsersPage = 0;
        selectedAdminUser = 0;
    };

    auto applyCaptureScreen = [&](const std::string& screen) {
        setMessage(messageText, "", sf::Color::Red);
        title.setString("Gloomthorn");
        centerText(title, 400.0f);
        captureHoverPoint.reset();
        usernameInput.setError(false);
        passwordInput.setError(false);
        confirmInput.setError(false);
        exitDesktopPopupVisible = false;
        deckUnsavedChangesPopupVisible = false;
        passwordChangedPopupVisible = false;
        addCardPopupVisible = false;
        giveStarterDeckPopupVisible = false;
        deckEditorMode = DeckEditorMode::DeckList;
        starterDeckMode = false;

        if (screen == "title-screen")
        {
            // The pre-sign-in screen had no capture key at all, so nobody had
            // ever reviewed it.
            currentState = GameState::Menu;
        }
        else if (screen == "login")
        {
            currentState = GameState::Login;
            usernameInput.setContent("Thistlewisp");
            passwordInput.setContent("marshlight");
            usernameInput.setActive(true);
            passwordInput.setActive(false);
        }
        else if (screen == "login-error")
        {
            currentState = GameState::Login;
            usernameInput.setContent("Thistlewisp");
            passwordInput.setContent("wrongpass");
            usernameInput.setActive(false);
            passwordInput.setActive(true);
            passwordInput.setError(true);
            setMessage(messageText, "That username and password do not match.", palette::DangerBright);
        }
        else if (screen == "create-account")
        {
            currentState = GameState::CreateAccount;
            usernameInput.setActive(true);
        }
        else if (screen == "create-account-invalid")
        {
            currentState = GameState::CreateAccount;
            usernameInput.setContent("Thistlewisp");
            passwordInput.setContent("marsh");
            confirmInput.setContent("marshlight");
            confirmInput.setActive(true);
            passwordInput.setError(true);
            confirmInput.setError(true);
            setMessage(messageText, "Passwords do not match.", palette::DangerBright);
        }
        else if (screen == "options" || screen == "options-audio" || screen == "options-account")
        {
            currentState = GameState::Options;
            optionsReturnState = GameState::Authenticated;
            activeOptionsTab = screen == "options-audio" ? OptionsTab::Audio
                : screen == "options-account"           ? OptionsTab::Account
                                                        : OptionsTab::Graphics;
            optionsTabs.setActive(static_cast<std::size_t>(activeOptionsTab));
            // Without this the graphics tab captures with empty value plates: the
            // labels are only refreshed when a human opens the screen.
            updateOptionsLabels();
        }
        else if (screen == "main-menu")
        {
            currentState = GameState::Authenticated;
        }
        else if (screen == "main-menu-hover")
        {
            currentState = GameState::Authenticated;
            // Centre of the primary Play plate.
            captureHoverPoint = sf::Vector2f{400.0f, 199.0f};
        }
        else if (screen == "main-menu-exit")
        {
            currentState = GameState::Authenticated;
            exitDesktopPopupVisible = true;
        }
        else if (screen == "deck-select")
        {
            currentState = GameState::DeckSelect;
        }
        else if (screen == "matchmaking")
        {
            currentState = GameState::Matchmaking;
            title.setString("Matchmaking");
            centerText(title, 400.0f);
            setMessage(messageText, "Searching for an opponent...", sf::Color(226, 196, 118));
        }
        else if (screen == "deck-editor")
        {
            currentState = GameState::DeckEditor;
        }
        else if (screen == "deck-editor-cards")
        {
            currentState = GameState::DeckEditor;
            deckEditorMode = DeckEditorMode::EditDeck;
        }
        else if (screen == "shop")
        {
            currentState = GameState::Shop;
        }
        else if (screen == "starter-decks")
        {
            currentState = GameState::StarterDecks;
        }
        else if (screen == "admin-users")
        {
            currentState = GameState::AdminUsers;
        }
        else if (screen == "admin-tools")
        {
            currentState = GameState::AdminTools;
        }
        else if (screen == "card-editor")
        {
            currentState = GameState::CardEditor;
        }
        else if (screen == "conquest")
        {
            currentState = GameState::Conquest;
        }
        else if (screen == "game")
        {
            beginStory();
        }
    };

    if (captureRequest)
    {
        seedCaptureState();
        applyCaptureScreen(captureRequest->screens.front());
        captureScreenReady = true;
    }
    else if (const std::optional<std::string> savedToken = loadRememberToken())
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
        if (captureHoverPoint)
        {
            // Capture runs have no real pointer, so a screen that wants to show a
            // hover treatment pins one here.
            mousePos = *captureHoverPoint;
        }

        if (currentState == GameState::Game)
        {
            pollGameSocket();
            updateClockWarnings();
            updatePieceFidgetAnimations();
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
                    title.setString("Gloomthorn");
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
                if (!result.hasStarterDeck && currentState == GameState::Authenticated)
                {
                    // The account has never taken its free starter deck, so the
                    // picker stands in for the menu until one is claimed.
                    loadStarterDecksScreen(true);
                }
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

        if (pendingAdminCardListLoad &&
            pendingAdminCardListLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            CardListResult result = pendingAdminCardListLoad->get();
            pendingAdminCardListLoad.reset();
            if (result.success)
            {
                adminCardLibrary = std::move(result.cards);
                adminCardLoadError.clear();
            }
            else
            {
                adminCardLoadError = result.message;
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

        if (pendingAdminUserCard &&
            pendingAdminUserCard->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AdminUserCardResult result = pendingAdminUserCard->get();
            pendingAdminUserCard.reset();
            if (!loggedInUsername.empty() && currentState == GameState::AdminUsers)
            {
                if (result.success)
                {
                    dismissAddCardPopup();
                }
                else if (addCardPopupVisible)
                {
                    adminCardInput.setActive(true);
                }
                setMessage(
                    messageText,
                    result.message,
                    result.success ? sf::Color(120, 220, 150) : sf::Color::Red);
            }
        }

        if (pendingAdminUserStarterDeck &&
            pendingAdminUserStarterDeck->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AdminUserStarterDeckResult result = pendingAdminUserStarterDeck->get();
            pendingAdminUserStarterDeck.reset();
            if (!loggedInUsername.empty() && currentState == GameState::AdminUsers)
            {
                if (result.success)
                {
                    giveStarterDeckPopupVisible = false;
                }
                setMessage(
                    messageText,
                    result.message,
                    result.success ? sf::Color(120, 220, 150) : sf::Color::Red);
            }
        }

        if (pendingStarterDeckOffers &&
            pendingStarterDeckOffers->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            StarterDeckOffersResult result = pendingStarterDeckOffers->get();
            pendingStarterDeckOffers.reset();
            if (!loggedInUsername.empty() && currentState == GameState::StarterDecks)
            {
                if (result.success)
                {
                    starterDeckOffers = std::move(result.offers);
                    playerCoins = result.coins;
                    if (!selectedStarterDeckOffer || *selectedStarterDeckOffer >= starterDeckOffers.size())
                    {
                        selectedStarterDeckOffer =
                            starterDeckOffers.empty() ? std::optional<std::size_t>() : std::optional<std::size_t>(0);
                    }
                    setMessage(messageText, result.message, sf::Color(120, 220, 150));
                }
                else
                {
                    setMessage(messageText, result.message, sf::Color::Red);
                }
            }
        }

        if (pendingStarterDeckClaim &&
            pendingStarterDeckClaim->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            StarterDeckClaimResult result = pendingStarterDeckClaim->get();
            pendingStarterDeckClaim.reset();
            if (!loggedInUsername.empty() && currentState == GameState::StarterDecks)
            {
                if (result.success)
                {
                    playerCoins = result.coins;
                    if (starterDeckPickRequired)
                    {
                        // The free pick is done; the menu takes over from here.
                        starterDeckPickRequired = false;
                        showAuthenticatedScreen();
                        setMessage(messageText, result.message, sf::Color(120, 220, 150));
                    }
                    else
                    {
                        pendingStarterDeckOffers =
                            std::async(std::launch::async, fetchStarterDeckOffers, activeAccessToken);
                        setMessage(messageText, result.message, sf::Color(120, 220, 150));
                    }
                }
                else
                {
                    setMessage(messageText, result.message, sf::Color::Red);
                }
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
                    if (loggedInIsAdmin)
                    {
                        loadAdminToolsScreen();
                    }
                    else
                    {
                        showAuthenticatedScreen();
                    }
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
                    playerDecks = std::move(result.decks);
                    editingDeck = {};
                    applyCollectionFilters();
                    activeDeckOriginalName.clear();
                    deckNameInput.clear();
                    selectedDeck = playerDecks.empty() ? std::optional<std::size_t>() : std::optional<std::size_t>(0);
                    selectedDeckCard.reset();
                    deckListOffset = 0;
                    deckCardListOffset = 0;
                    libraryOffset = 0;
                    setMessage(
                        messageText,
                        "Choose a starter deck to edit.",
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

        if (pendingConquestBattleJoin &&
            pendingConquestBattleJoin->wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready)
        {
            ConquestBattleJoinResult result = pendingConquestBattleJoin->get();
            pendingConquestBattleJoin.reset();
            const bool sameAuthenticatedSession =
                !pendingConquestBattleAccessToken.empty() &&
                pendingConquestBattleAccessToken == activeAccessToken &&
                pendingConquestBattleUsername == loggedInUsername &&
                pendingConquestBattleGeneration == conquestScreenGeneration &&
                pendingConquestBattleEventId != 0 &&
                pendingConquestBattleEventId == conquestScreen.activeEventId();
            pendingConquestBattleAccessToken.clear();
            pendingConquestBattleUsername.clear();
            if (result.success && currentState == GameState::Conquest && sameAuthenticatedSession)
            {
                showGameScreen(std::move(result.socket), true);
            }
            else
            {
                if (result.socket)
                {
                    leaveConquestBattle(*result.socket);
                }
                if (currentState == GameState::Conquest)
                {
                    conquestScreen.setStatus(result.message, false);
                    conquestScreen.refresh();
                }
            }
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                continue;
            }

            if (currentState == GameState::Conquest)
            {
                conquestScreen.handleEvent(*event, window);
                handleConquestScreenAction();
                continue;
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
                        starterDeckExitTab = 0;
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
                    if (authenticatedSettingsButtonClicked(clickPos))
                    {
                        playButtonClickSound();
                        showOptionsScreen(GameState::Authenticated);
                    }
                    else if (storyButton.isClicked(clickPos))
                    {
                        showStoryIntro();
                    }
                    else if (playButton.isClicked(clickPos))
                    {
                        showDeckSelect();
                    }
                    else if (conquestButton.isClicked(clickPos))
                    {
                        ++conquestScreenGeneration;
                        conquestScreen.open(
                            activeAccessToken, loggedInUsername, loggedInIsAdmin);
                        currentState = GameState::Conquest;
                        title.setString("");
                        clearFocus();
                    }
                    else if (deckEditorButton.isClicked(clickPos))
                    {
                        loadDeckEditor();
                    }
                    else if (shopButton.isClicked(clickPos))
                    {
                        loadShop();
                    }
                    else if (loggedInIsAdmin && adminUsersButton.isClicked(clickPos))
                    {
                        adminUsersPage = 0;
                        loadAdminUsersScreen();
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
                    else if (addCardPopupVisible)
                    {
                        if (confirmAddCardButton.isClicked(clickPos))
                        {
                            confirmAddCard();
                        }
                        else if (cancelAddCardButton.isClicked(clickPos))
                        {
                            dismissAddCardPopup();
                        }
                        else if (adminCardInput.contains(clickPos))
                        {
                            clearFocus();
                            adminCardInput.setActive(true);
                        }
                        else
                        {
                            const std::vector<std::string> cardTitles = visibleAdminCardTitles();
                            if (const std::optional<std::size_t> cardIndex = rowIndexAt(
                                    clickPos,
                                    220.0f,
                                    AdminCardRowY,
                                    360.0f,
                                    AdminCardRowHeight,
                                    VisibleAdminCardRows,
                                    0,
                                    cardTitles.size()))
                            {
                                adminCardInput.setContent(cardTitles[*cardIndex]);
                                adminCardInput.setActive(true);
                            }
                        }
                    }
                    else if (giveStarterDeckPopupVisible)
                    {
                        if (confirmGiveStarterDeckButton.isClicked(clickPos))
                        {
                            confirmGiveStarterDeck();
                        }
                        else if (cancelGiveStarterDeckButton.isClicked(clickPos))
                        {
                            dismissGiveStarterDeckPopup();
                        }
                        else if (const std::optional<std::size_t> deckIndex = rowIndexAt(
                                     clickPos,
                                     220.0f,
                                     AdminStarterDeckRowY,
                                     360.0f,
                                     AdminStarterDeckRowHeight,
                                     starter_decks::Names.size(),
                                     0,
                                     starter_decks::Names.size()))
                        {
                            selectedAdminStarterDeck = *deckIndex;
                        }
                    }
                    else if (adminBackButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (const std::optional<std::size_t> tabIndex = adminTabs.clickedIndex(clickPos);
                             tabIndex && *tabIndex != 0)
                    {
                        openAdminTab(*tabIndex);
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
                        else if (adminAddCardButton.isClicked(clickPos))
                        {
                            openAddCardPopup();
                        }
                        else if (adminGiveStarterDeckButton.isClicked(clickPos))
                        {
                            openGiveStarterDeckPopup();
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
                else if (currentState == GameState::AdminTools)
                {
                    if (adminBackButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (const std::optional<std::size_t> tabIndex = adminTabs.clickedIndex(clickPos);
                             tabIndex && *tabIndex != 2)
                    {
                        openAdminTab(*tabIndex);
                    }
                    else if (adminSandboxButton.isClicked(clickPos))
                    {
                        loadSandbox();
                    }
                    else if (adminCardEditorButton.isClicked(clickPos))
                    {
                        showCardEditorScreen();
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
                            piece && pieceCanTakeGameAction(*piece) &&
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
                        else if (starterDeckMode)
                        {
                            leaveStarterDeckEditor();
                        }
                        else
                        {
                            showAuthenticatedScreen();
                        }
                    }
                    else if (const std::optional<std::size_t> tabIndex = adminTabs.clickedIndex(clickPos);
                             starterDeckMode && !deckEditorBusy() && tabIndex && *tabIndex != 1)
                    {
                        starterDeckExitTab = *tabIndex;
                        if (deckEditorMode == DeckEditorMode::EditDeck)
                        {
                            requestLeaveDeckEdit(true);
                        }
                        else
                        {
                            leaveStarterDeckEditor();
                        }
                    }
                    else if (!deckEditorBusy())
                    {
                        if (deckEditorMode == DeckEditorMode::DeckList && !starterDeckMode &&
                            newDeckButton.isClicked(clickPos))
                        {
                            createNewDeck();
                            applyCollectionFilters();
                            setMessage(messageText, "Editing a new deck. Save to store it.", sf::Color::Yellow);
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList && refreshDeckButton.isClicked(clickPos))
                        {
                            if (starterDeckMode)
                            {
                                loadStarterDeckEditor();
                            }
                            else
                            {
                                loadDeckEditor();
                            }
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList && editDeckButton.isClicked(clickPos))
                        {
                            editSelectedDeck();
                            applyCollectionFilters();
                        }
                        else if (deckEditorMode == DeckEditorMode::DeckList && !starterDeckMode &&
                                 deleteDeckButton.isClicked(clickPos))
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
                        else if (deckEditorMode == DeckEditorMode::EditDeck && clickCollectionTraitFilter(clickPos))
                        {
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
                    else if (!revealedCardTitle &&
                             shopStarterDecksButton.isClicked(clickPos) &&
                             !shopBusy())
                    {
                        loadStarterDecksScreen(false);
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
                else if (currentState == GameState::StarterDecks)
                {
                    if (starterDeckBackButton.isClicked(clickPos) && !starterDecksBusy())
                    {
                        if (starterDeckPickRequired)
                        {
                            returnToMenu();
                        }
                        else
                        {
                            loadShop();
                        }
                    }
                    else if (starterDeckActionEnabled() &&
                             claimStarterDeckButton.isClicked(clickPos) &&
                             !starterDecksBusy())
                    {
                        claimSelectedStarterDeck();
                    }
                    else if (const std::optional<std::size_t> offerIndex = starterDeckOfferAt(clickPos))
                    {
                        selectedStarterDeckOffer = *offerIndex;
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
                        const DetailRows details = deckEditorCardDetails(*card);
                        inspectedDeckEditorCardScroll = std::clamp(
                            inspectedDeckEditorCardScroll - wheel->delta * 34.0f,
                            0.0f,
                            detailRowsMaxScroll(details));
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
                    DetailRows actionDescriptions;
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
            if (currentState == GameState::AdminUsers && addCardPopupVisible)
            {
                adminCardInput.handleEvent(*event, window);
            }
            else if (currentState == GameState::AdminUsers && !deleteUserPopupVisible)
            {
                adminSearchInput.handleEvent(*event, window);
                adminGoldInput.handleEvent(*event, window);
            }

            if (currentState == GameState::CardEditor)
            {
                if (cardEditorScreen.handleEvent(*event, window))
                {
                    if (loggedInIsAdmin)
                    {
                        loadAdminToolsScreen();
                    }
                    else
                    {
                        showAuthenticatedScreen();
                    }
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
                    else if (currentState == GameState::AdminUsers && addCardPopupVisible)
                    {
                        dismissAddCardPopup();
                    }
                    else if (currentState == GameState::AdminUsers && giveStarterDeckPopupVisible)
                    {
                        dismissGiveStarterDeckPopup();
                    }
                    else if (currentState == GameState::AdminUsers || currentState == GameState::AdminTools)
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
                    else if (currentState == GameState::StarterDecks && !starterDecksBusy())
                    {
                        // The free pick cannot be skipped, but the player can
                        // still back out to the menu and sign in again later.
                        if (starterDeckPickRequired)
                        {
                            returnToMenu();
                        }
                        else
                        {
                            loadShop();
                        }
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
                    else if (currentState == GameState::DeckEditor ||
                             currentState == GameState::Shop ||
                             currentState == GameState::StarterDecks)
                    {
                        // Busy editor/shop requests keep their screen until they
                        // finish, and the mandatory free pick has no way out.
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
                else if (currentState == GameState::AdminUsers &&
                         addCardPopupVisible &&
                         keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    confirmAddCard();
                }
                else if (currentState == GameState::AdminUsers &&
                         !deleteUserPopupVisible && !addCardPopupVisible)
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
                // Keep the hit boxes in step with the tiered layout the draw
                // pass applies.
                loginButton.setSize({236.0f, 58.0f});
                loginButton.setPosition({282.0f, 232.0f});
                createButton.setSize({204.0f, 46.0f});
                createButton.setPosition({298.0f, 314.0f});
                menuOptionsButton.setSize({150.0f, 38.0f});
                menuOptionsButton.setPosition({325.0f, 378.0f});
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
            // Lay out before hit-testing, so the first frame on the screen picks
            // up the same geometry the draw pass will use.
            layoutOptionsScreen();
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
            layoutLoginForm();
            rememberMeCheckbox.update(mousePos);
            passwordVisibilityIcon.update(mousePos);
            loginSubmitButton.update(mousePos);
            backButton.update(mousePos);
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            layoutCreateAccountForm();
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
            authenticatedSettingsHovered =
                !exitDesktopPopupVisible && authenticatedSettingsButtonClicked(mousePos);
            if (exitDesktopPopupVisible)
            {
                cancelExitDesktopButton.update(mousePos);
                confirmExitDesktopButton.update(mousePos);
            }
            else
            {
                storyButton.update(mousePos);
                playButton.update(mousePos);
                conquestButton.update(mousePos);
                deckEditorButton.update(mousePos);
                shopButton.update(mousePos);
                if (loggedInIsAdmin)
                {
                    adminUsersButton.update(mousePos);
                }
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
            else if (addCardPopupVisible)
            {
                cancelAddCardButton.update(mousePos);
                if (!pendingAdminUserCard)
                {
                    confirmAddCardButton.update(mousePos);
                }
            }
            else if (giveStarterDeckPopupVisible)
            {
                cancelGiveStarterDeckButton.update(mousePos);
                if (!pendingAdminUserStarterDeck)
                {
                    confirmGiveStarterDeckButton.update(mousePos);
                }
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
                    adminAddCardButton.update(mousePos);
                    adminGiveStarterDeckButton.update(mousePos);
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
            adminCardInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::AdminTools)
        {
            adminTabs.update(mousePos);
            adminBackButton.update(mousePos);
            adminSandboxButton.update(mousePos);
            adminCardEditorButton.update(mousePos);
        }
        else if (currentState == GameState::DeckSelect)
        {
            findMatchButton.update(mousePos);
            backButton.update(mousePos);
        }
        else if (currentState == GameState::Matchmaking)
        {
            if (lastFrameState != GameState::Matchmaking)
            {
                matchmakingSearchStart = animationTime;
            }
            cancelMatchmakingButton.setSize({148.0f, 40.0f});
            cancelMatchmakingButton.setPosition({248.0f, 464.0f});
            playAiButton.setSize({148.0f, 40.0f});
            playAiButton.setPosition({404.0f, 464.0f});
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
                for (CheckboxControl& checkbox : collectionTraitFilterCheckboxes)
                {
                    checkbox.update(mousePos);
                }
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
        else if (currentState == GameState::Conquest)
        {
            conquestScreen.update(mousePos, deltaTime);
            handleConquestScreenAction();
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
                shopStarterDecksButton.update(mousePos);
                buyCardButton.update(mousePos);
            }
        }
        else if (currentState == GameState::StarterDecks)
        {
            starterDeckBackButton.update(mousePos);
            if (starterDeckActionEnabled())
            {
                claimStarterDeckButton.update(mousePos);
            }
            else
            {
                claimStarterDeckButton.hovered = false;
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
                        piece && pieceCanTakeGameAction(*piece) &&
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
            currentState != GameState::StarterDecks &&
            currentState != GameState::AdminUsers &&
            currentState != GameState::AdminTools &&
            currentState != GameState::CardEditor &&
            currentState != GameState::Conquest &&
            currentState != GameState::Game &&
            currentState != GameState::Authenticated)
        {
            const std::string titleValue = title.getString().toAnsiString();
            if (titleValue == "Gloomthorn")
            {
                // Slightly larger than the wordmark so the title has a few px of padding.
                drawTitlePlaque(window, font, " ", {400.0f, 64.0f}, {384.0f, 82.0f});
                drawGloomthornWordmark({400.0f, 64.0f}, {326.0f, 60.0f});
            }
            else
            {
                drawTitlePlaque(window, font, titleValue, {400.0f, 64.0f}, {360.0f, 70.0f});
            }
        }

        if (currentState == GameState::Menu)
        {
            // Sign in is the only reason to be on this screen, so it is the only
            // plate at full weight; Options is quiet metal.
            loginButton.setVariant(ButtonVariant::Primary);
            loginButton.setSize({236.0f, 58.0f});
            loginButton.setPosition({282.0f, 232.0f});
            loginButton.setLabelSize(type::Hero);
            createButton.setSize({204.0f, 46.0f});
            createButton.setPosition({298.0f, 314.0f});
            createButton.setLabelSize(type::Subheading);
            menuOptionsButton.setVariant(ButtonVariant::Quiet);
            menuOptionsButton.setSize({150.0f, 38.0f});
            menuOptionsButton.setPosition({325.0f, 378.0f});
            menuOptionsButton.setLabelSize(type::Body);

            drawAmbientMotes(window, animationTime, 36, sf::Color(178, 138, 224, 132));
            loginButton.draw(window, animationTime);
            createButton.draw(window, animationTime);
            menuOptionsButton.draw(window, animationTime);
            drawBuildStamp();
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
            layoutOptionsScreen();
            drawPanel(window, OptionsPanelPosition, OptionsPanelSize);
            optionsTabs.draw(window);

            constexpr float RowLabelX = 168.0f;
            const float ruleLeft = OptionsPanelPosition.x + 28.0f;
            const float ruleWidth = OptionsPanelSize.x - 56.0f;

            if (activeOptionsTab == OptionsTab::Graphics)
            {
                drawLabelText(
                    window, font, "display", type::Label, {RowLabelX, 172.0f}, palette::Brass, 2.0f);
                drawSeparatorRule(window, {ruleLeft, 190.0f}, ruleWidth);

                drawText(window, font, "Display Mode", type::Body, {RowLabelX, 214.0f}, palette::Ink);
                displayModeButton.draw(window, animationTime);
                drawSeparatorRule(window, {ruleLeft, 258.0f}, ruleWidth, false);

                drawText(window, font, "Resolution", type::Body, {RowLabelX, 282.0f}, palette::Ink);
                previousResolutionButton.draw(window, animationTime);
                resolutionButton.draw(window, animationTime);
                nextResolutionButton.draw(window, animationTime);
                drawSeparatorRule(window, {ruleLeft, 326.0f}, ruleWidth, false);

                drawText(
                    window,
                    font,
                    "Display changes take effect when you apply them.",
                    type::Caption,
                    {RowLabelX, 348.0f},
                    palette::InkMuted);
                applyOptionsButton.draw(window, animationTime);
            }
            else if (activeOptionsTab == OptionsTab::Audio)
            {
                drawLabelText(
                    window, font, "volume", type::Label, {RowLabelX, 172.0f}, palette::Brass, 2.0f);
                drawSeparatorRule(window, {ruleLeft, 190.0f}, ruleWidth);
                allAudioSlider.draw(window);
                musicAudioSlider.draw(window);
                soundFxAudioSlider.draw(window);
                muteAllAudioCheckbox.draw(window, audioSystem.isAllMuted());
                muteMusicCheckbox.draw(window, audioSystem.isMusicMuted());
                muteSoundFxCheckbox.draw(window, audioSystem.isSoundEffectsMuted());
            }
            else
            {
                drawLabelText(
                    window, font, "account", type::Label, {RowLabelX, 172.0f}, palette::Brass, 2.0f);
                drawSeparatorRule(window, {ruleLeft, 190.0f}, ruleWidth);

                if (optionsReturnState == GameState::Authenticated)
                {
                    drawText(window, font, "Signed in as", type::Body, {RowLabelX, 214.0f}, palette::InkMuted);
                    sf::Text signedIn(displayFontOr(font), loggedInUsername, type::Subheading);
                    signedIn.setFillColor(palette::Ink);
                    signedIn.setPosition({RowLabelX + 96.0f, 210.0f});
                    drawCrispText(window, signedIn);
                    drawSeparatorRule(window, {ruleLeft, 258.0f}, ruleWidth, false);

                    drawText(window, font, "Password", type::Body, {RowLabelX, 282.0f}, palette::Ink);
                    changePasswordOptionButton.draw(window, animationTime);
                }
                else
                {
                    // A designed empty state rather than a bare sentence in the
                    // middle of an otherwise empty panel.
                    const sf::Vector2f center{
                        OptionsPanelPosition.x + OptionsPanelSize.x * 0.5f,
                        OptionsPanelPosition.y + OptionsPanelSize.y * 0.52f};
                    drawRadialGlow(window, center - sf::Vector2f(0.0f, 26.0f), 46.0f, sf::Color(123, 79, 168, 52));
                    drawLeagueSigil(center - sf::Vector2f(0.0f, 26.0f), 17.0f, palette::Arcane);
                    drawCenteredText(
                        window,
                        displayFontOr(font),
                        "Not Signed In",
                        type::Subheading,
                        center + sf::Vector2f(0.0f, 12.0f),
                        palette::Ink);
                    drawCenteredText(
                        window,
                        font,
                        "Sign in to manage your account settings.",
                        type::Caption,
                        center + sf::Vector2f(0.0f, 34.0f),
                        palette::InkMuted);
                }
            }

            optionsBackButton.draw(window, animationTime);
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
            drawPasswordRequirementHint(358.0f);
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
            layoutLoginForm();
            drawPanel(window, LoginPanelPosition, LoginPanelSize);
            drawEntryFormHeader(
                LoginPanelPosition,
                LoginPanelSize,
                "Sign In",
                "The mire remembers you.");
            usernameInput.draw(window);
            passwordInput.draw(window);
            passwordVisibilityIcon.draw(window, passwordVisible);
            rememberMeCheckbox.draw(window, rememberMeChecked);
            drawFormNotice(LoginNoticeY);
            loginSubmitButton.draw(window, animationTime);
            backButton.draw(window, animationTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            layoutCreateAccountForm();
            drawPanel(window, CreatePanelPosition, CreatePanelSize);
            drawEntryFormHeader(CreatePanelPosition, CreatePanelSize, "Create Account", "");
            usernameInput.draw(window);
            passwordInput.draw(window);
            confirmInput.draw(window);
            drawPasswordRequirementHint(372.0f);
            passwordVisibilityIcon.draw(window, passwordVisible);
            confirmVisibilityIcon.draw(window, passwordVisible);
            drawFormNotice(CreateNoticeY);
            createSubmitButton.draw(window, animationTime);
            backButton.draw(window, animationTime);
        }
        else if (currentState == GameState::Authenticated)
        {
            drawAuthenticatedMenuChrome();
            drawAuthenticatedMenuButton(playButton, mainMenuPlayIconTexture);
            drawAuthenticatedMenuButton(storyButton, mainMenuStoryIconTexture);
            drawAuthenticatedMenuButton(conquestButton, mainMenuConquestIconTexture);
            drawAuthenticatedMenuButton(deckEditorButton, mainMenuDeckEditorIconTexture);
            drawAuthenticatedMenuButton(shopButton, mainMenuShopIconTexture);
            if (loggedInIsAdmin)
            {
                drawAuthenticatedMenuButton(adminUsersButton, mainMenuAdminIconTexture);
            }
            drawAuthenticatedMenuButton(logoutButton, mainMenuLogoutIconTexture);
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
        else if (currentState == GameState::AdminTools)
        {
            drawAdminTools();
        }
        else if (currentState == GameState::DeckSelect)
        {
            drawDeckSelect();
        }
        else if (currentState == GameState::Matchmaking)
        {
            drawMatchmakingScreen();
            cancelMatchmakingButton.draw(window, animationTime);
            playAiButton.draw(window, animationTime);
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
        else if (currentState == GameState::StarterDecks)
        {
            drawStarterDecks();
        }
        else if (currentState == GameState::CardEditor)
        {
            cardEditorScreen.render(window);
        }
        else if (currentState == GameState::Conquest)
        {
            conquestScreen.draw(window);
        }
        else if (currentState == GameState::Game)
        {
            drawGame();
        }

        // Lets a screen tell that this is its first frame (matchmaking arms its
        // elapsed-search timer off this).
        lastFrameState = currentState;
        window.display();

        if (captureRequest && captureScreenReady)
        {
            if (++captureFramesOnScreen >= captureRequest->warmupFrames)
            {
                const std::string& screen = captureRequest->screens[captureIndex];
                char ordinal[8] = {};
                std::snprintf(ordinal, sizeof(ordinal), "%02zu", captureIndex + 1);
                ui_capture::saveWindow(
                    window,
                    captureRequest->outputDirectory / (std::string(ordinal) + "-" + screen + ".png"));

                captureFramesOnScreen = 0;
                ++captureIndex;
                if (captureIndex >= captureRequest->screens.size())
                {
                    window.close();
                }
                else
                {
                    applyCaptureScreen(captureRequest->screens[captureIndex]);
                }
            }
        }
    }

    return 0;
}
