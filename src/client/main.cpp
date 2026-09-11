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
#include "client_story.hpp"
#include "client_story_cards.hpp"
#include "client_story_keyboard.hpp"
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
#include "../gameserver/ai_player.hpp"

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
#include <iterator>
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
constexpr const char* PieceBaseBlueArtworkPath = "bases/basic0_blue.png";
constexpr const char* PieceBaseRedArtworkPath = "bases/basic0_red.png";
constexpr const char* PieceBaseLargeBlueArtworkPath = "bases/basic0_large_blue.png";
constexpr const char* PieceBaseLargeRedArtworkPath = "bases/basic0_large_red.png";
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

    account_data::AudioSettings getSettings() const
    {
        return {
            toPercent(allVolume),
            toPercent(musicVolume),
            toPercent(soundEffectsVolume),
            allMuted,
            musicMuted,
            soundEffectsMuted};
    }

    void applySettings(const account_data::AudioSettings& settings)
    {
        setAllVolume(static_cast<float>(settings.allVolumePercent) / 100.0f);
        setMusicVolume(static_cast<float>(settings.musicVolumePercent) / 100.0f);
        setSoundEffectsVolume(static_cast<float>(settings.soundEffectsVolumePercent) / 100.0f);
        setAllMuted(settings.allMuted);
        setMusicMuted(settings.musicMuted);
        setSoundEffectsMuted(settings.soundEffectsMuted);
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

    static std::uint8_t toPercent(float volume)
    {
        return static_cast<std::uint8_t>(std::lround(std::clamp(volume, 0.0f, 1.0f) * 100.0f));
    }

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
        if (!dematerializePath || !std::filesystem::exists(*dematerializePath) ||
            !dematerializeBuffer.loadFromFile(*dematerializePath))
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
        const std::optional<std::filesystem::path> musicPath = resolveAssetPath("audio/GT soundtrack 006.mp3");
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
// What a mystery card costs. It was spelled out as a literal 5 in the shop copy,
// the affordability check and the error message, which could drift apart.
constexpr int CardPackPrice = 5;
constexpr float CoinPurchasePollIntervalSeconds = 2.0f;
constexpr float CoinPurchasePollTimeoutSeconds = 300.0f;
constexpr float FidgetDelayMinimumSeconds = 3.0f;
constexpr float FidgetDelayMaximumSeconds = 8.0f;
constexpr float FidgetAnimationDurationSeconds = 0.75f;
// Keep the fidget animation path available, but leave it disabled until we
// want stationary pieces to animate again.
constexpr bool EnableFidgetAnimations = false;
constexpr float PieceMoveAnimationDurationSeconds = 0.95f;
#ifdef NDEBUG
constexpr const char* ClientConfigFileName = "client_release.cfg";
#else
constexpr const char* ClientConfigFileName = "client_debug.cfg";
#endif

// Deck picker: a roster of decks on the left, the selected deck's portrait on
// the right. The single narrow centred panel it replaces was two thirds empty.
constexpr float DeckPickerPanelX = 24.0f;
constexpr float DeckPickerPanelY = 92.0f;
constexpr float DeckPickerPanelWidth = 352.0f;
constexpr float DeckPickerPanelHeight = 404.0f;
constexpr float DeckDetailPanelX = 392.0f;
constexpr float DeckDetailPanelWidth = 384.0f;
// Pre-match deck picker. It sits lower than the editor's because the Gloomthorn
// wordmark plaque owns the top of this screen.
constexpr float DeckSelectPanelY = 112.0f;
constexpr float DeckSelectPanelHeight = 372.0f;
constexpr float DeckSelectListY = 150.0f;
constexpr float DeckPanelX = 24.0f;
constexpr float CurrentDeckPanelX = 24.0f;
constexpr float CurrentDeckPanelWidth = 364.0f;
constexpr float LibraryPanelX = 404.0f;
constexpr float LibraryPanelWidth = 372.0f;
constexpr float DeckEditorPanelY = 92.0f;
constexpr float DeckEditorPanelHeight = 404.0f;
constexpr float DeckListX = 40.0f;
constexpr float DeckListY = 146.0f;
constexpr float DeckListWidth = 320.0f;
constexpr float DeckRowHeight = 66.0f;
constexpr std::size_t VisibleDeckRows = 5;

constexpr float DeckCardsX = 40.0f;
constexpr float DeckCardsY = 186.0f;
constexpr float DeckCardsWidth = 324.0f;
constexpr float DeckCardRowHeight = 42.0f;
constexpr std::size_t VisibleDeckCardRows = 6;
// Reserved slot for deck-legality messages, so they are never drawn across the
// collection panel the way the old centred warning string was.
constexpr float DeckValidationY = 444.0f;
constexpr float DeckValidationHeight = 44.0f;
constexpr float PasswordIconInset = 42.0f;
constexpr std::uint32_t AdminUsersPageSize = 6;
constexpr float AdminUserRowY = 174.0f;
constexpr float AdminUserRowHeight = 43.0f;
constexpr float AdminCardRowY = 276.0f;
constexpr float AdminCardRowHeight = 36.0f;
constexpr std::size_t VisibleAdminCardRows = 5;
constexpr float AdminStarterDeckRowY = 232.0f;
constexpr float AdminStarterDeckRowHeight = 42.0f;

constexpr float LibraryX = 420.0f;
constexpr float LibraryY = 284.0f;
constexpr float LibraryWidth = 332.0f;
constexpr float LibraryRowHeight = 42.0f;
constexpr std::size_t VisibleLibraryRows = 5;
// "Spells & Enchantments" overran the filter row and was clipped by the panel
// edge. The category still covers both; the label no longer has to spell it out.
constexpr std::array<const char*, 3> CollectionTypeLabels = {"Heroes", "Units", "Spells"};
// Filter chips are measured from the font and wrapped inside the panel.
constexpr float CollectionTypeChipsY = 160.0f;
constexpr float CollectionTraitChipsY = 200.0f;
constexpr float CollectionChipHeight = 20.0f;
constexpr float CollectionChipGap = 4.0f;
constexpr unsigned int CollectionChipTextSize = 12;

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
    StorySelect,
    StoryMissionSelect,
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

enum class GameConfirmationAction
{
    Resign,
    ExitStory,
    RestartStory
};

enum class StoryGameKeyboardFocus
{
    Board,
    Hand,
    PlayerOne,
    PlayerTwo,
    DrawPile,
    Ability,
    EndTurn,
    Restart,
    Exit
};

// The 16:9 canvas has a narrow gutter on either side of the legacy board. Keep
// owner readouts in those gutters so the upper board row can never cover them.
constexpr float GameTopBarY = 5.0f;
constexpr float GamePlayerBannerWidth = 180.0f;
constexpr float GamePlayerBannerHeight = 90.0f;
// On the legacy 4:3 canvas the back rank begins at y=66. Keep compact HUD
// hitboxes and paint wholly above it so A8 and H8 remain visible and clickable.
constexpr float GameCompactPlayerBannerHeight = 56.0f;
constexpr float GamePlayerBannerLeftX = ui_canvas::Left + 4.0f;
constexpr float GamePlayerBannerRightX = ui_canvas::Right - GamePlayerBannerWidth - 4.0f;
constexpr float GameCompactPlayerBannerLeftX = 4.0f;
constexpr float GameCompactPlayerBannerRightX =
    ui_canvas::LegacyWidth - GamePlayerBannerWidth - 4.0f;
constexpr float GameTurnPlaqueWidth = GamePlayerBannerWidth;
constexpr float GameTurnPlaqueHeight = 48.0f;
constexpr float GameTurnPlaqueY = GameTopBarY + GamePlayerBannerHeight + 8.0f;
constexpr float GameCompactStoryExitButtonX = 190.0f;
constexpr float GameCompactStoryRestartButtonX = 496.0f;
constexpr float GameCompactStoryButtonY = 9.0f;
constexpr float GameCompactStoryButtonWidth = 114.0f;
constexpr float GameCompactStoryButtonHeight = 40.0f;
constexpr float ResignDialogX = 220.0f;
constexpr float ResignDialogY = 188.0f;
constexpr float ResignDialogWidth = 360.0f;
constexpr float ResignDialogHeight = 220.0f;
constexpr float ActionChoiceDialogX = 176.0f;
constexpr float ActionChoiceDialogWidth = 448.0f;
constexpr float ActionChoiceHeaderHeight = 82.0f;
constexpr float ActionChoiceRowHeight = 62.0f;
constexpr float ActionChoiceFooterHeight = 52.0f;
constexpr float ActionChoiceRowInset = 18.0f;
// The player-enchantment drop test targets the owner banners.
constexpr float GameLabelY = GameTopBarY;
constexpr float GamePlayerReadoutWidth = GamePlayerBannerWidth;

static_assert(
    GameTopBarY + GameCompactPlayerBannerHeight + 3.0f < BoardOriginY,
    "Compact player-banner glow must stay above every board square.");
static_assert(
    GameCompactStoryButtonY + GameCompactStoryButtonHeight < BoardOriginY,
    "Compact story controls must stay above every board square.");
static_assert(
    GameCompactStoryExitButtonX >
            GameCompactPlayerBannerLeftX + GamePlayerBannerWidth + 3.0f &&
        GameCompactStoryExitButtonX + GameCompactStoryButtonWidth <
            BoardCenterX - GameTurnPlaqueWidth * 0.5f &&
        GameCompactStoryRestartButtonX >
            BoardCenterX + GameTurnPlaqueWidth * 0.5f &&
        GameCompactStoryRestartButtonX + GameCompactStoryButtonWidth <
            GameCompactPlayerBannerRightX,
    "Compact story controls must stay in the gaps between HUD plaques.");

bool usesCompactGameHud(const sf::RenderWindow& window)
{
    const sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0)
    {
        return false;
    }
    return static_cast<float>(size.x) / static_cast<float>(size.y) <
        ui_canvas::Aspect - 0.01f;
}

float gamePlayerBannerX(const sf::RenderWindow& window, int playerNumber)
{
    if (usesCompactGameHud(window))
    {
        return playerNumber == 1
            ? GameCompactPlayerBannerLeftX
            : GameCompactPlayerBannerRightX;
    }
    return playerNumber == 1 ? GamePlayerBannerLeftX : GamePlayerBannerRightX;
}

float gamePlayerBannerHeight(const sf::RenderWindow& window)
{
    return usesCompactGameHud(window)
        ? GameCompactPlayerBannerHeight
        : GamePlayerBannerHeight;
}

// Bottom command bar: piles at the left, the hand across the middle, turn
// actions at the right.
constexpr float GameBottomBarY = 468.0f;
constexpr float GameBottomBarHeight = 126.0f;
constexpr float GameBottomLeftX = 22.0f;
// Reserve the final 20 logical pixels for Story keyboard help instead of
// painting that help through the cards, piles, and action buttons.
constexpr float GamePileY = GameBottomBarY + 2.0f;
constexpr float GamePileWidth = 70.0f;
constexpr float GamePileHeight = 104.0f;
constexpr float GameDeckPileX = GameBottomLeftX;
constexpr float HandY = GameBottomBarY + 2.0f;
constexpr float HandCardWidth = 72.0f;
constexpr float HandCardHeight = 104.0f;
constexpr float HandGap = 5.0f;
// Leaves room either side of the fan for the overflow chevrons.
constexpr float HandStartX = 190.0f;
constexpr float HandRightX = 582.0f;
constexpr float HandHoverLift = 24.0f;
constexpr std::size_t VisibleGameHandCards = 4;
inline float gameHandCardPitch(std::size_t visibleCards)
{
    if (visibleCards <= 1)
    {
        return HandCardWidth + HandGap;
    }
    return std::min(
        HandCardWidth + HandGap,
        (HandRightX - HandStartX - HandCardWidth) /
            static_cast<float>(visibleCards - 1));
}
inline float gameHandCardX(std::size_t visibleIndex, std::size_t visibleCards)
{
    return HandStartX +
        static_cast<float>(visibleIndex) * gameHandCardPitch(visibleCards);
}
constexpr std::size_t ForesightChoiceColumns = 8;
constexpr std::size_t ForesightVisibleRows = 3;
// Leave a readable guidance/correction band above the revealed cards. Three
// rows still fit inside the 800x600 modal at this position.
constexpr float ForesightChoiceY = 164.0f;
constexpr float ForesightChoiceRowPitch = 134.0f;
constexpr float ForesightChoiceGap = 10.0f;
constexpr float TrashCanWidth = GamePileWidth;
constexpr float TrashCanHeight = GamePileHeight;
constexpr float TrashCanSize = GamePileWidth;
constexpr float TrashCanX = GameBottomLeftX + 82.0f;
constexpr float TrashCanY = GamePileY;
constexpr float TrashCanDropPadding = 12.0f;
constexpr float GameActionButtonGap = 6.0f;
constexpr float GameActionButtonX = 598.0f;
constexpr float GameActionButtonWidth = 178.0f;
// Ending the turn is the primary action, so it gets the tall row at the top;
// leaving and the contextual ability sit below it in fixed slots, which keeps the
// primary from shifting when the ability slot appears.
constexpr float GamePrimaryButtonHeight = 44.0f;
constexpr float GameActionButtonHeight = 26.0f;
// The contextual ability takes the top slot; when no ability is available the
// slot carries the opponent's hand count instead of sitting empty.
constexpr float GameAbilityButtonY = GameBottomBarY + 4.0f;
constexpr float GameActionButtonY = GameAbilityButtonY + 30.0f;
constexpr float GameLeaveButtonY = GameActionButtonY + GamePrimaryButtonHeight + 2.0f;
constexpr float GameAbilityButtonWidth = GameActionButtonWidth;
constexpr float GameEndTurnButtonWidth = GameActionButtonWidth;
constexpr float GameLeaveButtonWidth = GameActionButtonWidth;
constexpr float PiecePopupX = 150.0f;
constexpr float PiecePopupY = 74.0f;
constexpr float PiecePopupWidth = 500.0f;
constexpr float PiecePopupHeight = 382.0f;
constexpr float PiecePopupTextX = PiecePopupX + 24.0f;
constexpr float PiecePopupTextWidth = PiecePopupWidth - 48.0f;
constexpr float PiecePopupActionHeadingY = PiecePopupY + 168.0f;
constexpr float PiecePopupScrollY = PiecePopupActionHeadingY + 26.0f;
constexpr float PiecePopupScrollHeight = PiecePopupHeight - (PiecePopupScrollY - PiecePopupY) - 62.0f;
constexpr float PiecePopupScrollTextXInset = 24.0f;
constexpr float PiecePopupScrollTextYInset = 14.0f;

// The deck editor's card inspector needs enough width for the face and its
// descriptive column, plus a genuinely useful abilities viewport below them.
// Keep the panel inside the 800x600 logical canvas with a deliberate bottom
// margin for the close control and the panel border.
constexpr float CardPopupX = 100.0f;
constexpr float CardPopupY = 40.0f;
constexpr float CardPopupWidth = 600.0f;
constexpr float CardPopupHeight = 520.0f;
constexpr float CardPopupFaceX = CardPopupX + 24.0f;
constexpr float CardPopupFaceY = CardPopupY + 22.0f;
constexpr float CardPopupFaceWidth = 200.0f;
constexpr float CardPopupFaceHeight = 256.0f;
constexpr float CardPopupStatsX = CardPopupX + 250.0f;
constexpr float CardPopupStatsWidth = CardPopupWidth - 274.0f;
constexpr float CardPopupAbilitiesX = CardPopupX + 24.0f;
constexpr float CardPopupAbilitiesY = CardPopupY + 298.0f;
constexpr float CardPopupAbilitiesWidth = CardPopupWidth - 48.0f;
constexpr float CardPopupAbilitiesHeight = 150.0f;
constexpr float PieceDoubleClickSeconds = 0.38f;
constexpr float DeckCardDoubleClickSeconds = 0.38f;
constexpr float GameDragStartDistanceSquared = 36.0f;
constexpr int StoryMissionPageSize = 8;
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

    std::optional<ui_capture::Request> captureRequest =
        ui_capture::parseCommandLine(argc, argv);

    if (captureRequest)
    {
        std::string outputError;
        const std::filesystem::path executablePath =
            argc > 0 && argv[0] ? argv[0] : std::filesystem::path{};
        if (!ui_capture::prepareOutputDirectory(
                *captureRequest, executablePath, outputError))
        {
            fmt::println(stderr, "[UI capture output rejection] {}", outputError);
            return 1;
        }
    }

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
    sf::Texture* boardSurfaceTexture = textures.load("ui/board-surface-v2.png");
    sf::Texture* pieceBaseBlueArtwork = textures.load(PieceBaseBlueArtworkPath);
    sf::Texture* pieceBaseRedArtwork = textures.load(PieceBaseRedArtworkPath);
    sf::Texture* pieceBaseLargeBlueArtwork = textures.load(PieceBaseLargeBlueArtworkPath);
    sf::Texture* pieceBaseLargeRedArtwork = textures.load(PieceBaseLargeRedArtworkPath);
    const std::array<sf::Texture*, 4> rarityGemArtworks = {
        textures.load("bases/gem1.png"),
        textures.load("bases/gem2.png"),
        textures.load("bases/gem3.png"),
        textures.load("bases/gem4.png")};
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
    InputBox deckNameInput({304.0f, 154.0f}, {212.0f, 32.0f}, "", font);
    InputBox adminSearchInput({120.0f, 94.0f}, {390.0f, 36.0f}, "", font);
    // No built-in label: InputBox draws one at 18px, which outweighed every
    // button label around it and read as a heading. The admin screen draws its
    // own 11px caption in the same style as its other column captions.
    InputBox adminGoldInput({236.0f, 482.0f}, {124.0f, 36.0f}, "", font);
    // Caption drawn by the dialog instead of InputBox's own 18px label, which
    // outweighed the dialog's body copy.
    InputBox adminCardInput({240.0f, 224.0f}, {320.0f, 36.0f}, "", font);

    CheckboxControl rememberMeCheckbox({300.0f, 286.0f}, "Remember me", font, rememberCheckTexture);
    Button loginSubmitButton({300.0f, 342.0f}, {200.0f, 50.0f}, "Login", font);
    Button createSubmitButton({300.0f, 410.0f}, {200.0f, 50.0f}, "Create Account", font);
    Button backButton({24.0f, 500.0f}, {120.0f, 40.0f}, "Back", font);
    Button exitDesktopButton({20.0f, 520.0f}, {200.0f, 45.0f}, "Exit to Desktop", font);
    Button cancelMatchmakingButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Cancel", font);
    Button playAiButton({150.0f, 520.0f}, {160.0f, 45.0f}, "Play vs AI", font);
    Button storyButton({300.0f, 152.0f}, {200.0f, 48.0f}, "GUIDED STORY", font);
    Button playButton({300.0f, 215.0f}, {200.0f, 48.0f}, "PLAY ONLINE", font);
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
    // The type and trait filters were a fixed grid of checkboxes: the widest
    // label ran off the panel and the last trait row collided with the card list
    // below it. Chips are measured from the font and wrapped inside the panel's
    // inner width, so no label can clip however long it is.
    const std::vector<std::string> collectionTypeLabels(
        CollectionTypeLabels.begin(), CollectionTypeLabels.end());
    const std::vector<std::string> collectionTraitLabels(
        game_data::CardTraitLabels.begin(), game_data::CardTraitLabels.end());
    const std::vector<FilterChip> collectionTypeChips = layoutFilterChips(
        font,
        collectionTypeLabels,
        {LibraryX, CollectionTypeChipsY},
        LibraryWidth,
        CollectionChipTextSize,
        CollectionChipHeight,
        CollectionChipGap);
    const std::vector<FilterChip> collectionTraitChips = layoutFilterChips(
        font,
        collectionTraitLabels,
        {LibraryX, CollectionTraitChipsY},
        LibraryWidth,
        CollectionChipTextSize,
        CollectionChipHeight,
        CollectionChipGap);
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
    Button claimStarterDeckButton({220.0f, 502.0f}, {360.0f, 46.0f}, "Claim Deck", font);
    // The Tools tab collects the admin-only screens that used to sit on the main
    // menu. Tabs are a little narrower than three at the old width would be so
    // the signed-in text still fits between the strip and the Back button.
    TabStrip adminTabs({24.0f, 22.0f}, {140.0f, 38.0f}, {"Users", "Starter Decks", "Tools"}, font);
    Button adminBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button adminPrevPageButton({530.0f, 93.0f}, {52.0f, 38.0f}, "<", font);
    Button adminNextPageButton({706.0f, 93.0f}, {52.0f, 38.0f}, ">", font);
    Button adminRefreshButton({592.0f, 93.0f}, {104.0f, 38.0f}, "Refresh", font);
    // Two aligned rows inside the actions panel. The old row sat at y=458 with
    // the gold input at 460, so nothing shared a baseline, and 150px plates were
    // narrower than labels like "Revoke Admin".
    Button adminGrantButton({40.0f, 482.0f}, {176.0f, 36.0f}, "Grant Admin", font);
    Button adminRevokeButton({40.0f, 482.0f}, {176.0f, 36.0f}, "Revoke Admin", font);
    Button adminGrantGoldButton({376.0f, 482.0f}, {156.0f, 36.0f}, "Grant Gold", font);
    Button adminRemoveGoldButton({548.0f, 482.0f}, {172.0f, 36.0f}, "Remove Gold", font);
    Button adminAddCardButton({40.0f, 526.0f}, {156.0f, 34.0f}, "Add Card", font);
    Button adminGiveStarterDeckButton({212.0f, 526.0f}, {156.0f, 34.0f}, "Give Deck", font);
    // Seated inside their tool cards on the admin Tools tab.
    Button adminSandboxButton({58.0f, 134.0f}, {206.0f, 52.0f}, "Sandbox", font);
    Button adminCardEditorButton({58.0f, 234.0f}, {206.0f, 52.0f}, "Card Editor", font);
    // Held apart from the benign actions so the destructive one is not adjacent
    // to anything routine.
    Button adminDeleteButton({564.0f, 526.0f}, {156.0f, 34.0f}, "Delete User", font);
    // 130px was narrower than "Add Card" and "Give Deck" render at this face.
    Button cancelAddCardButton({246.0f, 476.0f}, {132.0f, 42.0f}, "Cancel", font);
    Button confirmAddCardButton({408.0f, 476.0f}, {156.0f, 42.0f}, "Add Card", font);
    Button cancelGiveStarterDeckButton({246.0f, 424.0f}, {132.0f, 42.0f}, "Cancel", font);
    Button confirmGiveStarterDeckButton({408.0f, 424.0f}, {156.0f, 42.0f}, "Give Deck", font);
    Button cancelDeleteUserButton({250.0f, 366.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmDeleteUserButton({420.0f, 366.0f}, {130.0f, 42.0f}, "Delete", font);
    Button cancelExitDesktopButton({250.0f, 356.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmExitDesktopButton({420.0f, 356.0f}, {130.0f, 42.0f}, "Exit", font);
    // Centred as a pair inside the unsaved-changes dialog.
    Button keepEditingDeckButton({250.0f, 352.0f}, {140.0f, 40.0f}, "Keep Editing", font);
    Button discardDeckChangesButton({410.0f, 352.0f}, {140.0f, 40.0f}, "Discard", font);
    Button closeDeckCardPopupButton(
        {CardPopupX + (CardPopupWidth - 120.0f) * 0.5f, CardPopupY + CardPopupHeight - 48.0f},
        {120.0f, 38.0f},
        "Close",
        font);

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
    int authenticatedMenuFocus = -1;
    OptionsTab activeOptionsTab = OptionsTab::Graphics;
    DisplaySettings pendingDisplaySettings = displaySettings;
    std::size_t selectedResolution = displayResolutionIndex(
        displayResolutions,
        {displaySettings.width, displaySettings.height});
    std::optional<std::future<ServerResult>> pendingRequest;
    std::optional<std::future<ServerResult>> pendingMatchmaking;
    std::optional<std::future<CardListResult>> pendingSandboxLoad;
    std::optional<std::future<CardListResult>> pendingStoryCardLoad;
    std::shared_ptr<MatchmakingCancelState> activeMatchmakingCancel;
    bool matchmakingCancelRequested = false;
    std::optional<std::future<void>> pendingLogout;
    std::optional<std::future<DeckEditorLoadResult>> pendingDeckEditorLoad;
    std::optional<std::future<StarterDeckLoadResult>> pendingStarterDeckLoad;
    std::optional<std::future<DeckCommandResult>> pendingDeckSave;
    std::optional<std::future<DeckCommandResult>> pendingDeckDelete;
    std::optional<std::future<AccountStateResult>> pendingAccountState;
    std::optional<std::future<AudioSettingsSaveResult>> pendingAudioSettingsSave;
    std::optional<account_data::AudioSettings> queuedAudioSettingsSave;
    std::string pendingAudioSettingsSaveToken;
    bool audioSettingsLoaded = false;
    bool audioSettingsDirty = false;
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
    bool resignConfirmPopupVisible = false;
    GameConfirmationAction gameConfirmationAction = GameConfirmationAction::Resign;
    // Zero is the safe Cancel choice; one is the destructive/committing choice.
    int gameConfirmationKeyboardFocus = 0;
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
    StoryCampaign storyCampaign = StoryCampaign::Blackthorn;
    std::unique_ptr<GameEngine> storyEngine;
    bool storyAiPending = false;
    float storyAiActionAt = 0.0f;
    double storyTimerAccumulatorMs = 0.0;
    bool storyClockPausedForReading = false;
    enum class StoryStage
    {
        None,
        Objective,
        Failed,
        Complete
    };
    StoryStage storyStage = StoryStage::None;
    int storyMissionIndex = 0;
    int storyMissionPage = 0;
    int storyMissionStep = 0;
    std::string storyCorrection;
    bool storyUsedAim = false;
    bool storyUsedHide = false;
    bool storyUsedSummon = false;
    std::vector<std::pair<std::string_view, int>> storyRolePieceIds;
    std::vector<StoryPanel> storyPopupPanels;
    std::size_t storyPopupPage = 0;
    bool storyCompleteAfterPopup = false;
    float storyScriptActionAt = 0.0f;
    int storyCompletedCount = 0;
    std::array<int, 3> storyCampaignProgress{};
    std::array<StoryProgress, 3> storyCampaignProgressDetails{};
    int storyGenuineDefeatCount = 0;
    bool storySpoilerConfirmationVisible = false;
    int storySpoilerKeyboardFocus = 0;
    std::uint64_t storyGeneration = 0;
    std::optional<std::future<std::pair<std::uint64_t, AiAction>>> pendingStoryAi;
    int storyComicPage = 0;
    int storySelectKeyboardFocus = 1;
    int storyMissionKeyboardFocus = 0;
    int storyIntroKeyboardFocus = 2;
    int storyPopupKeyboardFocus = 1;
    StoryGameKeyboardFocus storyGameKeyboardFocus = StoryGameKeyboardFocus::Board;
    StoryBoardCursor storyBoardKeyboardCursor{};
    std::size_t storyKeyboardHandIndex = 0;
    std::size_t storyKeyboardForesightIndex = 0;
    bool storyKeyboardNavigationActive = true;
    int storyTargetRow = -1;
    int storyTargetColumn = -1;
    int sandboxPlacementPlayer = 1;
    int nextSandboxPieceId = 1;
    std::size_t gameHandOffset = 0;
    std::size_t foresightChoiceRowOffset = 0;
    std::optional<int> selectedPieceId;
    std::optional<std::size_t> selectedHandIndex;
    std::optional<int> inspectedPieceId;
    std::optional<std::size_t> inspectedHandIndex;
    struct PendingPieceActionChoice
    {
        int pieceId = 0;
        int row = -1;
        int column = -1;
        std::vector<int> actionIndices;
        int focusedOption = 0;
    };
    std::optional<PendingPieceActionChoice> pendingPieceActionChoice;
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

    Button findMatchButton({300.0f, 496.0f}, {200.0f, 48.0f}, "Find Match", font);
    auto layoutDeckSelectControls = [&]() {
        // These actions share one footer row, below the two deck panels. They
        // intentionally do not reuse the generic Back position: that position
        // overlaps Find Match on this screen and produces competing hitboxes.
        findMatchButton.setVariant(ButtonVariant::Primary);
        findMatchButton.setSize({208.0f, 42.0f});
        findMatchButton.setPosition({286.0f, 500.0f});
        findMatchButton.setLabelSize(type::Subheading);

        backButton.setVariant(ButtonVariant::Quiet);
        backButton.setSize({112.0f, 42.0f});
        backButton.setPosition({510.0f, 500.0f});
        backButton.setLabelSize(type::Body);
    };
    Button abilityButton(
        {GameActionButtonX, GameAbilityButtonY},
        {GameAbilityButtonWidth, GameActionButtonHeight},
        "Use Ability",
        font);
    Button endTurnButton(
        {GameActionButtonX, GameActionButtonY},
        {GameEndTurnButtonWidth, GamePrimaryButtonHeight},
        "End Turn",
        font);
    Button sandboxPlayerButton(
        {GameActionButtonX, GameActionButtonY}, {52.0f, GamePrimaryButtonHeight}, "P1", font);
    Button sandboxAdvanceTurnButton(
        {GameActionButtonX + 52.0f + GameActionButtonGap, GameActionButtonY},
        {GameActionButtonWidth - 52.0f - GameActionButtonGap, GamePrimaryButtonHeight},
        "Advance",
        font);
    Button leaveGameButton(
        {GameActionButtonX, GameLeaveButtonY},
        {GameLeaveButtonWidth, GameActionButtonHeight},
        "Resign",
        font);
    Button storyContinueButton({558.0f, 520.0f}, {194.0f, 48.0f}, "Continue", font);
    Button storySkipDrillButton(
        {326.0f, 520.0f}, {216.0f, 48.0f}, "Skip (No Mastery)", font);
    // Keep the popup controls above a dedicated keyboard-help row. The former
    // y=472 placement put the hint directly through the Previous button.
    Button storyPopupPreviousButton({312.0f, 456.0f}, {196.0f, 46.0f}, "Previous", font);
    Button storyPopupContinueButton({524.0f, 456.0f}, {196.0f, 46.0f}, "Continue", font);
    Button storyBackButton({48.0f, 526.0f}, {112.0f, 40.0f}, "Back", font);
    Button storyBlackthornButton({42.0f, 466.0f}, {204.0f, 42.0f}, "Begin", font);
    Button storyMirewatchButton({298.0f, 466.0f}, {204.0f, 42.0f}, "Begin", font);
    Button storySeelieButton({554.0f, 466.0f}, {204.0f, 42.0f}, "Begin", font);
    Button storySpoilerMirewatchButton(
        {220.0f, 390.0f}, {176.0f, 44.0f}, "Play Mirewatch First", font);
    Button storySpoilerContinueButton(
        {412.0f, 390.0f}, {168.0f, 44.0f}, "Continue Anyway", font);
    Button storySelectBackButton({48.0f, 536.0f}, {112.0f, 40.0f}, "Back", font);
    std::array<Button, 8> storyMissionButtons{
        Button({58.0f, 132.0f}, {326.0f, 72.0f}, "Mission 1", font),
        Button({416.0f, 132.0f}, {326.0f, 72.0f}, "Mission 2", font),
        Button({58.0f, 222.0f}, {326.0f, 72.0f}, "Mission 3", font),
        Button({416.0f, 222.0f}, {326.0f, 72.0f}, "Mission 4", font),
        Button({58.0f, 312.0f}, {326.0f, 72.0f}, "Mission 5", font),
        Button({416.0f, 312.0f}, {326.0f, 72.0f}, "Mission 6", font),
        Button({58.0f, 402.0f}, {326.0f, 72.0f}, "Mission 7", font),
        Button({416.0f, 402.0f}, {326.0f, 72.0f}, "Mission 8", font),
    };
    Button storyRestartCampaignButton(
        {570.0f, 526.0f}, {172.0f, 40.0f}, "Replay Mission 1", font);
    Button storyMissionSelectBackButton({58.0f, 526.0f}, {112.0f, 40.0f}, "Back", font);
    Button storyMissionPreviousPageButton({188.0f, 526.0f}, {94.0f, 40.0f}, "Prev Page", font);
    Button storyMissionNextPageButton({292.0f, 526.0f}, {94.0f, 40.0f}, "Next Page", font);
    Button storyRestartButton(
        {GamePlayerBannerLeftX + 12.0f, 198.0f}, {156.0f, 32.0f}, "Restart Mission", font);
    storyContinueButton.setVariant(ButtonVariant::Primary);
    storyContinueButton.setLabelSize(type::Subheading);
    storySkipDrillButton.setVariant(ButtonVariant::Secondary);
    storySkipDrillButton.setLabelSize(type::Body);
    storyPopupPreviousButton.setVariant(ButtonVariant::Secondary);
    storyPopupPreviousButton.setLabelSize(type::Subheading);
    storyPopupContinueButton.setVariant(ButtonVariant::Primary);
    storyPopupContinueButton.setLabelSize(type::Subheading);
    storyBackButton.setVariant(ButtonVariant::Quiet);
    storyBlackthornButton.setVariant(ButtonVariant::Primary);
    storyBlackthornButton.setLabelSize(type::Body);
    storyMirewatchButton.setVariant(ButtonVariant::Primary);
    storyMirewatchButton.setLabelSize(type::Body);
    storySeelieButton.setVariant(ButtonVariant::Primary);
    storySeelieButton.setLabelSize(type::Body);
    storySpoilerMirewatchButton.setVariant(ButtonVariant::Primary);
    storySpoilerMirewatchButton.setLabelSize(type::Body);
    storySpoilerContinueButton.setVariant(ButtonVariant::Secondary);
    storySpoilerContinueButton.setLabelSize(type::Body);
    storySelectBackButton.setVariant(ButtonVariant::Quiet);
    for (Button& button : storyMissionButtons)
    {
        button.setLabelSize(type::Body);
    }
    storyRestartCampaignButton.setVariant(ButtonVariant::Quiet);
    storyRestartCampaignButton.setLabelSize(type::Caption);
    storyMissionSelectBackButton.setVariant(ButtonVariant::Quiet);
    storyMissionPreviousPageButton.setVariant(ButtonVariant::Quiet);
    storyMissionNextPageButton.setVariant(ButtonVariant::Quiet);
    storyMissionPreviousPageButton.setLabelSize(type::Caption);
    storyMissionNextPageButton.setLabelSize(type::Caption);
    storyRestartButton.setVariant(ButtonVariant::Quiet);
    storyRestartButton.setLabelSize(type::Caption);
    Button cancelResignButton({250.0f, 356.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmResignButton({420.0f, 356.0f}, {130.0f, 42.0f}, "Resign", font);
    cancelResignButton.setVariant(ButtonVariant::Quiet);
    confirmResignButton.setVariant(ButtonVariant::Danger);
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
            // The action row sits under the panels it belongs to: roster verbs on
            // the left, the verbs that act on the selected deck under its portrait.
            newDeckButton.shape.setSize({112.0f, 38.0f});
            refreshDeckButton.shape.setSize({112.0f, 38.0f});
            // Starter decks cannot be created or deleted, so Edit takes the
            // detail column on its own.
            editDeckButton.shape.setSize({starterDeckMode ? DeckDetailPanelWidth : 184.0f, 38.0f});
            deleteDeckButton.shape.setSize({184.0f, 38.0f});
            newDeckButton.setPosition({DeckPickerPanelX, 510.0f});
            refreshDeckButton.setPosition({DeckPickerPanelX + 122.0f, 510.0f});
            editDeckButton.setPosition({DeckDetailPanelX, 510.0f});
            deleteDeckButton.setPosition({DeckDetailPanelX + 200.0f, 510.0f});
        }
        else
        {
            removeCardButton.shape.setSize({120.0f, 38.0f});
            addCardButton.shape.setSize({110.0f, 38.0f});
            saveDeckButton.shape.setSize({124.0f, 38.0f});
            deckNameInput.setPosition({DeckCardsX, 112.0f});
            removeCardButton.setPosition({DeckCardsX, 510.0f});
            addCardButton.setPosition({LibraryX, 510.0f});
            saveDeckButton.setPosition({652.0f, 510.0f});
        }
    };

    auto layoutAuthenticatedButtons = [&]() {
        // Three tiers instead of seven identical plates. Guided Story is the
        // first-time route and therefore the only wide plate; the four modes
        // below share one rhythm. Admin and Log Out are a quiet footer pair.
        constexpr float centerX = 400.0f;
        constexpr float primaryWidth = 264.0f;
        constexpr float primaryHeight = 62.0f;
        constexpr float secondaryWidth = 214.0f;
        constexpr float secondaryHeight = 46.0f;
        constexpr float footerWidth = 104.0f;
        constexpr float footerHeight = 32.0f;

        storyButton.setVariant(ButtonVariant::Primary);
        storyButton.setSize({primaryWidth, primaryHeight});
        storyButton.setPosition({centerX - primaryWidth * 0.5f, 168.0f});

        float y = 256.0f;
        auto placeSecondary = [&](Button& button) {
            button.setVariant(ButtonVariant::Secondary);
            button.setSize({secondaryWidth, secondaryHeight});
            button.setPosition({centerX - secondaryWidth * 0.5f, y});
            y += secondaryHeight + 13.0f;
        };

        placeSecondary(playButton);
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
        const bool focused = button.focused;
        const bool active = button.hovered || focused;
        const float pressOffset = button.pressed ? 1.0f : 0.0f;

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
                sf::Color(232, 168, 76, active ? 62 : 34));
        }

        // Scale button_blank slightly past the hitbox so the plaque reads larger.
        const float bgPadX = primary ? 20.0f : 14.0f;
        const float bgPadY = primary ? 11.0f : 8.0f;
        drawTextureRectContain(
            window,
            *mainMenuButtonTexture,
            sf::IntRect({48, 312}, {1435, 306}),
            {
                {position.x - bgPadX, position.y - bgPadY + pressOffset},
                {size.x + bgPadX * 2.0f, size.y + bgPadY * 2.0f}},
            button.pressed
                ? sf::Color(214, 202, 184)
                : active
                    ? sf::Color::White
                    : sf::Color(primary ? 244 : 226, primary ? 244 : 226, primary ? 244 : 226));

        if (focused)
        {
            drawFocusRing(
                window,
                {position.x - bgPadX - 3.0f, position.y - bgPadY - 3.0f},
                {size.x + bgPadX * 2.0f + 6.0f, size.y + bgPadY * 2.0f + 6.0f},
                11.0f,
                animationTime);
        }

        const float iconSize = primary ? 30.0f : 23.0f;
        const float iconLeft = position.x + (primary ? 24.0f : 18.0f);
        drawMainMenuTextureContained(
            iconTexture,
            {iconLeft, position.y + (size.y - iconSize) * 0.5f + pressOffset},
            {iconSize, iconSize},
            active ? sf::Color::White : sf::Color(235, 225, 202));

        // Menu labels take the display face: a main menu is exactly where
        // display type belongs, and it separates navigation from body copy.
        sf::Text label(
            displayFontOr(font),
            button.text.getString(),
            primary ? 27u : 19u);
        label.setFillColor(active ? palette::InkBright : palette::Ink);
        label.setOutlineThickness(primary ? 1.0f : 0.0f);
        label.setOutlineColor(sf::Color(58, 33, 14, 190));
        // The ornate texture has a tall crest above its inset face, so its text
        // needs a lower anchor than the geometrically centred metal buttons.
        const float faceCenterOffsetY = primary ? 4.0f : 3.0f;
        centerButtonText(
            label,
            {center.x + (primary ? 12.0f : 8.0f), center.y + faceCenterOffsetY + pressOffset});
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
        const bool compactBadge = window.getView().getSize().x <=
            ui_canvas::LegacyWidth + 0.5f;
        const float BadgeLeft = compactBadge ? 12.0f : -122.0f;
        const sf::Vector2f BadgePosition{BadgeLeft, 14.0f};
        // The role tag lives inside the plate, so the badge grows a row to hold
        // it. Hanging it underneath read as an element that had escaped its
        // container.
        // Leave enough room for the Wood leaf and future longer league names
        // without letting the rank row touch the plate's right border.
        const float BadgeWidth = compactBadge ? 166.0f : 210.0f;
        const sf::Vector2f BadgeSize{BadgeWidth, loggedInIsAdmin ? 136.0f : 112.0f};

        PlateStyle badge;
        badge.fill = sf::Color(11, 16, 17, 226);
        badge.frame = palette::Brass;
        badge.cut = 13.0f;
        badge.rivets = false;
        drawMaterialPlate(window, BadgePosition, BadgeSize, badge);

        // ---- portrait -----------------------------------------------------
        const sf::Vector2f PortraitCenter{BadgeLeft + (compactBadge ? 30.0f : 36.0f), 55.0f};
        const float PortraitRadius = compactBadge ? 22.0f : 25.0f;
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
        const float AvatarRadius = compactBadge ? 18.5f : 21.5f;
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
        const float TextLeft = BadgeLeft + (compactBadge ? 58.0f : 70.0f);
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
            // metadata, not part of what the player calls themselves. It sits on
            // its own row inside the plate, clear of the rank and the rating.
            const sf::Vector2f tagSize{54.0f, 15.0f};
            const sf::Vector2f tagPosition{
                TextLeft,
                BadgePosition.y + BadgeSize.y - 15.0f - 9.0f};
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
        // Spans the full text column: at 78px the numerals were squeezed into
        // ~48px after the coin inset and read as an afterthought next to the
        // rating above them.
        // Clear of the rating numerals above: at y = 80 the pill's top edge cut
        // through their descenders.
        const sf::Vector2f coinPill{TextLeft, 87.0f};
        const sf::Vector2f pillSize{column, 22.0f};
        drawValuePill(
            window,
            font,
            coinPill,
            pillSize,
            std::to_string(playerCoins),
            palette::Gold,
            20.0f);
        drawCoinIcon({coinPill.x + 3.0f, coinPill.y + 3.0f}, 8.0f);
    };

    // A tracked-caps build mark on a hairline, the way a shipping client marks
    // itself, rather than bare text in the corner.
    auto drawBuildStamp = [&]() {
        const float StampRight =
            window.getView().getCenter().x + window.getView().getSize().x * 0.5f - 11.0f;
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

        const bool compactChrome = window.getView().getSize().x <=
            ui_canvas::LegacyWidth + 0.5f;
        const float settingsX = compactChrome ? 704.0f : 817.0f;
        drawMainMenuTextureContained(
            mainMenuSmallHexTexture,
            {settingsX, 14.0f},
            {36.0f, 38.0f},
            authenticatedSettingsHovered ? sf::Color::White : sf::Color(225, 218, 202));
        drawMainMenuTextureContained(
            mainMenuSettingsTexture,
            {settingsX + 8.0f, 22.0f},
            {20.0f, 20.0f},
            authenticatedSettingsHovered ? sf::Color::White : sf::Color(235, 225, 202));

        drawBuildStamp();
    };

    auto authenticatedSettingsButtonClicked = [&](sf::Vector2f point) {
        const float settingsX = window.getView().getSize().x <=
                ui_canvas::LegacyWidth + 0.5f
            ? 704.0f
            : 817.0f;
        return isInsideRect(point, settingsX - 1.0f, 13.0f, 38.0f, 40.0f);
    };

    auto drawExitDesktopCloseButton = [&]() {
        const bool authenticatedExit = currentState == GameState::Authenticated;
        const float authenticatedExitX = window.getView().getSize().x <=
                ui_canvas::LegacyWidth + 0.5f
            ? 742.0f
            : 864.0f;
        if (currentState == GameState::Authenticated && mainMenuExitTexture)
        {
            drawMainMenuTextureStretched(
                mainMenuExitTexture,
                {authenticatedExitX, -2.0f},
                {58.0f, 90.0f},
                exitDesktopCloseHovered ? sf::Color::White : sf::Color(224, 214, 202));
            return;
        }

        const sf::Vector2f position = authenticatedExit
            ? sf::Vector2f{authenticatedExitX + 3.0f, 18.0f}
            : sf::Vector2f{724.0f, 18.0f};
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
            const float exitX = window.getView().getSize().x <=
                    ui_canvas::LegacyWidth + 0.5f
                ? 742.0f
                : 864.0f;
            return isInsideRect(point, exitX, 0.0f, 58.0f, 72.0f);
        }
        return isInsideRect(point, 724.0f, 18.0f, 52.0f, 52.0f);
    };

    auto drawExitDesktopPopup = [&]() {
        sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
        overlay.setPosition({ui_canvas::Left, 0.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 170));
        window.draw(overlay);
        drawPanel(window, {220.0f, 188.0f}, {360.0f, 220.0f});
        drawText(window, font, "Exit to Desktop?", 28, {266.0f, 218.0f}, sf::Color(248, 224, 172), 270.0f);
        drawText(window, font, "Are you sure you want to exit", 16, {260.0f, 276.0f}, sf::Color(220, 224, 230), 280.0f);
        drawText(window, font, "to desktop?", 16, {350.0f, 302.0f}, sf::Color(220, 224, 230), 120.0f);
        cancelExitDesktopButton.draw(window);
        confirmExitDesktopButton.draw(window);
    };

    auto drawResignConfirmationPopup = [&]() {
        std::string heading = "Resign Match?";
        std::string firstLine = "Are you sure you want to resign";
        std::string secondLine = "this game?";
        std::string confirmation = "Resign";
        if (gameConfirmationAction == GameConfirmationAction::ExitStory)
        {
            heading = "Exit to Missions?";
            firstLine = "Completed missions and unlocks stay saved.";
            secondLine = "This unfinished attempt will reset.";
            confirmation = "Exit";
        }
        else if (gameConfirmationAction == GameConfirmationAction::RestartStory)
        {
            heading = "Restart This Attempt?";
            firstLine = "Only the current mission attempt will reset.";
            secondLine = "Completed missions and unlocks stay saved.";
            confirmation = "Restart";
        }
        confirmResignButton.setLabel(confirmation);
        confirmResignButton.setVariant(ButtonVariant::Danger);
        cancelResignButton.setFocused(gameConfirmationKeyboardFocus == 0);
        confirmResignButton.setFocused(gameConfirmationKeyboardFocus == 1);
        sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
        overlay.setPosition({ui_canvas::Left, 0.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 182));
        window.draw(overlay);
        drawPanel(window, {ResignDialogX, ResignDialogY}, {ResignDialogWidth, ResignDialogHeight});
        drawCenteredText(
            window, font, heading, 28,
            {ResignDialogX + ResignDialogWidth * 0.5f, 236.0f},
            sf::Color(248, 224, 172));
        drawCenteredText(
            window, font, firstLine, 16,
            {ResignDialogX + ResignDialogWidth * 0.5f, 286.0f},
            sf::Color(220, 224, 230));
        drawCenteredText(
            window, font, secondLine, 16,
            {ResignDialogX + ResignDialogWidth * 0.5f, 312.0f},
            sf::Color(220, 224, 230));
        drawCenteredText(
            window,
            font,
            "LEFT / RIGHT OR TAB: CHOOSE  |  ENTER: CONFIRM  |  ESC: CANCEL",
            11,
            {ResignDialogX + ResignDialogWidth * 0.5f, 337.0f},
            sf::Color(190, 198, 214));
        cancelResignButton.draw(window);
        confirmResignButton.draw(window);
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

    auto startQueuedAudioSettingsSave = [&]() {
        if (pendingAudioSettingsSave || !queuedAudioSettingsSave || activeAccessToken.empty())
        {
            return;
        }

        const account_data::AudioSettings settings = *queuedAudioSettingsSave;
        queuedAudioSettingsSave.reset();
        pendingAudioSettingsSaveToken = activeAccessToken;
        pendingAudioSettingsSave = std::async(
            std::launch::async,
            saveAudioSettings,
            pendingAudioSettingsSaveToken,
            settings);
    };

    auto queueAudioSettingsSave = [&]() {
        audioSettingsDirty = true;
        if (!audioSettingsLoaded || activeAccessToken.empty())
        {
            return;
        }

        queuedAudioSettingsSave = audioSystem.getSettings();
        startQueuedAudioSettingsSave();
    };

    auto applyAccountState = [&](const AccountStateResult& result) {
        playerCoins = result.coins;
        playerRating = result.rating;
        playerLeague = result.league;
        loggedInIsAdmin = result.isAdmin;
        playerCollection = result.collection;
        if (!audioSettingsDirty)
        {
            audioSystem.applySettings(result.audioSettings);
        }
        audioSettingsLoaded = true;
        if (audioSettingsDirty)
        {
            queuedAudioSettingsSave = audioSystem.getSettings();
            startQueuedAudioSettingsSave();
        }
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

    auto deckCardByTitle = [&](const std::string& title) -> const card_data::Card* {
        const auto findCard = [&](const std::vector<card_data::Card>& library) -> const card_data::Card* {
            const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
                return card.title == title;
            });
            return found == library.end() ? nullptr : &*found;
        };

        if (const card_data::Card* card = findCard(cardLibrary))
        {
            return card;
        }
        return findCard(allCardLibrary);
    };

    auto deckCardSortCategory = [&](const std::string& title) {
        const card_data::Card* card = deckCardByTitle(title);
        if (!card)
        {
            return 4;
        }
        if (game_data::isHeroCard(*card))
        {
            return 0;
        }
        if (game_data::isUnitCard(*card))
        {
            return 1;
        }
        if (card->type == "Spell")
        {
            return 2;
        }
        if (card->type == "Enchantment")
        {
            return 3;
        }
        return 4;
    };

    // Deck rows show one entry per card title; copies are conveyed by the X/Y
    // count. Keep their order independent of insertion/database order so both
    // regular and starter-deck editors present the same card grouping.
    auto deckUniqueTitles = [&]() {
        std::vector<std::string> unique;
        for (const std::string& title : editingDeck.cardTitles)
        {
            if (std::find(unique.begin(), unique.end(), title) == unique.end())
            {
                unique.push_back(title);
            }
        }

        std::sort(unique.begin(), unique.end(), [&](const std::string& left, const std::string& right) {
            const int leftCategory = deckCardSortCategory(left);
            const int rightCategory = deckCardSortCategory(right);
            if (leftCategory != rightCategory)
            {
                return leftCategory < rightCategory;
            }

            const std::string leftKey = lowerKey(left);
            const std::string rightKey = lowerKey(right);
            return leftKey == rightKey ? left < right : leftKey < rightKey;
        });
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
        for (std::size_t i = 0; i < collectionTraitChips.size(); ++i)
        {
            if (collectionTraitChips[i].rect.contains(clickPos))
            {
                clearFocus();
                toggleCollectionTraitFilter(i);
                return true;
            }
        }
        return false;
    };

    auto clickCollectionTypeFilter = [&](sf::Vector2f clickPos) {
        for (std::size_t i = 0; i < collectionTypeChips.size(); ++i)
        {
            if (collectionTypeChips[i].rect.contains(clickPos))
            {
                clearFocus();
                toggleCollectionTypeFilter(i);
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
        audioSystem.applySettings(account_data::AudioSettings{});
        queuedAudioSettingsSave.reset();
        audioSettingsLoaded = false;
        audioSettingsDirty = false;
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
        resignConfirmPopupVisible = false;
        gameConfirmationAction = GameConfirmationAction::Resign;
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
        pendingPieceActionChoice.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        sandboxMode = false;
        storyMode = false;
        storyEngine.reset();
        storyAiPending = false;
        storyStage = StoryStage::None;
        storyComicPage = 0;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        sandboxPlacementPlayer = 1;
        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        foresightChoiceRowOffset = 0;
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
        authenticatedMenuFocus = -1;
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

    // The Add Card dialog sizes itself to the number of suggestions on show, so
    // its footer moves. Both the draw and the click path resolve the button row
    // through this, otherwise the hit regions trail the drawn buttons by a frame
    // as the player types.
    auto layoutAddCardPopupButtons = [&]() {
        const bool showsMessage = pendingAdminCardListLoad || !adminCardLoadError.empty() ||
            visibleAdminCardTitles().empty();
        const float listHeight = showsMessage
            ? 30.0f
            : static_cast<float>(visibleAdminCardTitles().size()) * AdminCardRowHeight;
        const float buttonsY = AdminCardRowY + listHeight + 18.0f;
        cancelAddCardButton.setPosition({246.0f, buttonsY});
        confirmAddCardButton.setPosition({408.0f, buttonsY});
        return buttonsY;
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
        abilityButton.setPosition({GameActionButtonX, GameAbilityButtonY});
        endTurnButton.setLabel("End Turn");
        leaveGameButton.setLabel(isConquestBattle ? "Map" : "Resign");
        leaveGameButton.setLabelSize(type::Body);
        leaveGameButton.setPosition({GameActionButtonX, GameLeaveButtonY});
        leaveGameButton.setSize({GameLeaveButtonWidth, GameActionButtonHeight});
        resignConfirmPopupVisible = false;
        gameConfirmationAction = GameConfirmationAction::Resign;
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();

        haveSnapshot = false;
        sandboxMode = false;
        storyMode = false;
        storyEngine.reset();
        storyAiPending = false;
        storyStage = StoryStage::None;
        storyComicPage = 0;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        sandboxPlacementPlayer = 1;
        gameHandOffset = 0;
        foresightChoiceRowOffset = 0;
        nextSandboxPieceId = 1;
        gameSnapshot = {};
        gameSnapshotReceivedAt = {};
        clockWarningTracker.reset();
        displayedClockWarning.reset();
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        pendingPieceActionChoice.reset();
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

        // Carry the rank context into the queue: the player can see what the
        // search is trying to match without leaving the screen to inspect the
        // profile badge.
        const sf::Color playerAccent = leagueAccent(playerLeague);
        drawLeagueSigil({centerX - 112.0f, slotY + 84.0f}, 6.0f, playerAccent);
        drawCenteredText(
            window,
            font,
            std::string(ranking::leagueName(playerLeague)) + " " + std::to_string(playerRating),
            type::Caption,
            {centerX - 72.0f, slotY + 84.0f},
            playerAccent);

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
            "Searching near your " + std::to_string(playerRating) + " rating.",
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

    // Everything the collection presentation kit needs. The display face carries
    // headings so the screens have a type hierarchy beyond Roboto at four sizes.
    const UiContext collectionUi{
        window,
        font,
        gloomthornFontLoaded ? gloomthornFont : font,
        textures};

    // The draw lambdas are defined before the frame loop's mouse position exists,
    // so keep the current logical pointer here for draw-time hover states. This
    // also lets UI captures supply their pinned hover point.
    sf::Vector2f currentPointer;
    auto collectionPointer = [&]() {
        return currentPointer;
    };

    // Index of the list row under the pointer, for hover treatment.
    auto hoveredRow = [&](float x, float y, float width, float rowHeight,
                          std::size_t visibleRows, std::size_t offset, std::size_t totalRows) {
        return rowIndexAt(collectionPointer(), x, y, width, rowHeight, visibleRows, offset, totalRows);
    };

    // Deck rows and the inspect popup need art for cards the player may not own,
    // so they fall back to the full catalogue. Defined here rather than beside the
    // other card helpers because every collection screen below needs it.
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

    auto deckSummaryFor = [&](const deck_data::Deck& deck) {
        // Prefer the full catalogue so a deck's art and curve still resolve when
        // the player does not own every card in it.
        return summarizeDeck(deck, allCardLibrary.empty() ? cardLibrary : allCardLibrary);
    };

    // A thin track that appears only when a list actually overflows.
    auto drawListScrollTrack = [&](float x, float y, float height,
                                   std::size_t offset, std::size_t visibleRows, std::size_t totalRows) {
        if (totalRows <= visibleRows)
        {
            return;
        }
        sf::RectangleShape track({3.0f, height});
        track.setPosition({x, y});
        track.setFillColor(sf::Color(0, 0, 0, 170));
        window.draw(track);

        const float ratio = static_cast<float>(visibleRows) / static_cast<float>(totalRows);
        const float thumbHeight = std::max(20.0f, height * ratio);
        const float travel = height - thumbHeight;
        const float progress = static_cast<float>(offset) / static_cast<float>(totalRows - visibleRows);
        sf::RectangleShape thumb({3.0f, thumbHeight});
        thumb.setPosition({x, y + travel * progress});
        thumb.setFillColor(sf::Color(198, 146, 70, 225));
        window.draw(thumb);
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

    auto addResourceNumber = [&](int playerNumber, int value, int /*displayedResources*/) {
        if (value == 0)
        {
            return;
        }
        const std::string effectText = (value > 0 ? "+" : "") + std::to_string(value);
        const sf::Text floatingValue(font, effectText, 20);
        // Float the delta off the resources figure inside that player's banner.
        const float pipCenterX = gamePlayerBannerX(window, playerNumber) + 61.0f;
        const float x = pipCenterX - floatingValue.getLocalBounds().size.x * 0.5f;
        floatingNumberEffects.push_back({
            0,
            0,
            {x, GameTopBarY + GamePlayerBannerHeight + 2.0f},
            false,
            effectText,
            value > 0 ? sf::Color(146, 232, 166) : sf::Color(233, 128, 106),
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
        if (!EnableFidgetAnimations || piece.fidgetAnimPath.empty())
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
        if (storyMode && storyStage != StoryStage::Objective)
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
        const bool normalActionSpent =
            playerNumber >= 1 && playerNumber <= 2 &&
            gameSnapshot.players[static_cast<std::size_t>(playerNumber - 1)]
                .pieceActionUsedThisTurn;
        if (!sandboxMode && normalActionSpent && piece.repeatActionIndex < 0)
        {
            return false;
        }
        if (storyMode)
        {
            return piece.owner == playerNumber && !piece.hasActed &&
                piece.growTurnsRemaining <= 0 && piece.disabledTurns <= 0;
        }
        return sandboxMode || (piece.owner == playerNumber && !piece.hasActed);
    };

    auto pieceCanTakeGameAction = [&](const game_data::Piece& piece) {
        return pieceCanTakeTurnAction(piece, gameSnapshot.yourPlayer);
    };

    auto updatePieceFidgetAnimations = [&]() {
        if (!EnableFidgetAnimations)
        {
            pieceFidgetAnimations.clear();
            return;
        }

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

    auto commitLocalSnapshot = [&](game_data::Snapshot nextSnapshot) {
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

    auto commitSandboxSnapshot = [&](game_data::Snapshot nextSnapshot) {
        recomputeSandboxControl(nextSnapshot);
        refreshSandboxPlayerSnapshots(nextSnapshot);
        commitLocalSnapshot(std::move(nextSnapshot));
    };

    const auto storyProgressIndex = [](StoryCampaign campaign) {
        switch (campaign)
        {
        case StoryCampaign::Blackthorn: return std::size_t{0};
        case StoryCampaign::Mirewatch: return std::size_t{1};
        case StoryCampaign::Seelie: return std::size_t{2};
        }
        return std::size_t{0};
    };

    const auto refreshStoryCampaignProgress = [&](StoryCampaign campaign) {
        const std::size_t index = storyProgressIndex(campaign);
        storyCampaignProgressDetails[index] =
            loadStoryProgress(loggedInUsername, campaign);
        storyCampaignProgress[index] =
            storyCampaignProgressDetails[index].advancedCount;
    };

    const auto storyCardsReady = [&]() {
        // cardLibrary is the player's owned subset. Story missions may use any
        // faction's units, so only the full card-server catalog is sufficient.
        return captureRequest || !allCardLibrary.empty();
    };

    const auto requestStoryCards = [&]() {
        if (captureRequest || storyCardsReady() || pendingStoryCardLoad)
        {
            return;
        }
        storyBlackthornButton.setEnabled(false);
        storyMirewatchButton.setEnabled(false);
        storySeelieButton.setEnabled(false);
        setMessageY(messageText, 556.0f);
        setMessage(messageText, "Loading authoritative cards...", sf::Color::Yellow);
        pendingStoryCardLoad = std::async(std::launch::async, fetchCards);
    };

    auto showStorySelect = [&]() {
        currentState = GameState::StorySelect;
        refreshStoryCampaignProgress(StoryCampaign::Blackthorn);
        refreshStoryCampaignProgress(StoryCampaign::Mirewatch);
        refreshStoryCampaignProgress(StoryCampaign::Seelie);
        storySpoilerConfirmationVisible = false;
        storySpoilerKeyboardFocus = 0;
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 556.0f);
        if (pendingStoryCardLoad)
        {
            setMessage(messageText, "Loading authoritative cards...", sf::Color::Yellow);
        }
        else
        {
            setMessage(messageText, "", sf::Color::White);
        }
        const bool cardsSelectable = !pendingStoryCardLoad.has_value();
        storyBlackthornButton.setEnabled(cardsSelectable);
        storyMirewatchButton.setEnabled(cardsSelectable);
        storySeelieButton.setEnabled(cardsSelectable);
        storySelectKeyboardFocus = 1;
        storyKeyboardNavigationActive = true;
        requestStoryCards();
        clearFocus();
    };

    auto showStoryMissionSelect = [&](StoryCampaign campaign) {
        storyCampaign = campaign;
        refreshStoryCampaignProgress(storyCampaign);
        storyCompletedCount =
            storyCampaignProgress[storyProgressIndex(storyCampaign)];
        storySpoilerConfirmationVisible = false;
        const int missionCount = static_cast<int>(storyMissions(storyCampaign).size());
        const int currentMission = missionCount > 0
            ? std::min(storyCompletedCount, missionCount - 1)
            : 0;
        storyMissionPage = currentMission / StoryMissionPageSize;
        storyMissionKeyboardFocus = currentMission % StoryMissionPageSize;
        storyKeyboardNavigationActive = true;
        currentState = GameState::StoryMissionSelect;
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::White);
        clearFocus();
    };

    auto showStoryIntro = [&](int requestedMission = -1) {
        currentState = GameState::StoryIntro;
        storyCompletedCount = loadStoryCompletedCount(loggedInUsername, storyCampaign);
        const int lastMission = static_cast<int>(storyMissions(storyCampaign).size()) - 1;
        storyMissionIndex = requestedMission >= 0
            ? std::clamp(requestedMission, 0, lastMission)
            : std::min(storyCompletedCount, lastMission);
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 560.0f);
        setMessage(messageText, "", sf::Color::White);
        storyComicPage = 0;
        storyGenuineDefeatCount = 0;
        storyIntroKeyboardFocus = 2;
        storyKeyboardNavigationActive = true;
        clearFocus();
    };

    auto requestStoryCampaignSelection = [&](StoryCampaign campaign) {
        const int mirewatchMissionCount =
            static_cast<int>(storyMissions(StoryCampaign::Mirewatch).size());
        if (storyRequiresSeelieSpoilerConfirmation(
                campaign,
                storyCampaignProgress[storyProgressIndex(StoryCampaign::Mirewatch)],
                mirewatchMissionCount))
        {
            storySpoilerConfirmationVisible = true;
            storySpoilerKeyboardFocus = 0;
            storyKeyboardNavigationActive = true;
            return;
        }
        showStoryMissionSelect(campaign);
    };

    auto activateStorySpoilerChoice = [&](bool continueAnyway) {
        playButtonClickSound();
        storySpoilerConfirmationVisible = false;
        showStoryMissionSelect(
            continueAnyway ? StoryCampaign::Seelie : StoryCampaign::Mirewatch);
    };

    bool captureValidationFailed = false;
    std::string captureValidationScreen;
    const auto failCaptureValidation = [&](std::string message) {
        captureValidationFailed = true;
        storyCorrection = message;
        gameSnapshot.status = message;
        fmt::println(
            "[UI capture validation failure]{}{}",
            captureValidationScreen.empty()
                ? std::string{}
                : " [" + captureValidationScreen + "] ",
            message);
    };

    const auto activeStoryMission = [&]() -> const StoryMission& {
        return storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
    };

    const auto activeStoryCatchUpMayBeSkipped = [&]() {
        return storyMasteryCatchUpMayBeSkipped(
            storyMissions(storyCampaign),
            storyMissionIndex,
            storyCampaignProgressDetails[storyProgressIndex(storyCampaign)]);
    };

    const auto storyMissionIndexById = [&](StoryCampaign campaign, std::string_view id) {
        const std::span<const StoryMission> missions = storyMissions(campaign);
        const auto found = std::find_if(
            missions.begin(), missions.end(),
            [&](const StoryMission& mission) { return mission.id == id; });
        if (found == missions.end())
        {
            failCaptureValidation(
                "Capture setup error: unknown Story mission id '" +
                std::string(id) + "'.");
            return 0;
        }
        return static_cast<int>(std::distance(missions.begin(), found));
    };

    const auto storyTacticalMissionIndex = [&](StoryCampaign campaign, int ordinal) {
        const std::span<const StoryMission> missions = storyMissions(campaign);
        if (ordinal < 0)
        {
            failCaptureValidation(
                "Capture setup error: tactical Story ordinal must be positive.");
            return 0;
        }
        int tactical = 0;
        for (std::size_t index = 0; index < missions.size(); ++index)
        {
            if (missions[index].objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                continue;
            }
            if (tactical == ordinal)
            {
                return static_cast<int>(index);
            }
            ++tactical;
        }
        failCaptureValidation(
            "Capture setup error: tactical Story ordinal " +
            std::to_string(ordinal + 1) + " does not exist.");
        return 0;
    };

    enum class StoryPageCaptureKind
    {
        Briefing,
        ActionStep,
        BeforeStep,
        Aftermath
    };
    struct StoryPageCaptureTarget
    {
        StoryCampaign campaign = StoryCampaign::Blackthorn;
        int missionIndex = 0;
        StoryPageCaptureKind kind = StoryPageCaptureKind::Briefing;
        std::size_t stepIndex = 0;
        std::size_t panelIndex = 0;
    };
    struct StoryActionCaptureInvariant
    {
        StoryCampaign campaign = StoryCampaign::Blackthorn;
        int missionIndex = 0;
        std::size_t stepIndex = 0;
        bool openObjective = false;
        int completedCount = 0;
        int campaignProgress = 0;
        game_data::Phase phase = game_data::Phase::HeroPlacement;
        int activeOwner = 0;
    };
    std::optional<StoryActionCaptureInvariant> storyActionCaptureInvariant;
    static constexpr std::array<StoryCampaign, 3> CaptureStoryCampaigns = {
        StoryCampaign::Mirewatch,
        StoryCampaign::Blackthorn,
        StoryCampaign::Seelie};
    const auto storyCaptureCampaignKey =
        [](StoryCampaign campaign) -> std::string_view {
            switch (campaign)
            {
            case StoryCampaign::Mirewatch: return "mw";
            case StoryCampaign::Blackthorn: return "bt";
            case StoryCampaign::Seelie: return "se";
            }
            return "unknown";
        };
    const auto storyCaptureOrdinal = [](std::size_t index) {
        std::string result = std::to_string(index + 1);
        if (result.size() < 2)
        {
            result.insert(result.begin(), '0');
        }
        return result;
    };
    const auto storyBriefingPageCaptureKey =
        [&](StoryCampaign campaign,
            const StoryMission& mission,
            std::size_t panelIndex) {
            return std::string("story-page-briefing-") +
                std::string(storyCaptureCampaignKey(campaign)) + "-" +
                std::string(mission.id) + "-p" +
                storyCaptureOrdinal(panelIndex);
        };
    const auto storyBeforeStepPageCaptureKey =
        [&](StoryCampaign campaign,
            const StoryMission& mission,
            std::size_t stepIndex,
            std::size_t panelIndex) {
            return std::string("story-page-beat-before-") +
                std::string(storyCaptureCampaignKey(campaign)) + "-" +
                std::string(mission.id) + "-s" +
                storyCaptureOrdinal(stepIndex) + "-p" +
                storyCaptureOrdinal(panelIndex);
        };
    const auto storyActionPageCaptureKey =
        [&](StoryCampaign campaign,
            const StoryMission& mission,
            std::size_t stepIndex) {
            return std::string("story-page-action-") +
                std::string(storyCaptureCampaignKey(campaign)) + "-" +
                std::string(mission.id) + "-s" +
                storyCaptureOrdinal(stepIndex);
        };
    const auto storyAftermathPageCaptureKey =
        [&](StoryCampaign campaign,
            const StoryMission& mission,
            std::size_t panelIndex) {
            return std::string("story-page-beat-aftermath-") +
                std::string(storyCaptureCampaignKey(campaign)) + "-" +
                std::string(mission.id) + "-p" +
                storyCaptureOrdinal(panelIndex);
        };
    const auto storyPageCaptureForKey =
        [&](std::string_view key) -> std::optional<StoryPageCaptureTarget> {
            for (StoryCampaign campaign : CaptureStoryCampaigns)
            {
                const std::span<const StoryMission> missions =
                    storyMissions(campaign);
                for (std::size_t missionIndex = 0;
                     missionIndex < missions.size();
                     ++missionIndex)
                {
                    const StoryMission& mission = missions[missionIndex];
                    for (std::size_t panelIndex = 0;
                         panelIndex < mission.briefing.size();
                         ++panelIndex)
                    {
                        if (key == storyBriefingPageCaptureKey(
                                       campaign, mission, panelIndex))
                        {
                            return StoryPageCaptureTarget{
                                campaign,
                                static_cast<int>(missionIndex),
                                StoryPageCaptureKind::Briefing,
                                0,
                                panelIndex};
                        }
                    }

                    if (mission.objectiveSpec.kind ==
                        StoryObjectiveKind::StoryOnly)
                    {
                        continue;
                    }
                    const std::size_t actionCount = mission.script.empty()
                        ? 1
                        : mission.script.size();
                    for (std::size_t stepIndex = 0;
                         stepIndex < actionCount;
                         ++stepIndex)
                    {
                        if (key == storyActionPageCaptureKey(
                                       campaign, mission, stepIndex))
                        {
                            return StoryPageCaptureTarget{
                                campaign,
                                static_cast<int>(missionIndex),
                                StoryPageCaptureKind::ActionStep,
                                stepIndex,
                                0};
                        }
                    }
                    for (std::size_t stepIndex = 0;
                         stepIndex < mission.script.size();
                         ++stepIndex)
                    {
                        const StoryScriptAction& step =
                            mission.script[stepIndex];
                        for (std::size_t panelIndex = 0;
                             panelIndex < step.panelsBefore.size();
                             ++panelIndex)
                        {
                            if (key == storyBeforeStepPageCaptureKey(
                                           campaign,
                                           mission,
                                           stepIndex,
                                           panelIndex))
                            {
                                return StoryPageCaptureTarget{
                                    campaign,
                                    static_cast<int>(missionIndex),
                                    StoryPageCaptureKind::BeforeStep,
                                    stepIndex,
                                    panelIndex};
                            }
                        }
                    }
                    for (std::size_t panelIndex = 0;
                         panelIndex < mission.aftermath.size();
                         ++panelIndex)
                    {
                        if (key == storyAftermathPageCaptureKey(
                                       campaign, mission, panelIndex))
                        {
                            return StoryPageCaptureTarget{
                                campaign,
                                static_cast<int>(missionIndex),
                                StoryPageCaptureKind::Aftermath,
                                0,
                                panelIndex};
                        }
                    }
                }
            }
            return std::nullopt;
        };

    if (captureRequest)
    {
        std::vector<std::string> sortedCaptureKeys = ui_capture::knownScreens();
        std::sort(sortedCaptureKeys.begin(), sortedCaptureKeys.end());
        if (std::adjacent_find(
                sortedCaptureKeys.begin(), sortedCaptureKeys.end()) !=
            sortedCaptureKeys.end())
        {
            failCaptureValidation(
                "Capture registry error: screen keys must be globally unique.");
        }

        const auto validateTacticalCaptureCoverage = [&](StoryCampaign campaign,
                                                         std::string_view prefix) {
            const auto& registered = ui_capture::knownScreens();
            std::size_t ordinal = 0;
            for (const StoryMission& mission : storyMissions(campaign))
            {
                if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
                {
                    continue;
                }
                ++ordinal;
                const std::string base =
                    std::string(prefix) + std::to_string(ordinal);
                const std::string key = base + "-board";
                if (std::find(registered.begin(), registered.end(), key) ==
                    registered.end())
                {
                    failCaptureValidation(
                        "Capture registry error: tactical Story mission '" +
                        std::string(mission.id) + "' lacks registered screen '" + key +
                        "'. Every tactical mission requires clean-board evidence.");
                }
            }
        };
        validateTacticalCaptureCoverage(StoryCampaign::Blackthorn, "story-game-");
        validateTacticalCaptureCoverage(
            StoryCampaign::Mirewatch, "story-mirewatch-game-");
        validateTacticalCaptureCoverage(StoryCampaign::Seelie, "story-seelie-game-");

        const auto validateScenarioArtCaptureCoverage =
            [&](StoryCampaign campaign, std::string_view prefix) {
                const auto& registered = ui_capture::knownScreens();
                for (const StoryMission& mission : storyMissions(campaign))
                {
                    const std::string key =
                        std::string(prefix) + std::string(mission.id);
                    if (std::find(registered.begin(), registered.end(), key) ==
                        registered.end())
                    {
                        failCaptureValidation(
                            "Capture registry error: Story mission '" +
                            std::string(mission.id) +
                            "' lacks registered in-layout scenario-art screen '" +
                            key + "'.");
                    }
                }
            };
        validateScenarioArtCaptureCoverage(
            StoryCampaign::Mirewatch, "story-art-mw-");
        validateScenarioArtCaptureCoverage(
            StoryCampaign::Blackthorn, "story-art-bt-");
        validateScenarioArtCaptureCoverage(
            StoryCampaign::Seelie, "story-art-se-");

        const auto validateScenarioArtPopupCaptureCoverage =
            [&](StoryCampaign campaign, std::string_view prefix) {
                const auto& registered = ui_capture::knownScreens();
                for (const StoryMission& mission : storyMissions(campaign))
                {
                    if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
                    {
                        continue;
                    }
                    const std::string key =
                        std::string(prefix) + std::string(mission.id);
                    if (std::find(registered.begin(), registered.end(), key) ==
                        registered.end())
                    {
                        failCaptureValidation(
                            "Capture registry error: tactical Story mission '" +
                            std::string(mission.id) +
                            "' lacks registered in-mission scenario-art popup '" +
                            key + "'.");
                    }
                }
            };
        validateScenarioArtPopupCaptureCoverage(
            StoryCampaign::Mirewatch, "story-art-popup-mw-");
        validateScenarioArtPopupCaptureCoverage(
            StoryCampaign::Blackthorn, "story-art-popup-bt-");
        validateScenarioArtPopupCaptureCoverage(
            StoryCampaign::Seelie, "story-art-popup-se-");

        std::vector<std::string> expectedStoryPageKeys;
        std::size_t expectedStoryActionPageCount = 0;
        for (StoryCampaign campaign : CaptureStoryCampaigns)
        {
            for (const StoryMission& mission : storyMissions(campaign))
            {
                for (std::size_t panelIndex = 0;
                     panelIndex < mission.briefing.size();
                     ++panelIndex)
                {
                    expectedStoryPageKeys.push_back(
                        storyBriefingPageCaptureKey(
                            campaign, mission, panelIndex));
                }
                if (mission.objectiveSpec.kind ==
                    StoryObjectiveKind::StoryOnly)
                {
                    continue;
                }
                const std::size_t actionCount = mission.script.empty()
                    ? 1
                    : mission.script.size();
                for (std::size_t stepIndex = 0;
                     stepIndex < actionCount;
                     ++stepIndex)
                {
                    ++expectedStoryActionPageCount;
                    expectedStoryPageKeys.push_back(
                        storyActionPageCaptureKey(
                            campaign, mission, stepIndex));
                }
                for (std::size_t stepIndex = 0;
                     stepIndex < mission.script.size();
                     ++stepIndex)
                {
                    const StoryScriptAction& step = mission.script[stepIndex];
                    for (std::size_t panelIndex = 0;
                         panelIndex < step.panelsBefore.size();
                         ++panelIndex)
                    {
                        expectedStoryPageKeys.push_back(
                            storyBeforeStepPageCaptureKey(
                                campaign,
                                mission,
                                stepIndex,
                                panelIndex));
                    }
                }
                for (std::size_t panelIndex = 0;
                     panelIndex < mission.aftermath.size();
                     ++panelIndex)
                {
                    expectedStoryPageKeys.push_back(
                        storyAftermathPageCaptureKey(
                            campaign, mission, panelIndex));
                }
            }
        }
        const auto& registeredCaptureKeys = ui_capture::knownScreens();
        const std::size_t registeredStoryActionPageCount =
            static_cast<std::size_t>(std::count_if(
                registeredCaptureKeys.begin(),
                registeredCaptureKeys.end(),
                [](const std::string& key) {
                    return key.rfind("story-page-action-", 0) == 0;
                }));
        if (registeredStoryActionPageCount != expectedStoryActionPageCount)
        {
            failCaptureValidation(
                "Capture registry error: registered Story action-page count " +
                std::to_string(registeredStoryActionPageCount) +
                " does not match authored tactical action-state count " +
                std::to_string(expectedStoryActionPageCount) + ".");
        }
        for (const std::string& key : expectedStoryPageKeys)
        {
            if (std::find(
                    registeredCaptureKeys.begin(),
                    registeredCaptureKeys.end(),
                    key) == registeredCaptureKeys.end())
            {
                failCaptureValidation(
                    "Capture registry error: expected Story page screen '" +
                    key + "' is not registered.");
            }
        }
        for (const std::string& key : registeredCaptureKeys)
        {
            if (key.rfind("story-page-", 0) == 0 &&
                !storyPageCaptureForKey(key))
            {
                failCaptureValidation(
                    "Capture registry error: unexpected or stale Story page "
                    "screen '" + key + "' is registered.");
            }
        }
        if (std::find(
                ui_capture::knownScreens().begin(),
                ui_capture::knownScreens().end(),
                "story-blackthorn-open-mastery-choice") ==
            ui_capture::knownScreens().end())
        {
            failCaptureValidation(
                "Capture registry error: the optional timed ordinary match "
                "requires pre-clock choice-screen evidence.");
        }
        if (std::find(
                ui_capture::knownScreens().begin(),
                ui_capture::knownScreens().end(),
                "story-blackthorn-synthesis-bypass") ==
            ui_capture::knownScreens().end())
        {
            failCaptureValidation(
                "Capture registry error: the completed-drill path requires "
                "dedicated synthesis-bypass choice evidence.");
        }
    }

    auto completeStoryMission = [&](game_data::Snapshot& snapshot) {
        if (!storyMode || !storyEngine ||
            !storyScriptProgressAllowsCompletion(
                activeStoryMission(), storyMissionStep) ||
            !storyCompletionMayBeAwarded(
                storyStage == StoryStage::Objective,
                storyEngine->scenarioObjectiveProgress().failed,
                snapshot.winner))
        {
            return;
        }
        storyStage = StoryStage::Complete;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        storyAiPending = false;
        storyCompletedCount = std::max(storyCompletedCount, storyMissionIndex + 1);
        recordStoryMissionProgress(
            loggedInUsername, storyCampaign, storyMissionIndex, true);
        refreshStoryCampaignProgress(storyCampaign);
        storyCompletedCount =
            storyCampaignProgress[storyProgressIndex(storyCampaign)];
        snapshot.status = storyMissionIndex + 1 < static_cast<int>(storyMissions(storyCampaign).size())
            ? "Chapter complete. Continue when you are ready."
            : std::string(storyCampaignName(storyCampaign)) +
                " story complete. This faction is ready for a full match.";
        endTurnButton.setLabel(
            storyMissionIndex + 1 < static_cast<int>(storyMissions(storyCampaign).size())
                ? "Continue Story"
                : "Finish Story");
        storyGameKeyboardFocus = StoryGameKeyboardFocus::EndTurn;
        storyKeyboardNavigationActive = true;
    };

    auto completeStoryChronicle = [&]() {
        storyCompletedCount = std::max(storyCompletedCount, storyMissionIndex + 1);
        recordStoryMissionProgress(
            loggedInUsername,
            storyCampaign,
            storyMissionIndex,
            !activeStoryMission().optionalRehearsal);
        refreshStoryCampaignProgress(storyCampaign);
        storyCompletedCount =
            storyCampaignProgress[storyProgressIndex(storyCampaign)];
        const bool hasNext =
            storyMissionIndex + 1 < static_cast<int>(storyMissions(storyCampaign).size());
        if (hasNext)
        {
            showStoryIntro(storyMissionIndex + 1);
        }
        else
        {
            showStoryMissionSelect(storyCampaign);
        }
    };

    const auto storyPieceIdForRole = [&](std::string_view role) {
        const auto found = std::find_if(
            storyRolePieceIds.begin(),
            storyRolePieceIds.end(),
            [&](const auto& entry) { return entry.first == role; });
        return found == storyRolePieceIds.end() ? 0 : found->second;
    };

    auto queueStoryPanels = [&](const std::vector<StoryPanel>& panels, bool completeAfter) {
        storyPopupPanels = panels;
        storyPopupPage = 0;
        storyPopupKeyboardFocus = 1;
        storyKeyboardNavigationActive = true;
        storyCompleteAfterPopup = completeAfter && !storyPopupPanels.empty();
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        pendingPieceActionChoice.reset();
    };

    auto enterStoryScriptStep = [&]() {
        storyTargetRow = -1;
        storyTargetColumn = -1;
        storyCorrection.clear();
        const StoryMission& mission = activeStoryMission();
        if (storyMissionStep < 0 ||
            storyMissionStep >= static_cast<int>(mission.script.size()))
        {
            return;
        }
        const StoryScriptAction& step = mission.script[static_cast<std::size_t>(storyMissionStep)];
        storyTargetRow = step.targetRow;
        storyTargetColumn = step.targetColumn;
        if (!step.panelsBefore.empty())
        {
            queueStoryPanels(step.panelsBefore, false);
        }
        if (storyScriptActionRequiresPlayerInput(mission, step))
        {
            storyKeyboardNavigationActive = true;
            if (step.kind == StoryActionKind::Move ||
                step.kind == StoryActionKind::Attack ||
                step.kind == StoryActionKind::UseAbility)
            {
                const int actorId = storyPieceIdForRole(step.actorRole);
                const auto actor = storyEngine
                    ? std::find_if(
                          storyEngine->boardPieces().begin(),
                          storyEngine->boardPieces().end(),
                          [&](const game_data::Piece& piece) {
                              return piece.id == actorId;
                          })
                    : std::vector<game_data::Piece>::const_iterator{};
                if (storyEngine && actor != storyEngine->boardPieces().end())
                {
                    storyBoardKeyboardCursor = {actor->row, actor->column};
                }
                storyGameKeyboardFocus = StoryGameKeyboardFocus::Board;
            }
            else if (step.kind == StoryActionKind::PlayCard ||
                     step.kind == StoryActionKind::DiscardCard)
            {
                if (storyEngine)
                {
                    const auto& hand = storyEngine->playerState(1).hand;
                    const auto card = std::find_if(
                        hand.begin(), hand.end(), [&](const game_data::GameCard& value) {
                            return step.cardTitle.empty() || value.title == step.cardTitle;
                        });
                    storyKeyboardHandIndex = card == hand.end()
                        ? 0
                        : static_cast<std::size_t>(std::distance(hand.begin(), card));
                }
                if (game_data::inBounds(step.targetRow, step.targetColumn))
                {
                    storyBoardKeyboardCursor = {step.targetRow, step.targetColumn};
                }
                storyGameKeyboardFocus = StoryGameKeyboardFocus::Hand;
            }
            else if (step.kind == StoryActionKind::DrawCard)
            {
                storyGameKeyboardFocus = StoryGameKeyboardFocus::DrawPile;
            }
            else if (step.kind == StoryActionKind::ChooseForesight)
            {
                storyKeyboardForesightIndex = 0;
                if (storyEngine)
                {
                    const game_data::Snapshot snapshot = storyEngine->snapshotFor(1);
                    const auto choice = std::find_if(
                        snapshot.foresightChoices.begin(),
                        snapshot.foresightChoices.end(),
                        [&](const game_data::GameCard& value) {
                            return step.cardTitle.empty() || value.title == step.cardTitle;
                        });
                    if (choice != snapshot.foresightChoices.end())
                    {
                        storyKeyboardForesightIndex = static_cast<std::size_t>(
                            std::distance(snapshot.foresightChoices.begin(), choice));
                    }
                }
            }
            else if (step.kind == StoryActionKind::EndTurn)
            {
                storyGameKeyboardFocus = StoryGameKeyboardFocus::EndTurn;
            }
        }
        storyScriptActionAt = animationTime + 0.65f;
    };

    auto advanceStoryScript = [&]() {
        const StoryMission& mission = activeStoryMission();
        ++storyMissionStep;
        if (storyMissionStep < static_cast<int>(mission.script.size()))
        {
            enterStoryScriptStep();
            return false;
        }
        storyTargetRow = -1;
        storyTargetColumn = -1;
        if (!mission.aftermath.empty())
        {
            queueStoryPanels(mission.aftermath, true);
            return false;
        }
        return true;
    };

    auto storyActionAllowed = [&](StoryActionKind kind,
                                  int owner,
                                  int actorId,
                                  int targetRow,
                                  int targetColumn,
                                  std::string_view cardTitle = {},
                                  int selectedActionIndex = -1) {
        if (!storyMode)
        {
            return true;
        }
        const StoryMission& mission = activeStoryMission();
        if (mission.script.empty())
        {
            return storyPopupPanels.empty();
        }
        if (!storyPopupPanels.empty() || storyMissionStep < 0 ||
            storyMissionStep >= static_cast<int>(mission.script.size()))
        {
            return false;
        }
        const StoryScriptAction& expected =
            mission.script[static_cast<std::size_t>(storyMissionStep)];
        const bool expectedPieceAction =
            expected.kind == StoryActionKind::Move ||
            expected.kind == StoryActionKind::Attack;
        const bool submittedPieceAction =
            kind == StoryActionKind::Move || kind == StoryActionKind::Attack;
        bool allowed = !storyScriptActionAutoResolves(mission, expected) &&
            expected.owner == owner &&
            (expectedPieceAction ? submittedPieceAction : expected.kind == kind);
        if (allowed && !expected.actorRole.empty())
        {
            allowed = actorId != 0 && actorId == storyPieceIdForRole(expected.actorRole);
        }
        if (allowed && !expected.targetRole.empty())
        {
            const int targetId = storyPieceIdForRole(expected.targetRole);
            const game_data::Piece* target = gamePieceById(targetId);
            allowed = target != nullptr && target->row == targetRow &&
                target->column == targetColumn;
        }
        if (allowed && expected.targetRow >= 0)
        {
            allowed = expected.targetRow == targetRow &&
                expected.targetColumn == targetColumn;
        }
        if (allowed && !expected.cardTitle.empty())
        {
            allowed = expected.cardTitle == cardTitle;
        }
        if (allowed && expectedPieceAction)
        {
            const game_data::Piece* actor = gamePieceById(actorId);
            allowed = actor != nullptr && storySelectedPieceActionMatches(
                expected, kind, *actor, selectedActionIndex);
        }
        if (allowed && expected.kind == StoryActionKind::UseAbility)
        {
            const game_data::Piece* actor = gamePieceById(actorId);
            allowed = actor != nullptr && storyAbilityStepMatches(expected, *actor);
        }
        if (!allowed)
        {
            storyCorrection = expected.correction.empty()
                ? std::string("Follow the highlighted lesson step first.")
                : std::string(expected.correction);
        }
        else
        {
            storyCorrection.clear();
        }
        return allowed;
    };

    const StoryCardResolutionMode storyCardResolutionMode = captureRequest
        ? StoryCardResolutionMode::AllowPackagedFixtureFallback
        : StoryCardResolutionMode::AuthoritativeOnly;
    const auto authoritativeStoryCards = [&](const std::vector<card_data::Card>& library)
        -> std::span<const card_data::Card> {
        // UI capture owns a deliberately fabricated screen-design library. It
        // opts into the reviewed package instead of treating those cards as
        // game authority.
        return captureRequest
            ? std::span<const card_data::Card>{}
            : std::span<const card_data::Card>(library.data(), library.size());
    };
    const auto resolvedStoryCardNamed = [&](std::string_view cardTitle) {
        return resolveStoryCardDefinition(
            cardTitle,
            authoritativeStoryCards(allCardLibrary),
            authoritativeStoryCards(cardLibrary),
            storyCardResolutionMode);
    };
    const auto missingStoryCardNamed = [](std::string_view cardTitle) {
        game_data::GameCard card;
        card.title = "Missing Story Card: " + std::string(cardTitle);
        card.type = "Story Error";
        card.health = 1;
        return card;
    };
    const auto storyCardNamed = [&](std::string_view cardTitle) {
        const std::optional<game_data::GameCard> resolved =
            resolvedStoryCardNamed(cardTitle);
        return resolved ? *resolved : missingStoryCardNamed(cardTitle);
    };

    auto beginStory = [&]() {
        ++storyGeneration;
        sandboxMode = false;
        storyMode = true;
        storyAiPending = false;
        storyTimerAccumulatorMs = 0.0;
        resignConfirmPopupVisible = false;
        gameConfirmationAction = GameConfirmationAction::Resign;
        leaveGameButton.setLabelSize(type::Caption);
        storyRestartButton.setVariant(ButtonVariant::Quiet);
        storyRestartButton.setLabelSize(type::Caption);
        if (usesCompactGameHud(window))
        {
            leaveGameButton.setLabelSize(14);
            storyRestartButton.setLabelSize(14);
            leaveGameButton.setLabel("Exit Entry");
            leaveGameButton.setPosition(
                {GameCompactStoryExitButtonX, GameCompactStoryButtonY});
            leaveGameButton.setSize(
                {GameCompactStoryButtonWidth, GameCompactStoryButtonHeight});
            storyRestartButton.setLabel("Restart");
            storyRestartButton.setPosition(
                {GameCompactStoryRestartButtonX, GameCompactStoryButtonY});
            storyRestartButton.setSize(
                {GameCompactStoryButtonWidth, GameCompactStoryButtonHeight});
        }
        else
        {
            leaveGameButton.setLabelSize(type::Caption);
            storyRestartButton.setLabelSize(type::Caption);
            leaveGameButton.setLabel("Exit to Missions");
            leaveGameButton.setPosition({GamePlayerBannerLeftX + 12.0f, 158.0f});
            leaveGameButton.setSize({156.0f, 32.0f});
            storyRestartButton.setLabel("Restart Attempt");
            storyRestartButton.setPosition({GamePlayerBannerLeftX + 12.0f, 198.0f});
            storyRestartButton.setSize({156.0f, 32.0f});
        }
        endTurnButton.setLabel("End Turn");
        abilityButton.setPosition({GameActionButtonX, GameAbilityButtonY});
        storyStage = StoryStage::Objective;
        storyMissionStep = 0;
        storyUsedAim = false;
        storyUsedHide = false;
        storyUsedSummon = false;
        storyRolePieceIds.clear();
        storyCorrection.clear();
        storyPopupPanels.clear();
        storyPopupPage = 0;
        storyCompleteAfterPopup = false;
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
        pendingPieceActionChoice.reset();
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

        const StoryMission& authoredMission = activeStoryMission();
        std::vector<card_data::Card> engineLibrary = captureRequest
            ? std::vector<card_data::Card>{}
            : allCardLibrary;
        const auto appendMissingCards = [&](const std::vector<card_data::Card>& source) {
            for (const card_data::Card& card : source)
            {
                const bool alreadyPresent = std::any_of(
                    engineLibrary.begin(), engineLibrary.end(), [&](const card_data::Card& existing) {
                        return existing.title == card.title;
                    });
                if (!alreadyPresent)
                {
                    engineLibrary.push_back(card);
                }
            }
        };
        if (!captureRequest)
        {
            appendMissingCards(cardLibrary);
        }
        const StoryCardDependencyReport cardDependencies =
            validateStoryCardDependencies(
                authoredMission,
                authoritativeStoryCards(allCardLibrary),
                authoritativeStoryCards(cardLibrary),
                storyCardResolutionMode);
        storyEngine = std::make_unique<GameEngine>(
            0x474c4f4fu + static_cast<unsigned int>(storyMissionIndex),
            engineLibrary);
        if (!cardDependencies.complete())
        {
            // Fail before loading any playable mission state. A connected
            // session must never borrow a stale packaged rule definition.
            const std::string& missingTitle = cardDependencies.missingTitles.front();
            const game_data::GameCard missingCard = missingStoryCardNamed(missingTitle);
            const std::string failureStatus = missingCard.title;
            const std::vector<GameEngine::ScenarioPiece> failurePieces = {
                {1, missingCard, 3, 3, false, -1}};
            storyEngine->loadScenario(
                failurePieces,
                {},
                {},
                0,
                0,
                1,
                failureStatus,
                false);
            haveSnapshot = false;
            commitLocalSnapshot(storyEngine->snapshotFor(1));
            storyStage = StoryStage::Failed;
            storyTargetRow = -1;
            storyTargetColumn = -1;
            endTurnButton.setLabel("Retry Mission");
            return;
        }
        for (const std::string& requiredTitle : cardDependencies.requiredTitles)
        {
            if (const std::optional<game_data::GameCard> card =
                    resolvedStoryCardNamed(requiredTitle))
            {
                storyEngine->registerScenarioCard(*card);
            }
        }

        if (authoredMission.standardMatch)
        {
            const auto failStandardMatchSetup = [&](std::string failureStatus) {
                game_data::Snapshot failedSnapshot = storyEngine->snapshotFor(1);
                failedSnapshot.status = std::move(failureStatus);
                haveSnapshot = false;
                commitLocalSnapshot(std::move(failedSnapshot));
                storyStage = StoryStage::Failed;
                storyTargetRow = -1;
                storyTargetColumn = -1;
                endTurnButton.setLabel("Retry Mission");
            };
            const auto resolvedDeck = [&](const std::vector<std::string_view>& titles) {
                std::vector<game_data::GameCard> deck;
                deck.reserve(titles.size());
                for (std::string_view cardTitle : titles)
                {
                    deck.push_back(storyCardNamed(cardTitle));
                }
                return deck;
            };
            const auto authoritativeDeck = [&](const std::vector<std::string_view>& titles) {
                std::vector<card_data::Card> deck;
                deck.reserve(titles.size());
                for (std::string_view cardTitle : titles)
                {
                    const auto findDefinition = [&](const std::vector<card_data::Card>& source)
                        -> const card_data::Card* {
                        const auto found = std::find_if(
                            source.begin(), source.end(), [&](const card_data::Card& card) {
                                return card.title == cardTitle;
                            });
                        return found == source.end() ? nullptr : &*found;
                    };
                    const card_data::Card* definition = findDefinition(allCardLibrary);
                    if (definition == nullptr)
                    {
                        definition = findDefinition(cardLibrary);
                    }
                    if (definition != nullptr)
                    {
                        deck.push_back(*definition);
                    }
                }
                return deck;
            };

            if (authoredMission.playerDeck.empty() || authoredMission.enemyDeck.empty())
            {
                failStandardMatchSetup(
                    "Mission data error: the standard-match decks are missing.");
                return;
            }
            if (!captureRequest)
            {
                const std::vector<card_data::Card> playerDefinitions =
                    authoritativeDeck(authoredMission.playerDeck);
                const std::vector<card_data::Card> enemyDefinitions =
                    authoritativeDeck(authoredMission.enemyDeck);
                if (playerDefinitions.size() != authoredMission.playerDeck.size() ||
                    enemyDefinitions.size() != authoredMission.enemyDeck.size())
                {
                    failStandardMatchSetup(
                        "Mission data error: an authoritative standard-match card is missing.");
                    return;
                }
                if (const std::optional<std::string> error =
                        game_data::deckRulesError(playerDefinitions))
                {
                    failStandardMatchSetup(
                        "Mission data error: the player deck is illegal: " + *error);
                    return;
                }
                if (const std::optional<std::string> error =
                        game_data::deckRulesError(enemyDefinitions))
                {
                    failStandardMatchSetup(
                        "Mission data error: the opponent deck is illegal: " + *error);
                    return;
                }
            }

            storyEngine->enableTimers();
            storyEngine->submitResolvedDeck(1, resolvedDeck(authoredMission.playerDeck));
            storyEngine->submitResolvedDeck(2, resolvedDeck(authoredMission.enemyDeck));
            if (!storyEngine->bothDecksSubmitted() ||
                storyEngine->phase() != game_data::Phase::HeroPlacement ||
                storyEngine->playerState(1).heroesToPlace.empty() ||
                storyEngine->playerState(2).heroesToPlace.empty())
            {
                failStandardMatchSetup(
                    "Mission data error: ordinary hero placement did not begin.");
                return;
            }
            haveSnapshot = false;
            commitLocalSnapshot(storyEngine->snapshotFor(1));
            storyKeyboardHandIndex = 0;
            storyGameKeyboardFocus = StoryGameKeyboardFocus::Hand;
            if (const auto home = game_data::homeSquares(1); !home.empty())
            {
                storyBoardKeyboardCursor = {home.front().first, home.front().second};
            }
            storyKeyboardNavigationActive = true;
            return;
        }

        std::vector<GameEngine::ScenarioPiece> scenarioPieces;
        std::vector<game_data::GameCard> playerHand;
        std::vector<game_data::GameCard> enemyHand;
        std::vector<game_data::GameCard> playerDrawPile;
        std::vector<game_data::GameCard> enemyDrawPile;
        std::string scenarioStatus;
        const auto spawnStoryPiece = [&](int owner,
                                         const std::string& cardTitle,
                                         int row,
                                         int column,
                                         bool isHero,
                                         int initialHealth = -1) {
            scenarioPieces.push_back(
                {owner, storyCardNamed(cardTitle), row, column, isHero, initialHealth});
        };

        const bool hasAuthoredSetup = !authoredMission.pieces.empty();
        if (hasAuthoredSetup)
        {
            for (const StoryPiecePlacement& piece : authoredMission.pieces)
            {
                spawnStoryPiece(
                    piece.owner,
                    std::string(piece.cardTitle),
                    piece.row,
                    piece.column,
                    piece.isHero,
                    piece.initialHealth);
            }
            for (std::string_view cardTitle : authoredMission.playerHand)
            {
                playerHand.push_back(storyCardNamed(std::string(cardTitle)));
            }
            for (std::string_view cardTitle : authoredMission.enemyHand)
            {
                enemyHand.push_back(storyCardNamed(std::string(cardTitle)));
            }
            for (std::string_view cardTitle : authoredMission.playerDrawPile)
            {
                playerDrawPile.push_back(storyCardNamed(std::string(cardTitle)));
            }
            for (std::string_view cardTitle : authoredMission.enemyDrawPile)
            {
                enemyDrawPile.push_back(storyCardNamed(std::string(cardTitle)));
            }
            scenarioStatus = std::string(authoredMission.objective);
        }
        else
        {
            scenarioStatus = "Story mission data error: authored setup is missing.";
        }

        const bool scenarioLoaded = storyEngine->loadScenario(
            scenarioPieces,
            std::move(playerHand),
            std::move(enemyHand),
            hasAuthoredSetup ? authoredMission.playerResources : 12,
            hasAuthoredSetup ? authoredMission.enemyResources : 12,
            hasAuthoredSetup ? authoredMission.firstPlayer : 1,
            std::move(scenarioStatus),
            false,
            std::move(playerDrawPile),
            std::move(enemyDrawPile));
        if (!scenarioLoaded)
        {
            game_data::Snapshot failedSnapshot = storyEngine->snapshotFor(1);
            failedSnapshot.status =
                "Story mission data error: an authored piece is off the board or overlaps another piece.";
            haveSnapshot = false;
            commitLocalSnapshot(std::move(failedSnapshot));
            storyStage = StoryStage::Failed;
            storyTargetRow = -1;
            storyTargetColumn = -1;
            endTurnButton.setLabel("Retry Mission");
            return;
        }
        haveSnapshot = false;
        bool objectiveConfigured = hasAuthoredSetup;
        if (hasAuthoredSetup)
        {
            for (const StoryPiecePlacement& placement : authoredMission.pieces)
            {
                const auto found = std::find_if(
                    storyEngine->boardPieces().begin(),
                    storyEngine->boardPieces().end(),
                    [&](const game_data::Piece& piece) {
                        return game_data::pieceOriginalOwner(piece) == placement.owner &&
                            piece.name == placement.cardTitle &&
                            piece.row == placement.row &&
                            piece.column == placement.column;
                    });
                if (found != storyEngine->boardPieces().end())
                {
                    storyRolePieceIds.emplace_back(placement.role, found->id);
                }
            }
            objectiveConfigured =
                storyRolePieceIds.size() == authoredMission.pieces.size();

            GameEngine::ScenarioObjective objective;
            objective.successPlayer = 1;
            objective.requiredForceOriginalOwner = 1;
            switch (authoredMission.objectiveSpec.kind)
            {
            case StoryObjectiveKind::DefeatAllEnemies:
                objective.kind =
                    GameEngine::ScenarioObjectiveKind::DefeatOriginalOwner;
                objective.opposingOriginalOwner = 2;
                break;
            case StoryObjectiveKind::DefeatRole:
                objective.kind = GameEngine::ScenarioObjectiveKind::DefeatPiece;
                objective.targetPieceId = storyPieceIdForRole(
                    authoredMission.objectiveSpec.targetRole);
                objectiveConfigured = objectiveConfigured &&
                    objective.targetPieceId != 0;
                break;
            case StoryObjectiveKind::ReachSquare:
                objective.kind = GameEngine::ScenarioObjectiveKind::ReachSquare;
                objective.targetPieceId = storyPieceIdForRole(
                    authoredMission.objectiveSpec.targetRole);
                objective.targetRow = authoredMission.objectiveSpec.targetRow;
                objective.targetColumn = authoredMission.objectiveSpec.targetColumn;
                objectiveConfigured = objectiveConfigured &&
                    objective.targetPieceId != 0;
                break;
            case StoryObjectiveKind::ControlSquares:
                objective.kind = GameEngine::ScenarioObjectiveKind::ControlSquares;
                objective.controlAmount = authoredMission.objectiveSpec.amount;
                break;
            case StoryObjectiveKind::Scripted:
            case StoryObjectiveKind::DeployCard:
            case StoryObjectiveKind::Legacy:
            case StoryObjectiveKind::StoryOnly:
                // Script sequencing and any future deployment lesson still use
                // their explicit client step, but the engine owns force and
                // required-survivor failure throughout the scenario.
                objective.kind = GameEngine::ScenarioObjectiveKind::None;
                break;
            }
            for (std::string_view role : authoredMission.requiredSurvivorRoles)
            {
                const int pieceId = storyPieceIdForRole(role);
                objectiveConfigured = objectiveConfigured && pieceId != 0;
                if (pieceId != 0)
                {
                    objective.requiredSurvivorPieceIds.push_back(pieceId);
                }
            }
            objectiveConfigured = objectiveConfigured &&
                storyEngine->configureScenarioObjective(std::move(objective));
        }

        game_data::Snapshot initialSnapshot = storyEngine->snapshotFor(1);
        if (!objectiveConfigured)
        {
            storyStage = StoryStage::Failed;
            initialSnapshot.status =
                "Mission data error: the authoritative objective could not be configured.";
            endTurnButton.setLabel("Retry Mission");
        }
        commitLocalSnapshot(std::move(initialSnapshot));
        if (hasAuthoredSetup && objectiveConfigured)
        {
            if (!authoredMission.script.empty())
            {
                enterStoryScriptStep();
            }
            else
            {
                storyTargetRow = authoredMission.objectiveSpec.targetRow;
                storyTargetColumn = authoredMission.objectiveSpec.targetColumn;
                const auto firstPlayerPiece = std::find_if(
                    storyEngine->boardPieces().begin(),
                    storyEngine->boardPieces().end(),
                    [](const game_data::Piece& piece) {
                        return piece.owner == 1;
                    });
                if (game_data::inBounds(storyTargetRow, storyTargetColumn))
                {
                    storyBoardKeyboardCursor = {storyTargetRow, storyTargetColumn};
                }
                else if (firstPlayerPiece != storyEngine->boardPieces().end())
                {
                    storyBoardKeyboardCursor = {
                        firstPlayerPiece->row, firstPlayerPiece->column};
                }
                storyGameKeyboardFocus = StoryGameKeyboardFocus::Board;
                storyKeyboardNavigationActive = true;
            }
        }
    };

    auto beginSandbox = [&](std::vector<card_data::Card> cards) {
        sandboxMode = true;
        storyMode = false;
        storyEngine.reset();
        storyAiPending = false;
        resignConfirmPopupVisible = false;
        gameConfirmationAction = GameConfirmationAction::Resign;
        leaveGameButton.setLabel("Leave");
        leaveGameButton.setLabelSize(type::Body);
        leaveGameButton.setPosition({GameActionButtonX, GameLeaveButtonY});
        leaveGameButton.setSize({GameLeaveButtonWidth, GameActionButtonHeight});
        endTurnButton.setLabel("End Turn");
        abilityButton.setPosition({GameActionButtonX, GameAbilityButtonY});
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
        pendingPieceActionChoice.reset();
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

        // Sandbox cards come from the authoritative card server too. Retain the
        // catalogue after each GameCard is converted into the lean Piece shape.
        allCardLibrary = cards;

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

    auto pieceBaseArtworkFor = [&](int owner, int width, int height) -> const sf::Texture* {
        const bool usesLargeBase = width == 4 && height == 4;
        if (owner == 1)
        {
            return usesLargeBase ? pieceBaseLargeBlueArtwork : pieceBaseBlueArtwork;
        }
        return usesLargeBase ? pieceBaseLargeRedArtwork : pieceBaseRedArtwork;
    };

    auto rarityGemArtworkFor = [&](const std::string& title) -> const sf::Texture* {
        const auto findCard = [&](const std::vector<card_data::Card>& library)
            -> const card_data::Card* {
            const auto found = std::find_if(
                library.begin(), library.end(), [&](const card_data::Card& card) {
                    return card.title == title;
                });
            return found == library.end() ? nullptr : &*found;
        };

        if (storyMode && captureRequest)
        {
            // Packaged Story GameCards own the capture's gameplay and visual
            // metadata, but GameCard intentionally has no rarity field.  A
            // generic UI sample with the same title is not an authority, so do
            // not fabricate a rarity socket from it.
            if (!packagedStoryCard(title) && !captureValidationFailed)
            {
                failCaptureValidation(
                    "Capture render error: Story piece '" + title +
                    "' has no exact packaged render definition.");
            }
            return nullptr;
        }

        const card_data::Card* definition = findCard(allCardLibrary);
        if (!definition) definition = findCard(cardLibrary);
        // A live Story mission may use only the authoritative full/owned
        // catalogues accepted by its engine.  Match and sample collections are
        // UI conveniences and must not silently supply Story render metadata.
        if (!storyMode)
        {
            if (!definition) definition = findCard(matchDeck);
            if (!definition) definition = findCard(matchHeroes);
            if (!definition)
            {
                static const std::vector<card_data::Card> sampleCards =
                    ui_capture::sampleCardLibrary();
                definition = findCard(sampleCards);
            }
        }

        if (storyMode && !definition)
        {
            return nullptr;
        }

        const std::string rarity = definition
            ? game_data::cardRarity(*definition)
            : "common";
        std::size_t gemIndex = 0;
        if (rarity == "uncommon") gemIndex = 1;
        else if (rarity == "rare") gemIndex = 2;
        else if (rarity == "legendary") gemIndex = 3;
        return rarityGemArtworks[gemIndex];
    };

    // Legacy per-card base paths remain in network card data for compatibility.
    // Team-specific basic0 artwork and the rarity socket overlay are drawn
    // separately below, so baked-in team gems never replace card rarity.
    auto drawPieceVisual = [&](
        const std::string& tokenPath,
        const std::string& walkPath,
        const std::string& idlePath,
        const std::string& /*basePath*/,
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
                                    sf::Vector2f baseCenter,
                                    sf::Vector2f healthBadgeCenter,
                                    float scale,
                                    bool valid) {
        const sf::Color tint = valid ? sf::Color(255, 255, 255, 220) : sf::Color(220, 120, 110, 190);
        const std::string& tokenPath = cardTokenPath(card);
        const std::string& walkPath = cardWalkAnimPath(card);

        drawPieceBase(
            window,
            baseCenter,
            scale,
            owner,
            false,
            static_cast<float>(card.width),
            static_cast<float>(card.height),
            pieceBaseArtworkFor(owner, card.width, card.height),
            rarityGemArtworkFor(card.title));
        drawPieceSelectionRing(
            window,
            baseCenter,
            scale,
            0.7f,
            valid ? sf::Color(132, 232, 186) : sf::Color(232, 104, 92),
            static_cast<float>(card.width),
            static_cast<float>(card.height));
        const bool drewPiece = drawPieceVisual(
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
            }
        }
        drawPieceHealthBadge(
            window,
            healthBadgeCenter,
            scale,
            card.health,
            owner,
            !valid,
            font,
            card.type == "Hero");
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

    // The deck editor's inspector shows rarity, cost, holdings and health itself,
    // on the card face and in its badge row. deckEditorCardDetails leads with the
    // same five facts, so the popup used to print every one of them twice; keep
    // only what the face does not already say.
    auto deckEditorAbilityRows = [&](const card_data::Card& card) {
        static constexpr const char* Duplicated[] = {
            "Rarity:", "Hero cost:", "Cost:", "Deck limit:", "Health:"};
        DetailRows rows;
        for (DetailRow& row : deckEditorCardDetails(card))
        {
            const bool duplicated = !row.action &&
                std::any_of(std::begin(Duplicated), std::end(Duplicated), [&](const char* prefix) {
                    return row.text.rfind(prefix, 0) == 0;
                });
            if (!duplicated)
            {
                rows.push_back(std::move(row));
            }
        }
        return rows;
    };

    struct ActionModifierBadge
    {
        std::string label;
        std::string title;
        std::string text;
    };

    auto actionModifierBadges = [](const ActionDescription& action) {
        std::vector<ActionModifierBadge> badges;
        if (action.type == "Capture")
        {
            badges.push_back({
                "Enemy required",
                "Capture",
                "Capture cannot move onto an empty square. It must target an enemy: the attacker stops before a survivor and occupies the destination only if every target there is defeated."});
        }
        if (!action.targetFilter.empty())
        {
            badges.push_back({
                "Only: " + joinStrings(action.targetFilter, "+"),
                "Required target traits",
                "A legal target must have every listed word as either a Trait or a Keyword: " +
                    joinStrings(action.targetFilter, ", ") + "."});
        }
        if (action.passThrough)
        {
            badges.push_back({
                "Pass-through",
                "Pass-through movement",
                "Intermediate occupied squares do not block this movement. The destination must still be a legal empty square or an attackable enemy square."});
        }
        if (action.clearPath)
        {
            badges.push_back({
                "Clear path",
                "Ranged path",
                "A straight or diagonal ranged attack needs an unobstructed path between at least one square of the attacker and one square of the target."});
        }
        if (action.push > 0)
        {
            badges.push_back({
                "Push " + std::to_string(action.push),
                "Push",
                "After the attack resolves, even if it dealt zero damage, the surviving effective target moves up to " +
                    std::to_string(action.push) +
                    " square(s) directly away from the attack's staging square. Every square blocked by a piece or board edge becomes 1 collision damage."});
        }
        if (action.pull)
        {
            badges.push_back({
                "Pull",
                "Pull",
                "After this ranged hit, the surviving effective target moves toward the attacker until their footprints are adjacent, stopping early if another piece blocks the route. A defeated target does not move."});
        }
        if (action.repeat > 0)
        {
            badges.push_back({
                "Repeat +" + std::to_string(action.repeat),
                "Repeat",
                "After the first use, this same piece must use this same action up to " +
                    std::to_string(action.repeat) +
                    " additional time(s), or End Turn. Other pieces, card plays, draws, discards, and abilities cannot interrupt."});
        }
        if (!action.infest.empty())
        {
            badges.push_back({
                "Infest: " + action.infest,
                "Infest",
                "Marks a non-Hero target before destruction resolves. If that hit kills it, or if it later dies while marked, " + action.infest +
                    " replaces it on that square for the infesting side and cannot act until its owner's next turn. A newer Infest overrides an older one, and Infest takes priority over Rebirth."});
        }
        return badges;
    };

    auto actionModifierLineCount = [&](const ActionDescription& action, float contentWidth) {
        const std::vector<ActionModifierBadge> badges = actionModifierBadges(action);
        if (badges.empty())
        {
            return 0;
        }
        const float width = contentWidth - PiecePopupScrollTextXInset * 2.0f;
        int lines = 1;
        float used = 0.0f;
        for (const ActionModifierBadge& badge : badges)
        {
            sf::Text measuring(font, badge.label, 10);
            const float badgeWidth = std::min(width, measuring.getLocalBounds().size.x + 14.0f);
            if (used > 0.0f && used + 5.0f + badgeWidth > width)
            {
                ++lines;
                used = badgeWidth;
            }
            else
            {
                used += (used > 0.0f ? 5.0f : 0.0f) + badgeWidth;
            }
        }
        return lines;
    };

    auto detailRowsHeight = [&](const DetailRows& details, float contentWidth) {
        float height = 0.0f;
        for (const DetailRow& row : details)
        {
            if (row.action)
            {
                height += 54.0f + 20.0f * static_cast<float>(
                    actionModifierLineCount(*row.action, contentWidth));
                continue;
            }
            height += static_cast<float>(
                wrapText(font, row.text, 14, contentWidth - PiecePopupScrollTextXInset * 2.0f).size()) * 18.0f;
            height += 8.0f;
        }
        return height + PiecePopupScrollTextYInset;
    };

    auto detailRowsScrollContentHeight = [&](const DetailRows& details, float contentWidth) {
        // The renderer starts with a top inset; reserve the matching bottom
        // inset in the scroll extent so the last row clears the inner frame.
        return detailRowsHeight(details, contentWidth) + PiecePopupScrollTextYInset;
    };

    auto detailRowsMaxScroll = [&](const DetailRows& details) {
        return std::max(
            0.0f,
            detailRowsScrollContentHeight(details, PiecePopupTextWidth) - PiecePopupScrollHeight);
    };

    // The inspector has a wider and taller abilities viewport than the in-game
    // popup, so calculate wrapping and travel from its own content width.
    auto deckEditorAbilityMaxScroll = [&](const DetailRows& details) {
        return std::max(0.0f, detailRowsHeight(details, CardPopupAbilitiesWidth) - CardPopupAbilitiesHeight);
    };

    struct DetailTooltip
    {
        std::string title;
        std::string text;
    };

    auto drawDetailRows = [&](const DetailRows& details,
                              float y,
                              float contentX,
                              float contentWidth,
                              const std::optional<sf::Vector2f>& pointer) {
        std::optional<DetailTooltip> hoveredTooltip;
        const float left = contentX + PiecePopupScrollTextXInset;
        const float width = contentWidth - PiecePopupScrollTextXInset * 2.0f;
        auto measuredTextWidth = [&](const std::string& value, unsigned int size) {
            sf::Text measuring(font, value, size);
            return measuring.getLocalBounds().size.x;
        };
        auto drawInlineIcon = [&](const std::string& path,
                                  float x,
                                  float iconY,
                                  const std::string& tooltipTitle,
                                  const std::string& tooltipText) {
            if (sf::Texture* icon = textures.load(path))
            {
                drawContainSprite(window, *icon, {{x, iconY}, {18.0f, 18.0f}});
                if (pointer && isInsideRect(*pointer, x, iconY, 18.0f, 18.0f))
                {
                    hoveredTooltip = DetailTooltip{tooltipTitle, tooltipText};
                }
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

            drawInlineIcon(
                action.moveIconPath,
                x,
                y,
                action.moveTooltipTitle,
                action.moveTooltipText);
            x += 21.0f;
            drawText(window, font, action.range, 13, {x, y + 1.0f}, row.color);
            x += measuredTextWidth(action.range, 13) + 5.0f;

            auto drawAmount = [&](const std::string& iconPath,
                                  int amount,
                                  const std::string& tooltipTitle,
                                  const std::string& tooltipText) {
                if (amount <= 0)
                {
                    return;
                }
                x += 5.0f;
                drawInlineIcon(iconPath, x, y, tooltipTitle, tooltipText);
                x += 21.0f;
                const std::string value = std::to_string(amount);
                drawText(window, font, value, 13, {x, y + 1.0f}, row.color);
                x += measuredTextWidth(value, 13);
            };
            drawAmount(
                "ui/damage.png",
                action.damage,
                "Damage",
                "Removes this much Health from each target. Any positive damage also makes a surviving target miss its next activation and blocks its movement until that owner turn ends; a printed Disable value can extend the missed activations.");
            drawAmount(
                "ui/heal.png",
                action.heal,
                "Healing",
                "Targets a friendly piece that is below maximum Health and restores this much Health, without exceeding that maximum.");
            drawAmount(
                "ui/stun.png",
                action.stun,
                "Disable",
                "Makes each surviving target miss this many of its next owner-turn activations.");
            drawAmount(
                "ui/cooldown.png",
                action.cooldown,
                "Cooldown",
                "After using this action, its user cannot act on this many of its owner's turn activations.");
            drawAmount(
                "ui/under-control.png",
                action.control,
                "Control",
                "Takes control of an enemy non-Hero for this many of your later turns.");
            const std::vector<ActionModifierBadge> modifiers = actionModifierBadges(action);
            // Keep the first short modifier on the core stats line whenever it
            // fits. Pull, Capture requirements, and Infest are then visible on
            // first inspection instead of sitting just below the viewport.
            float modifierX = x + 8.0f;
            float modifierY = y;
            for (const ActionModifierBadge& modifier : modifiers)
            {
                sf::Text badgeText(font, modifier.label, 10);
                const float badgeWidth = std::min(
                    width, badgeText.getLocalBounds().size.x + 14.0f);
                if (modifierX + badgeWidth > left + width)
                {
                    modifierX = left;
                    modifierY = std::max(modifierY + 20.0f, y + 24.0f);
                }
                drawBeveledPlate(
                    window,
                    {modifierX, modifierY},
                    {badgeWidth, 16.0f},
                    withAlpha(row.color, 34),
                    withAlpha(row.color, 165),
                    false,
                    4.0f);
                badgeText.setFillColor(withAlpha(row.color, 242));
                centerText(
                    badgeText,
                    {modifierX + badgeWidth * 0.5f, modifierY + 8.0f});
                drawCrispText(window, badgeText);
                if (pointer && isInsideRect(
                        *pointer, modifierX, modifierY, badgeWidth, 16.0f))
                {
                    hoveredTooltip = DetailTooltip{modifier.title, modifier.text};
                }
                modifierX += badgeWidth + 5.0f;
            }
            y += 33.0f + 20.0f * static_cast<float>(
                actionModifierLineCount(action, contentWidth));
        }
        return hoveredTooltip;
    };

    auto drawDetailTooltip = [&](const std::optional<DetailTooltip>& tooltip) {
        if (!tooltip)
        {
            return;
        }

        constexpr float TooltipWidth = 238.0f;
        constexpr float TooltipPadding = 12.0f;
        const std::vector<std::string> lines =
            wrapText(font, tooltip->text, 12, TooltipWidth - TooltipPadding * 2.0f);
        const float tooltipHeight = 38.0f + static_cast<float>(lines.size()) * 16.0f;

        sf::Vector2f position = collectionPointer() + sf::Vector2f(14.0f, 14.0f);
        if (position.x + TooltipWidth > ui_canvas::Right - 8.0f)
        {
            position.x = collectionPointer().x - TooltipWidth - 14.0f;
        }
        if (position.y + tooltipHeight > ui_canvas::Height - 8.0f)
        {
            position.y = collectionPointer().y - tooltipHeight - 14.0f;
        }
        position.x = std::clamp(position.x, ui_canvas::Left + 8.0f, ui_canvas::Right - TooltipWidth - 8.0f);
        position.y = std::clamp(position.y, 8.0f, ui_canvas::Height - tooltipHeight - 8.0f);

        drawBeveledPlate(
            window,
            position,
            {TooltipWidth, tooltipHeight},
            sf::Color(8, 14, 15, 250),
            sf::Color(198, 146, 70, 235),
            false,
            6.0f);
        drawText(
            window,
            font,
            tooltip->title,
            13,
            position + sf::Vector2f(TooltipPadding, 9.0f),
            sf::Color(248, 239, 216),
            TooltipWidth - TooltipPadding * 2.0f);
        float lineY = position.y + 29.0f;
        for (const std::string& line : lines)
        {
            drawText(
                window,
                font,
                line,
                12,
                {position.x + TooltipPadding, lineY},
                sf::Color(143, 220, 205),
                TooltipWidth - TooltipPadding * 2.0f);
            lineY += 16.0f;
        }
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
        const std::size_t visibleCards = last - gameHandOffset;
        const float pitch = gameHandCardPitch(visibleCards);
        for (std::size_t visibleIndex = visibleCards; visibleIndex-- > 0;)
        {
            const std::size_t i = gameHandOffset + visibleIndex;
            const float x = gameHandCardX(visibleIndex, visibleCards);
            const float exposedWidth =
                visibleIndex + 1 < visibleCards ? pitch : HandCardWidth;
            if (isInsideRect(
                    point,
                    x,
                    HandY - HandHoverLift,
                    exposedWidth,
                    HandCardHeight + HandHoverLift))
            {
                return i;
            }
        }
        return std::nullopt;
    };

    auto foresightChoiceAtPixel = [&](sf::Vector2f point) -> std::optional<std::size_t> {
        if (gameSnapshot.foresightChoices.empty())
        {
            return std::nullopt;
        }
        const std::size_t totalRows =
            (gameSnapshot.foresightChoices.size() + ForesightChoiceColumns - 1) /
            ForesightChoiceColumns;
        clampListOffset(foresightChoiceRowOffset, totalRows, ForesightVisibleRows);
        const std::size_t visibleRows = std::min(
            ForesightVisibleRows, totalRows - foresightChoiceRowOffset);
        for (std::size_t visibleRow = 0; visibleRow < visibleRows; ++visibleRow)
        {
            const std::size_t row = foresightChoiceRowOffset + visibleRow;
            const std::size_t rowStart = row * ForesightChoiceColumns;
            const std::size_t rowCount = std::min(
                ForesightChoiceColumns, gameSnapshot.foresightChoices.size() - rowStart);
            const float rowWidth = static_cast<float>(rowCount) * HandCardWidth +
                static_cast<float>(rowCount - 1) * ForesightChoiceGap;
            const float startX = (ui_canvas::Width - rowWidth) * 0.5f;
            const float y = ForesightChoiceY + static_cast<float>(visibleRow) *
                ForesightChoiceRowPitch;
            for (std::size_t column = 0; column < rowCount; ++column)
            {
                const float x = startX + static_cast<float>(column) *
                    (HandCardWidth + ForesightChoiceGap);
                if (isInsideRect(point, x, y, HandCardWidth, HandCardHeight + 34.0f))
                {
                    return rowStart + column;
                }
            }
        }
        return std::nullopt;
    };
    auto isDiscardTrashCanAtPixel = [&](sf::Vector2f point) {
        return isInsideRect(
            point,
            TrashCanX - TrashCanDropPadding,
            TrashCanY - TrashCanDropPadding,
            TrashCanWidth + TrashCanDropPadding * 2.0f,
            TrashCanHeight + TrashCanDropPadding * 2.0f);
    };
    auto isDrawPileAtPixel = [&](sf::Vector2f point) {
        return isInsideRect(
            point,
            GameDeckPileX,
            GamePileY - 4.0f,
            GamePileWidth,
            GamePileHeight - 6.0f);
    };

    auto playerReadoutAtPixel = [&](sf::Vector2f point) -> std::optional<int> {
        if (isInsideRect(
                point,
                gamePlayerBannerX(window, 1),
                GameTopBarY,
                GamePlayerReadoutWidth,
                gamePlayerBannerHeight(window)))
        {
            return 1;
        }
        if (isInsideRect(
                point,
                gamePlayerBannerX(window, 2),
                GameTopBarY,
                GamePlayerReadoutWidth,
                gamePlayerBannerHeight(window)))
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

    auto updateStoryAfterAction = [&](game_data::Snapshot& snapshot) {
        if (!storyMode || !storyEngine || storyStage != StoryStage::Objective)
        {
            return;
        }

        const StoryMission& mission = activeStoryMission();
        const GameEngine::ScenarioObjectiveProgress& objectiveProgress =
            storyEngine->scenarioObjectiveProgress();
        const bool engineFinished =
            static_cast<game_data::Phase>(snapshot.phase) == game_data::Phase::GameOver;
        std::string missingSurvivorName;
        if (objectiveProgress.failure ==
            GameEngine::ScenarioObjectiveFailure::RequiredPieceMissing)
        {
            for (std::string_view role : mission.requiredSurvivorRoles)
            {
                if (storyPieceIdForRole(role) != objectiveProgress.failedPieceId)
                {
                    continue;
                }
                const auto placement = std::find_if(
                    mission.pieces.begin(), mission.pieces.end(),
                    [&](const StoryPiecePlacement& piece) { return piece.role == role; });
                missingSurvivorName = placement == mission.pieces.end()
                    ? std::string(role)
                    : std::string(placement->cardTitle);
                break;
            }
        }
        if (engineFinished && snapshot.winner == 2)
        {
            ++storyGenuineDefeatCount;
            storyStage = StoryStage::Failed;
            storyTargetRow = -1;
            storyTargetColumn = -1;
            // A final scripted command may have queued aftermath before the
            // authoritative engine reports a required-survivor defeat. Defeat
            // owns the result: discard that queue so closing it can never
            // promote a failed mission to Complete.
            storyPopupPanels.clear();
            storyPopupPage = 0;
            storyCompleteAfterPopup = false;
            snapshot.status = !missingSurvivorName.empty()
                ? "Mission failed: " + missingSurvivorName +
                    " fell before the objective was secured."
                : storyCampaign == StoryCampaign::Blackthorn
                    ? "Mission failed: the Blackthorn force was defeated."
                    : storyCampaign == StoryCampaign::Mirewatch
                        ? "Mission failed: the Mirewatch force was defeated."
                        : "Mission failed: the Seelie defenders were defeated.";
            endTurnButton.setLabel("Retry Mission");
            if (storyContinueWithoutMasteryAvailable(
                    mission, storyGenuineDefeatCount))
            {
                snapshot.status +=
                    " Continue Anyway unlocks the next story entry without a completion stamp; replay this mission later to earn it.";
                storyRestartButton.setVariant(ButtonVariant::Secondary);
                storyRestartButton.setLabel("Continue Anyway");
                if (usesCompactGameHud(window))
                {
                    // A continuation decision is a result action, not a persistent
                    // HUD utility. Stack it below Retry where the full label is
                    // readable instead of squeezing it between top banners.
                    storyRestartButton.setPosition(
                        {GameActionButtonX, GameLeaveButtonY - 2.0f});
                    storyRestartButton.setSize(
                        {GameActionButtonWidth, GameActionButtonHeight + 4.0f});
                    storyRestartButton.setLabelSize(14);
                }
            }
            return;
        }

        bool completed = engineFinished && snapshot.winner == 1 &&
            (mission.standardMatch || objectiveProgress.complete);
        if (!engineFinished &&
            mission.objectiveSpec.kind == StoryObjectiveKind::DeployCard)
        {
            completed = std::any_of(
                storyEngine->boardPieces().begin(),
                storyEngine->boardPieces().end(),
                [&](const game_data::Piece& piece) {
                    const bool correctSquare = mission.objectiveSpec.targetRow < 0 ||
                        (piece.row == mission.objectiveSpec.targetRow &&
                         piece.column == mission.objectiveSpec.targetColumn);
                    return game_data::pieceOriginalOwner(piece) == 1 &&
                        piece.name == mission.objectiveSpec.cardTitle && correctSquare;
                });
        }
        if (completed)
        {
            if (!mission.aftermath.empty() && storyPopupPanels.empty())
            {
                queueStoryPanels(mission.aftermath, true);
            }
            else if (mission.aftermath.empty())
            {
                completeStoryMission(snapshot);
            }
        }
    };

    const auto sandboxCardNamed = [&](const game_data::Snapshot& snapshot, const std::string& title) {
        for (const game_data::GameCard& card : snapshot.hand)
        {
            if (card.title == title)
            {
                return card;
            }
        }
        for (const card_data::Card& card : allCardLibrary)
        {
            if (card.title == title)
            {
                return game_data::toGameCard(card);
            }
        }
        for (const card_data::Card& card : cardLibrary)
        {
            if (card.title == title)
            {
                return game_data::toGameCard(card);
            }
        }
        return game_data::GameCard{};
    };

    auto destroySandboxPiece = [&](game_data::Snapshot& snapshot, int pieceId) {
        const game_data::Piece* piece = pieceByIdInSnapshot(snapshot, pieceId);
        const game_data::GameCard* rebirthCard = nullptr;
        game_data::GameCard infestationCardValue;
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
        if (piece != nullptr && !piece->infestationTitle.empty())
        {
            infestationCardValue = sandboxCardNamed(snapshot, piece->infestationTitle);
        }
        const game_data::GameCard* infestationCard =
            infestationCardValue.type == "Unit" ? &infestationCardValue : nullptr;
        return destroyPieceInSnapshot(
            snapshot,
            nextSandboxPieceId,
            pieceId,
            rebirthCard,
            infestationCard);
    };

    auto sandboxPlayCard = [&](int handIndex, int row, int column) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing ||
            handIndex < 0 || handIndex >= static_cast<int>(gameSnapshot.hand.size()))
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        const auto pendingRepeat = std::find_if(
            next.pieces.begin(),
            next.pieces.end(),
            [](const game_data::Piece& piece) { return piece.repeatActionIndex >= 0; });
        if (pendingRepeat != next.pieces.end())
        {
            next.status = "Finish the repeatable action or advance the turn before playing a card.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
        if (next.relentlessPieceId != 0)
        {
            next.status = "The Relentless piece must act again or you must advance the turn.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
        const game_data::GameCard card = next.hand[static_cast<std::size_t>(handIndex)];
        if (card.type == "Spell" && !game_data::isSupportedSpellEffect(card))
        {
            next.status = "This spell has no defined game effect and cannot be played.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
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
            if (card.type == "Unit")
            {
                next.pieces.back().hasActed = true;
            }
            next.status = "Sandbox played " + card.title + " for Player " + std::to_string(actingPlayer) + ".";
            updateStoryAfterAction(next);
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

    auto sandboxActWithPiece = [&](int pieceId, int row, int column,
                                   int selectedActionIndex = -1) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing)
        {
            return;
        }
        if (storyMode && storyStage != StoryStage::Objective)
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        game_data::Piece* piece = pieceByIdInSnapshotMutable(next, pieceId);
        if (!piece)
        {
            return;
        }
        const auto pendingRepeat = std::find_if(
            next.pieces.begin(),
            next.pieces.end(),
            [](const game_data::Piece& candidate) { return candidate.repeatActionIndex >= 0; });
        if (pendingRepeat != next.pieces.end() && pendingRepeat->id != piece->id)
        {
            next.status = "Finish the repeatable action with that piece or advance the turn.";
            commitSandboxSnapshot(std::move(next));
            return;
        }
        const bool continuingRepeat = piece->repeatActionIndex >= 0;
        const int requiredActionIndex = continuingRepeat
            ? piece->repeatActionIndex
            : selectedActionIndex;
        if (continuingRepeat)
        {
            if (selectedActionIndex >= 0 &&
                selectedActionIndex != piece->repeatActionIndex)
            {
                next.status = "Repeat must continue with the same printed action.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            if (requiredActionIndex >= static_cast<int>(piece->actions.size()) ||
                piece->repeatActionUses < 0)
            {
                piece->repeatActionIndex = -1;
                piece->repeatActionState = 0;
                piece->repeatActionUses = 0;
                return;
            }
            piece->actionState = piece->repeatActionState;
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
            game_data::resolvePieceActionThroughHidden(
                next.pieces, next.holes, *piece, row, column, requiredActionIndex);
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
        const int attackerActionState = piece->actionState;
        std::vector<std::string> damagedTargetNames;
        std::vector<std::string> healedTargetNames;
        std::vector<std::string> controlledTargetNames;
        bool anyTargetDestroyed = false;
        bool anyTargetReborn = false;
        bool anyTargetInfestationSpawned = false;
        bool anyTargetWasHidden = false;
        int pushedSquares = 0;
        int pushCollisionDamage = 0;
        int pulledSquares = 0;
        std::vector<int> revealedPieceIds = outcome.revealedPieceIds;
        const auto rememberRevealedPieces = [&](const std::vector<int>& ids) {
            for (int id : ids)
            {
                if (std::find(revealedPieceIds.begin(), revealedPieceIds.end(), id) ==
                    revealedPieceIds.end())
                {
                    revealedPieceIds.push_back(id);
                }
            }
        };
        const int attackDamage = action.damage +
            game_data::pieceEnchantmentDamageBonus(next.enchantments, attackerId);
        const std::string infestationTitle = action.actionIndex >= 0 &&
                action.actionIndex < static_cast<int>(piece->actions.size())
            ? piece->actions[static_cast<std::size_t>(action.actionIndex)].infest
            : std::string();
        const game_data::GameCard infestationCard = sandboxCardNamed(next, infestationTitle);
        const bool actionHasInfest = !infestationTitle.empty() && infestationCard.type == "Unit";

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
                    const game_data::DamageResolution damageResolution =
                        game_data::resolveDamageWithBodyguardsAndIntercepts(
                            next.pieces,
                            targetId,
                            attackDamage,
                            action.statusTurns,
                            sandboxDamageRandomEngine,
                            true);
                    const int effectiveTargetId = damageResolution.effectiveTargetId;
                    const game_data::Piece* effectiveTarget =
                        pieceByIdInSnapshot(next, effectiveTargetId);
                    const std::string effectiveTargetName = effectiveTarget == nullptr
                        ? targetName
                        : (effectiveTarget->hidden ? "a hidden " : "") + effectiveTarget->name;
                    damagedTargetNames.push_back(effectiveTargetName);
                    if (actionHasInfest && effectiveTarget != nullptr && !effectiveTarget->isHero)
                    {
                        game_data::Piece* infestTarget =
                            pieceByIdInSnapshotMutable(next, effectiveTargetId);
                        if (infestTarget != nullptr)
                        {
                            infestTarget->infestationTitle = infestationTitle;
                            infestTarget->infestationOwner = attackerOwner;
                        }
                    }
                    for (const game_data::DamageAssignment& assignment : damageResolution.assignments)
                    {
                        game_data::Piece* damagedPiece =
                            pieceByIdInSnapshotMutable(next, assignment.pieceId);
                        if (damagedPiece && damagedPiece->health <= 0)
                        {
                            const PieceDestructionResult destruction = destroySandboxPiece(next, damagedPiece->id);
                            anyTargetReborn = anyTargetReborn || destruction.wasRebirth;
                            anyTargetInfestationSpawned =
                                anyTargetInfestationSpawned || destruction.wasInfestation;
                            anyTargetDestroyed = anyTargetDestroyed || !destruction.replacementSpawned;
                        }
                    }
                    const game_data::PushResult pushResult = game_data::applyActionPush(
                        next.pieces,
                        effectiveTargetId,
                        action.stagingRow,
                        action.stagingColumn,
                        action.push);
                    pushedSquares += pushResult.movedSquares;
                    pushCollisionDamage += pushResult.preventedSquares;
                    rememberRevealedPieces(pushResult.revealedPieceIds);
                    if (game_data::Piece* pushedTarget =
                            pieceByIdInSnapshotMutable(next, effectiveTargetId);
                        pushedTarget && pushedTarget->health <= 0)
                    {
                        const PieceDestructionResult destruction = destroySandboxPiece(next, pushedTarget->id);
                        anyTargetReborn = anyTargetReborn || destruction.wasRebirth;
                        anyTargetInfestationSpawned =
                            anyTargetInfestationSpawned || destruction.wasInfestation;
                        anyTargetDestroyed = anyTargetDestroyed || !destruction.replacementSpawned;
                    }
                    if (action.pull)
                    {
                        const game_data::PullResult pullResult = game_data::applyActionPull(
                            next.pieces,
                            effectiveTargetId,
                            attackerId);
                        pulledSquares += pullResult.movedSquares;
                        rememberRevealedPieces(pullResult.revealedPieceIds);
                    }
                    if (action.control > 0)
                    {
                        if (game_data::Piece* controllableTarget =
                                pieceByIdInSnapshotMutable(next, effectiveTargetId);
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

        std::vector<std::string> revealedNames;
        for (int revealedPieceId : revealedPieceIds)
        {
            if (game_data::Piece* revealed = pieceByIdInSnapshotMutable(next, revealedPieceId))
            {
                revealedNames.push_back(revealed->name);
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
        bool repeatRemaining = false;
        if (continuingRepeat)
        {
            ++acting->repeatActionUses;
            repeatRemaining = acting->repeatActionUses < action.repeat;
        }
        else if (action.repeat > 0)
        {
            acting->repeatActionIndex = action.actionIndex;
            acting->repeatActionState = attackerActionState;
            acting->repeatActionUses = 0;
            repeatRemaining = true;
        }
        if (repeatRemaining)
        {
            acting->actionState = acting->repeatActionState;
        }
        else
        {
            acting->repeatActionIndex = -1;
            acting->repeatActionState = 0;
            acting->repeatActionUses = 0;
            game_data::setPieceActionState(*acting, action.nextState);
        }
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
            if (pulledSquares > 0)
                next.status += " and pulled targets " + std::to_string(pulledSquares) +
                    " square(s)";
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
            if (anyTargetInfestationSpawned)
            {
                next.status += " Infestation spawned a unit!";
            }
            if (!revealedNames.empty())
            {
                next.status += " Hidden " + joinTargets(revealedNames) +
                    " materialized and became Disabled for " +
                    (revealedNames.size() == 1 ? "its" : "their") +
                    " next owner-turn activation.";
            }
            else if (anyTargetWasHidden)
            {
                next.status += " It materialized!";
            }
        }
        else if (!revealedNames.empty())
        {
            std::string revealedList;
            for (std::size_t index = 0; index < revealedNames.size(); ++index)
            {
                if (index > 0)
                    revealedList += index + 1 == revealedNames.size() ? " and " : ", ";
                revealedList += revealedNames[index];
            }
            next.status = attackerName + " bumped into hidden " + revealedList + "! " +
                (revealedNames.size() == 1 ? "It" : "They") +
                " materialized and became Disabled for " +
                (revealedNames.size() == 1 ? "its" : "their") +
                " next owner-turn activation.";
        }
        else
        {
            next.status = attackerName + " moved.";
        }
        if (repeatRemaining)
        {
            next.status += " " + std::to_string(action.repeat - acting->repeatActionUses) +
                " repeat(s) remaining for this action.";
        }
        if (anyTargetDestroyed && game_data::hasKeyword(acting->keywords, "relentless"))
        {
            next.relentlessPieceId = attackerId;
            acting->hasActed = false;
            next.status += " Relentless: it may act again immediately.";
        }
        else if (next.relentlessPieceId == attackerId)
        {
            next.relentlessPieceId = 0;
        }
        updateStoryAfterAction(next);
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
        if (storyMode && storyStage != StoryStage::Objective)
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        game_data::Piece* piece = pieceByIdInSnapshotMutable(next, pieceId);
        if (!piece || !game_data::pieceAbilityAvailable(next.pieces, *piece))
        {
            return;
        }
        const auto pendingRepeat = std::find_if(
            next.pieces.begin(),
            next.pieces.end(),
            [](const game_data::Piece& candidate) { return candidate.repeatActionIndex >= 0; });
        if (pendingRepeat != next.pieces.end())
        {
            next.status = "Finish the repeatable action or advance the turn before using an ability.";
            commitSandboxSnapshot(std::move(next));
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
            actingPiece->hasActed = storyMode;
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
        for (game_data::Piece& piece : next.pieces)
        {
            piece.repeatActionIndex = -1;
            piece.repeatActionState = 0;
            piece.repeatActionUses = 0;
        }
        const int endingPlayer = std::clamp(next.activePlayer, 1, 2);
        for (game_data::Piece& piece : next.pieces)
        {
            if (piece.owner == endingPlayer && piece.sleepTurnsRemaining > 0)
            {
                --piece.sleepTurnsRemaining;
            }
        }
        game_data::applyHealingAuras(next.pieces, endingPlayer);
        game_data::applyRevealKeywords(next.pieces, endingPlayer);

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

    auto refreshStoryReplacementRoles = [&]() {
        if (!storyMode || !storyEngine)
        {
            return;
        }

        const StoryMission& mission = activeStoryMission();
        for (auto& [role, pieceId] : storyRolePieceIds)
        {
            const bool originalStillPresent = std::any_of(
                storyEngine->boardPieces().begin(), storyEngine->boardPieces().end(),
                [&](const game_data::Piece& piece) { return piece.id == pieceId; });
            if (originalStillPresent)
            {
                continue;
            }

            const auto placement = std::find_if(
                mission.pieces.begin(), mission.pieces.end(),
                [&](const StoryPiecePlacement& value) { return value.role == role; });
            if (placement == mission.pieces.end())
            {
                continue;
            }

            const game_data::GameCard originalCard =
                storyCardNamed(std::string(placement->cardTitle));
            if (originalCard.rebirthTitle.empty())
            {
                continue;
            }

            int replacementRow = placement->row;
            int replacementColumn = placement->column;
            const auto previousPiece = std::find_if(
                gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
                [&](const game_data::Piece& piece) { return piece.id == pieceId; });
            if (previousPiece != gameSnapshot.pieces.end())
            {
                replacementRow = previousPiece->row;
                replacementColumn = previousPiece->column;
            }

            const auto replacement = std::find_if(
                storyEngine->boardPieces().begin(), storyEngine->boardPieces().end(),
                [&](const game_data::Piece& piece) {
                    return piece.owner == placement->owner &&
                        piece.name == originalCard.rebirthTitle &&
                        piece.row == replacementRow &&
                        piece.column == replacementColumn;
                });
            if (replacement != storyEngine->boardPieces().end())
            {
                pieceId = replacement->id;
            }
        }
    };

    auto syncStoryEngine = [&]() {
        if (!storyMode || !storyEngine)
        {
            return;
        }

        refreshStoryReplacementRoles();
        game_data::Snapshot next = storyEngine->snapshotFor(1);
        updateStoryAfterAction(next);
        commitLocalSnapshot(std::move(next));
        const bool scriptedMission = !activeStoryMission().script.empty();
        storyAiPending = storyStage == StoryStage::Objective &&
            storyEngine->phase() == game_data::Phase::Playing &&
            storyEngine->currentPlayer() == 2 && !scriptedMission;
        if (storyAiPending)
        {
            storyAiActionAt = animationTime + 0.65f;
        }
    };

    auto updateStoryAi = [&]() {
        // Restart/exit confirmation is a decision boundary. Keep both the
        // scripted auto-chain and ordinary opponent planner frozen behind it;
        // confirming rebuilds the mission, while cancelling resumes the same
        // authored step without a hidden state change.
        if (resignConfirmPopupVisible)
        {
            return;
        }
        if (storyMode && storyEngine && storyStage == StoryStage::Objective &&
            !activeStoryMission().script.empty() && storyPopupPanels.empty() &&
            storyMissionStep >= 0 &&
            storyMissionStep < static_cast<int>(activeStoryMission().script.size()))
        {
            const StoryScriptAction& step =
                activeStoryMission().script[static_cast<std::size_t>(storyMissionStep)];
            const StoryMission& mission = activeStoryMission();
            if (storyScriptActionAutoResolves(mission, step) &&
                animationTime >= storyScriptActionAt)
            {
                const int actorId = step.actorRole.empty()
                    ? 0
                    : storyPieceIdForRole(step.actorRole);
                int targetRow = step.targetRow;
                int targetColumn = step.targetColumn;
                if (!step.targetRole.empty())
                {
                    const int targetId = storyPieceIdForRole(step.targetRole);
                    const auto target = std::find_if(
                        storyEngine->boardPieces().begin(),
                        storyEngine->boardPieces().end(),
                        [&](const game_data::Piece& piece) { return piece.id == targetId; });
                    if (target != storyEngine->boardPieces().end())
                    {
                        targetRow = target->row;
                        targetColumn = target->column;
                    }
                }

                bool accepted = false;
                std::optional<int> scriptedActionIndex;
                std::optional<game_data::Piece> abilityActorBefore;
                if (step.kind == StoryActionKind::Move ||
                    step.kind == StoryActionKind::Attack)
                {
                    const auto actor = std::find_if(
                        storyEngine->boardPieces().begin(),
                        storyEngine->boardPieces().end(),
                        [&](const game_data::Piece& piece) {
                            return piece.id == actorId;
                        });
                    if (actor != storyEngine->boardPieces().end())
                    {
                        scriptedActionIndex =
                            storyExpectedActionProfileIndex(step, *actor);
                        if (scriptedActionIndex)
                        {
                            const std::vector<game_data::Piece> visiblePieces =
                                game_data::piecesVisibleTo(
                                    storyEngine->boardPieces(), step.owner);
                            const auto visibleActor = std::find_if(
                                visiblePieces.begin(), visiblePieces.end(),
                                [&](const game_data::Piece& piece) {
                                    return piece.id == actorId;
                                });
                            const game_data::ActionResolution sourceResolution =
                                visibleActor == visiblePieces.end()
                                ? game_data::ActionResolution{}
                                : game_data::resolvePieceAction(
                                      visiblePieces,
                                      storyEngine->boardHoles(),
                                      *visibleActor,
                                      targetRow,
                                      targetColumn,
                                      false,
                                      *scriptedActionIndex);
                            const StoryActionKind resolvedSourceKind =
                                sourceResolution.attacks
                                ? StoryActionKind::Attack
                                : StoryActionKind::Move;
                            if (!sourceResolution.legal ||
                                !storySelectedPieceActionMatches(
                                    step,
                                    resolvedSourceKind,
                                    *actor,
                                    *scriptedActionIndex))
                            {
                                scriptedActionIndex.reset();
                            }
                        }
                    }
                }
                else if (step.kind == StoryActionKind::UseAbility)
                {
                    const auto actor = std::find_if(
                        storyEngine->boardPieces().begin(),
                        storyEngine->boardPieces().end(),
                        [&](const game_data::Piece& piece) {
                            return piece.id == actorId;
                        });
                    if (actor != storyEngine->boardPieces().end() &&
                        storyAbilityStepMatches(step, *actor))
                    {
                        abilityActorBefore = *actor;
                    }
                }
                switch (step.kind)
                {
                case StoryActionKind::Move:
                    accepted = scriptedActionIndex && storyEngine->movePiece(
                        step.owner,
                        actorId,
                        targetRow,
                        targetColumn,
                        *scriptedActionIndex);
                    break;
                case StoryActionKind::Attack:
                    accepted = scriptedActionIndex && storyEngine->attackPiece(
                        step.owner,
                        actorId,
                        targetRow,
                        targetColumn,
                        *scriptedActionIndex);
                    break;
                case StoryActionKind::UseAbility:
                    accepted = abilityActorBefore &&
                        storyEngine->useAbility(step.owner, actorId);
                    break;
                case StoryActionKind::DrawCard:
                    accepted = storyEngine->drawCard(step.owner);
                    break;
                case StoryActionKind::ChooseForesight:
                {
                    const auto& choices =
                        storyEngine->playerState(step.owner).foresightChoices;
                    const auto found = std::find_if(
                        choices.begin(), choices.end(), [&](const game_data::GameCard& card) {
                            return card.title == step.cardTitle;
                        });
                    accepted = found != choices.end() && storyEngine->chooseForesightCard(
                        step.owner,
                        static_cast<int>(std::distance(choices.begin(), found)));
                    break;
                }
                case StoryActionKind::EndTurn:
                    accepted = storyEngine->endTurn(step.owner);
                    break;
                default:
                    break;
                }

                if (!accepted)
                {
                    storyStage = StoryStage::Failed;
                    gameSnapshot.status =
                        "Mission data error: a scripted automatic action was no longer legal.";
                    endTurnButton.setLabel("Retry Mission");
                    return;
                }
                if (abilityActorBefore &&
                    !storyAbilityOutcomeMatches(
                        step,
                        *abilityActorBefore,
                        storyEngine->boardPieces(),
                        storyEngine->commandingPiece()))
                {
                    storyStage = StoryStage::Failed;
                    gameSnapshot.status =
                        "Mission data error: a scripted automatic ability did not produce its authored result.";
                    endTurnButton.setLabel("Retry Mission");
                    return;
                }
                const bool completeNow = advanceStoryScript();
                syncStoryEngine();
                if (completeNow && storyStage == StoryStage::Objective)
                {
                    completeStoryMission(gameSnapshot);
                }
                return;
            }
        }
        if (storyMode && storyEngine && storyAiPending &&
            storyEngine->hasPendingForesightChoice(2))
        {
            storyEngine->chooseForesightCard(2, chooseAiForesightCard(*storyEngine, 2));
            syncStoryEngine();
            return;
        }
        if (pendingStoryAi)
        {
            if (pendingStoryAi->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                return;
            }

            const auto [generation, action] = pendingStoryAi->get();
            pendingStoryAi.reset();
            if (generation == storyGeneration && storyMode && storyEngine && storyAiPending &&
                storyStage == StoryStage::Objective &&
                storyEngine->phase() == game_data::Phase::Playing &&
                storyEngine->currentPlayer() == 2)
            {
                if (!applyAiAction(*storyEngine, 2, action))
                {
                    storyAiPending = false;
                    storyStage = StoryStage::Failed;
                    gameSnapshot.status =
                        "Opponent AI error: its planned action was not legal in the current game state.";
                    endTurnButton.setLabel("Retry Mission");
                    return;
                }
                syncStoryEngine();
            }
            return;
        }

        if (!storyMode || !storyEngine || !storyAiPending || animationTime < storyAiActionAt)
        {
            return;
        }
        if (storyStage != StoryStage::Objective ||
            storyEngine->phase() != game_data::Phase::Playing ||
            storyEngine->currentPlayer() != 2)
        {
            storyAiPending = false;
            return;
        }

        const std::uint64_t generation = storyGeneration;
        const int searchDepth =
            storyAiSearchDepth(storyCampaign, activeStoryMission().id);
        GameEngine engineCopy = *storyEngine;
        pendingStoryAi.emplace(std::async(
            std::launch::async,
            [generation, searchDepth, engine = std::move(engineCopy)]() mutable {
                // Story encounters are authored teaching positions, so the
                // opponent scales from a one-ply lesson partner to the same
                // four-ply search used by a full-strength match opponent.
                return std::pair{
                    generation, chooseAiAction(engine, 2, searchDepth)};
            }));
    };

    auto sendGamePacket = [&](sf::Packet& packet) {
        if (activeGameSocket)
        {
            [[maybe_unused]] auto result = activeGameSocket->send(packet);
        }
    };

    auto settleStoryAction = [&]
        (bool accepted,
         std::optional<game_data::Piece> abilityActorBefore = std::nullopt) {
        bool completeNow = false;
        if (accepted && !activeStoryMission().script.empty())
        {
            const StoryScriptAction& completedStep = activeStoryMission().script[
                static_cast<std::size_t>(storyMissionStep)];
            if (abilityActorBefore &&
                !storyAbilityOutcomeMatches(
                    completedStep,
                    *abilityActorBefore,
                    storyEngine->boardPieces(),
                    storyEngine->commandingPiece()))
            {
                accepted = false;
                storyStage = StoryStage::Failed;
                gameSnapshot.status =
                    "Mission data error: the ability did not produce its authored result.";
                endTurnButton.setLabel("Retry Mission");
            }
            if (completedStep.kind == StoryActionKind::PlayCard &&
                !completedStep.effectRole.empty() && accepted)
            {
                const auto spawned = std::find_if(
                    storyEngine->boardPieces().begin(),
                    storyEngine->boardPieces().end(),
                    [&](const game_data::Piece& piece) {
                        return piece.owner == completedStep.owner &&
                            piece.name == completedStep.cardTitle &&
                            piece.row == completedStep.targetRow &&
                            piece.column == completedStep.targetColumn &&
                            piece.hasActed;
                    });
                if (spawned == storyEngine->boardPieces().end())
                {
                    accepted = false;
                    storyStage = StoryStage::Failed;
                    gameSnapshot.status =
                        "Mission data error: the deployed unit did not arrive exhausted on its authored square.";
                    endTurnButton.setLabel("Retry Mission");
                }
                else
                {
                    storyRolePieceIds.emplace_back(completedStep.effectRole, spawned->id);
                }
            }
            if (accepted)
            {
                completeNow = advanceStoryScript();
            }
        }
        syncStoryEngine();
        if (completeNow && storyStage == StoryStage::Objective)
        {
            completeStoryMission(gameSnapshot);
        }
    };

    auto sendPlaceHero = [&](int heroIndex, int row, int column) {
        if (storyMode && storyEngine && activeStoryMission().standardMatch)
        {
            if (!storyEngine->placeHero(1, heroIndex, row, column))
            {
                storyCorrection =
                    "Place that hero on empty squares inside your highlighted two-by-four home zone.";
                syncStoryEngine();
                return;
            }
            if (storyEngine->phase() == game_data::Phase::HeroPlacement &&
                storyEngine->playerState(1).heroesToPlace.empty())
            {
                placeAiHeroes(*storyEngine, 2);
            }
            syncStoryEngine();
            return;
        }
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
        if (storyMode && storyEngine)
        {
            const std::string cardTitle = handIndex >= 0 &&
                    handIndex < static_cast<int>(gameSnapshot.hand.size())
                ? gameSnapshot.hand[static_cast<std::size_t>(handIndex)].title
                : std::string();
            if (!storyActionAllowed(
                    StoryActionKind::PlayCard, 1, 0, row, column, cardTitle))
            {
                return;
            }
            settleStoryAction(storyEngine->playCard(1, handIndex, row, column));
            return;
        }
        if (sandboxMode)
        {
            sandboxPlayCard(handIndex, row, column);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::PlayCard) << handIndex << row << column;
        sendGamePacket(packet);
    };

    auto sendMovePiece = [&](int pieceId, int row, int column,
                             int selectedActionIndex = -1) {
        if (storyMode && storyEngine)
        {
            if (!storyActionAllowed(
                    StoryActionKind::Move,
                    1,
                    pieceId,
                    row,
                    column,
                    {},
                    selectedActionIndex))
            {
                return;
            }
            settleStoryAction(storyEngine->movePiece(
                1, pieceId, row, column, selectedActionIndex));
            return;
        }
        if (sandboxMode)
        {
            sandboxActWithPiece(pieceId, row, column, selectedActionIndex);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::MovePiece)
               << pieceId << row << column;
        if (selectedActionIndex >= 0)
        {
            packet << network::encodeActionProfileSelection(selectedActionIndex);
        }
        sendGamePacket(packet);
    };

    auto sendAttackPiece = [&](int attackerId, int row, int column,
                               int selectedActionIndex = -1) {
        if (storyMode && storyEngine)
        {
            if (!storyActionAllowed(
                    StoryActionKind::Attack,
                    1,
                    attackerId,
                    row,
                    column,
                    {},
                    selectedActionIndex))
            {
                return;
            }
            settleStoryAction(storyEngine->attackPiece(
                1, attackerId, row, column, selectedActionIndex));
            return;
        }
        if (sandboxMode)
        {
            sandboxActWithPiece(attackerId, row, column, selectedActionIndex);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::AttackPiece)
               << attackerId << row << column;
        if (selectedActionIndex >= 0)
        {
            packet << network::encodeActionProfileSelection(selectedActionIndex);
        }
        sendGamePacket(packet);
    };

    auto requestPieceAction = [&](int pieceId, int row, int column) {
        const game_data::Piece* piece = gamePieceById(pieceId);
        if (piece == nullptr)
        {
            return;
        }
        const int requiredActionIndex = piece->repeatActionIndex >= 0
            ? piece->repeatActionIndex
            : -1;
        const game_data::PieceActionOutcome outcome = game_data::resolvePieceActionThroughHidden(
            gameSnapshot.pieces,
            gameSnapshot.holes,
            *piece,
            row,
            column,
            requiredActionIndex);
        if (!outcome.action.legal)
        {
            if (storyMode && storyStage == StoryStage::Objective &&
                storyPopupPanels.empty())
            {
                const StoryMission& mission = activeStoryMission();
                const game_data::Piece* friendlyBlocker = nullptr;
                for (int targetRow = row;
                     friendlyBlocker == nullptr && targetRow < row + piece->height;
                     ++targetRow)
                {
                    for (int targetColumn = column;
                         targetColumn < column + piece->width;
                         ++targetColumn)
                    {
                        const game_data::Piece* occupant = game_data::findPieceAt(
                            gameSnapshot.pieces, targetRow, targetColumn);
                        if (occupant != nullptr && occupant->id != piece->id &&
                            occupant->owner == piece->owner)
                        {
                            friendlyBlocker = occupant;
                            break;
                        }
                    }
                }
                const bool hasExpectedStep =
                    !mission.script.empty() && storyMissionStep >= 0 &&
                    storyMissionStep < static_cast<int>(mission.script.size());
                if (friendlyBlocker != nullptr)
                {
                    const std::string squareName =
                        game_data::inBounds(row, column)
                        ? std::string(1, static_cast<char>('A' + column)) +
                            std::to_string(row + 1)
                        : std::string("That square");
                    storyCorrection = squareName + " is occupied by " +
                        friendlyBlocker->name + ", so " + piece->name +
                        " cannot finish there.";
                    if (hasExpectedStep)
                    {
                        const StoryScriptAction& expected =
                            mission.script[static_cast<std::size_t>(storyMissionStep)];
                        if (!expected.correction.empty())
                        {
                            storyCorrection += " " + std::string(expected.correction);
                        }
                    }
                }
                else if (hasExpectedStep)
                {
                    const StoryScriptAction& expected =
                        mission.script[static_cast<std::size_t>(storyMissionStep)];
                    storyCorrection = expected.correction.empty()
                        ? std::string("That unit has no legal action on that square. Follow the glowing ACT and TARGET markers.")
                        : std::string(expected.correction);
                }
                else
                {
                    storyCorrection =
                        "That unit has no legal action on that square. Inspect it with double-click or focus + I.";
                }
            }
            return;
        }

        std::vector<int> legalActionIndices;
        if (requiredActionIndex >= 0)
        {
            legalActionIndices.push_back(requiredActionIndex);
        }
        else
        {
            for (std::size_t index = 0; index < piece->actions.size(); ++index)
            {
                const game_data::PieceActionOutcome candidate =
                    game_data::resolvePieceActionThroughHidden(
                        gameSnapshot.pieces,
                        gameSnapshot.holes,
                        *piece,
                        row,
                        column,
                        static_cast<int>(index));
                if (candidate.action.legal)
                {
                    legalActionIndices.push_back(static_cast<int>(index));
                }
            }
        }

        if (legalActionIndices.size() > 1)
        {
            pendingPieceActionChoice = PendingPieceActionChoice{
                pieceId, row, column, std::move(legalActionIndices), 0};
            storyCorrection.clear();
            return;
        }

        const int selectedActionIndex = legalActionIndices.empty()
            ? outcome.action.actionIndex
            : legalActionIndices.front();
        const game_data::PieceActionOutcome selectedOutcome =
            game_data::resolvePieceActionThroughHidden(
                gameSnapshot.pieces,
                gameSnapshot.holes,
                *piece,
                row,
                column,
                selectedActionIndex);
        const std::vector<game_data::Piece> sourceVisiblePieces =
            game_data::piecesVisibleTo(gameSnapshot.pieces, piece->owner);
        const game_data::ActionResolution selectedSourceAction =
            game_data::resolvePieceAction(
                sourceVisiblePieces,
                gameSnapshot.holes,
                *piece,
                row,
                column,
                false,
                selectedActionIndex);
        if (!selectedSourceAction.legal)
        {
            gameSnapshot.status =
                "That printed action is no longer legal on the chosen square.";
            return;
        }
        if (selectedSourceAction.attacks)
        {
            sendAttackPiece(pieceId, row, column, selectedActionIndex);
        }
        else
        {
            sendMovePiece(pieceId, row, column, selectedActionIndex);
        }
    };

    auto submitPendingPieceActionChoice = [&](int optionIndex) {
        if (!pendingPieceActionChoice || optionIndex < 0 ||
            optionIndex >= static_cast<int>(
                pendingPieceActionChoice->actionIndices.size()))
        {
            return;
        }
        const PendingPieceActionChoice choice = *pendingPieceActionChoice;
        pendingPieceActionChoice.reset();
        const game_data::Piece* piece = gamePieceById(choice.pieceId);
        if (piece == nullptr)
        {
            return;
        }
        const int selectedActionIndex =
            choice.actionIndices[static_cast<std::size_t>(optionIndex)];
        const game_data::PieceActionOutcome selectedOutcome =
            game_data::resolvePieceActionThroughHidden(
                gameSnapshot.pieces,
                gameSnapshot.holes,
                *piece,
                choice.row,
                choice.column,
                selectedActionIndex);
        if (!selectedOutcome.action.legal)
        {
            gameSnapshot.status =
                "That printed action is no longer legal on the chosen square.";
            return;
        }
        const std::vector<game_data::Piece> sourceVisiblePieces =
            game_data::piecesVisibleTo(gameSnapshot.pieces, piece->owner);
        const game_data::ActionResolution selectedSourceAction =
            game_data::resolvePieceAction(
                sourceVisiblePieces,
                gameSnapshot.holes,
                *piece,
                choice.row,
                choice.column,
                false,
                selectedActionIndex);
        if (!selectedSourceAction.legal)
        {
            gameSnapshot.status =
                "That printed action is no longer legal on the chosen square.";
            return;
        }
        if (selectedSourceAction.attacks)
        {
            sendAttackPiece(
                choice.pieceId,
                choice.row,
                choice.column,
                selectedActionIndex);
        }
        else
        {
            sendMovePiece(
                choice.pieceId,
                choice.row,
                choice.column,
                selectedActionIndex);
        }
    };

    auto sendUseAbility = [&](int pieceId) {
        if (storyMode && storyEngine)
        {
            if (!storyActionAllowed(StoryActionKind::UseAbility, 1, pieceId, -1, -1))
            {
                return;
            }
            const auto found = std::find_if(
                storyEngine->boardPieces().begin(),
                storyEngine->boardPieces().end(),
                [&](const game_data::Piece& piece) { return piece.id == pieceId; });
            const std::string pieceName = found == storyEngine->boardPieces().end()
                ? std::string()
                : found->name;
            const std::string ability = found == storyEngine->boardPieces().end()
                ? std::string()
                : found->ability;
            const std::optional<game_data::Piece> abilityActorBefore =
                found == storyEngine->boardPieces().end()
                ? std::nullopt
                : std::optional<game_data::Piece>(*found);
            const bool accepted = storyEngine->useAbility(1, pieceId);
            if (accepted)
            {
                storyUsedAim = storyUsedAim ||
                    (pieceName == "Goblin Sharpshooter" && ability == "transform");
                storyUsedHide = storyUsedHide ||
                    (pieceName == "Goblin Ambusher" && ability == "dematerialize");
                storyUsedSummon = storyUsedSummon ||
                    (pieceName == "Blackthorn Foreman" && ability == "summon");
            }
            settleStoryAction(accepted, abilityActorBefore);
            return;
        }
        if (sandboxMode)
        {
            sandboxUseAbility(pieceId);
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::UseAbility) << pieceId;
        sendGamePacket(packet);
    };

    auto sendChooseForesightCard = [&](int choiceIndex) {
        if (storyMode && storyEngine)
        {
            const std::string cardTitle = choiceIndex >= 0 &&
                    choiceIndex < static_cast<int>(gameSnapshot.foresightChoices.size())
                ? gameSnapshot.foresightChoices[static_cast<std::size_t>(choiceIndex)].title
                : std::string();
            if (!storyActionAllowed(
                    StoryActionKind::ChooseForesight, 1, 0, -1, -1, cardTitle))
            {
                return;
            }
            settleStoryAction(storyEngine->chooseForesightCard(1, choiceIndex));
            return;
        }
        if (sandboxMode)
        {
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::ChooseForesightCard) << choiceIndex;
        sendGamePacket(packet);
    };
    auto sendDrawCard = [&]() {
        if (storyMode && storyEngine)
        {
            if (!storyActionAllowed(StoryActionKind::DrawCard, 1, 0, -1, -1))
            {
                return;
            }
            settleStoryAction(storyEngine->drawCard(1));
            return;
        }
        if (sandboxMode)
        {
            return;
        }
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::DrawCard);
        sendGamePacket(packet);
    };
    auto sendEndTurn = [&]() {
        if (storyMode && storyEngine)
        {
            if (!storyActionAllowed(StoryActionKind::EndTurn, 1, 0, -1, -1))
            {
                return;
            }
            settleStoryAction(storyEngine->endTurn(1));
            return;
        }
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
        if (storyMode && storyEngine)
        {
            const std::string cardTitle = handIndex >= 0 &&
                    handIndex < static_cast<int>(gameSnapshot.hand.size())
                ? gameSnapshot.hand[static_cast<std::size_t>(handIndex)].title
                : std::string();
            if (!storyActionAllowed(
                    StoryActionKind::DiscardCard, 1, 0, -1, -1, cardTitle))
            {
                return;
            }
            settleStoryAction(storyEngine->discardCard(1, handIndex));
            return;
        }
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
        const bool pendingRepeat = me >= 1 && me <= 2 && std::any_of(
            gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
            [&](const game_data::Piece& piece) {
                return piece.owner == me && piece.repeatActionIndex >= 0;
            });
        if (gameSnapshot.relentlessPieceId != 0 ||
            gameSnapshot.commandingPieceId != 0 || pendingRepeat)
        {
            return false;
        }
        if (me < 1 || me > 2 || gameSnapshot.activePlayer != me)
        {
            return false;
        }
        const game_data::PlayerSnapshot& mine = gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        return mine.discardsThisTurn < game_data::MaxDiscardsPerTurn && !gameSnapshot.hand.empty();
    };

    auto playerCanDrawCard = [&]() {
        if (!haveSnapshot || sandboxMode ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing ||
            !gameSnapshot.foresightChoices.empty())
        {
            return false;
        }
        const int me = gameSnapshot.yourPlayer;
        if (me < 1 || me > 2 || gameSnapshot.activePlayer != me)
        {
            return false;
        }
        const bool pendingRepeat = std::any_of(
            gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
            [&](const game_data::Piece& piece) {
                return piece.owner == me && piece.repeatActionIndex >= 0;
            });
        if (pendingRepeat || gameSnapshot.relentlessPieceId != 0 ||
            gameSnapshot.commandingPieceId != 0)
        {
            return false;
        }
        const game_data::PlayerSnapshot& mine =
            gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        return mine.drawPileCount > 0 &&
            mine.resources >= game_data::DrawCardResourceCost &&
            static_cast<int>(gameSnapshot.hand.size()) < game_data::MaxHandSize;
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
        const int me = gameSnapshot.yourPlayer;
        const bool pendingRepeat = me >= 1 && me <= 2 && std::any_of(
            gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
            [&](const game_data::Piece& piece) {
                return piece.owner == me && piece.repeatActionIndex >= 0;
            });
        if (gameSnapshot.relentlessPieceId != 0 ||
            gameSnapshot.commandingPieceId != 0 || pendingRepeat)
        {
            return false;
        }

        const game_data::GameCard& card = gameSnapshot.hand[handIndex];
        selectedPieceId.reset();
        if (card.type == "Spell" && !game_data::isSupportedSpellEffect(card))
        {
            gameSnapshot.status =
                "This spell has no defined game effect and cannot be played.";
            selectedHandIndex.reset();
            return true;
        }
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
                if (storyMode)
                {
                    storyCorrection =
                        "Finish the current unit's extra action or End Turn before using a card.";
                }
                return;
            }
            const game_data::GameCard& card = gameSnapshot.hand[*handIndex];
            if (card.type == "Spell" && !game_data::isSupportedSpellEffect(card))
            {
                gameSnapshot.status =
                    "This spell has no defined game effect and cannot be played.";
                selectedHandIndex.reset();
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
        else if (piece && storyMode && storyStage == StoryStage::Objective)
        {
            if (piece->owner != me)
            {
                storyCorrection =
                    "That is an enemy unit. Select one of your own ready units to act.";
            }
            else if (piece->disabledTurns > 0)
            {
                storyCorrection =
                    piece->name + " is Disabled and must miss this activation.";
            }
            else if (piece->growTurnsRemaining > 0)
            {
                storyCorrection =
                    piece->name + " is not ready to act yet.";
            }
            else if (piece->hasActed)
            {
                storyCorrection =
                    piece->name + " has already acted this turn. Use End Turn.";
            }
            else
            {
                storyCorrection =
                    "The normal piece action for this turn is already spent. Use End Turn.";
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
            else if (storyMode)
            {
                storyCorrection =
                    "Discard is not available now. Follow the current lesson step or End Turn.";
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
            if (storyMode && storyStage == StoryStage::Objective)
            {
                storyCorrection = gameDragKind == GameDragKind::HandCard
                    ? "Drop the card on a legal board square, or on the discard area when Discard is available."
                    : "Drop the unit on a board square. Guided missions mark the required TARGET.";
            }
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
                // Keep the legality check in requestPieceAction. Story Mode uses
                // that single gate to explain an illegal on-board drop instead
                // of silently snapping the piece back before feedback can run.
                requestPieceAction(piece->id, row, column);
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
        leaveGameButton.setLabelSize(type::Body);
        leaveGameButton.setPosition({GameActionButtonX, GameLeaveButtonY});
        leaveGameButton.setSize({GameLeaveButtonWidth, GameActionButtonHeight});
        resignConfirmPopupVisible = false;
        gameConfirmationAction = GameConfirmationAction::Resign;
        haveSnapshot = false;
        gameSnapshot = {};
        clockWarningTracker.reset();
        displayedClockWarning.reset();
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        pendingPieceActionChoice.reset();
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
        storyEngine.reset();
        storyAiPending = false;
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

    const auto canContinueStoryWithoutMastery = [&]() {
        return storyMode && haveSnapshot && storyStage == StoryStage::Failed &&
            storyContinueWithoutMasteryAvailable(
                activeStoryMission(), storyGenuineDefeatCount);
    };

    auto continueStoryWithoutMastery = [&]() {
        if (!canContinueStoryWithoutMastery())
        {
            return;
        }

        recordStoryMissionProgress(
            loggedInUsername, storyCampaign, storyMissionIndex, false);
        refreshStoryCampaignProgress(storyCampaign);
        storyCompletedCount =
            storyCampaignProgress[storyProgressIndex(storyCampaign)];
        const bool hasNext = storyMissionIndex + 1 <
            static_cast<int>(storyMissions(storyCampaign).size());
        leaveGame();
        if (hasNext)
        {
            storyComicPage = 0;
            showStoryIntro();
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

        if (!gameSnapshot.foresightChoices.empty())
        {
            if (const std::optional<std::size_t> choiceIndex = foresightChoiceAtPixel(clickPos))
            {
                sendChooseForesightCard(static_cast<int>(*choiceIndex));
            }
            return;
        }
        const std::optional<std::pair<int, int>> square = squareAtPixel(clickPos);
        if (phase == game_data::Phase::HeroPlacement)
        {
            if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
            {
                handleHandCardClick(*handIndex);
            }
            return;
        }

        // Playing phase: only the active player may act.
        if (isDrawPileAtPixel(clickPos))
        {
            if (playerCanDrawCard())
            {
                selectedHandIndex.reset();
                sendDrawCard();
            }
            return;
        }
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

        const game_data::Piece* clicked = gamePieceAt(square->first, square->second);

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

        // Board actions are drag-only. A click can select a card or piece for
        // previews and ability inspection, but it must never submit a card
        // placement, move, or attack. Those actions are submitted exclusively
        // by finishGameDrag() on mouse release after the drag threshold.
        if (selectedHandIndex)
        {
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

    const auto drawStorySpeakerPortraitInset = [&](
        std::string_view scenarioArtPath,
        std::string_view speakerArtPath,
        sf::Vector2f center,
        float radius) {
        // Commissioned scene art establishes place and action, while the card
        // portrait keeps a changing speaker visually identifiable. Only add
        // the medallion when it contributes a genuinely different image.
        if (scenarioArtPath.empty() || speakerArtPath.empty() ||
            scenarioArtPath == speakerArtPath)
        {
            return;
        }
        sf::Texture* portraitTexture = textures.load(std::string(speakerArtPath));
        if (portraitTexture == nullptr)
        {
            return;
        }

        sf::CircleShape shadow(radius + 3.0f, 48);
        shadow.setOrigin({radius + 3.0f, radius + 3.0f});
        shadow.setPosition(center + sf::Vector2f(2.0f, 3.0f));
        shadow.setFillColor(sf::Color(2, 5, 6, 188));
        window.draw(shadow);

        sf::CircleShape bezel(radius, 48);
        bezel.setOrigin({radius, radius});
        bezel.setPosition(center);
        bezel.setFillColor(sf::Color(53, 39, 25, 255));
        bezel.setOutlineThickness(2.0f);
        bezel.setOutlineColor(sf::Color(232, 190, 102, 245));
        window.draw(bezel);

        const float portraitRadius = radius - 5.0f;
        sf::CircleShape portrait(portraitRadius, 48);
        portrait.setOrigin({portraitRadius, portraitRadius});
        portrait.setPosition(center);
        portrait.setFillColor(sf::Color::White);
        portrait.setTexture(portraitTexture);
        const sf::Vector2u textureSize = portraitTexture->getSize();
        const int shortestSide = static_cast<int>(std::min(textureSize.x, textureSize.y));
        const bool characterPortrait =
            speakerArtPath.starts_with("cards/") ||
            speakerArtPath.starts_with("characters/");
        const int cropSide = characterPortrait
            ? std::max(1, static_cast<int>(static_cast<float>(shortestSide) * 0.58f))
            : shortestSide;
        if (cropSide > 0)
        {
            const int availableY = static_cast<int>(textureSize.y) - cropSide;
            const int portraitY = characterPortrait
                ? std::min(
                    availableY,
                    static_cast<int>(static_cast<float>(textureSize.y) * 0.035f))
                : availableY / 2;
            portrait.setTextureRect(sf::IntRect(
                {static_cast<int>(textureSize.x - cropSide) / 2,
                 portraitY},
                {cropSide, cropSide}));
        }
        window.draw(portrait);

        sf::CircleShape innerRing(portraitRadius + 0.5f, 48);
        innerRing.setOrigin({portraitRadius + 0.5f, portraitRadius + 0.5f});
        innerRing.setPosition(center);
        innerRing.setFillColor(sf::Color::Transparent);
        innerRing.setOutlineThickness(1.5f);
        innerRing.setOutlineColor(sf::Color(255, 225, 157, 190));
        window.draw(innerRing);
    };

    #include "screens/game_screen.inl"

    #include "screens/story_select_screen.inl"

    #include "screens/story_mission_select_screen.inl"

    #include "screens/story_intro_screen.inl"

    #include "screens/deck_select_screen.inl"

    const auto actionChoiceDialogHeight = [&]() {
        const std::size_t count = pendingPieceActionChoice
            ? pendingPieceActionChoice->actionIndices.size()
            : 0;
        return ActionChoiceHeaderHeight +
            ActionChoiceRowHeight * static_cast<float>(count) +
            ActionChoiceFooterHeight;
    };
    const auto actionChoiceDialogY = [&]() {
        return (ui_canvas::Height - actionChoiceDialogHeight()) * 0.5f;
    };
    const auto actionChoiceOptionAt = [&](sf::Vector2f point)
        -> std::optional<int> {
        if (!pendingPieceActionChoice)
        {
            return std::nullopt;
        }
        const float firstY = actionChoiceDialogY() + ActionChoiceHeaderHeight;
        const float left = ActionChoiceDialogX + ActionChoiceRowInset;
        const float width = ActionChoiceDialogWidth - ActionChoiceRowInset * 2.0f;
        for (std::size_t index = 0;
             index < pendingPieceActionChoice->actionIndices.size();
             ++index)
        {
            const float top = firstY +
                static_cast<float>(index) * ActionChoiceRowHeight + 4.0f;
            if (isInsideRect(
                    point,
                    left,
                    top,
                    width,
                    ActionChoiceRowHeight - 8.0f))
            {
                return static_cast<int>(index);
            }
        }
        return std::nullopt;
    };
    const auto actionChoiceCancelRect = [&]() {
        const float dialogY = actionChoiceDialogY();
        const float height = actionChoiceDialogHeight();
        return sf::FloatRect(
            {ActionChoiceDialogX + ActionChoiceDialogWidth - 104.0f,
             dialogY + height - 39.0f},
            {86.0f, 27.0f});
    };
    const auto drawActionChoicePopup = [&]() {
        if (!pendingPieceActionChoice)
        {
            return;
        }
        const game_data::Piece* piece =
            gamePieceById(pendingPieceActionChoice->pieceId);
        if (piece == nullptr)
        {
            pendingPieceActionChoice.reset();
            return;
        }

        const float dialogHeight = actionChoiceDialogHeight();
        const float dialogY = actionChoiceDialogY();
        const sf::Vector2f pointer = window.mapPixelToCoords(
            sf::Mouse::getPosition(window));
        sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
        overlay.setPosition({ui_canvas::Left, 0.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 188));
        window.draw(overlay);
        drawBeveledPlate(
            window,
            {ActionChoiceDialogX, dialogY},
            {ActionChoiceDialogWidth, dialogHeight},
            sf::Color(20, 25, 27, 252),
            sf::Color(213, 170, 84),
            true,
            11.0f);

        drawCenteredText(
            window,
            displayFontOr(font),
            "Choose Printed Action",
            23,
            {ActionChoiceDialogX + ActionChoiceDialogWidth * 0.5f,
             dialogY + 24.0f},
            sf::Color(250, 232, 188));
        const std::string squareName =
            std::string(1, static_cast<char>('A' + pendingPieceActionChoice->column)) +
            std::to_string(pendingPieceActionChoice->row + 1);
        drawCenteredText(
            window,
            font,
            piece->name + " has multiple legal actions on " + squareName + ".",
            13,
            {ActionChoiceDialogX + ActionChoiceDialogWidth * 0.5f,
             dialogY + 54.0f},
            sf::Color(207, 211, 211));

        const float rowX = ActionChoiceDialogX + ActionChoiceRowInset;
        const float rowWidth = ActionChoiceDialogWidth - ActionChoiceRowInset * 2.0f;
        const float firstY = dialogY + ActionChoiceHeaderHeight;
        for (std::size_t option = 0;
             option < pendingPieceActionChoice->actionIndices.size();
             ++option)
        {
            const int actionIndex = pendingPieceActionChoice->actionIndices[option];
            if (actionIndex < 0 ||
                actionIndex >= static_cast<int>(piece->actions.size()))
            {
                continue;
            }
            const game_data::ActionProfile& profile =
                piece->actions[static_cast<std::size_t>(actionIndex)];
            const ActionDescription description =
                actionDescription(profile, static_cast<std::size_t>(actionIndex));
            const float rowY = firstY +
                static_cast<float>(option) * ActionChoiceRowHeight + 4.0f;
            const bool hovered = isInsideRect(
                pointer,
                rowX,
                rowY,
                rowWidth,
                ActionChoiceRowHeight - 8.0f);
            const bool focused =
                pendingPieceActionChoice->focusedOption == static_cast<int>(option);
            drawBeveledPlate(
                window,
                {rowX, rowY},
                {rowWidth, ActionChoiceRowHeight - 8.0f},
                hovered || focused
                    ? sf::Color(48, 69, 66, 252)
                    : sf::Color(28, 36, 38, 248),
                hovered
                    ? sf::Color(148, 230, 204)
                    : focused
                        ? sf::Color(226, 192, 111)
                        : sf::Color(95, 110, 108),
                hovered || focused,
                7.0f);

            const sf::Vector2f numberCenter{rowX + 24.0f, rowY + 27.0f};
            drawLeagueSigil(numberCenter, 13.0f, sf::Color(184, 137, 71));
            drawCenteredText(
                window,
                font,
                std::to_string(option + 1),
                12,
                numberCenter,
                sf::Color(255, 244, 210));
            drawText(
                window,
                displayFontOr(font),
                description.name,
                17,
                {rowX + 48.0f, rowY + 7.0f},
                sf::Color(248, 239, 216),
                rowWidth - 60.0f);

            std::string summary = description.type + "  " + description.range;
            const auto appendAmount = [&](std::string_view label, int amount) {
                if (amount > 0)
                {
                    summary += "  " + std::string(label) + " " +
                        std::to_string(amount);
                }
            };
            appendAmount("DMG", description.damage);
            appendAmount("HEAL", description.heal);
            appendAmount("DISABLE", description.stun);
            appendAmount("COOLDOWN", description.cooldown);
            appendAmount("CONTROL", description.control);
            if (description.pull) summary += "  PULL";
            if (description.push > 0)
                summary += "  PUSH " + std::to_string(description.push);
            if (!description.infest.empty())
                summary += "  INFEST " + description.infest;
            drawText(
                window,
                font,
                summary,
                12,
                {rowX + 48.0f, rowY + 31.0f},
                sf::Color(164, 210, 198),
                rowWidth - 60.0f);
        }

        const sf::FloatRect cancel = actionChoiceCancelRect();
        const bool cancelHovered = cancel.contains(pointer);
        drawBeveledPlate(
            window,
            cancel.position,
            cancel.size,
            cancelHovered ? sf::Color(68, 46, 43) : sf::Color(38, 39, 40),
            cancelHovered ? sf::Color(235, 145, 126) : sf::Color(130, 125, 116),
            cancelHovered,
            6.0f);
        drawCenteredText(
            window,
            font,
            "Cancel",
            13,
            cancel.position + cancel.size * 0.5f,
            sf::Color(236, 224, 202));
        drawText(
            window,
            font,
            "Up/Down + Enter",
            12,
            {ActionChoiceDialogX + 20.0f,
             dialogY + dialogHeight - 31.0f},
            sf::Color(162, 165, 163));
    };

    // ---- offline screenshot harness ---------------------------------------
    // Populates the account state the services would normally supply, then
    // walks the requested screens writing one PNG each. See
    // client_ui_capture.hpp.
    std::size_t captureIndex = 0;
    int captureFramesOnScreen = 0;
    bool captureScreenReady = false;
    bool captureCompleted = false;
    std::vector<std::string> successfulCaptureFiles;
    if (captureRequest)
    {
        successfulCaptureFiles.reserve(captureRequest->screens.size());
    }
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
        deckNameInput.setContent(editingDeck.name);
        // The roster's detail panel needs a subject; a player arriving from the
        // menu has their first deck highlighted the same way.
        selectedDeck = playerDecks.empty() ? std::optional<std::size_t>{} : std::optional<std::size_t>{0};

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

    // Fabricates a mid-match snapshot for the game-* capture screens. Offline the
    // only reachable board is the story tutorial: one piece on an otherwise empty
    // grid, which hides nearly everything about the screen players live in.
    // Assigns gameSnapshot directly rather than going through
    // commitSandboxSnapshot, whose sandbox player refresh would overwrite the
    // spent resources and clocks that make the readouts worth reviewing.
    auto seedCaptureMatch = [&](const std::string& variant) {
        sandboxMode = false;
        storyMode = false;
        storyEngine.reset();
        storyAiPending = false;
        conquestBattleMode = false;
        resignConfirmPopupVisible = false;
        leaveGameButton.setLabel("Resign");
        leaveGameButton.setLabelSize(type::Body);
        leaveGameButton.setPosition({GameActionButtonX, GameLeaveButtonY});
        leaveGameButton.setSize({GameLeaveButtonWidth, GameActionButtonHeight});
        storyStage = StoryStage::None;
        storyTargetRow = -1;
        storyTargetColumn = -1;
        currentState = GameState::Game;
        activeGameSocket.reset();
        abilityButton.setPosition({GameActionButtonX, GameAbilityButtonY});
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);

        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        selectedPieceId.reset();
        selectedHandIndex.reset();
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        inspectedPieceScroll = 0.0f;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        gameResultReceived = false;
        gameResultSuccess = false;
        gameOverSoundPlayed = true;
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

        const auto gameCardNamed = [&](const std::string& cardTitle) {
            for (const card_data::Card& card : allCardLibrary)
            {
                if (card.title == cardTitle)
                {
                    return game_data::toGameCard(card);
                }
            }
            return game_data::GameCard{};
        };

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
        // A couple of collapsed squares, so the hole treatment is reviewable.
        snapshot.holes[static_cast<std::size_t>(game_data::squareIndex(6, 3))] = 1;
        snapshot.holes[static_cast<std::size_t>(game_data::squareIndex(1, 4))] = 1;
        if (variant == "bases" || variant == "bases-blue-large")
        {
            snapshot.holes.fill(0);
        }

        // Player 1 holds the left flank, player 2 the right. Health is left short
        // of maximum on several pieces so damage states are visible.
        struct CaptureDeployment
        {
            const char* title;
            int owner;
            int row;
            int column;
            int health;
            bool isHero;
            bool hasActed;
            int widthOverride = 0;
            int heightOverride = 0;
        };
        static constexpr CaptureDeployment Deployments[] = {
            {"Sylvara", 1, 3, 0, 16, true, false},
            {"Blightling", 1, 1, 1, 2, false, true},
            {"Duchess Dewbell", 1, 4, 1, 4, false, false},
            {"Bog Spearman", 1, 2, 2, 5, false, false},
            {"Thorn Griffin", 1, 5, 2, 6, false, false},
            {"Thaeron Baelstone", 2, 4, 7, 21, true, false},
            {"Gloom Fairy", 2, 2, 4, 1, false, false},
            {"Marshland Veteran", 2, 3, 5, 3, false, false},
            {"Goblin Sharpshooter", 2, 5, 6, 4, false, false},
            {"Erevan the Shadow", 2, 6, 6, 7, false, false},
        };
        static constexpr CaptureDeployment BaseDeployments[] = {
            {"Eyeblight", 1, 0, 0, 4, false, false, 1, 1},
            {"Gloom Fairy", 1, 0, 7, 3, false, false, 1, 1},
            {"Blackthorn Debt Collector", 2, 7, 0, 5, false, false, 1, 1},
            {"Goblin Sharpshooter", 2, 7, 7, 4, false, false, 1, 1},
            {"Crystal Unicorn", 2, 2, 2, 9, false, false, 4, 4},
        };
        const auto spawnDeployments = [&](const auto& deployments) {
            for (const CaptureDeployment& deployment : deployments)
            {
                game_data::GameCard card = gameCardNamed(deployment.title);
                if (card.title.empty())
                {
                    continue;
                }
                if (deployment.widthOverride > 0)
                {
                    card.width = deployment.widthOverride;
                }
                if (deployment.heightOverride > 0)
                {
                    card.height = deployment.heightOverride;
                }
                const int owner =
                    variant == "bases-blue-large" &&
                    deployment.widthOverride == 4 && deployment.heightOverride == 4
                    ? 1
                    : deployment.owner;
                spawnSandboxPiece(
                    snapshot,
                    nextSandboxPieceId,
                    owner,
                    card,
                    deployment.row,
                    deployment.column,
                    deployment.isHero);
                game_data::Piece& piece = snapshot.pieces.back();
                piece.health = std::min(deployment.health, piece.maxHealth);
                piece.hasActed = deployment.hasActed;
            }
        };
        if (variant == "bases" || variant == "bases-blue-large")
        {
            spawnDeployments(BaseDeployments);
        }
        else
        {
            spawnDeployments(Deployments);
        }

        if (variant == "action-choice")
        {
            snapshot.pieces.clear();
            nextSandboxPieceId = 1;
            const std::optional<game_data::GameCard> briar =
                packagedStoryCard("Briar Whisperthorn");
            const std::optional<game_data::GameCard> target =
                packagedStoryCard("Bristlejack");
            if (briar && target)
            {
                spawnSandboxPiece(
                    snapshot, nextSandboxPieceId, 1, *briar, 3, 2, false);
                spawnSandboxPiece(
                    snapshot, nextSandboxPieceId, 2, *target, 4, 3, false);
            }
        }

        // One held enemy, so the under-control badge is reviewable.
        for (game_data::Piece& piece : snapshot.pieces)
        {
            if (piece.row == 2 && piece.column == 4)
            {
                piece.controlTurnsRemaining = 2;
            }
        }

        // Sylvara's Seelie trait gates which units may be deployed, so the hand
        // mixes cards she permits with ones she does not and one the player cannot
        // yet afford. That makes every affordability state visible at once.
        static constexpr const char* HandTitles[] = {
            "Heartwood Sister", "Heartshoot", "Duchess Dewbell", "Crystal Unicorn"};
        for (const char* handTitle : HandTitles)
        {
            game_data::GameCard card = gameCardNamed(handTitle);
            if (!card.title.empty())
            {
                snapshot.hand.push_back(std::move(card));
            }
        }

        recomputeSandboxControl(snapshot);

        snapshot.timersEnabled = true;
        snapshot.turnRemainingMs = 47'000;
        snapshot.players[0].resources = 125;
        snapshot.players[0].controlledSquares = controlledCountInSnapshot(snapshot, 1);
        snapshot.players[0].handCount = static_cast<int>(snapshot.hand.size());
        snapshot.players[0].heroesAlive = heroesAliveInSnapshot(snapshot, 1);
        snapshot.players[0].drawPileCount = 18;
        snapshot.players[0].clockRemainingMs = 512'000;
        snapshot.players[1].resources = 4;
        snapshot.players[1].controlledSquares = controlledCountInSnapshot(snapshot, 2);
        snapshot.players[1].handCount = 5;
        snapshot.players[1].heroesAlive = heroesAliveInSnapshot(snapshot, 2);
        snapshot.players[1].drawPileCount = 21;
        snapshot.players[1].clockRemainingMs = 388'000;
        snapshot.status.clear();

        if (variant == "victory")
        {
            snapshot.phase = static_cast<std::uint8_t>(game_data::Phase::GameOver);
            snapshot.winner = 1;
            gameResultReceived = true;
            gameResultSuccess = true;
            gameRatingChange = 24;
            gameRewardText = "45 coins";
        }

        gameSnapshot = std::move(snapshot);
        gameSnapshotReceivedAt = std::chrono::steady_clock::now();
        haveSnapshot = true;

        // Bog Spearman sits within reach of the held Gloom Fairy, so selecting it
        // shows move and attack range together rather than one in isolation.
        if (variant == "selected" || variant == "popup")
        {
            if (const game_data::Piece* spearman = gamePieceAt(2, 2))
            {
                if (variant == "popup")
                {
                    inspectedPieceId = spearman->id;
                }
                else
                {
                    selectedPieceId = spearman->id;
                }
            }
        }
    };

    // Replays River Teeth through the same Story action entry points used by
    // mouse input. Each capture can therefore freeze before a specific player
    // action without inventing a board state or advancing the script counter.
    auto replayRiverTeethForCapture = [&](
                                             int completedPlayerActions,
                                             bool keepNextPlayerPanels = false) {
        storyCampaign = StoryCampaign::Mirewatch;
        storyMissionIndex = storyMissionIndexById(
            storyCampaign, "mw01_river_teeth");
        beginStory();

        const StoryMission& mission = activeStoryMission();
        const int totalPlayerActions = static_cast<int>(std::count_if(
            mission.script.begin(),
            mission.script.end(),
            [](const StoryScriptAction& step) { return step.owner == 1; }));
        const int requestedActions = std::clamp(
            completedPlayerActions, 0, totalPlayerActions);
        int completed = 0;

        // Briefing and between-step panels are advanced as ordinary Continue
        // clicks would advance them. They do not alter the engine or count as
        // game actions.
        const auto dismissNonFinalPanels = [&]() {
            storyPopupPanels.clear();
            storyPopupPage = 0;
            storyCompleteAfterPopup = false;
            storyScriptActionAt = animationTime + 0.35f;
        };

        for (int guard = 0;
             guard < 96 && storyStage == StoryStage::Objective;
             ++guard)
        {
            if (!storyPopupPanels.empty())
            {
                // The final aftermath is part of the end-to-end evidence. Keep
                // it on screen after all eleven real player actions.
                if (completed >= totalPlayerActions ||
                    (keepNextPlayerPanels && completed >= requestedActions))
                {
                    break;
                }
                dismissNonFinalPanels();
            }

            if (storyMissionStep < 0 ||
                storyMissionStep >= static_cast<int>(mission.script.size()))
            {
                break;
            }

            const StoryScriptAction step =
                mission.script[static_cast<std::size_t>(storyMissionStep)];
            if (step.owner == 1 && completed >= requestedActions)
            {
                break;
            }

            const int previousStep = storyMissionStep;
            if (step.owner == 2)
            {
                storyScriptActionAt = std::numeric_limits<float>::lowest();
                updateStoryAi();
            }
            else
            {
                int targetRow = step.targetRow;
                int targetColumn = step.targetColumn;
                if (!step.targetRole.empty())
                {
                    const int targetId = storyPieceIdForRole(step.targetRole);
                    const auto target = std::find_if(
                        storyEngine->boardPieces().begin(),
                        storyEngine->boardPieces().end(),
                        [&](const game_data::Piece& piece) {
                            return piece.id == targetId;
                        });
                    if (target == storyEngine->boardPieces().end())
                    {
                        break;
                    }
                    targetRow = target->row;
                    targetColumn = target->column;
                }

                const int actorId = step.actorRole.empty()
                    ? 0
                    : storyPieceIdForRole(step.actorRole);
                switch (step.kind)
                {
                case StoryActionKind::Move:
                case StoryActionKind::Attack:
                    // Resolve the drop exactly as the live board does. This
                    // proves the printed action is classified as a move or an
                    // attack before Story gating and the engine receive it.
                    requestPieceAction(actorId, targetRow, targetColumn);
                    break;
                case StoryActionKind::EndTurn:
                    sendEndTurn();
                    break;
                default:
                    // River Teeth intentionally teaches only movement, attack,
                    // and normal turn cadence.
                    break;
                }
                if (storyMissionStep != previousStep)
                {
                    ++completed;
                }
            }

            if (storyMissionStep == previousStep)
            {
                gameSnapshot.status =
                    "Capture replay error: River Teeth action was rejected.";
                break;
            }
        }

        return completed == requestedActions;
    };

    // Replays the Chapter 13 route lesson through ordinary Story entry points
    // and freezes on the genuine aftermath produced by Juniper's Intercept.
    const auto replayVaultCostForCapture = [&]() {
        storyCampaign = StoryCampaign::Mirewatch;
        storyMissionIndex = storyMissionIndexById(
            storyCampaign, "mw11_no_plan_saves_all");
        beginStory();
        storyPopupPanels.clear();
        storyPopupPage = 0;
        storyCompleteAfterPopup = false;

        const int birdieId = storyPieceIdForRole("birdie");
        const int reedId = storyPieceIdForRole("reed");
        const int juniperId = storyPieceIdForRole("juniper");
        const int runnerId = storyPieceIdForRole("runner");
        const int attackerId = storyPieceIdForRole("intercept_attacker");
        bool accepted = birdieId != 0 && reedId != 0 && juniperId != 0 &&
            runnerId != 0 && attackerId != 0;

        const auto playerDrop = [&](int actorId, int row, int column) {
            const int stepBefore = storyMissionStep;
            requestPieceAction(actorId, row, column);
            accepted = accepted && storyMissionStep == stepBefore + 1;
        };
        const auto playerPass = [&]() {
            const int stepBefore = storyMissionStep;
            sendEndTurn();
            accepted = accepted && storyMissionStep == stepBefore + 1;
        };
        const auto opponentStep = [&]() {
            const int stepBefore = storyMissionStep;
            storyScriptActionAt = std::numeric_limits<float>::lowest();
            updateStoryAi();
            accepted = accepted && storyMissionStep == stepBefore + 1;
        };

        playerDrop(birdieId, 2, 4);
        playerPass();
        opponentStep();
        playerDrop(reedId, 6, 4);
        playerPass();
        opponentStep();
        playerDrop(juniperId, 4, 1);
        playerPass();
        opponentStep();

        const game_data::Piece* runner = gamePieceById(runnerId);
        const game_data::Piece* attacker = gamePieceById(attackerId);
        const bool genuineAftermath = accepted && gamePieceById(juniperId) == nullptr &&
            runner != nullptr && runner->row == 4 && runner->column == 1 &&
            attacker != nullptr && attacker->row == 4 && attacker->column == 2 &&
            !storyPopupPanels.empty() && storyCompleteAfterPopup;
        if (!genuineAftermath)
        {
            failCaptureValidation(
                "Capture replay error: Chapter 13 did not reach its genuine Intercept aftermath.");
        }
        return genuineAftermath;
    };

    const auto dismissStoryPanelsForCapture = [&]() {
        storyPopupPanels.clear();
        storyPopupPage = 0;
        storyPopupKeyboardFocus = 1;
        storyCompleteAfterPopup = false;
        storyScriptActionAt = animationTime + 0.35f;
    };

    const auto storyCaptureSnapshotMatchesEngine = [&]() {
        if (!storyEngine)
        {
            return false;
        }
        game_data::Snapshot rendered = gameSnapshot;
        const game_data::Snapshot authoritative =
            storyEngine->snapshotFor(gameSnapshot.yourPlayer);
        // failCaptureValidation deliberately surfaces its message in the HUD.
        // Once another capture guard has already failed, do not misreport that
        // diagnostic-only status replacement as an engine synchronization bug.
        if (captureValidationFailed && rendered.status == storyCorrection)
        {
            rendered.status = authoritative.status;
        }
        sf::Packet renderedPacket;
        sf::Packet authoritativePacket;
        game_data::writeSnapshot(renderedPacket, rendered);
        game_data::writeSnapshot(authoritativePacket, authoritative);
        if (renderedPacket.getDataSize() != authoritativePacket.getDataSize())
        {
            return false;
        }
        const auto* renderedBytes = static_cast<const std::uint8_t*>(
            renderedPacket.getData());
        const auto* authoritativeBytes = static_cast<const std::uint8_t*>(
            authoritativePacket.getData());
        return std::equal(
            renderedBytes,
            renderedBytes + renderedPacket.getDataSize(),
            authoritativeBytes);
    };

    const auto storyCapturePopupMatches = [&](
                                                  const std::vector<StoryPanel>& expected,
                                                  bool completeAfter) {
        if (storyStage != StoryStage::Objective ||
            storyPopupPanels.size() != expected.size() ||
            storyCompleteAfterPopup != (completeAfter && !expected.empty()))
        {
            return false;
        }
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            if (storyPopupPanels[index].speaker != expected[index].speaker ||
                storyPopupPanels[index].text != expected[index].text ||
                storyPopupPanels[index].artPath != expected[index].artPath)
            {
                return false;
            }
        }
        return true;
    };

    // Replays authored steps through the same client action entry points used
    // by mouse/keyboard play. Opponent steps use the production scripted-AI
    // path. The target step's panelsBefore (or the genuine final aftermath)
    // remain queued; earlier modal panels are dismissed without writing
    // campaign progression.
    const auto replayScriptedStoryForCapture =
        [&](std::size_t stopBeforeStep) {
            const StoryMission& mission = activeStoryMission();
            const auto replayFailure = [&](std::string detail) {
                failCaptureValidation(
                    "Capture replay error: Story mission '" +
                    std::string(mission.id) + "' " + std::move(detail));
                return false;
            };
            if (mission.script.empty() ||
                stopBeforeStep > mission.script.size())
            {
                return replayFailure(
                    "has no authored scripted path to the requested beat.");
            }
            const int completedCountBefore = storyCompletedCount;
            const int campaignProgressBefore =
                storyCampaignProgress[storyProgressIndex(storyCampaign)];

            beginStory();
            if (!storyEngine || storyStage == StoryStage::Failed)
            {
                return replayFailure(
                    "could not initialize its authoritative engine state.");
            }

            for (std::size_t guard = 0;
                 guard < mission.script.size() + 8 &&
                 static_cast<std::size_t>(storyMissionStep) < stopBeforeStep;
                 ++guard)
            {
                if (!storyPopupPanels.empty())
                {
                    dismissStoryPanelsForCapture();
                }
                if (storyMissionStep < 0 ||
                    storyMissionStep >=
                        static_cast<int>(mission.script.size()))
                {
                    return replayFailure(
                        "left its authored script before the requested beat.");
                }

                const int previousStep = storyMissionStep;
                const StoryScriptAction step =
                    mission.script[static_cast<std::size_t>(previousStep)];
                if (storyScriptActionAutoResolves(mission, step))
                {
                    storyScriptActionAt =
                        std::numeric_limits<float>::lowest();
                    updateStoryAi();
                }
                else if (storyScriptActionRequiresPlayerInput(mission, step))
                {
                    int targetRow = step.targetRow;
                    int targetColumn = step.targetColumn;
                    if (!step.targetRole.empty())
                    {
                        const int targetId =
                            storyPieceIdForRole(step.targetRole);
                        const game_data::Piece* target =
                            gamePieceById(targetId);
                        if (!target)
                        {
                            return replayFailure(
                                "could not resolve target role '" +
                                std::string(step.targetRole) + "' at step " +
                                std::to_string(previousStep + 1) + ".");
                        }
                        targetRow = target->row;
                        targetColumn = target->column;
                    }

                    const int actorId = step.actorRole.empty()
                        ? 0
                        : storyPieceIdForRole(step.actorRole);
                    switch (step.kind)
                    {
                    case StoryActionKind::Move:
                    case StoryActionKind::Attack:
                    {
                        const game_data::Piece* actor =
                            gamePieceById(actorId);
                        if (!actor)
                        {
                            return replayFailure(
                                "could not resolve actor role '" +
                                std::string(step.actorRole) + "' at step " +
                                std::to_string(previousStep + 1) + ".");
                        }
                        const std::optional<int> actionIndex =
                            storyExpectedActionProfileIndex(step, *actor);
                        if (!actionIndex)
                        {
                            return replayFailure(
                                "could not resolve the exact printed action at "
                                "step " + std::to_string(previousStep + 1) + ".");
                        }
                        if (step.kind == StoryActionKind::Move)
                        {
                            sendMovePiece(
                                actorId,
                                targetRow,
                                targetColumn,
                                *actionIndex);
                        }
                        else
                        {
                            sendAttackPiece(
                                actorId,
                                targetRow,
                                targetColumn,
                                *actionIndex);
                        }
                        break;
                    }
                    case StoryActionKind::UseAbility:
                        sendUseAbility(actorId);
                        break;
                    case StoryActionKind::PlayCard:
                    {
                        const auto card = std::find_if(
                            gameSnapshot.hand.begin(),
                            gameSnapshot.hand.end(),
                            [&](const game_data::GameCard& value) {
                                return value.title == step.cardTitle;
                            });
                        if (card == gameSnapshot.hand.end())
                        {
                            return replayFailure(
                                "could not find card '" +
                                std::string(step.cardTitle) + "' at step " +
                                std::to_string(previousStep + 1) + ".");
                        }
                        sendPlayCard(
                            static_cast<int>(std::distance(
                                gameSnapshot.hand.begin(), card)),
                            targetRow,
                            targetColumn);
                        break;
                    }
                    case StoryActionKind::DrawCard:
                        sendDrawCard();
                        break;
                    case StoryActionKind::ChooseForesight:
                    {
                        const auto card = std::find_if(
                            gameSnapshot.foresightChoices.begin(),
                            gameSnapshot.foresightChoices.end(),
                            [&](const game_data::GameCard& value) {
                                return value.title == step.cardTitle;
                            });
                        if (card == gameSnapshot.foresightChoices.end())
                        {
                            return replayFailure(
                                "could not find Foresight choice '" +
                                std::string(step.cardTitle) + "' at step " +
                                std::to_string(previousStep + 1) + ".");
                        }
                        sendChooseForesightCard(static_cast<int>(
                            std::distance(
                                gameSnapshot.foresightChoices.begin(), card)));
                        break;
                    }
                    case StoryActionKind::DiscardCard:
                    {
                        const auto card = std::find_if(
                            gameSnapshot.hand.begin(),
                            gameSnapshot.hand.end(),
                            [&](const game_data::GameCard& value) {
                                return value.title == step.cardTitle;
                            });
                        if (card == gameSnapshot.hand.end())
                        {
                            return replayFailure(
                                "could not find discard '" +
                                std::string(step.cardTitle) + "' at step " +
                                std::to_string(previousStep + 1) + ".");
                        }
                        sendDiscardCard(static_cast<int>(std::distance(
                            gameSnapshot.hand.begin(), card)));
                        break;
                    }
                    case StoryActionKind::EndTurn:
                        sendEndTurn();
                        break;
                    case StoryActionKind::None:
                        return replayFailure(
                            "contains an inert authored action at step " +
                            std::to_string(previousStep + 1) + ".");
                    }
                }
                else
                {
                    return replayFailure(
                        "has an authored step with no valid automatic or player-input path for player " +
                        std::to_string(step.owner) + " at step " +
                        std::to_string(previousStep + 1) + ".");
                }

                if (storyStage == StoryStage::Failed ||
                    storyMissionStep != previousStep + 1)
                {
                    return replayFailure(
                        "rejected authored step " +
                        std::to_string(previousStep + 1) + ".");
                }
            }

            if (storyMissionStep !=
                    static_cast<int>(stopBeforeStep) ||
                !storyCaptureSnapshotMatchesEngine() ||
                storyCompletedCount != completedCountBefore ||
                storyCampaignProgress[storyProgressIndex(storyCampaign)] !=
                    campaignProgressBefore)
            {
                return replayFailure(
                    "did not stop on a synchronized, non-progressing "
                    "authoritative state.");
            }
            if (stopBeforeStep < mission.script.size())
            {
                const StoryScriptAction& target =
                    mission.script[stopBeforeStep];
                if (!storyCapturePopupMatches(target.panelsBefore, false))
                {
                    return replayFailure(
                        "did not queue the requested production before-step popup.");
                }
            }
            else if (!storyCapturePopupMatches(mission.aftermath, true))
            {
                return replayFailure(
                    "did not produce its genuine completion aftermath.");
            }
            return true;
        };

    // Verifies that an action-page fixture is the exact production state the
    // named authored command expects. Legality is tested on an isolated engine
    // copy so the screenshot remains immediately before the command.
    const auto storyActionCaptureStateError =
        [&](std::size_t stepIndex,
            bool openObjective) -> std::optional<std::string> {
            const StoryMission& mission = activeStoryMission();
            if (!storyEngine || currentState != GameState::Game ||
                !storyMode || storyStage != StoryStage::Objective)
            {
                return "is not in its live objective gameplay state.";
            }
            if (!storyPopupPanels.empty() || storyCompleteAfterPopup)
            {
                return "still has a modal Story panel over the requested action.";
            }
            if (!storyCaptureSnapshotMatchesEngine())
            {
                return "has a rendered snapshot that differs from its authoritative engine.";
            }

            if (openObjective)
            {
                if (!mission.script.empty() ||
                    mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly ||
                    stepIndex != 0 || storyMissionStep != 0)
                {
                    return "does not resolve to the sole initial open-objective state.";
                }
                const game_data::Phase expectedPhase = mission.standardMatch
                    ? game_data::Phase::HeroPlacement
                    : game_data::Phase::Playing;
                if (storyEngine->phase() != expectedPhase ||
                    static_cast<game_data::Phase>(gameSnapshot.phase) !=
                        expectedPhase ||
                    storyEngine->currentPlayer() != gameSnapshot.activePlayer)
                {
                    return "does not preserve its initial authoritative phase and active owner.";
                }
                return std::nullopt;
            }

            if (mission.script.empty() || stepIndex >= mission.script.size() ||
                storyMissionStep != static_cast<int>(stepIndex))
            {
                return "does not resolve to its exact authored script index.";
            }
            const StoryScriptAction& step = mission.script[stepIndex];
            if (step.kind == StoryActionKind::None)
            {
                return "resolves to an inert authored action.";
            }
            if (step.owner < 1 || step.owner > 2 ||
                storyEngine->phase() != game_data::Phase::Playing ||
                static_cast<game_data::Phase>(gameSnapshot.phase) !=
                    game_data::Phase::Playing ||
                storyEngine->currentPlayer() != step.owner ||
                gameSnapshot.activePlayer != step.owner)
            {
                return "has an incorrect authoritative active owner for the authored step.";
            }
            if (storyTargetRow != step.targetRow ||
                storyTargetColumn != step.targetColumn)
            {
                return "has a tutorial target square that differs from the authored step.";
            }
            if (step.heading.empty() || step.instruction.empty())
            {
                return "has no visible authored heading or instruction.";
            }

            const int actorId = step.actorRole.empty()
                ? 0
                : storyPieceIdForRole(step.actorRole);
            const game_data::Piece* actor = actorId == 0
                ? nullptr
                : gamePieceById(actorId);
            const bool needsActor =
                step.kind == StoryActionKind::Move ||
                step.kind == StoryActionKind::Attack ||
                step.kind == StoryActionKind::UseAbility;
            if ((needsActor && step.actorRole.empty()) ||
                (!step.actorRole.empty() &&
                 (actor == nullptr || actor->owner != step.owner)))
            {
                return "cannot resolve its authored actor role to a live piece owned by the active side.";
            }

            int targetRow = step.targetRow;
            int targetColumn = step.targetColumn;
            if (!step.targetRole.empty())
            {
                const int targetId = storyPieceIdForRole(step.targetRole);
                const game_data::Piece* target = targetId == 0
                    ? nullptr
                    : gamePieceById(targetId);
                if (target == nullptr)
                {
                    return "cannot resolve its authored target role to a live piece.";
                }
                if (step.targetRow >= 0 &&
                    (target->row != step.targetRow ||
                     target->column != step.targetColumn))
                {
                    return "resolves its authored target role on the wrong square.";
                }
                targetRow = target->row;
                targetColumn = target->column;
            }
            if ((step.kind == StoryActionKind::Move ||
                 step.kind == StoryActionKind::Attack) &&
                !game_data::inBounds(targetRow, targetColumn))
            {
                return "does not resolve its piece action to an in-bounds target square.";
            }

            GameEngine legalityProbe = *storyEngine;
            bool accepted = false;
            switch (step.kind)
            {
            case StoryActionKind::Move:
            case StoryActionKind::Attack:
            {
                const std::optional<int> actionIndex = actor
                    ? storyExpectedActionProfileIndex(step, *actor)
                    : std::nullopt;
                if (!actionIndex)
                {
                    return "cannot resolve its exact printed action profile.";
                }
                accepted = step.kind == StoryActionKind::Move
                    ? legalityProbe.movePiece(
                          step.owner,
                          actorId,
                          targetRow,
                          targetColumn,
                          *actionIndex)
                    : legalityProbe.attackPiece(
                          step.owner,
                          actorId,
                          targetRow,
                          targetColumn,
                          *actionIndex);
                break;
            }
            case StoryActionKind::UseAbility:
            {
                if (!actor || !storyAbilityStepMatches(step, *actor))
                {
                    return "does not match the actor's exact printed ability state.";
                }
                const game_data::Piece actorBefore = *actor;
                accepted = legalityProbe.useAbility(step.owner, actorId);
                if (accepted && !storyAbilityOutcomeMatches(
                                    step,
                                    actorBefore,
                                    legalityProbe.boardPieces(),
                                    legalityProbe.commandingPiece()))
                {
                    return "does not produce its authored ability result on an isolated legality probe.";
                }
                break;
            }
            case StoryActionKind::PlayCard:
            {
                const auto& hand = legalityProbe.playerState(step.owner).hand;
                const auto card = std::find_if(
                    hand.begin(), hand.end(), [&](const game_data::GameCard& value) {
                        return value.title == step.cardTitle;
                    });
                if (step.cardTitle.empty() || card == hand.end())
                {
                    return "cannot resolve its authored card in the active side's hand.";
                }
                accepted = legalityProbe.playCard(
                    step.owner,
                    static_cast<int>(std::distance(hand.begin(), card)),
                    targetRow,
                    targetColumn);
                if (accepted && !step.effectRole.empty())
                {
                    const auto spawned = std::find_if(
                        legalityProbe.boardPieces().begin(),
                        legalityProbe.boardPieces().end(),
                        [&](const game_data::Piece& piece) {
                            return piece.owner == step.owner &&
                                piece.name == step.cardTitle &&
                                piece.row == step.targetRow &&
                                piece.column == step.targetColumn &&
                                piece.hasActed;
                        });
                    if (spawned == legalityProbe.boardPieces().end())
                    {
                        return "does not deploy its authored result role exhausted on the requested square.";
                    }
                }
                break;
            }
            case StoryActionKind::DrawCard:
                accepted = legalityProbe.drawCard(step.owner);
                break;
            case StoryActionKind::ChooseForesight:
            {
                const auto& choices =
                    legalityProbe.playerState(step.owner).foresightChoices;
                const auto choice = std::find_if(
                    choices.begin(), choices.end(),
                    [&](const game_data::GameCard& value) {
                        return value.title == step.cardTitle;
                    });
                if (step.cardTitle.empty() || choice == choices.end())
                {
                    return "cannot resolve its authored Foresight choice.";
                }
                accepted = legalityProbe.chooseForesightCard(
                    step.owner,
                    static_cast<int>(std::distance(choices.begin(), choice)));
                break;
            }
            case StoryActionKind::DiscardCard:
            {
                const auto& hand = legalityProbe.playerState(step.owner).hand;
                const auto card = std::find_if(
                    hand.begin(), hand.end(), [&](const game_data::GameCard& value) {
                        return value.title == step.cardTitle;
                    });
                if (step.cardTitle.empty() || card == hand.end())
                {
                    return "cannot resolve its authored discard in the active side's hand.";
                }
                accepted = legalityProbe.discardCard(
                    step.owner,
                    static_cast<int>(std::distance(hand.begin(), card)));
                break;
            }
            case StoryActionKind::EndTurn:
                accepted = legalityProbe.endTurn(step.owner);
                break;
            case StoryActionKind::None:
                break;
            }
            if (!accepted)
            {
                return "is not a legal action in the replayed authoritative state.";
            }
            return std::nullopt;
        };

    const auto storyActionCaptureInvariantError =
        [&]() -> std::optional<std::string> {
            if (!storyActionCaptureInvariant)
            {
                return std::nullopt;
            }
            const StoryActionCaptureInvariant& expected =
                *storyActionCaptureInvariant;
            if (storyCampaign != expected.campaign ||
                storyMissionIndex != expected.missionIndex)
            {
                return "changed campaign or mission during capture warmup.";
            }
            if (storyCompletedCount != expected.completedCount ||
                storyCampaignProgress[storyProgressIndex(storyCampaign)] !=
                    expected.campaignProgress)
            {
                return "mutated campaign progress during capture warmup.";
            }
            if (!storyEngine || storyEngine->phase() != expected.phase ||
                storyEngine->currentPlayer() != expected.activeOwner ||
                static_cast<game_data::Phase>(gameSnapshot.phase) !=
                    expected.phase ||
                gameSnapshot.activePlayer != expected.activeOwner)
            {
                return "changed authoritative phase or active owner during capture warmup.";
            }
            return storyActionCaptureStateError(
                expected.stepIndex, expected.openObjective);
        };

    // Open objective entries have no authored action script. Complete them
    // legally: deployment uses the exact required card/square, while combat,
    // reach, and territory objectives use the objective-aware production AI.
    // The opponent passes when legal so the deterministic fixture demonstrates
    // the success path without manufacturing piece coordinates or damage.
    const auto replayOpenStoryAftermathForCapture = [&]() {
        const StoryMission& mission = activeStoryMission();
        const auto replayFailure = [&](std::string detail) {
            failCaptureValidation(
                "Capture replay error: open Story mission '" +
                std::string(mission.id) + "' " + std::move(detail));
            return false;
        };
        if (!mission.script.empty() ||
            mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
        {
            return replayFailure(
                "is not an open tactical objective.");
        }
        const int completedCountBefore = storyCompletedCount;
        const int campaignProgressBefore =
            storyCampaignProgress[storyProgressIndex(storyCampaign)];

        beginStory();
        if (!storyEngine || storyStage == StoryStage::Failed)
        {
            return replayFailure(
                "could not initialize its authoritative engine state.");
        }

        if (mission.standardMatch)
        {
            placeAiHeroes(*storyEngine, 1);
            placeAiHeroes(*storyEngine, 2);
            syncStoryEngine();
            if (storyEngine->phase() != game_data::Phase::Playing)
            {
                return replayFailure(
                    "could not complete ordinary Hero placement.");
            }
        }
        else if (mission.objectiveSpec.kind ==
                 StoryObjectiveKind::DeployCard)
        {
            const auto card = std::find_if(
                gameSnapshot.hand.begin(),
                gameSnapshot.hand.end(),
                [&](const game_data::GameCard& value) {
                    return value.title == mission.objectiveSpec.cardTitle;
                });
            if (card == gameSnapshot.hand.end())
            {
                return replayFailure(
                    "does not contain its required deployment card.");
            }
            sendPlayCard(
                static_cast<int>(std::distance(
                    gameSnapshot.hand.begin(), card)),
                mission.objectiveSpec.targetRow,
                mission.objectiveSpec.targetColumn);
        }

        for (int actionCount = 0;
             storyPopupPanels.empty() &&
             storyStage == StoryStage::Objective &&
             storyEngine->phase() == game_data::Phase::Playing &&
             actionCount < 2048;
             ++actionCount)
        {
            const int player = storyEngine->currentPlayer();
            bool accepted = false;
            if (player == 2)
            {
                accepted = storyEngine->endTurn(2);
                if (!accepted)
                {
                    accepted = applyAiAction(
                        *storyEngine,
                        2,
                        chooseAiAction(*storyEngine, 2, 1));
                }
            }
            else
            {
                accepted = applyAiAction(
                    *storyEngine,
                    1,
                    chooseAiAction(*storyEngine, 1, 1));
            }
            if (!accepted)
            {
                return replayFailure(
                    "could not apply a legal deterministic action at action " +
                    std::to_string(actionCount + 1) + ".");
            }
            syncStoryEngine();
            // This replay owns both sides synchronously; never leave a queued
            // background opponent task for the capture warmup frames.
            storyAiPending = false;
        }

        if (!storyCaptureSnapshotMatchesEngine() ||
            storyCompletedCount != completedCountBefore ||
            storyCampaignProgress[storyProgressIndex(storyCampaign)] !=
                campaignProgressBefore)
        {
            return replayFailure(
                "finished with a stale or progression-mutating client state.");
        }
        if (!storyCapturePopupMatches(mission.aftermath, true))
        {
            return replayFailure(
                "did not reach its production completion aftermath.");
        }
        if (mission.standardMatch)
        {
            if (storyEngine->phase() != game_data::Phase::GameOver ||
                storyEngine->winner() != 1)
            {
                return replayFailure(
                    "did not finish the ordinary match with Player 1 winning.");
            }
        }
        else if (mission.objectiveSpec.kind !=
                 StoryObjectiveKind::DeployCard)
        {
            const GameEngine::ScenarioObjectiveProgress& progress =
                storyEngine->scenarioObjectiveProgress();
            if (!progress.complete || progress.failed ||
                storyEngine->winner() != 1)
            {
                return replayFailure(
                    "did not satisfy its authoritative scenario objective.");
            }
        }
        return true;
    };

    const auto finishCapturedStoryAftermath = [&]() {
        const bool completeAfter = storyCompleteAfterPopup;
        storyPopupPanels.clear();
        storyPopupPage = 0;
        storyCompleteAfterPopup = false;
        storyScriptActionAt = animationTime + 0.35f;
        if (completeAfter)
        {
            completeStoryMission(gameSnapshot);
        }
    };

    const auto inspectStoryPieceForCapture = [&] (
                                                  StoryCampaign campaign,
                                                  std::string_view missionId,
                                                  std::string_view pieceName) {
        storyCampaign = campaign;
        storyMissionIndex = storyMissionIndexById(storyCampaign, missionId);
        beginStory();
        storyPopupPanels.clear();
        storyPopupPage = 0;
        storyCompleteAfterPopup = false;

        if (!storyEngine || storyStage == StoryStage::Failed)
        {
            failCaptureValidation(
                "Capture setup error: the authoritative Story mission did not load.");
            return;
        }

        const auto found = std::find_if(
            storyEngine->boardPieces().begin(),
            storyEngine->boardPieces().end(),
            [&](const game_data::Piece& piece) {
                return piece.name == pieceName;
            });
        if (found == storyEngine->boardPieces().end())
        {
            failCaptureValidation(
                "Capture setup error: the requested Story unit is not on the board.");
            return;
        }

        inspectedPieceId = found->id;
        selectedPieceId = found->id;
        inspectedPieceScroll = 0.0f;
    };

    auto applyCaptureScreen = [&](const std::string& screen) {
        captureValidationScreen = screen;
        storyActionCaptureInvariant.reset();
        setMessage(messageText, "", sf::Color::Red);
        title.setString("Gloomthorn");
        centerText(title, 400.0f);
        captureHoverPoint.reset();
        usernameInput.setError(false);
        passwordInput.setError(false);
        confirmInput.setError(false);
        exitDesktopPopupVisible = false;
        deckUnsavedChangesPopupVisible = false;
        resignConfirmPopupVisible = false;
        gameConfirmationAction = GameConfirmationAction::Resign;
        passwordChangedPopupVisible = false;
        addCardPopupVisible = false;
        giveStarterDeckPopupVisible = false;
        deckEditorMode = DeckEditorMode::DeckList;
        starterDeckMode = false;
        // Screens are visited in one process, so anything a previous screen turned
        // on has to be turned back off or it bleeds into the next capture.
        inspectedDeckEditorCardTitle.reset();
        inspectedDeckEditorCardScroll = 0.0f;
        lastDeckEditorClickedCardTitle.reset();
        revealedCardTitle.reset();
        starterDeckPickRequired = false;
        pendingPieceActionChoice.reset();
        deckListOffset = 0;
        deckCardListOffset = 0;
        libraryOffset = 0;

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
            // Centre of the primary Guided Story plate.
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
        else if (screen == "deck-editor-empty")
        {
            // The no-decks-yet roster, so the empty state can be reviewed.
            currentState = GameState::DeckEditor;
            playerDecks.clear();
            selectedDeck.reset();
        }
        else if (screen == "deck-editor-cards" || screen == "deck-editor-full" ||
                 screen == "deck-editor-popup" || screen == "deck-editor-unsaved")
        {
            currentState = GameState::DeckEditor;
            deckEditorMode = DeckEditorMode::EditDeck;
            // "cards" edits the half-built deck so the warning slot has something
            // to say; "full" edits the legal one so every counter is satisfied.
            editingDeck = screen == "deck-editor-cards" && playerDecks.size() > 1
                ? playerDecks[1]
                : ui_capture::sampleLegalDeck(allCardLibrary);
            activeDeckOriginalName = editingDeck.name;
            deckNameInput.setContent(editingDeck.name);
            selectedDeckCard = 0;
            selectedLibraryCard = 0;
            if (screen == "deck-editor-popup")
            {
                inspectedDeckEditorCardTitle = editingDeck.cardTitles.empty()
                    ? allCardLibrary.front().title
                    : editingDeck.cardTitles.front();
                inspectedDeckEditorCardScroll = 0.0f;
            }
            else if (screen == "deck-editor-unsaved")
            {
                // A pending rename is the cheapest way to make the deck dirty.
                editingDeck.name += " v2";
                deckNameInput.setContent(editingDeck.name);
                deckUnsavedChangesPopupVisible = true;
            }
        }
        else if (screen == "shop")
        {
            currentState = GameState::Shop;
        }
        else if (screen == "shop-reveal")
        {
            currentState = GameState::Shop;
            // Mid-reveal, a few frames in, so the burst has opened but not settled.
            revealedCardTitle = "Crystal Unicorn";
            revealStartedAt = animationTime - 0.42f;
        }
        else if (screen == "starter-decks" || screen == "starter-decks-pick")
        {
            currentState = GameState::StarterDecks;
            // The forced first pick reads differently from the shop's store: no
            // deck is owned yet and the only way out is signing back out.
            starterDeckPickRequired = screen == "starter-decks-pick";
            for (std::size_t i = 0; i < starterDeckOffers.size(); ++i)
            {
                starterDeckOffers[i].owned = !starterDeckPickRequired && i == 0;
                starterDeckOffers[i].price = starterDeckPickRequired
                    ? 0
                    : (i == 0 ? 0 : starter_decks::StarterDeckPrice);
            }
            selectedStarterDeckOffer = starterDeckPickRequired ? 2 : 1;
        }
        else if (screen == "admin-users")
        {
            currentState = GameState::AdminUsers;
            adminTabs.setActive(0);
        }
        else if (screen == "admin-tools")
        {
            // Without this the capture showed the Tools body under a highlighted
            // Users tab, which made the shot misleading to review.
            currentState = GameState::AdminTools;
            adminTabs.setActive(2);
        }
        else if (screen == "card-editor")
        {
            currentState = GameState::CardEditor;
        }
        else if (screen == "conquest")
        {
            currentState = GameState::Conquest;
        }
        // ---- admin / card-editor / Conquest review states ------------------
        // These screens draw from services the harness cannot reach, so each
        // key seeds the state that makes the layout reviewable.
        else if (screen == "admin-users-selected")
        {
            currentState = GameState::AdminUsers;
            adminTabs.setActive(0);
            selectedAdminUser = 3;
            adminGoldInput.setContent("250");
        }
        else if (screen == "admin-users-popup")
        {
            currentState = GameState::AdminUsers;
            adminTabs.setActive(0);
            selectedAdminUser = 3;
            addCardPopupVisible = true;
            // A partial query, so the suggestion list has several rows and the
            // dialog's content sizing is actually exercised.
            adminCardInput.setContent("th");
        }
        else if (screen == "card-editor-loaded")
        {
            currentState = GameState::CardEditor;
            cardEditorScreen.applyCaptureState(screen, allCardLibrary);
        }
        else if (screen == "conquest-events" || screen == "conquest-map" ||
                 screen == "conquest-loadouts")
        {
            currentState = GameState::Conquest;
            conquestScreen.applyCaptureState(screen, allCardLibrary);
        }
        else if (screen == "story-select" ||
                 screen == "story-seelie-spoiler-warning")
        {
            showStorySelect();
            // A first-run capture begins at each path's first playable mission.
            storyCampaignProgress[storyProgressIndex(StoryCampaign::Blackthorn)] = 0;
            storyCampaignProgress[storyProgressIndex(StoryCampaign::Mirewatch)] = 0;
            storyCampaignProgress[storyProgressIndex(StoryCampaign::Seelie)] = 0;
            if (screen == "story-seelie-spoiler-warning")
            {
                storySpoilerConfirmationVisible = true;
                storySpoilerKeyboardFocus = 0;
            }
        }
        else if (screen == "story-mission-select" ||
                 screen == "story-mirewatch-mission-select" ||
                 screen == "story-seelie-mission-select")
        {
            showStoryMissionSelect(
                screen.rfind("story-seelie-", 0) == 0
                    ? StoryCampaign::Seelie
                    : screen.rfind("story-mirewatch-", 0) == 0
                        ? StoryCampaign::Mirewatch
                        : StoryCampaign::Blackthorn);
            storyCompletedCount = 0;
            storyCampaignProgress[storyProgressIndex(storyCampaign)] = storyCompletedCount;
        }
        else if (screen == "story-blackthorn-mission-select-final" ||
                 screen == "story-mirewatch-mission-select-final" ||
                 screen == "story-seelie-mission-select-final")
        {
            const StoryCampaign captureCampaign =
                screen.rfind("story-seelie-", 0) == 0
                    ? StoryCampaign::Seelie
                    : screen.rfind("story-mirewatch-", 0) == 0
                        ? StoryCampaign::Mirewatch
                        : StoryCampaign::Blackthorn;
            showStoryMissionSelect(captureCampaign);
            const int missionCount = static_cast<int>(storyMissions(storyCampaign).size());
            storyCompletedCount = std::max(0, missionCount - 1);
            storyCampaignProgress[storyProgressIndex(storyCampaign)] = storyCompletedCount;
            storyMissionPage = std::max(0, (missionCount - 1) / StoryMissionPageSize);
        }
        else if (screen == "story-briefing" ||
                 screen == "story-briefing-actions" ||
                 screen == "story-briefing-control" ||
                 screen == "story-blackthorn-briefing-optional-skip" ||
                 screen == "story-mirewatch-briefing" ||
                 screen == "story-mirewatch-briefing-actions" ||
                 screen == "story-mirewatch-briefing-control" ||
                 screen == "story-seelie-briefing" ||
                 screen == "story-seelie-briefing-actions" ||
                 screen == "story-seelie-briefing-control" ||
                 screen == "story-seelie-briefing-optional-skip" ||
                 screen == "story-seelie-briefing-territory-required")
        {
            storyCampaign = screen.rfind("story-seelie-", 0) == 0
                ? StoryCampaign::Seelie
                : screen.rfind("story-mirewatch-", 0) == 0
                    ? StoryCampaign::Mirewatch
                    : StoryCampaign::Blackthorn;
            if (screen == "story-blackthorn-briefing-optional-skip")
            {
                storyMissionIndex = storyMissionIndexById(
                    StoryCampaign::Blackthorn, "bt01_harness_hunger");
                storyComicPage = static_cast<int>(activeStoryMission().briefing.size()) - 1;
            }
            else if (screen == "story-seelie-briefing-territory-required")
            {
                storyMissionIndex = storyMissionIndexById(
                    StoryCampaign::Seelie, "se14_rules_under_pressure");
                storyComicPage = static_cast<int>(activeStoryMission().briefing.size()) - 1;
            }
            else if (screen == "story-seelie-briefing-control" ||
                screen == "story-seelie-briefing-optional-skip")
            {
                storyMissionIndex = storyMissionIndexById(
                    StoryCampaign::Seelie, "se07_queens_price");
                storyComicPage = screen == "story-seelie-briefing-optional-skip"
                    ? static_cast<int>(activeStoryMission().briefing.size()) - 1
                    : 1;
            }
            else
            {
                storyMissionIndex = storyTacticalMissionIndex(storyCampaign, 0);
                storyComicPage =
                    screen == "story-briefing-actions" ||
                        screen == "story-mirewatch-briefing-actions" ||
                        screen == "story-seelie-briefing-actions"
                    ? 2
                    : screen == "story-briefing-control" ||
                          screen == "story-mirewatch-briefing-control"
                        ? 3
                        : 0;
            }
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-mirewatch-gilded-hold-art")
        {
            storyCampaign = StoryCampaign::Mirewatch;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "mw02_gilded_hold");
            storyComicPage = 0;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-mirewatch-speaker-inset-intro" ||
                 screen == "story-blackthorn-speaker-inset-intro" ||
                 screen == "story-seelie-speaker-inset-intro")
        {
            if (screen.rfind("story-mirewatch-", 0) == 0)
            {
                storyCampaign = StoryCampaign::Mirewatch;
                storyMissionIndex = storyMissionIndexById(
                    storyCampaign, "mw03_town_under_company");
                storyComicPage = 0;
            }
            else if (screen.rfind("story-seelie-", 0) == 0)
            {
                storyCampaign = StoryCampaign::Seelie;
                storyMissionIndex = storyMissionIndexById(
                    storyCampaign, "se01_cathedral_last_lights");
                storyComicPage = 0;
            }
            else
            {
                storyCampaign = StoryCampaign::Blackthorn;
                storyMissionIndex = storyMissionIndexById(
                    storyCampaign, "bt01_harness_hunger");
                storyComicPage = 1;
            }
            const StoryMission& mission = activeStoryMission();
            const StoryPanel& panel =
                mission.briefing[static_cast<std::size_t>(storyComicPage)];
            if (mission.scenarioArtPath.empty() || panel.artPath.empty() ||
                mission.scenarioArtPath == panel.artPath)
            {
                failCaptureValidation(
                    "Capture setup error: speaker inset needs distinct scenario and portrait art.");
            }
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-mirewatch-speaker-inset-popup" ||
                 screen == "story-blackthorn-speaker-inset-popup" ||
                 screen == "story-seelie-speaker-inset-popup")
        {
            std::size_t panelIndex = 0;
            if (screen.rfind("story-mirewatch-", 0) == 0)
            {
                storyCampaign = StoryCampaign::Mirewatch;
                storyMissionIndex = storyMissionIndexById(
                    storyCampaign, "mw03_town_under_company");
            }
            else if (screen.rfind("story-seelie-", 0) == 0)
            {
                storyCampaign = StoryCampaign::Seelie;
                storyMissionIndex = storyMissionIndexById(
                    storyCampaign, "se01_cathedral_last_lights");
            }
            else
            {
                storyCampaign = StoryCampaign::Blackthorn;
                storyMissionIndex = storyMissionIndexById(
                    storyCampaign, "bt01_harness_hunger");
                panelIndex = 1;
            }
            beginStory();
            const StoryMission& mission = activeStoryMission();
            if (panelIndex >= mission.briefing.size())
            {
                failCaptureValidation(
                    "Capture setup error: speaker inset panel is absent.");
            }
            else
            {
                const StoryPanel& panel = mission.briefing[panelIndex];
                if (mission.scenarioArtPath.empty() || panel.artPath.empty() ||
                    mission.scenarioArtPath == panel.artPath)
                {
                    failCaptureValidation(
                        "Capture setup error: popup speaker inset needs distinct art.");
                }
                storyPopupPanels = {panel};
                storyPopupPage = 0;
                storyPopupKeyboardFocus = 1;
                storyCompleteAfterPopup = false;
            }
        }
        else if (screen == "story-blackthorn-synthesis-bypass")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt17_natural_order");
            const std::span<const StoryMission> missions =
                storyMissions(storyCampaign);
            const std::size_t progressIndex =
                storyProgressIndex(storyCampaign);
            StoryProgress& progress =
                storyCampaignProgressDetails[progressIndex];
            progress.advancedCount = storyMissionIndex;
            progress.completedByPlay.assign(missions.size(), false);
            for (int index = 0; index < storyMissionIndex; ++index)
            {
                if (missions[static_cast<std::size_t>(index)].optionalRehearsal)
                {
                    progress.completedByPlay[static_cast<std::size_t>(index)] = true;
                }
            }
            storyCampaignProgress[progressIndex] = progress.advancedCount;
            storyCompletedCount = progress.advancedCount;
            if (!activeStoryCatchUpMayBeSkipped())
            {
                failCaptureValidation(
                    "Capture setup error: completed rehearsals did not unlock "
                    "the Blackthorn synthesis bypass.");
            }
            storyComicPage =
                static_cast<int>(activeStoryMission().briefing.size()) - 1;
            storyIntroKeyboardFocus = 2;
            storyKeyboardNavigationActive = true;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-mirewatch-four-losses" ||
                 screen == "story-mirewatch-epilogue-voice" ||
                 screen == "story-mirewatch-victor-protected" ||
                 screen == "story-mirewatch-first-open-check" ||
                 screen == "story-blackthorn-receipt-order" ||
                 screen == "story-blackthorn-first-open-check" ||
                 screen == "story-blackthorn-field-judgment" ||
                 screen == "story-blackthorn-open-mastery" ||
                 screen == "story-blackthorn-open-mastery-clocks" ||
                 screen == "story-blackthorn-open-mastery-timeouts" ||
                 screen == "story-blackthorn-open-mastery-choice" ||
                 screen == "story-seelie-seven-corrections" ||
                 screen == "story-seelie-mirror-remembers" ||
                 screen == "story-seelie-mirror-causal-setup" ||
                 screen == "story-seelie-mirror-marrowind" ||
                 screen == "story-seelie-mirror-orientation" ||
                 screen == "story-seelie-sella-pallid" ||
                 screen == "story-seelie-sella-name-eaten" ||
                 screen == "story-seelie-vow-web-cause" ||
                 screen == "story-seelie-vow-web-consequence" ||
                 screen == "story-seelie-pump-four-chronicle-boundary" ||
                 screen == "story-seelie-pump-four-rules-boundary" ||
                 screen == "story-seelie-before-next-dawn" ||
                 screen == "story-seelie-vow-record" ||
                 screen == "story-seelie-open-record" ||
                 screen == "story-seelie-final-recap" ||
                 screen == "story-seelie-final-recap-empty-place" ||
                 screen == "story-seelie-ascent-refusal" ||
                 screen == "story-seelie-workers-refuse" ||
                 screen == "story-seelie-first-release" ||
                 screen == "story-seelie-separate-withdrawals" ||
                 screen == "story-seelie-separate-withdrawals-coda" ||
                 screen == "story-seelie-epilogue")
        {
            std::string_view missionId;
            if (screen == "story-mirewatch-four-losses")
            {
                storyCampaign = StoryCampaign::Mirewatch;
                missionId = "mw21_four_losses";
                storyComicPage = 0;
            }
            else if (screen == "story-mirewatch-epilogue-voice")
            {
                storyCampaign = StoryCampaign::Mirewatch;
                missionId = "s06_town_owns_itself";
                storyComicPage = 5;
            }
            else if (screen == "story-mirewatch-victor-protected")
            {
                storyCampaign = StoryCampaign::Mirewatch;
                missionId = "mw24_agent_not_heir";
                storyComicPage = 2;
            }
            else if (screen == "story-mirewatch-first-open-check")
            {
                storyCampaign = StoryCampaign::Mirewatch;
                missionId = "s02_no_one_alone";
                storyComicPage = 8;
            }
            else if (screen == "story-blackthorn-receipt-order")
            {
                storyCampaign = StoryCampaign::Blackthorn;
                missionId = "bt06_receipt_book";
                storyComicPage = 3;
            }
            else if (screen == "story-blackthorn-first-open-check")
            {
                storyCampaign = StoryCampaign::Blackthorn;
                missionId = "bt06_receipt_book";
                storyComicPage = 5;
            }
            else if (screen == "story-blackthorn-open-mastery" ||
                     screen == "story-blackthorn-open-mastery-clocks" ||
                     screen == "story-blackthorn-open-mastery-timeouts" ||
                     screen == "story-blackthorn-open-mastery-choice")
            {
                storyCampaign = StoryCampaign::Blackthorn;
                missionId = "bt17b_open_mastery";
                storyComicPage = 0;
            }
            else if (screen == "story-blackthorn-field-judgment")
            {
                storyCampaign = StoryCampaign::Blackthorn;
                missionId = "bt17a_field_judgment";
                storyComicPage = 0;
            }
            else
            {
                storyCampaign = StoryCampaign::Seelie;
                if (screen == "story-seelie-mirror-remembers" ||
                    screen == "story-seelie-mirror-orientation")
                {
                    missionId = "se00_what_mirror_remembers";
                    storyComicPage = 0;
                }
                else if (screen == "story-seelie-mirror-causal-setup")
                {
                    missionId = "se00_what_mirror_remembers";
                    storyComicPage = 2;
                }
                else if (screen == "story-seelie-mirror-marrowind")
                {
                    missionId = "se00_what_mirror_remembers";
                    storyComicPage = 4;
                }
                else if (screen == "story-seelie-sella-pallid" ||
                         screen == "story-seelie-sella-name-eaten")
                {
                    missionId = "se08a_sella_receives_herself";
                    storyComicPage =
                        screen == "story-seelie-sella-name-eaten" ? 4 : 3;
                }
                else if (screen == "story-seelie-vow-web-cause" ||
                         screen == "story-seelie-vow-web-consequence")
                {
                    missionId = "se18_what_complete_means";
                    storyComicPage =
                        screen == "story-seelie-vow-web-consequence" ? 2 : 0;
                }
                else if (screen == "story-seelie-pump-four-chronicle-boundary" ||
                         screen == "story-seelie-pump-four-rules-boundary")
                {
                    missionId = "se27_emperor_at_tree";
                    storyComicPage =
                        screen == "story-seelie-pump-four-rules-boundary" ? 4 : 2;
                }
                else if (screen == "story-seelie-before-next-dawn")
                {
                    missionId = "se18c_before_next_dawn";
                    storyComicPage = 0;
                }
                else if (screen == "story-seelie-seven-corrections")
                {
                    missionId = "se28b_seven_corrections";
                    storyComicPage = 3;
                }
                else if (screen == "story-seelie-vow-record")
                {
                    missionId = "se28c_a_vow_can_end";
                    storyComicPage = 2;
                }
                else if (screen == "story-seelie-open-record")
                {
                    missionId = "se28d_an_unfinished_record";
                    storyComicPage = 3;
                }
                else if (screen == "story-seelie-final-recap" ||
                         screen == "story-seelie-final-recap-empty-place")
                {
                    missionId = "se28a_two_men_one_vacancy";
                    storyComicPage =
                        screen == "story-seelie-final-recap-empty-place" ? 1 : 0;
                }
                else if (screen == "story-seelie-ascent-refusal")
                {
                    missionId = "se29a_baalzapub_ascendant";
                    storyComicPage = 3;
                }
                else if (screen == "story-seelie-workers-refuse")
                {
                    missionId = "se29b_fizzlewick_says_no";
                    storyComicPage = 3;
                }
                else if (screen == "story-seelie-first-release")
                {
                    missionId = "se29c_all_authority_returns";
                    storyComicPage = 1;
                }
                else if (screen == "story-seelie-separate-withdrawals" ||
                         screen == "story-seelie-separate-withdrawals-coda")
                {
                    missionId = "se29c2_separate_withdrawals";
                    storyComicPage =
                        screen == "story-seelie-separate-withdrawals-coda" ? 5 : 1;
                }
                else
                {
                    missionId = "se30_names_we_keep";
                    storyComicPage = 4;
                }
            }
            storyMissionIndex = storyMissionIndexById(storyCampaign, missionId);
            if (screen == "story-mirewatch-first-open-check" ||
                screen == "story-blackthorn-first-open-check")
            {
                const StoryMission& openCheck = activeStoryMission();
                const int friendlyCount = static_cast<int>(std::count_if(
                    openCheck.pieces.begin(), openCheck.pieces.end(),
                    [](const StoryPiecePlacement& piece) { return piece.owner == 1; }));
                const int enemyCount = static_cast<int>(std::count_if(
                    openCheck.pieces.begin(), openCheck.pieces.end(),
                    [](const StoryPiecePlacement& piece) { return piece.owner == 2; }));
                if (!openCheck.optionalRehearsal || !openCheck.script.empty() ||
                    openCheck.objectiveSpec.kind != StoryObjectiveKind::DefeatAllEnemies ||
                    friendlyCount != 3 || enemyCount != 3 ||
                    !openCheck.masteryCards.empty() || !openCheck.masteryRules.empty())
                {
                    failCaptureValidation(
                        "Capture registry error: the first open check must remain an optional, unscripted, mastery-free ordinary 3v3 objective.");
                }
            }
            if (screen == "story-blackthorn-open-mastery-clocks")
            {
                storyComicPage = 6;
            }
            else if (screen == "story-blackthorn-open-mastery-timeouts")
            {
                storyComicPage = 7;
            }
            else if (screen == "story-blackthorn-open-mastery-choice" ||
                     screen == "story-seelie-mirror-orientation")
            {
                storyComicPage = static_cast<int>(activeStoryMission().briefing.size()) - 1;
            }
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (const std::optional<StoryPageCaptureTarget> storyPageCapture =
                     storyPageCaptureForKey(screen))
        {
            storyCampaign = storyPageCapture->campaign;
            storyMissionIndex = storyPageCapture->missionIndex;
            const StoryMission& mission = activeStoryMission();
            if (storyPageCapture->kind == StoryPageCaptureKind::Briefing)
            {
                if (storyPageCapture->panelIndex >= mission.briefing.size())
                {
                    failCaptureValidation(
                        "Capture seed error: Story briefing page is out of range.");
                }
                else
                {
                    storyComicPage =
                        static_cast<int>(storyPageCapture->panelIndex);
                    currentState = GameState::StoryIntro;
                    title.setString("");
                    centerText(title, 400.0f);
                }
            }
            else if (storyPageCapture->kind ==
                     StoryPageCaptureKind::ActionStep)
            {
                const bool openObjective = mission.script.empty();
                const bool indexInRange = openObjective
                    ? storyPageCapture->stepIndex == 0
                    : storyPageCapture->stepIndex < mission.script.size();
                if (!indexInRange ||
                    mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
                {
                    failCaptureValidation(
                        "Capture seed error: Story action step is out of range.");
                }
                else
                {
                    const int completedCountBefore = storyCompletedCount;
                    const int campaignProgressBefore =
                        storyCampaignProgress[storyProgressIndex(storyCampaign)];
                    const bool replayed = openObjective
                        ? (beginStory(),
                           storyEngine != nullptr &&
                               storyStage != StoryStage::Failed)
                        : replayScriptedStoryForCapture(
                              storyPageCapture->stepIndex);
                    if (replayed)
                    {
                        if (!openObjective)
                        {
                            // replayScriptedStoryForCapture first proves the
                            // exact target popup. Removing it here reveals the
                            // production instruction, highlights, hand state,
                            // and owner banner for the action itself.
                            dismissStoryPanelsForCapture();
                        }
                        storyAiPending = false;
                        storyScriptActionAt =
                            std::numeric_limits<float>::max();
                        if (storyCompletedCount != completedCountBefore ||
                            storyCampaignProgress[
                                storyProgressIndex(storyCampaign)] !=
                                campaignProgressBefore)
                        {
                            failCaptureValidation(
                                "Capture replay error: Story action fixture "
                                "mutated campaign progress while seeding.");
                        }
                        else if (const std::optional<std::string> error =
                                     storyActionCaptureStateError(
                                         storyPageCapture->stepIndex,
                                         openObjective))
                        {
                            failCaptureValidation(
                                "Capture replay error: Story mission '" +
                                std::string(mission.id) + "' action " +
                                std::to_string(storyPageCapture->stepIndex + 1) +
                                " " + *error);
                        }
                        else
                        {
                            storyActionCaptureInvariant =
                                StoryActionCaptureInvariant{
                                    storyCampaign,
                                    storyMissionIndex,
                                    storyPageCapture->stepIndex,
                                    openObjective,
                                    storyCompletedCount,
                                    storyCampaignProgress[
                                        storyProgressIndex(storyCampaign)],
                                    storyEngine->phase(),
                                    storyEngine->currentPlayer()};
                        }
                    }
                    else if (openObjective)
                    {
                        failCaptureValidation(
                            "Capture replay error: open Story mission '" +
                            std::string(mission.id) +
                            "' did not initialize its authoritative action state.");
                    }
                }
            }
            else if (storyPageCapture->kind ==
                     StoryPageCaptureKind::BeforeStep)
            {
                if (storyPageCapture->stepIndex >= mission.script.size() ||
                    storyPageCapture->panelIndex >=
                        mission.script[storyPageCapture->stepIndex]
                            .panelsBefore.size())
                {
                    failCaptureValidation(
                        "Capture seed error: Story before-step beat is out of range.");
                }
                else
                {
                    if (replayScriptedStoryForCapture(
                            storyPageCapture->stepIndex))
                    {
                        if (storyPageCapture->panelIndex >=
                                storyPopupPanels.size())
                        {
                            failCaptureValidation(
                                "Capture seed error: replayed before-step "
                                "popup page is out of range.");
                        }
                        else
                        {
                            storyPopupPage = storyPageCapture->panelIndex;
                        }
                    }
                }
            }
            else
            {
                if (storyPageCapture->panelIndex >= mission.aftermath.size())
                {
                    failCaptureValidation(
                        "Capture seed error: Story aftermath beat is out of range.");
                }
                else
                {
                    const bool replayed = mission.script.empty()
                        ? replayOpenStoryAftermathForCapture()
                        : replayScriptedStoryForCapture(
                              mission.script.size());
                    if (replayed)
                    {
                        if (storyPageCapture->panelIndex >=
                                storyPopupPanels.size())
                        {
                            failCaptureValidation(
                                "Capture seed error: replayed aftermath "
                                "popup page is out of range.");
                        }
                        else
                        {
                            storyPopupPage = storyPageCapture->panelIndex;
                        }
                    }
                }
            }
        }
        else if (screen.rfind("story-art-mw-", 0) == 0 ||
                 screen.rfind("story-art-bt-", 0) == 0 ||
                 screen.rfind("story-art-se-", 0) == 0)
        {
            constexpr std::string_view mirewatchPrefix = "story-art-mw-";
            constexpr std::string_view blackthornPrefix = "story-art-bt-";
            constexpr std::string_view seeliePrefix = "story-art-se-";
            std::string_view missionId;
            if (screen.rfind(mirewatchPrefix, 0) == 0)
            {
                storyCampaign = StoryCampaign::Mirewatch;
                missionId = std::string_view(screen).substr(mirewatchPrefix.size());
            }
            else if (screen.rfind(blackthornPrefix, 0) == 0)
            {
                storyCampaign = StoryCampaign::Blackthorn;
                missionId = std::string_view(screen).substr(blackthornPrefix.size());
            }
            else
            {
                storyCampaign = StoryCampaign::Seelie;
                missionId = std::string_view(screen).substr(seeliePrefix.size());
            }
            storyMissionIndex = storyMissionIndexById(storyCampaign, missionId);
            storyComicPage = 0;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen.rfind("story-art-popup-mw-", 0) == 0 ||
                 screen.rfind("story-art-popup-bt-", 0) == 0 ||
                 screen.rfind("story-art-popup-se-", 0) == 0)
        {
            constexpr std::string_view mirewatchPrefix = "story-art-popup-mw-";
            constexpr std::string_view blackthornPrefix = "story-art-popup-bt-";
            constexpr std::string_view seeliePrefix = "story-art-popup-se-";
            std::string_view missionId;
            if (screen.rfind(mirewatchPrefix, 0) == 0)
            {
                storyCampaign = StoryCampaign::Mirewatch;
                missionId = std::string_view(screen).substr(mirewatchPrefix.size());
            }
            else if (screen.rfind(blackthornPrefix, 0) == 0)
            {
                storyCampaign = StoryCampaign::Blackthorn;
                missionId = std::string_view(screen).substr(blackthornPrefix.size());
            }
            else
            {
                storyCampaign = StoryCampaign::Seelie;
                missionId = std::string_view(screen).substr(seeliePrefix.size());
            }
            storyMissionIndex = storyMissionIndexById(storyCampaign, missionId);
            const StoryMission& mission = activeStoryMission();
            if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly ||
                mission.briefing.empty() ||
                (mission.scenarioArtPath.empty() && mission.briefing.front().artPath.empty()))
            {
                failCaptureValidation(
                    "Capture seed error: tactical scenario-art popup requires a "
                    "first briefing panel with commissioned mission or panel art.");
            }
            beginStory();
            if (!mission.briefing.empty())
            {
                queueStoryPanels({mission.briefing.front()}, false);
            }
        }
        else if (screen == "story-blackthorn-victor-reckoning-climax")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt17c_victor_reckoning");
            storyComicPage = static_cast<int>(activeStoryMission().briefing.size()) - 1;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-blackthorn-deed" ||
                 screen == "story-mirewatch-deed")
        {
            storyCampaign = screen.rfind("story-mirewatch-", 0) == 0
                ? StoryCampaign::Mirewatch
                : StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign,
                storyCampaign == StoryCampaign::Mirewatch
                    ? "mw25_deed_own_hand"
                    : "bt18_deed_own_hand");
            storyComicPage = 0;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-seelie-story-long")
        {
            storyCampaign = StoryCampaign::Seelie;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "se29_no_complete_bearer");
            // The third panel is deliberately one of the campaign's longest;
            // keep it in the review suite so wrapping regressions are visible.
            storyComicPage = 2;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-seelie-witness-rail")
        {
            storyCampaign = StoryCampaign::Seelie;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "se29e_hold_the_witness_rail");
            storyComicPage = 1;
            currentState = GameState::StoryIntro;
            title.setString("");
            centerText(title, 400.0f);
        }
        else if (screen == "story-mirewatch-telos-panel" ||
                 screen == "story-mirewatch-aftermath")
        {
            const bool replayed = screen == "story-mirewatch-aftermath"
                ? replayRiverTeethForCapture(11)
                : replayRiverTeethForCapture(8, true);
            if (!replayed)
            {
                failCaptureValidation(
                    "Capture replay error: the requested River Teeth panel was not reached.");
            }
            else if (screen == "story-mirewatch-telos-panel")
            {
                const bool hasTelosPanel = std::any_of(
                    storyPopupPanels.begin(), storyPopupPanels.end(),
                    [](const StoryPanel& panel) {
                        return panel.speaker == "Telos the Merchant";
                    });
                if (!hasTelosPanel)
                {
                    failCaptureValidation(
                        "Capture replay error: the real Telos Travel panel is absent.");
                }
            }
            else if (storyPopupPanels.empty() || !storyCompleteAfterPopup)
            {
                failCaptureValidation(
                    "Capture replay error: River Teeth did not reach its real aftermath popup.");
            }
        }
        else if (screen == "story-deployment")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt03_sanctuary_debt");
            beginStory();
            const StoryMission& mission = activeStoryMission();
            if (!storyEngine || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 1 || storyMissionStep != 0 ||
                mission.script.empty() ||
                mission.script.front().kind != StoryActionKind::PlayCard ||
                mission.script.front().cardTitle != "Blackthorn Alchemist" ||
                mission.script.front().targetRow != 3 ||
                mission.script.front().targetColumn != 2)
            {
                failCaptureValidation(
                    "Capture setup error: Sanctuary Debt did not reach its exact deployment step.");
                return;
            }

            const GameEngine::EnginePlayer& player = storyEngine->playerState(1);
            const bool controlledTarget =
                storyEngine->boardControl()[static_cast<std::size_t>(
                    game_data::squareIndex(3, 2))] == 1;
            if (player.resources != mission.playerResources ||
                player.hand.size() != 1 ||
                player.hand.front().title != "Blackthorn Alchemist" ||
                player.hand.front().type != "Unit" ||
                player.hand.front().cost <= 0 ||
                player.pieceActionUsedThisTurn || !controlledTarget)
            {
                failCaptureValidation(
                    "Capture setup error: the deployment hand, Resources, action budget, or controlled target drifted.");
                return;
            }

            // Keep the displayed capture on the pre-action teaching frame, but
            // prove that this exact fixture can execute the real deployment and
            // reaches every claimed postcondition.
            GameEngine deploymentWitness = *storyEngine;
            const int resourcesBefore = player.resources;
            const int cardCost = player.hand.front().cost;
            const std::size_t handBefore = player.hand.size();
            const std::size_t piecesBefore = storyEngine->boardPieces().size();
            const bool deployed = deploymentWitness.playCard(1, 0, 3, 2);
            const auto alchemist = std::find_if(
                deploymentWitness.boardPieces().begin(),
                deploymentWitness.boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.owner == 1 &&
                        piece.name == "Blackthorn Alchemist" &&
                        piece.row == 3 && piece.column == 2;
                });
            const GameEngine::EnginePlayer& after = deploymentWitness.playerState(1);
            if (!deployed ||
                deploymentWitness.boardPieces().size() != piecesBefore + 1 ||
                alchemist == deploymentWitness.boardPieces().end() ||
                !alchemist->hasActed ||
                after.resources != resourcesBefore - cardCost ||
                after.hand.size() + 1 != handBefore ||
                after.pieceActionUsedThisTurn)
            {
                failCaptureValidation(
                    "Capture witness error: the Alchemist did not deploy at exact cost, leave the normal action available, and arrive exhausted.");
            }
        }
        else if (screen == "story-sharpshooter-aimed" ||
                 screen == "story-sharpshooter-state-lowered" ||
                 screen == "story-sharpshooter-state-raised")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt02_customs_bell");
            const StoryMission& mission = activeStoryMission();
            const auto stepByHeading = [&](std::string_view heading) {
                return std::find_if(
                    mission.script.begin(),
                    mission.script.end(),
                    [&](const StoryScriptAction& step) {
                        return step.heading == heading;
                    });
            };
            const auto raiseStep = stepByHeading("RAISE THE GUN");
            const auto fireStepIt = stepByHeading("SHARPSHOOTER - FIRE");
            if (raiseStep == mission.script.end() ||
                fireStepIt == mission.script.end())
            {
                failCaptureValidation(
                    "Capture replay error: Customs Bell lost its authored Sharpshooter state lesson.");
                return;
            }
            const std::size_t raiseStepIndex = static_cast<std::size_t>(
                std::distance(mission.script.begin(), raiseStep));
            if (!replayScriptedStoryForCapture(raiseStepIndex) ||
                !storyEngine || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 1 ||
                storyMissionStep != static_cast<int>(raiseStepIndex))
            {
                failCaptureValidation(
                    "Capture replay error: Customs Bell did not reach its authored Raise Gun beat.");
                return;
            }

            const auto sharpshooter = std::find_if(
                storyEngine->boardPieces().begin(),
                storyEngine->boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Goblin Sharpshooter";
                });
            if (sharpshooter == storyEngine->boardPieces().end())
            {
                failCaptureValidation(
                    "Capture replay error: the Goblin Sharpshooter is absent.");
                return;
            }

            const int pieceId = sharpshooter->id;
            const StoryScriptAction& transformStep = *raiseStep;
            if (!storyAbilityStepMatches(transformStep, *sharpshooter) ||
                game_data::pieceAbilityLabel(*sharpshooter) != "Raise Gun" ||
                sharpshooter->actionState != 0 || sharpshooter->hasActed)
            {
                failCaptureValidation(
                    "Capture replay error: the lowered Sharpshooter no longer exposes the authored Raise Gun state.");
                return;
            }

            if (screen != "story-sharpshooter-state-lowered")
            {
                const game_data::Piece beforeTransform = *sharpshooter;
                sendUseAbility(pieceId);
                const game_data::Piece* transformed = gamePieceById(pieceId);
                if (storyMissionStep != static_cast<int>(raiseStepIndex + 1) ||
                    transformed == nullptr ||
                    !storyUsedAim || transformed->actionState != 1 ||
                    !transformed->hasActed ||
                    game_data::pieceAbilityLabel(*transformed) != "Lower Gun" ||
                    !storyAbilityOutcomeMatches(
                        transformStep,
                        beforeTransform,
                        storyEngine->boardPieces(),
                        storyEngine->commandingPiece()))
                {
                    failCaptureValidation(
                        "Capture replay error: Raise Gun did not produce the exact transformed state.");
                    return;
                }

                const StoryScriptAction& fireStep = *fireStepIt;
                const std::optional<int> fireIndex =
                    storyExpectedActionProfileIndex(fireStep, *transformed);
                if (!fireIndex ||
                    *fireIndex < 0 ||
                    *fireIndex >= static_cast<int>(transformed->actions.size()) ||
                    !transformed->actions[static_cast<std::size_t>(*fireIndex)].canAttack ||
                    transformed->actions[static_cast<std::size_t>(*fireIndex)].canMove)
                {
                    failCaptureValidation(
                        "Capture replay error: the raised Sharpshooter has no unique active Fire profile.");
                    return;
                }
            }

            const game_data::Piece* capturedSharpshooter = gamePieceById(pieceId);
            const int expectedState =
                screen == "story-sharpshooter-state-lowered" ? 0 : 1;
            if (capturedSharpshooter == nullptr ||
                capturedSharpshooter->actionState != expectedState)
            {
                failCaptureValidation(
                    "Capture replay error: Sharpshooter did not reach the requested printed-action state.");
                return;
            }
            if (screen == "story-sharpshooter-aimed")
            {
                selectedPieceId = pieceId;
            }
            else
            {
                inspectedPieceId = pieceId;
                inspectedPieceScroll = 0.0f;
            }
        }
        else if (screen == "story-blackthorn-hidden-collision-choice" ||
                 screen == "story-blackthorn-hidden-collision-resolved")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt04_terms_conditions");
            beginStory();
            const int pathAmbusherId = storyPieceIdForRole("ambusher_path");
            const int collisionAmbusherId =
                storyPieceIdForRole("ambusher_collision");
            const int erevanId = storyPieceIdForRole("erevan");
            if (pathAmbusherId == 0 || collisionAmbusherId == 0 || erevanId == 0)
            {
                failCaptureValidation(
                    "Capture replay error: the Hidden Transit roles are incomplete.");
            }
            else
            {
                const auto runScriptedOpponentStep = [&]() {
                    storyScriptActionAt = animationTime;
                    updateStoryAi();
                };
                sendUseAbility(pathAmbusherId);
                sendEndTurn();
                runScriptedOpponentStep();
                runScriptedOpponentStep();
                requestPieceAction(pathAmbusherId, 3, 4);
                sendEndTurn();
                runScriptedOpponentStep();
                sendUseAbility(collisionAmbusherId);
                sendEndTurn();
                runScriptedOpponentStep();
                requestPieceAction(collisionAmbusherId, 5, 2);

                int ambushOption = -1;
                if (pendingPieceActionChoice)
                {
                    const game_data::Piece* actor = gamePieceById(collisionAmbusherId);
                    for (std::size_t option = 0;
                         actor != nullptr &&
                         option < pendingPieceActionChoice->actionIndices.size();
                         ++option)
                    {
                        const int actionIndex =
                            pendingPieceActionChoice->actionIndices[option];
                        if (actionIndex >= 0 &&
                            actionIndex < static_cast<int>(actor->actions.size()) &&
                            actor->actions[static_cast<std::size_t>(actionIndex)].name ==
                                "Ambush")
                        {
                            ambushOption = static_cast<int>(option);
                        }
                    }
                }
                if (!pendingPieceActionChoice ||
                    pendingPieceActionChoice->actionIndices.size() != 2 ||
                    ambushOption < 0)
                {
                    failCaptureValidation(
                        "Capture replay error: the hidden collision did not offer both source profiles.");
                }
                else if (screen == "story-blackthorn-hidden-collision-resolved")
                {
                    submitPendingPieceActionChoice(ambushOption);
                    const game_data::Piece* ambusher =
                        gamePieceById(collisionAmbusherId);
                    const game_data::Piece* erevan = gamePieceById(erevanId);
                    if (ambusher == nullptr || erevan == nullptr ||
                        ambusher->row != 5 || ambusher->column != 1 ||
                        ambusher->actionState != 0 || erevan->hidden ||
                        erevan->disabledTurns <= 0 ||
                        storyMissionStep != 11 || !storyPopupPanels.empty() ||
                        storyCompleteAfterPopup)
                    {
                        failCaptureValidation(
                            "Capture replay error: Ambush did not resolve the authored hidden collision.");
                    }
                }
            }
        }
        else if (screen == "story-powers-used")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt17a_field_judgment");
            beginStory();
            if (!storyEngine || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 1 ||
                !activeStoryMission().script.empty())
            {
                failCaptureValidation(
                    "Capture replay error: Field Judgment did not reach its open player turn.");
                return;
            }
            const auto foreman = std::find_if(
                storyEngine->boardPieces().begin(),
                storyEngine->boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Blackthorn Foreman";
                });
            if (foreman == storyEngine->boardPieces().end())
            {
                failCaptureValidation(
                    "Capture replay error: the Blackthorn Foreman is absent from Field Judgment.");
                return;
            }

            const int pieceId = foreman->id;
            const game_data::Piece foremanBefore = *foreman;
            const auto [summonRow, summonColumn] =
                game_data::summonDestination(foremanBefore);
            StoryScriptAction summonWitness;
            summonWitness.kind = StoryActionKind::UseAbility;
            summonWitness.owner = 1;
            summonWitness.expectedAbility = "summon";
            summonWitness.expectedAbilityLabel = "Summon";
            summonWitness.expectedSummonTitle = "Blackthorn Lumberjack";
            if (!storyAbilityStepMatches(summonWitness, foremanBefore) ||
                foremanBefore.hasActed ||
                storyEngine->playerState(1).pieceActionUsedThisTurn ||
                !game_data::pieceFootprintFree(
                    storyEngine->boardPieces(),
                    foremanBefore,
                    summonRow,
                    summonColumn))
            {
                failCaptureValidation(
                    "Capture replay error: the Foreman''s real Summon preconditions are not present.");
                return;
            }

            const std::size_t piecesBefore = storyEngine->boardPieces().size();
            const int stepBefore = storyMissionStep;
            // This is the same legal Summon a player can choose in the open
            // rehearsal, including the real front-square requirement.
            sendUseAbility(pieceId);
            const game_data::Piece* foremanAfter = gamePieceById(pieceId);
            const int summonedCount = static_cast<int>(std::count_if(
                storyEngine->boardPieces().begin(),
                storyEngine->boardPieces().end(),
                [&](const game_data::Piece& piece) {
                    return piece.id != pieceId && piece.owner == 1 &&
                        piece.name == "Blackthorn Lumberjack" &&
                        piece.row == summonRow && piece.column == summonColumn &&
                        piece.hasActed;
                }));
            if (storyMissionStep != stepBefore || !storyUsedSummon ||
                storyStage != StoryStage::Objective || foremanAfter == nullptr ||
                !foremanAfter->hasActed ||
                storyEngine->boardPieces().size() != piecesBefore + 1 ||
                summonedCount != 1 || storyEngine->commandingPiece() != 0 ||
                !storyEngine->playerState(1).pieceActionUsedThisTurn ||
                !storyAbilityOutcomeMatches(
                    summonWitness,
                    foremanBefore,
                    storyEngine->boardPieces(),
                    storyEngine->commandingPiece()))
            {
                failCaptureValidation(
                    "Capture replay error: Foreman Summon did not create exactly one exhausted Lumberjack and spend the ordinary action.");
                return;
            }
            selectedPieceId = pieceId;
        }
        else if (screen == "story-mirewatch-pull-popup")
        {
            inspectStoryPieceForCapture(
                StoryCampaign::Mirewatch,
                "mw03_town_under_company",
                "Bog Spearman");
        }
        else if (screen == "story-mirewatch-intercept-popup")
        {
            inspectStoryPieceForCapture(
                StoryCampaign::Mirewatch,
                "mw11_no_plan_saves_all",
                "Juniper Flash");
        }
        else if (screen == "story-mirewatch-reveal-popup")
        {
            inspectStoryPieceForCapture(
                StoryCampaign::Mirewatch,
                "mw13_making_credit",
                "Swamp Tracker");
        }
        else if (screen == "story-blackthorn-capture-popup")
        {
            inspectStoryPieceForCapture(
                StoryCampaign::Blackthorn,
                "bt05_freight_office",
                "Grask");
        }
        else if (screen == "story-seelie-infest-popup")
        {
            inspectStoryPieceForCapture(
                StoryCampaign::Seelie,
                "se07a_queens_price_scene",
                "Queen Nyxara");
        }
        else if (screen == "story-blackthorn-standard-placement" ||
                 screen == "story-blackthorn-standard-opening")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt17b_open_mastery");
            beginStory();
            storyPopupPanels.clear();
            storyPopupPage = 0;
            storyCompleteAfterPopup = false;
            if (!captureRequest || !storyEngine || storyStage == StoryStage::Failed)
            {
                failCaptureValidation(
                    "Capture setup error: the Blackthorn placement screen is a capture-only fixture and did not initialize.");
                return;
            }

            // This screen deliberately exercises the ordinary placement UI
            // with reviewed packaged definitions.  It is not evidence that a
            // connected card server supplied or approved either deck.
            const StoryMission& fixtureMission = activeStoryMission();
            const auto exactPackagedDeckState = [&](const std::vector<std::string_view>& authored,
                                                    const GameEngine::EnginePlayer& player) {
                std::vector<std::string> expectedHeroes;
                std::vector<std::string> expectedDrawPile;
                for (const std::string_view title : authored)
                {
                    const std::optional<game_data::GameCard> resolved =
                        resolvedStoryCardNamed(title);
                    if (!resolved || resolved->title != title ||
                        resolved->type == "Story Error")
                    {
                        return false;
                    }
                    (resolved->type == "Hero" ? expectedHeroes : expectedDrawPile)
                        .push_back(resolved->title);
                }

                std::vector<std::string> actualHeroes;
                std::vector<std::string> actualDrawPile;
                for (const game_data::GameCard& card : player.heroesToPlace)
                {
                    actualHeroes.push_back(card.title);
                }
                for (const game_data::GameCard& card : player.drawPile)
                {
                    actualDrawPile.push_back(card.title);
                }
                std::sort(expectedHeroes.begin(), expectedHeroes.end());
                std::sort(expectedDrawPile.begin(), expectedDrawPile.end());
                std::sort(actualHeroes.begin(), actualHeroes.end());
                std::sort(actualDrawPile.begin(), actualDrawPile.end());
                return authored.size() == 22 && expectedHeroes.size() == 2 &&
                    expectedDrawPile.size() == 20 && actualHeroes == expectedHeroes &&
                    actualDrawPile == expectedDrawPile && player.hand.empty() &&
                    player.foresightChoices.empty() && player.resources == 0 &&
                    player.discardsThisTurn == 0 &&
                    !player.pieceActionUsedThisTurn && player.deckSubmitted;
            };
            const auto snapshotHandIs = [&](std::string_view first, std::string_view second) {
                if (gameSnapshot.hand.size() != 2)
                {
                    return false;
                }
                std::array<std::string, 2> actual = {
                    gameSnapshot.hand[0].title, gameSnapshot.hand[1].title};
                std::array<std::string, 2> expected = {
                    std::string(first), std::string(second)};
                std::sort(actual.begin(), actual.end());
                std::sort(expected.begin(), expected.end());
                return actual == expected;
            };
            const bool exactAuthoredHeroes =
                fixtureMission.playerDeck.size() == 22 &&
                fixtureMission.enemyDeck.size() == 22 &&
                fixtureMission.playerDeck[0] == "Thaeron Baelstone" &&
                fixtureMission.playerDeck[1] == "Ashenfang" &&
                fixtureMission.enemyDeck[0] == "Maggie Mudroot" &&
                fixtureMission.enemyDeck[1] == "Joni Pumpernickel";
            const GameEngine::EnginePlayer& player = storyEngine->playerState(1);
            const GameEngine::EnginePlayer& opponent = storyEngine->playerState(2);
            if (!fixtureMission.standardMatch || !exactAuthoredHeroes ||
                !storyEngine->bothDecksSubmitted() ||
                storyEngine->phase() != game_data::Phase::HeroPlacement ||
                storyEngine->currentPlayer() != 1 ||
                !storyEngine->timersAreEnabled() ||
                !exactPackagedDeckState(fixtureMission.playerDeck, player) ||
                !exactPackagedDeckState(fixtureMission.enemyDeck, opponent) ||
                !storyEngine->boardPieces().empty() ||
                !storyEngine->boardEnchantments().empty() ||
                static_cast<game_data::Phase>(gameSnapshot.phase) !=
                    game_data::Phase::HeroPlacement ||
                gameSnapshot.activePlayer != 1 || gameSnapshot.yourPlayer != 1 ||
                gameSnapshot.winner != 0 || !gameSnapshot.timersEnabled ||
                gameSnapshot.turnRemainingMs != GameEngine::FullTurnTimerMs ||
                gameSnapshot.players[0].clockRemainingMs != GameEngine::RegularClockMs ||
                gameSnapshot.players[1].clockRemainingMs != GameEngine::RegularClockMs ||
                gameSnapshot.players[0].heroesToPlace != 2 ||
                gameSnapshot.players[1].heroesToPlace != 2 ||
                gameSnapshot.players[0].drawPileCount != 20 ||
                gameSnapshot.players[1].drawPileCount != 20 ||
                !snapshotHandIs("Thaeron Baelstone", "Ashenfang"))
            {
                failCaptureValidation(
                    fmt::format(
                        "Capture setup error: packaged-only Blackthorn placement fixture did not reach its exact two-Hero, 20-card, timed placement state (stage {}, phase {}, heroes {}, status: {}).",
                        static_cast<int>(storyStage),
                        static_cast<int>(storyEngine->phase()),
                        gameSnapshot.hand.size(),
                        gameSnapshot.status));
                return;
            }
            if (screen == "story-blackthorn-standard-opening")
            {
                const auto placeNamedHero = [&](int playerNumber,
                                                std::string_view title,
                                                int row,
                                                int column) {
                    const auto& heroes =
                        storyEngine->playerState(playerNumber).heroesToPlace;
                    const auto found = std::find_if(
                        heroes.begin(), heroes.end(),
                        [&](const game_data::GameCard& card) {
                            return card.title == title;
                        });
                    return found != heroes.end() &&
                        storyEngine->placeHero(
                            playerNumber,
                            static_cast<int>(std::distance(heroes.begin(), found)),
                            row,
                            column);
                };

                const bool legalPlacement =
                    placeNamedHero(1, "Thaeron Baelstone", 2, 0) &&
                    placeNamedHero(1, "Ashenfang", 5, 1) &&
                    placeNamedHero(2, "Maggie Mudroot", 3, 6) &&
                    placeNamedHero(2, "Joni Pumpernickel", 2, 7);
                game_data::Snapshot opening = storyEngine->snapshotFor(1);
                const auto hasHeroAt = [&](int owner,
                                           std::string_view title,
                                           int row,
                                           int column,
                                           int width,
                                           int height) {
                    return std::any_of(
                        opening.pieces.begin(), opening.pieces.end(),
                        [&](const game_data::Piece& piece) {
                            return piece.owner == owner && piece.isHero &&
                                piece.name == title && piece.row == row &&
                                piece.column == column && piece.width == width &&
                                piece.height == height;
                        });
                };
                const GameEngine::EnginePlayer& openingPlayer =
                    storyEngine->playerState(1);
                const GameEngine::EnginePlayer& openingOpponent =
                    storyEngine->playerState(2);
                if (!legalPlacement ||
                    storyEngine->phase() != game_data::Phase::Playing ||
                    storyEngine->currentPlayer() != 1 ||
                    !storyEngine->timersAreEnabled() ||
                    storyStage != StoryStage::Objective ||
                    opening.phase !=
                        static_cast<std::uint8_t>(game_data::Phase::Playing) ||
                    opening.activePlayer != 1 || opening.yourPlayer != 1 ||
                    opening.winner != 0 || !opening.timersEnabled ||
                    opening.turnRemainingMs != GameEngine::FullTurnTimerMs ||
                    opening.players[0].clockRemainingMs !=
                        GameEngine::RegularClockMs ||
                    opening.players[1].clockRemainingMs !=
                        GameEngine::RegularClockMs ||
                    opening.hand.size() != 4 ||
                    opening.players[0].handCount != 4 ||
                    opening.players[1].handCount != 4 ||
                    opening.players[0].drawPileCount != 16 ||
                    opening.players[1].drawPileCount != 16 ||
                    openingPlayer.hand.size() != 4 ||
                    openingOpponent.hand.size() != 4 ||
                    openingPlayer.drawPile.size() != 16 ||
                    openingOpponent.drawPile.size() != 16 ||
                    opening.players[0].controlledSquares <= 0 ||
                    opening.players[0].resources !=
                        opening.players[0].controlledSquares ||
                    opening.players[1].resources != 0 ||
                    opening.pieces.size() != 4 ||
                    !storyEngine->boardEnchantments().empty() ||
                    !hasHeroAt(1, "Thaeron Baelstone", 2, 0, 1, 1) ||
                    !hasHeroAt(1, "Ashenfang", 5, 1, 1, 1) ||
                    !hasHeroAt(2, "Maggie Mudroot", 3, 6, 2, 2) ||
                    !hasHeroAt(2, "Joni Pumpernickel", 2, 7, 1, 1))
                {
                    failCaptureValidation(
                        fmt::format(
                            "Capture setup error: Blackthorn ordinary opening drifted (placed {}, stage {}, phase {}, active {}, hand {}, draw {}/{}, resources {}/{}, pieces {}, status: {}).",
                            legalPlacement,
                            static_cast<int>(storyStage),
                            static_cast<int>(storyEngine->phase()),
                            storyEngine->currentPlayer(),
                            opening.hand.size(),
                            opening.players[0].drawPileCount,
                            opening.players[1].drawPileCount,
                            opening.players[0].resources,
                            opening.players[1].resources,
                            opening.pieces.size(),
                            opening.status));
                    return;
                }
                commitLocalSnapshot(std::move(opening));
                storyAiPending = false;
                gameSnapshot.status =
                    "ORDINARY STORY MATCH - opening turn; packaged capture fixture.";
            }
            else
            {
                gameSnapshot.status =
                    "UI CAPTURE FIXTURE ONLY - packaged cards; live catalog authority is not claimed.";
            }
        }
        else if (screen == "story-ai-turn")
        {
            storyCampaign = StoryCampaign::Blackthorn;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "bt17a_field_judgment");
            beginStory();
            if (!storyEngine || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 1 ||
                gameSnapshot.activePlayer != 1 ||
                !activeStoryMission().script.empty())
            {
                failCaptureValidation(
                    "Capture replay error: Field Judgment did not begin on an ordinary open player turn.");
                return;
            }
            const int stepBefore = storyMissionStep;
            // Open rehearsals use the ordinary turn rules, so this legal pass
            // reaches the exact opponent-turn state the capture is reviewing.
            sendEndTurn();
            const bool plannerWasScheduled = storyAiPending;
            if (storyStage != StoryStage::Objective ||
                storyMissionStep != stepBefore ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 2 ||
                gameSnapshot.activePlayer != 2 ||
                !plannerWasScheduled)
            {
                failCaptureValidation(
                    "Capture replay error: the legal pass did not reach a genuine scheduled opponent turn.");
                return;
            }
            // Hold the screenshot on the post-pass frame; live play still starts
            // the normal planner after its short readability pause.
            storyAiPending = false;
        }
        else if (screen == "story-ai-attack")
        {
            storyCampaign = StoryCampaign::Seelie;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "se02_broken_bridge");
            beginStory();
            if (!storyEngine || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 1 || storyMissionStep != 0 ||
                activeStoryMission().script.size() < 4)
            {
                failCaptureValidation(
                    "Capture replay error: The Broken Bridge did not reach its opening scripted state.");
                return;
            }

            // Follow the authored lesson through two legal player inputs, then
            // let its real Bristlejack attack resolve through the story engine.
            const int sisterId = storyPieceIdForRole("sister");
            const int messengerId = storyPieceIdForRole("messenger");
            const int knightId = storyPieceIdForRole("knight");
            const int bristleId = storyPieceIdForRole("bristle");
            const StoryScriptAction& moveStep = activeStoryMission().script[0];
            const StoryScriptAction& passStep = activeStoryMission().script[1];
            const StoryScriptAction& attackStep = activeStoryMission().script[2];
            const game_data::Piece* sisterBefore = gamePieceById(sisterId);
            const game_data::Piece* bristleBeforeSetup = gamePieceById(bristleId);
            if (sisterId == 0 || messengerId == 0 || knightId == 0 ||
                bristleId == 0 || sisterBefore == nullptr ||
                bristleBeforeSetup == nullptr ||
                moveStep.kind != StoryActionKind::Move || moveStep.owner != 1 ||
                moveStep.actorRole != "sister" || moveStep.targetRow != 4 ||
                moveStep.targetColumn != 2 ||
                passStep.kind != StoryActionKind::EndTurn || passStep.owner != 1 ||
                attackStep.kind != StoryActionKind::Attack || attackStep.owner != 2 ||
                attackStep.actorRole != "bristle" ||
                attackStep.targetRole != "messenger" ||
                !storyExpectedActionProfileIndex(attackStep, *bristleBeforeSetup))
            {
                failCaptureValidation(
                    "Capture replay error: The Broken Bridge roles or authored move/attack identities drifted.");
                return;
            }

            requestPieceAction(sisterId, 4, 2);
            const game_data::Piece* sisterAfter = gamePieceById(sisterId);
            if (storyMissionStep != 1 || sisterAfter == nullptr ||
                sisterAfter->row != 4 || sisterAfter->column != 2 ||
                !sisterAfter->hasActed ||
                !storyEngine->playerState(1).pieceActionUsedThisTurn)
            {
                failCaptureValidation(
                    "Capture replay error: the Heartwood Sister''s authored Grovewalk was not accepted exactly.");
                return;
            }
            sendEndTurn();
            if (storyMissionStep != 2 || storyEngine->currentPlayer() != 2 ||
                gameSnapshot.activePlayer != 2)
            {
                failCaptureValidation(
                    "Capture replay error: The Broken Bridge did not enter its scripted Bristlejack turn.");
                return;
            }

            const game_data::Piece* messengerBefore = gamePieceById(messengerId);
            const game_data::Piece* knightBefore = gamePieceById(knightId);
            const game_data::Piece* bristleBefore = gamePieceById(bristleId);
            if (messengerBefore == nullptr || knightBefore == nullptr ||
                bristleBefore == nullptr)
            {
                failCaptureValidation(
                    "Capture replay error: a required Broken Bridge combat piece vanished before the attack.");
                return;
            }
            const game_data::Piece messengerStateBefore = *messengerBefore;
            const game_data::Piece knightStateBefore = *knightBefore;
            const game_data::Piece bristleStateBefore = *bristleBefore;
            storyScriptActionAt = std::numeric_limits<float>::lowest();
            updateStoryAi();
            const game_data::Piece* messengerAfter = gamePieceById(messengerId);
            const game_data::Piece* knightAfter = gamePieceById(knightId);
            const game_data::Piece* bristleAfter = gamePieceById(bristleId);
            if (storyMissionStep != 3 || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 2 || gameSnapshot.activePlayer != 2 ||
                messengerAfter == nullptr || knightAfter == nullptr ||
                bristleAfter == nullptr ||
                messengerAfter->health != messengerStateBefore.health ||
                messengerAfter->disabledTurns != messengerStateBefore.disabledTurns ||
                messengerAfter->sleepTurnsRemaining !=
                    messengerStateBefore.sleepTurnsRemaining ||
                knightAfter->health != knightStateBefore.health - 2 ||
                knightAfter->disabledTurns != game_data::DamageDisabledTurns ||
                knightAfter->sleepTurnsRemaining != 1 ||
                bristleAfter->row != bristleStateBefore.row ||
                bristleAfter->column != bristleStateBefore.column ||
                !bristleAfter->hasActed ||
                !storyEngine->playerState(2).pieceActionUsedThisTurn)
            {
                failCaptureValidation(
                    "Capture replay error: Bristle Charge did not leave the Messenger unharmed, redirect exactly 2 damage and Disable to the Knight, and retain the opposing turn.");
                return;
            }
            // Preserve the genuine after-attack/before-pass moment for all six
            // warm-up frames instead of allowing the next scripted End Turn.
            storyScriptActionAt = std::numeric_limits<float>::max();
        }
        else if (screen == "story-mirewatch-exit-confirmation" ||
                 screen == "story-mirewatch-restart-confirmation")
        {
            storyCampaign = StoryCampaign::Mirewatch;
            storyMissionIndex = storyTacticalMissionIndex(storyCampaign, 0);
            beginStory();
            gameConfirmationAction =
                screen == "story-mirewatch-exit-confirmation"
                    ? GameConfirmationAction::ExitStory
                    : GameConfirmationAction::RestartStory;
            resignConfirmPopupVisible = true;
        }
        else if (screen.rfind("story-game-", 0) == 0)
        {
            storyCampaign = StoryCampaign::Blackthorn;
            try
            {
                storyMissionIndex = storyTacticalMissionIndex(
                    storyCampaign,
                    std::stoi(screen.substr(std::string("story-game-").size())) - 1);
            }
            catch (const std::exception&)
            {
                failCaptureValidation(
                    "Capture setup error: invalid Blackthorn tactical screen key '" +
                    screen + "'.");
                return;
            }
            beginStory();
            if (screen.size() >= 6 &&
                screen.compare(screen.size() - 6, 6, "-board") == 0)
            {
                storyPopupPanels.clear();
                storyPopupPage = 0;
                storyCompleteAfterPopup = false;
            }
        }
        else if (screen.rfind("story-mirewatch-river-teeth-action-", 0) == 0)
        {
            constexpr std::string_view prefix =
                "story-mirewatch-river-teeth-action-";
            const std::string_view suffix =
                std::string_view(screen).substr(prefix.size());
            if (suffix.size() != 2 ||
                !std::all_of(suffix.begin(), suffix.end(), [](const char ch) {
                    return ch >= '0' && ch <= '9';
                }))
            {
                failCaptureValidation(
                    "Capture setup error: invalid River Teeth action screen key '" +
                    screen + "'; expected action-01 through action-11.");
                return;
            }
            const int actionNumber =
                (suffix[0] - '0') * 10 + (suffix[1] - '0');
            if (actionNumber < 1 || actionNumber > 11)
            {
                failCaptureValidation(
                    "Capture setup error: invalid River Teeth action screen key '" +
                    screen + "'; expected action-01 through action-11.");
                return;
            }
            if (!replayRiverTeethForCapture(actionNumber - 1))
            {
                failCaptureValidation(
                    "Capture replay error: the requested River Teeth action was not reached.");
                return;
            }
        }
        else if (screen == "story-mirewatch-river-teeth-aftermath-live" ||
                 screen == "story-mirewatch-river-teeth-complete-live")
        {
            if (!replayRiverTeethForCapture(11))
            {
                failCaptureValidation(
                    "Capture replay error: River Teeth did not reach its aftermath.");
            }
            else if (screen == "story-mirewatch-river-teeth-complete-live")
            {
                finishCapturedStoryAftermath();
                if (storyStage != StoryStage::Complete)
                {
                    failCaptureValidation(
                        "Capture replay error: River Teeth did not reach genuine completion.");
                }
            }
            else if (storyPopupPanels.empty() || !storyCompleteAfterPopup)
            {
                failCaptureValidation(
                    "Capture replay error: River Teeth aftermath is not genuine.");
            }
        }
        else if (screen == "story-mirewatch-intercept-aftermath-live" ||
                 screen == "story-mirewatch-intercept-footprint-live" ||
                 screen == "story-mirewatch-intercept-reset-live" ||
                 screen == "story-mirewatch-intercept-exclusions-live")
        {
            if (replayVaultCostForCapture())
            {
                if (screen == "story-mirewatch-intercept-footprint-live")
                {
                    storyPopupPage = 1;
                }
                else if (screen == "story-mirewatch-intercept-reset-live")
                {
                    storyPopupPage = 2;
                }
                else if (screen == "story-mirewatch-intercept-exclusions-live")
                {
                    storyPopupPage = 3;
                }
                if (storyPopupPage >= storyPopupPanels.size())
                {
                    failCaptureValidation(
                        "Capture replay error: requested Intercept explanation page is absent.");
                }
            }
        }
        else if (screen.rfind("story-mirewatch-game-", 0) == 0)
        {
            storyCampaign = StoryCampaign::Mirewatch;
            try
            {
                storyMissionIndex = storyTacticalMissionIndex(
                    storyCampaign,
                    std::stoi(screen.substr(std::string("story-mirewatch-game-").size())) - 1);
            }
            catch (const std::exception&)
            {
                failCaptureValidation(
                    "Capture setup error: invalid Mirewatch tactical screen key '" +
                    screen + "'.");
                return;
            }
            beginStory();
            if (screen.size() >= 6 &&
                screen.compare(screen.size() - 6, 6, "-board") == 0)
            {
                storyPopupPanels.clear();
                storyPopupPage = 0;
                storyCompleteAfterPopup = false;
            }
        }
        else if (screen.rfind("story-seelie-review-", 0) == 0)
        {
            storyCampaign = StoryCampaign::Seelie;
            const bool survivorFailure =
                screen == "story-seelie-review-survivor-failure" ||
                screen == "story-seelie-review-defeat-no-mastery";
            storyMissionIndex = storyMissionIndexById(
                storyCampaign,
                survivorFailure
                    ? "se27_emperor_at_tree"
                    : "se01_cathedral_last_lights");
            beginStory();
            storyPopupPanels.clear();
            storyPopupPage = 0;
            storyCompleteAfterPopup = false;
            if (screen == "story-seelie-review-guided-correction")
            {
                // Pavo's Quickstep from C3 to E2 is a legal printed action, but
                // the first guided step requires the Messenger. Route the
                // attempt through ordinary action resolution and let the Story
                // gate provide its real correction.
                const int pavoId = storyPieceIdForRole("pavo");
                const int stepBefore = storyMissionStep;
                requestPieceAction(pavoId, 1, 4);
                const game_data::Piece* pavo = gamePieceById(pavoId);
                if (pavo == nullptr || pavo->row != 2 || pavo->column != 2 ||
                    storyMissionStep != stepBefore ||
                    (storyCorrection.empty() && gameSnapshot.status.empty()))
                {
                    failCaptureValidation(
                        "Capture replay error: the genuine wrong-actor correction was not reproduced.");
                }
            }
            else if (screen == "story-seelie-review-illegal-drop")
            {
                // C3 is occupied by Pavo, so this genuine Messenger drop is
                // rejected by normal board legality before Story gating.
                const int messengerId = storyPieceIdForRole("messenger");
                const int stepBefore = storyMissionStep;
                requestPieceAction(messengerId, 2, 2);
                const game_data::Piece* messenger = gamePieceById(messengerId);
                if (messenger == nullptr || messenger->row != 2 || messenger->column != 1 ||
                    storyMissionStep != stepBefore ||
                    (storyCorrection.empty() && gameSnapshot.status.empty()))
                {
                    failCaptureValidation(
                        "Capture replay error: the genuine occupied-square rejection was not reproduced.");
                }
            }
            else if (screen == "story-seelie-review-already-acted")
            {
                const int messengerId = storyPieceIdForRole("messenger");
                const int pavoId = storyPieceIdForRole("pavo");
                requestPieceAction(messengerId, 2, 3);
                sendEndTurn();
                storyScriptActionAt = std::numeric_limits<float>::lowest();
                updateStoryAi();
                requestPieceAction(messengerId, 2, 4);
                sendEndTurn();
                storyScriptActionAt = std::numeric_limits<float>::lowest();
                updateStoryAi();
                requestPieceAction(pavoId, 3, 4);

                const game_data::Piece* pavo = gamePieceById(pavoId);
                if (pavo != nullptr)
                {
                    // Starting another drag on the same unit now exercises the
                    // normal hasActed feedback path; no status is fabricated.
                    beginPotentialGameDrag(boardFootprintCenter(
                        pavo->row,
                        pavo->column,
                        pavo->width,
                        pavo->height,
                        gameSnapshot.yourPlayer));
                }
                const std::string actedFeedback =
                    storyCorrection + " " + gameSnapshot.status;
                if (pavo == nullptr || !pavo->hasActed ||
                    actedFeedback.find("already acted") == std::string::npos)
                {
                    failCaptureValidation(
                        "Capture replay error: the genuine already-acted feedback was not reproduced.");
                }
            }
            else if (survivorFailure)
            {
                const int knightId = storyPieceIdForRole("knight_north");
                const int sylvaraId = storyPieceIdForRole("sylvara");
                const int bristleId = storyPieceIdForRole("bristle_north");
                bool replayAccepted = knightId != 0 && sylvaraId != 0 &&
                    bristleId != 0;
                const auto opponentAction = [&](bool accepted) {
                    replayAccepted = replayAccepted && accepted;
                    if (accepted)
                    {
                        syncStoryEngine();
                    }
                };

                // Every transition below is a legal open-mission action. The
                // north Knight vacates Bodyguard range, Sylvara advances, and
                // the Bristlejack spends three separate turns reaching her.
                sendMovePiece(knightId, 2, 2);          // B3 -> C3
                sendEndTurn();
                opponentAction(storyEngine->endTurn(2));
                sendMovePiece(sylvaraId, 0, 3);         // A4 -> D1
                sendEndTurn();
                opponentAction(storyEngine->movePiece(2, bristleId, 0, 5));
                opponentAction(storyEngine->endTurn(2));
                sendEndTurn();
                opponentAction(storyEngine->movePiece(2, bristleId, 0, 4));
                opponentAction(storyEngine->endTurn(2));
                sendEndTurn();
                opponentAction(storyEngine->attackPiece(2, bristleId, 0, 3));

                if (!replayAccepted || storyStage != StoryStage::Failed)
                {
                    failCaptureValidation(
                        "Capture replay error: the legal survivor-loss sequence did not finish.");
                }
                else if (gamePieceById(sylvaraId) != nullptr || gameSnapshot.winner != 2)
                {
                    failCaptureValidation(
                        "Capture replay error: survivor failure was not caused by Sylvara's real defeat.");
                }
                else if (!canContinueStoryWithoutMastery())
                {
                    failCaptureValidation(
                        "Capture replay error: a genuine required open-mission defeat did not expose no-mastery continuation.");
                }
            }
        }
        else if (screen == "story-seelie-reading-draw" ||
                 screen == "story-seelie-reading-foresight" ||
                 screen == "story-seelie-reading-foresight-correction" ||
                 screen == "story-seelie-reading-discard")
        {
            storyCampaign = StoryCampaign::Seelie;
            storyMissionIndex = storyMissionIndexById(
                storyCampaign, "se04_reading_room");
            beginStory();
            storyPopupPanels.clear();
            storyPopupPage = 0;
            storyCompleteAfterPopup = false;
            const StoryMission& mission = activeStoryMission();
            if (!storyEngine || storyStage != StoryStage::Objective ||
                storyEngine->phase() != game_data::Phase::Playing ||
                storyEngine->currentPlayer() != 1 || storyMissionStep != 0 ||
                mission.script.size() < 4 ||
                mission.script[0].kind != StoryActionKind::PlayCard ||
                mission.script[0].cardTitle != "Archivist Mosswake" ||
                mission.script[0].targetRow != 3 ||
                mission.script[0].targetColumn != 1 ||
                mission.script[1].kind != StoryActionKind::DrawCard ||
                mission.script[2].kind != StoryActionKind::ChooseForesight ||
                mission.script[2].cardTitle != "Fey Messenger" ||
                mission.script[3].kind != StoryActionKind::DiscardCard ||
                mission.script[3].cardTitle != "Fey Messenger")
            {
                failCaptureValidation(
                    "Capture replay error: The Reading Room did not reach its exact deploy/draw/Foresight script.");
                return;
            }

            const GameEngine::EnginePlayer& initialPlayer = storyEngine->playerState(1);
            if (initialPlayer.resources != mission.playerResources ||
                initialPlayer.hand.size() != 1 ||
                initialPlayer.hand.front().title != "Archivist Mosswake" ||
                initialPlayer.hand.front().cost != 35 ||
                initialPlayer.drawPile.size() != 2 ||
                initialPlayer.drawPile.front().title != "Starbloom Knight" ||
                initialPlayer.drawPile.back().title != "Fey Messenger" ||
                initialPlayer.pieceActionUsedThisTurn ||
                storyEngine->boardControl()[static_cast<std::size_t>(
                    game_data::squareIndex(3, 1))] != 1)
            {
                failCaptureValidation(
                    "Capture replay error: the Reading Room hand, pile order, Resources, or controlled deploy square drifted.");
                return;
            }

            sendPlayCard(0, 3, 1);
            const int mosswakeId = storyPieceIdForRole("mosswake");
            const game_data::Piece* mosswake = gamePieceById(mosswakeId);
            const GameEngine::EnginePlayer& deployedPlayer = storyEngine->playerState(1);
            if (storyMissionStep != 1 || mosswakeId == 0 || mosswake == nullptr ||
                mosswake->owner != 1 || mosswake->name != "Archivist Mosswake" ||
                mosswake->row != 3 || mosswake->column != 1 ||
                !mosswake->hasActed ||
                !game_data::hasKeyword(mosswake->keywords, "foresight") ||
                deployedPlayer.resources != 65 || !deployedPlayer.hand.empty() ||
                deployedPlayer.drawPile.size() != 2 ||
                !deployedPlayer.foresightChoices.empty() ||
                deployedPlayer.pieceActionUsedThisTurn)
            {
                failCaptureValidation(
                    "Capture replay error: Mosswake did not deploy for 35 Resources, arrive exhausted, and preserve the normal piece action.");
                return;
            }

            // Prove the rest of the exact rules cycle on a copy even when the
            // requested screenshot intentionally remains on the pre-draw frame.
            GameEngine foresightWitness = *storyEngine;
            const bool witnessDrew = foresightWitness.drawCard(1);
            const GameEngine::EnginePlayer& witnessAfterDraw =
                foresightWitness.playerState(1);
            const bool witnessChoices = witnessDrew &&
                witnessAfterDraw.resources == 15 &&
                witnessAfterDraw.hand.empty() &&
                witnessAfterDraw.drawPile.empty() &&
                witnessAfterDraw.foresightChoices.size() == 2 &&
                witnessAfterDraw.foresightChoices[0].title == "Fey Messenger" &&
                witnessAfterDraw.foresightChoices[1].title == "Starbloom Knight";
            const bool witnessChose = witnessChoices &&
                foresightWitness.chooseForesightCard(1, 0);
            const GameEngine::EnginePlayer& witnessAfterChoice =
                foresightWitness.playerState(1);
            const bool witnessChoiceOutcome = witnessChose &&
                witnessAfterChoice.resources == 15 &&
                witnessAfterChoice.foresightChoices.empty() &&
                witnessAfterChoice.hand.size() == 1 &&
                witnessAfterChoice.hand.front().title == "Fey Messenger" &&
                witnessAfterChoice.drawPile.size() == 1 &&
                witnessAfterChoice.drawPile.front().title == "Starbloom Knight";
            const bool witnessDiscarded = witnessChoiceOutcome &&
                foresightWitness.discardCard(1, 0);
            const GameEngine::EnginePlayer& witnessAfterDiscard =
                foresightWitness.playerState(1);
            if (!witnessDiscarded || !witnessAfterDiscard.hand.empty() ||
                witnessAfterDiscard.resources != 15 ||
                witnessAfterDiscard.discardsThisTurn != 1 ||
                witnessAfterDiscard.pieceActionUsedThisTurn ||
                witnessAfterDiscard.drawPile.size() != 2 ||
                witnessAfterDiscard.drawPile.front().title != "Fey Messenger" ||
                witnessAfterDiscard.drawPile.back().title != "Starbloom Knight")
            {
                failCaptureValidation(
                    "Capture witness error: paid Foresight did not reveal two, keep the chosen card, and return both rejected/discarded cards to the exact bottom order.");
                return;
            }

            if (screen != "story-seelie-reading-draw")
            {
                const int resourcesBeforeDraw = storyEngine->playerState(1).resources;
                sendDrawCard();
                const GameEngine::EnginePlayer& afterDraw = storyEngine->playerState(1);
                if (storyMissionStep != 2 ||
                    afterDraw.resources !=
                        resourcesBeforeDraw - game_data::DrawCardResourceCost ||
                    !afterDraw.hand.empty() || !afterDraw.drawPile.empty() ||
                    afterDraw.foresightChoices.size() != 2 ||
                    afterDraw.foresightChoices[0].title != "Fey Messenger" ||
                    afterDraw.foresightChoices[1].title != "Starbloom Knight" ||
                    !storyEngine->hasPendingForesightChoice(1))
                {
                    failCaptureValidation(
                        "Capture replay error: the paid draw did not reach the exact two-card Foresight choice.");
                    return;
                }
            }

            if (screen == "story-seelie-reading-foresight-correction")
            {
                const auto knight = std::find_if(
                    gameSnapshot.foresightChoices.begin(),
                    gameSnapshot.foresightChoices.end(),
                    [](const game_data::GameCard& card) {
                        return card.title == "Starbloom Knight";
                    });
                if (knight == gameSnapshot.foresightChoices.end())
                {
                    failCaptureValidation(
                        "Capture replay error: Starbloom Knight is absent from the genuine Foresight choices.");
                    return;
                }
                sendChooseForesightCard(static_cast<int>(
                    std::distance(gameSnapshot.foresightChoices.begin(), knight)));
                const GameEngine::EnginePlayer& afterWrongChoice =
                    storyEngine->playerState(1);
                if (storyMissionStep != 2 ||
                    afterWrongChoice.foresightChoices.size() != 2 ||
                    afterWrongChoice.hand.size() != 0 ||
                    !storyEngine->hasPendingForesightChoice(1) ||
                    storyCorrection.find("Fey Messenger is required") ==
                        std::string::npos)
                {
                    failCaptureValidation(
                        "Capture replay error: the rejected Starbloom Knight choice did not preserve the modal with its Fey Messenger correction.");
                    return;
                }
            }
            else if (screen == "story-seelie-reading-discard")
            {
                const auto messenger = std::find_if(
                    gameSnapshot.foresightChoices.begin(),
                    gameSnapshot.foresightChoices.end(),
                    [](const game_data::GameCard& card) {
                        return card.title == "Fey Messenger";
                    });
                if (messenger == gameSnapshot.foresightChoices.end())
                {
                    failCaptureValidation(
                        "Capture replay error: Fey Messenger is absent from the genuine Foresight choices.");
                    return;
                }
                sendChooseForesightCard(static_cast<int>(
                    std::distance(gameSnapshot.foresightChoices.begin(), messenger)));
                const GameEngine::EnginePlayer& afterChoice = storyEngine->playerState(1);
                if (storyMissionStep != 3 ||
                    afterChoice.resources != 15 ||
                    !afterChoice.foresightChoices.empty() ||
                    storyEngine->hasPendingForesightChoice(1) ||
                    afterChoice.hand.size() != 1 ||
                    afterChoice.hand.front().title != "Fey Messenger" ||
                    afterChoice.drawPile.size() != 1 ||
                    afterChoice.drawPile.front().title != "Starbloom Knight" ||
                    afterChoice.pieceActionUsedThisTurn)
                {
                    failCaptureValidation(
                        "Capture replay error: the Foresight choice did not keep Messenger and return Knight to the bottom without another cost or action.");
                    return;
                }
            }
        }
        else if (screen.rfind("story-seelie-game-", 0) == 0)
        {
            storyCampaign = StoryCampaign::Seelie;
            try
            {
                storyMissionIndex = storyTacticalMissionIndex(
                    storyCampaign,
                    std::stoi(screen.substr(std::string("story-seelie-game-").size())) - 1);
            }
            catch (const std::exception&)
            {
                failCaptureValidation(
                    "Capture setup error: invalid Seelie tactical screen key '" +
                    screen + "'.");
                return;
            }
            beginStory();
            if (screen.size() >= 6 &&
                screen.compare(screen.size() - 6, 6, "-board") == 0)
            {
                storyPopupPanels.clear();
                storyPopupPage = 0;
                storyCompleteAfterPopup = false;
            }
        }
        else if (screen == "game")
        {
            beginStory();
        }
        else if (screen == "game-bases")
        {
            seedCaptureMatch("bases");
        }
        else if (screen == "game-bases-blue-large")
        {
            seedCaptureMatch("bases-blue-large");
        }
        else if (screen == "game-midgame" || screen == "game-hand-hover")
        {
            seedCaptureMatch("midgame");
            if (screen == "game-midgame")
            {
                if (!gameSnapshot.hand.empty())
                {
                    gameSnapshot.hand.pop_back();
                    gameSnapshot.players[0].handCount =
                        static_cast<int>(gameSnapshot.hand.size());
                }
                captureHoverPoint = sf::Vector2f{
                    GameDeckPileX + GamePileWidth * 0.5f,
                    GamePileY + (GamePileHeight - 14.0f) * 0.5f};
            }
            else
            {
                captureHoverPoint = sf::Vector2f{
                    gameHandCardX(3, VisibleGameHandCards) + 24.0f,
                    HandY + 46.0f};
            }
        }
        else if (screen == "game-selected")
        {
            seedCaptureMatch("selected");
        }
        else if (screen == "game-popup" || screen == "game-popup-tooltip")
        {
            seedCaptureMatch("popup");
            if (screen == "game-popup-tooltip")
            {
                // Advance's movement-pattern symbol in the first action row.
                captureHoverPoint = sf::Vector2f{244.0f, 312.0f};
            }
        }
        else if (screen == "game-action-choice")
        {
            seedCaptureMatch("action-choice");
            const auto briar = std::find_if(
                gameSnapshot.pieces.begin(),
                gameSnapshot.pieces.end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Briar Whisperthorn";
                });
            const auto target = std::find_if(
                gameSnapshot.pieces.begin(),
                gameSnapshot.pieces.end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Bristlejack";
                });
            if (briar != gameSnapshot.pieces.end() &&
                target != gameSnapshot.pieces.end())
            {
                requestPieceAction(briar->id, target->row, target->column);
            }
            if (!pendingPieceActionChoice ||
                pendingPieceActionChoice->actionIndices.size() != 2)
            {
                failCaptureValidation(
                    "Capture setup error: both legal Briar profiles were not offered.");
            }
        }
        else if (screen == "game-undefined-spell-popup" ||
                 screen == "game-undefined-spell-rejected")
        {
            seedCaptureMatch("midgame");
            const auto packagedFixture = std::find_if(
                allCardLibrary.begin(), allCardLibrary.end(),
                [](const card_data::Card& card) {
                    return card.title == "Hidden Camp";
                });
            if (packagedFixture == allCardLibrary.end())
            {
                failCaptureValidation(
                    "Capture setup error: packaged Hidden Camp fixture is missing.");
                return;
            }
            game_data::GameCard hiddenCamp = game_data::toGameCard(*packagedFixture);
            if (hiddenCamp.title != "Hidden Camp" || hiddenCamp.type != "Spell" ||
                hiddenCamp.cost != 10 ||
                hiddenCamp.imagePath != "cards/hiddenCamp.png" ||
                std::find(hiddenCamp.traits.begin(), hiddenCamp.traits.end(), "Wild") ==
                    hiddenCamp.traits.end() ||
                game_data::isSupportedSpellEffect(hiddenCamp))
            {
                failCaptureValidation(
                    "Capture setup error: packaged Hidden Camp no longer represents the intended unsupported Spell fixture.");
                return;
            }
            gameSnapshot.hand.insert(gameSnapshot.hand.begin(), std::move(hiddenCamp));
            gameSnapshot.players[0].handCount =
                static_cast<int>(gameSnapshot.hand.size());
            if (screen == "game-undefined-spell-popup")
            {
                inspectedHandIndex = 0;
                inspectedPieceScroll = 0.0f;
                if (!inspectedHandIndex || *inspectedHandIndex != 0 ||
                    gameSnapshot.hand.empty() ||
                    gameSnapshot.hand.front().title != "Hidden Camp" ||
                    gameSnapshot.hand.front().type != "Spell" ||
                    game_data::isSupportedSpellEffect(gameSnapshot.hand.front()))
                {
                    failCaptureValidation(
                        "Capture setup error: packaged undefined Spell popup state was not reached.");
                    return;
                }
            }
            else
            {
                const std::size_t beforeHandSize = gameSnapshot.hand.size();
                const int beforeResources = gameSnapshot.players[0].resources;
                const std::size_t beforePieceCount = gameSnapshot.pieces.size();
                const int beforeActivePlayer = gameSnapshot.activePlayer;
                const std::uint8_t beforePhase = gameSnapshot.phase;
                const int beforeDrawPile = gameSnapshot.players[0].drawPileCount;
                const bool handled = handleHandCardClick(0);
                if (!handled || gameSnapshot.hand.size() != beforeHandSize ||
                    gameSnapshot.players[0].resources != beforeResources ||
                    gameSnapshot.pieces.size() != beforePieceCount ||
                    gameSnapshot.activePlayer != beforeActivePlayer ||
                    gameSnapshot.phase != beforePhase ||
                    gameSnapshot.players[0].drawPileCount != beforeDrawPile ||
                    gameSnapshot.hand.empty() ||
                    gameSnapshot.hand.front().title != "Hidden Camp" ||
                    selectedHandIndex || selectedPieceId ||
                    gameSnapshot.status !=
                        "This spell has no defined game effect and cannot be played.")
                {
                    failCaptureValidation(
                        "Capture setup error: an undefined Spell did not fail closed.");
                    return;
                }
            }
        }
        else if (screen == "game-resign-confirmation")
        {
            seedCaptureMatch("midgame");
            resignConfirmPopupVisible = true;
        }
        else if (screen == "game-victory")
        {
            seedCaptureMatch("victory");
        }
        else
        {
            failCaptureValidation(
                "Capture setup error: unknown or unhandled UI capture screen key '" +
                screen + "'.");
            return;
        }

        if (!captureValidationFailed && captureRequest && storyMode && storyEngine &&
            currentState == GameState::Game)
        {
            // Capture Story gameplay resolves through packaged GameCards, not
            // the generic account-screen sample catalogue.  Verify every
            // dependency and every currently renderable object against that
            // exact source so a same-title sample row can never mask drift in
            // type, card art, token art, or animation identity. GameCard does
            // not carry rarity; rarityGemArtworkFor therefore renders no gem
            // for this fixture instead of inventing one from sample metadata.
            const auto renderMetadataMatches = [](
                const game_data::GameCard& actual,
                const game_data::GameCard& expected) {
                return actual.title == expected.title &&
                    actual.type == expected.type &&
                    actual.imagePath == expected.imagePath &&
                    actual.tokenPath == expected.tokenPath &&
                    actual.state1TokenPath == expected.state1TokenPath &&
                    actual.pieceBaseBluePath == expected.pieceBaseBluePath &&
                    actual.pieceBaseRedPath == expected.pieceBaseRedPath &&
                    actual.walkAnimPath == expected.walkAnimPath &&
                    actual.idleAnimPath == expected.idleAnimPath &&
                    actual.attackAnimPath == expected.attackAnimPath &&
                    actual.damagedAnimPath == expected.damagedAnimPath &&
                    actual.killedAnimPath == expected.killedAnimPath &&
                    actual.fidgetAnimPath == expected.fidgetAnimPath &&
                    actual.walkAnimFrames == expected.walkAnimFrames &&
                    actual.idleAnimFrames == expected.idleAnimFrames &&
                    actual.attackAnimFrames == expected.attackAnimFrames &&
                    actual.damagedAnimFrames == expected.damagedAnimFrames &&
                    actual.killedAnimFrames == expected.killedAnimFrames &&
                    actual.fidgetAnimFrames == expected.fidgetAnimFrames &&
                    actual.width == expected.width &&
                    actual.height == expected.height;
            };
            std::string metadataFailure;
            const auto recordFailure = [&](std::string_view kind, std::string_view title) {
                if (metadataFailure.empty())
                {
                    metadataFailure = std::string(kind) + " '" +
                        std::string(title) + "'";
                }
            };

            const StoryCardDependencyReport dependencies =
                validateStoryCardDependencies(
                    activeStoryMission(),
                    {},
                    {},
                    StoryCardResolutionMode::AllowPackagedFixtureFallback);
            if (!dependencies.complete())
            {
                recordFailure("dependency", dependencies.missingTitles.front());
            }
            for (const std::string& title : dependencies.requiredTitles)
            {
                const std::optional<game_data::GameCard> expected =
                    packagedStoryCard(title);
                const std::optional<game_data::GameCard> resolved =
                    resolvedStoryCardNamed(title);
                if (!expected || !resolved ||
                    !renderMetadataMatches(*resolved, *expected))
                {
                    recordFailure("dependency", title);
                }
            }

            const auto validateCards = [&](const auto& cards, std::string_view kind) {
                for (const game_data::GameCard& card : cards)
                {
                    const std::optional<game_data::GameCard> expected =
                        packagedStoryCard(card.title);
                    if (!expected || !renderMetadataMatches(card, *expected))
                    {
                        recordFailure(kind, card.title);
                    }
                }
            };
            for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
            {
                const GameEngine::EnginePlayer& player =
                    storyEngine->playerState(playerNumber);
                validateCards(player.heroesToPlace, "Hero placement card");
                validateCards(player.hand, "hand card");
                validateCards(player.drawPile, "draw-pile card");
                validateCards(player.foresightChoices, "Foresight card");
            }
            validateCards(gameSnapshot.hand, "visible hand card");
            validateCards(gameSnapshot.foresightChoices, "visible Foresight card");

            const auto validatePiece = [&](const game_data::Piece& piece,
                                           std::string_view kind) {
                const std::optional<game_data::GameCard> expected =
                    packagedStoryCard(piece.name);
                const bool typeMatches = expected &&
                    ((expected->type == "Hero" && piece.isHero) ||
                     (expected->type == "Unit" && !piece.isHero));
                if (!expected || !typeMatches ||
                    piece.imagePath != expected->imagePath ||
                    piece.tokenPath != expected->tokenPath ||
                    piece.state1TokenPath != expected->state1TokenPath ||
                    piece.pieceBaseBluePath != expected->pieceBaseBluePath ||
                    piece.pieceBaseRedPath != expected->pieceBaseRedPath ||
                    piece.walkAnimPath != expected->walkAnimPath ||
                    piece.idleAnimPath != expected->idleAnimPath ||
                    piece.attackAnimPath != expected->attackAnimPath ||
                    piece.damagedAnimPath != expected->damagedAnimPath ||
                    piece.killedAnimPath != expected->killedAnimPath ||
                    piece.fidgetAnimPath != expected->fidgetAnimPath ||
                    piece.walkAnimFrames != expected->walkAnimFrames ||
                    piece.idleAnimFrames != expected->idleAnimFrames ||
                    piece.attackAnimFrames != expected->attackAnimFrames ||
                    piece.damagedAnimFrames != expected->damagedAnimFrames ||
                    piece.killedAnimFrames != expected->killedAnimFrames ||
                    piece.fidgetAnimFrames != expected->fidgetAnimFrames ||
                    piece.width != expected->width ||
                    piece.height != expected->height)
                {
                    recordFailure(kind, piece.name);
                }
            };
            for (const game_data::Piece& piece : gameSnapshot.pieces)
            {
                validatePiece(piece, "board piece");
            }
            for (const PieceKilledAnimation& animation : pieceKilledAnimations)
            {
                validatePiece(animation.piece, "killed-piece animation");
            }
            for (const DematerializeGhost& ghost : dematerializeGhosts)
            {
                validatePiece(ghost.piece, "dematerialized-piece animation");
            }
            for (const game_data::Enchantment& enchantment : gameSnapshot.enchantments)
            {
                const std::optional<game_data::GameCard> expected =
                    packagedStoryCard(enchantment.title);
                if (!expected || expected->type != "Enchantment" ||
                    enchantment.imagePath != expected->imagePath)
                {
                    recordFailure("enchantment", enchantment.title);
                }
            }

            if (!metadataFailure.empty())
            {
                failCaptureValidation(
                    "Capture render error: exact packaged Story metadata did not resolve for " +
                    metadataFailure + ".");
                return;
            }
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

    auto authenticatedMenuButtonCount = [&]() {
        return loggedInIsAdmin ? 7 : 6;
    };

    auto syncAuthenticatedMenuFocus = [&]() {
        storyButton.setFocused(authenticatedMenuFocus == 0);
        playButton.setFocused(authenticatedMenuFocus == 1);
        conquestButton.setFocused(authenticatedMenuFocus == 2);
        deckEditorButton.setFocused(authenticatedMenuFocus == 3);
        shopButton.setFocused(authenticatedMenuFocus == 4);
        adminUsersButton.setFocused(loggedInIsAdmin && authenticatedMenuFocus == 5);
        logoutButton.setFocused(authenticatedMenuFocus == (loggedInIsAdmin ? 6 : 5));
    };

    auto activateAuthenticatedMenuButton = [&](int index) {
        if (currentState != GameState::Authenticated || exitDesktopPopupVisible)
        {
            return;
        }

        playButtonClickSound();
        if (index == 0)
        {
            showStorySelect();
        }
        else if (index == 1)
        {
            showDeckSelect();
        }
        else if (index == 2)
        {
            ++conquestScreenGeneration;
            conquestScreen.open(activeAccessToken, loggedInUsername, loggedInIsAdmin);
            currentState = GameState::Conquest;
            title.setString("");
            clearFocus();
        }
        else if (index == 3)
        {
            loadDeckEditor();
        }
        else if (index == 4)
        {
            loadShop();
        }
        else if (loggedInIsAdmin && index == 5)
        {
            adminUsersPage = 0;
            loadAdminUsersScreen();
        }
        else if (index == (loggedInIsAdmin ? 6 : 5))
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
    };

    auto activateStorySelectKeyboardFocus = [&]() {
        playButtonClickSound();
        if (storySelectKeyboardFocus == 3)
        {
            showAuthenticatedScreen();
        }
        else if (!storyCardsReady())
        {
            requestStoryCards();
        }
        else if (storySelectKeyboardFocus == 0)
        {
            requestStoryCampaignSelection(StoryCampaign::Blackthorn);
        }
        else if (storySelectKeyboardFocus == 1)
        {
            requestStoryCampaignSelection(StoryCampaign::Mirewatch);
        }
        else if (storySelectKeyboardFocus == 2)
        {
            requestStoryCampaignSelection(StoryCampaign::Seelie);
        }
    };

    const auto storyMissionKeyboardTargets = [&]() {
        std::vector<int> targets;
        const int missionCount = static_cast<int>(storyMissions(storyCampaign).size());
        const int completed = storyCampaignProgress[storyProgressIndex(storyCampaign)];
        const int unlockedCount =
            std::min(missionCount, completed + (completed < missionCount ? 1 : 0));
        for (int slot = 0; slot < StoryMissionPageSize; ++slot)
        {
            const int missionIndex = storyMissionPage * StoryMissionPageSize + slot;
            if (missionIndex < missionCount && missionIndex < unlockedCount)
            {
                targets.push_back(slot);
            }
        }
        targets.push_back(8); // Back
        if (storyMissionPage > 0)
        {
            targets.push_back(9); // Previous page
        }
        const int pageCount =
            std::max(1, (missionCount + StoryMissionPageSize - 1) / StoryMissionPageSize);
        if (storyMissionPage + 1 < pageCount)
        {
            targets.push_back(10); // Next page
        }
        if (missionCount > 0)
        {
            targets.push_back(11); // Continue/replay
        }
        return targets;
    };

    auto moveStoryMissionKeyboardFocus = [&](int delta) {
        const std::vector<int> targets = storyMissionKeyboardTargets();
        if (targets.empty())
        {
            storyMissionKeyboardFocus = -1;
            return;
        }
        const auto current = std::find(
            targets.begin(), targets.end(), storyMissionKeyboardFocus);
        const int currentIndex = current == targets.end()
            ? -1
            : static_cast<int>(std::distance(targets.begin(), current));
        const int nextIndex = wrapStoryKeyboardIndex(
            currentIndex, delta, static_cast<int>(targets.size()));
        storyMissionKeyboardFocus = targets[static_cast<std::size_t>(nextIndex)];
    };

    const auto focusFirstStoryMissionOnPage = [&]() {
        const std::vector<int> targets = storyMissionKeyboardTargets();
        const auto missionTarget = std::find_if(
            targets.begin(), targets.end(), [](int value) {
                return value >= 0 && value < StoryMissionPageSize;
            });
        storyMissionKeyboardFocus = missionTarget == targets.end() ? 8 : *missionTarget;
    };

    auto activateStoryMissionKeyboardFocus = [&]() {
        const int missionCount = static_cast<int>(storyMissions(storyCampaign).size());
        const int completed = storyCampaignProgress[storyProgressIndex(storyCampaign)];
        const int unlockedCount =
            std::min(missionCount, completed + (completed < missionCount ? 1 : 0));
        if (storyMissionKeyboardFocus >= 0 &&
            storyMissionKeyboardFocus < StoryMissionPageSize)
        {
            const int missionIndex =
                storyMissionPage * StoryMissionPageSize + storyMissionKeyboardFocus;
            if (missionIndex < missionCount && missionIndex < unlockedCount)
            {
                playButtonClickSound();
                showStoryIntro(missionIndex);
            }
            return;
        }
        if (storyMissionKeyboardFocus == 8)
        {
            playButtonClickSound();
            showStorySelect();
        }
        else if (storyMissionKeyboardFocus == 9 && storyMissionPage > 0)
        {
            playButtonClickSound();
            --storyMissionPage;
            focusFirstStoryMissionOnPage();
        }
        else if (storyMissionKeyboardFocus == 10)
        {
            const int lastPage =
                std::max(0, (missionCount - 1) / StoryMissionPageSize);
            if (storyMissionPage < lastPage)
            {
                playButtonClickSound();
                ++storyMissionPage;
                focusFirstStoryMissionOnPage();
            }
        }
        else if (storyMissionKeyboardFocus == 11 && missionCount > 0)
        {
            playButtonClickSound();
            showStoryIntro(std::min(completed, missionCount - 1));
        }
    };

    const auto storyIntroKeyboardTargets = [&]() {
        std::vector<int> targets = {0}; // Previous/missions
        const StoryMission& mission = activeStoryMission();
        if ((mission.optionalRehearsal || activeStoryCatchUpMayBeSkipped()) &&
            storyComicPage + 1 >= static_cast<int>(mission.briefing.size()))
        {
            targets.push_back(1); // Skip an optional drill or replay mastered catch-up
        }
        targets.push_back(2); // Continue/start
        return targets;
    };

    auto moveStoryIntroKeyboardFocus = [&](int delta) {
        const std::vector<int> targets = storyIntroKeyboardTargets();
        const auto current = std::find(
            targets.begin(), targets.end(), storyIntroKeyboardFocus);
        const int currentIndex = current == targets.end()
            ? -1
            : static_cast<int>(std::distance(targets.begin(), current));
        storyIntroKeyboardFocus = targets[static_cast<std::size_t>(
            wrapStoryKeyboardIndex(
                currentIndex, delta, static_cast<int>(targets.size())))];
    };

    auto storyIntroBack = [&]() {
        playButtonClickSound();
        if (storyComicPage > 0)
        {
            --storyComicPage;
            storyIntroKeyboardFocus = 2;
        }
        else
        {
            showStoryMissionSelect(storyCampaign);
        }
    };

    auto storyIntroContinue = [&]() {
        playButtonClickSound();
        const StoryMission& mission = activeStoryMission();
        if (storyComicPage + 1 < static_cast<int>(mission.briefing.size()))
        {
            ++storyComicPage;
            storyIntroKeyboardFocus = 2;
        }
        else if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
        {
            completeStoryChronicle();
        }
        else if (activeStoryCatchUpMayBeSkipped())
        {
            completeStoryChronicle();
        }
        else
        {
            beginStory();
        }
    };

    auto advanceStoryPopup = [&]() {
        playButtonClickSound();
        if (storyPopupPage + 1 < storyPopupPanels.size())
        {
            ++storyPopupPage;
            storyPopupKeyboardFocus = 1;
            return;
        }
        const bool completeAfter = storyCompleteAfterPopup;
        storyPopupPanels.clear();
        storyPopupPage = 0;
        storyCompleteAfterPopup = false;
        storyScriptActionAt = animationTime + 0.35f;
        if (completeAfter)
        {
            completeStoryMission(gameSnapshot);
        }
    };

    auto retreatStoryPopup = [&]() {
        if (storyPopupPage > 0)
        {
            playButtonClickSound();
            --storyPopupPage;
            storyPopupKeyboardFocus = storyPopupPage > 0 ? 0 : 1;
        }
    };

    const auto storyEndTurnAvailable = [&]() {
        if (!storyMode || !haveSnapshot || !storyPopupPanels.empty())
        {
            return false;
        }
        if (storyStage == StoryStage::Complete || storyStage == StoryStage::Failed)
        {
            return true;
        }
        if (static_cast<game_data::Phase>(gameSnapshot.phase) !=
                game_data::Phase::Playing ||
            gameSnapshot.activePlayer != gameSnapshot.yourPlayer)
        {
            return false;
        }
        const StoryMission& mission = activeStoryMission();
        if (mission.script.empty())
        {
            return true;
        }
        if (storyMissionStep < 0 ||
            storyMissionStep >= static_cast<int>(mission.script.size()))
        {
            return false;
        }
        const StoryScriptAction& step =
            mission.script[static_cast<std::size_t>(storyMissionStep)];
        return step.owner == gameSnapshot.yourPlayer &&
            step.kind == StoryActionKind::EndTurn;
    };

    const auto storyAbilityAvailable = [&]() {
        if (!storyMode || !haveSnapshot || !selectedPieceId ||
            static_cast<game_data::Phase>(gameSnapshot.phase) !=
                game_data::Phase::Playing ||
            gameSnapshot.activePlayer != gameSnapshot.yourPlayer)
        {
            return false;
        }
        const game_data::Piece* piece = gamePieceById(*selectedPieceId);
        return piece && pieceCanTakeGameAction(*piece) &&
            game_data::pieceAbilityAvailable(gameSnapshot.pieces, *piece);
    };

    const auto storyGameKeyboardTargets = [&]() {
        std::vector<StoryGameKeyboardFocus> targets = {
            StoryGameKeyboardFocus::Board};
        if (haveSnapshot && !gameSnapshot.hand.empty() &&
            (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                 game_data::Phase::HeroPlacement ||
             (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                  game_data::Phase::Playing &&
              gameSnapshot.activePlayer == gameSnapshot.yourPlayer)))
        {
            targets.push_back(StoryGameKeyboardFocus::Hand);
        }
        if (selectedHandIndex && *selectedHandIndex < gameSnapshot.hand.size())
        {
            const game_data::GameCard& selectedCard = gameSnapshot.hand[*selectedHandIndex];
            if (selectedCard.type == "Enchantment" && selectedCard.target == "player")
            {
                targets.push_back(StoryGameKeyboardFocus::PlayerOne);
                targets.push_back(StoryGameKeyboardFocus::PlayerTwo);
            }
        }
        if (playerCanDrawCard())
        {
            targets.push_back(StoryGameKeyboardFocus::DrawPile);
        }
        if (storyAbilityAvailable())
        {
            targets.push_back(StoryGameKeyboardFocus::Ability);
        }
        if (storyEndTurnAvailable())
        {
            targets.push_back(StoryGameKeyboardFocus::EndTurn);
        }
        targets.push_back(StoryGameKeyboardFocus::Restart);
        targets.push_back(StoryGameKeyboardFocus::Exit);
        return targets;
    };

    auto moveStoryGameKeyboardFocus = [&](int delta) {
        const std::vector<StoryGameKeyboardFocus> targets =
            storyGameKeyboardTargets();
        const auto current = std::find(
            targets.begin(), targets.end(), storyGameKeyboardFocus);
        const int currentIndex = current == targets.end()
            ? -1
            : static_cast<int>(std::distance(targets.begin(), current));
        const int nextIndex = wrapStoryKeyboardIndex(
            currentIndex, delta, static_cast<int>(targets.size()));
        storyGameKeyboardFocus = targets[static_cast<std::size_t>(nextIndex)];
    };

    auto ensureStoryKeyboardHandVisible = [&]() {
        if (gameSnapshot.hand.empty())
        {
            storyKeyboardHandIndex = 0;
            gameHandOffset = 0;
            return;
        }
        storyKeyboardHandIndex = std::min(
            storyKeyboardHandIndex, gameSnapshot.hand.size() - 1);
        if (storyKeyboardHandIndex < gameHandOffset)
        {
            gameHandOffset = storyKeyboardHandIndex;
        }
        else if (storyKeyboardHandIndex >= gameHandOffset + VisibleGameHandCards)
        {
            gameHandOffset = storyKeyboardHandIndex - VisibleGameHandCards + 1;
        }
        clampListOffset(
            gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
    };

    auto cycleStoryKeyboardHand = [&](int delta) {
        if (gameSnapshot.hand.empty())
        {
            return;
        }
        storyKeyboardHandIndex = static_cast<std::size_t>(wrapStoryKeyboardIndex(
            static_cast<int>(storyKeyboardHandIndex),
            delta,
            static_cast<int>(gameSnapshot.hand.size())));
        ensureStoryKeyboardHandVisible();
    };

    auto activateStoryKeyboardHand = [&]() {
        if (!haveSnapshot || gameSnapshot.hand.empty())
        {
            return;
        }
        ensureStoryKeyboardHandVisible();
        const game_data::GameCard focusedCard =
            gameSnapshot.hand[storyKeyboardHandIndex];
        const bool immediateAction = handleHandCardClick(storyKeyboardHandIndex);
        if (!immediateAction && selectedHandIndex)
        {
            if (focusedCard.type == "Enchantment" &&
                focusedCard.target == "player")
            {
                storyGameKeyboardFocus = gameSnapshot.yourPlayer == 1
                    ? StoryGameKeyboardFocus::PlayerOne
                    : StoryGameKeyboardFocus::PlayerTwo;
            }
            else
            {
                if (game_data::inBounds(storyTargetRow, storyTargetColumn))
                {
                    storyBoardKeyboardCursor = {storyTargetRow, storyTargetColumn};
                }
                storyGameKeyboardFocus = StoryGameKeyboardFocus::Board;
            }
        }
    };

    auto activateStoryKeyboardBoardSquare = [&]() {
        if (!haveSnapshot || !game_data::inBounds(
                storyBoardKeyboardCursor.row, storyBoardKeyboardCursor.column))
        {
            return;
        }
        const game_data::Phase phase =
            static_cast<game_data::Phase>(gameSnapshot.phase);
        if (selectedHandIndex && *selectedHandIndex < gameSnapshot.hand.size())
        {
            const int handIndex = static_cast<int>(*selectedHandIndex);
            if (phase == game_data::Phase::HeroPlacement)
            {
                sendPlaceHero(
                    handIndex,
                    storyBoardKeyboardCursor.row,
                    storyBoardKeyboardCursor.column);
            }
            else if (phase == game_data::Phase::Playing &&
                     gameSnapshot.activePlayer == gameSnapshot.yourPlayer)
            {
                const game_data::GameCard& card =
                    gameSnapshot.hand[static_cast<std::size_t>(handIndex)];
                if (card.type == "Enchantment" && card.target == "player")
                {
                    sendPlayCard(handIndex, -1, gameSnapshot.yourPlayer);
                }
                else
                {
                    sendPlayCard(
                        handIndex,
                        storyBoardKeyboardCursor.row,
                        storyBoardKeyboardCursor.column);
                }
            }
            selectedHandIndex.reset();
            selectedPieceId.reset();
            ensureStoryKeyboardHandVisible();
            if (phase == game_data::Phase::HeroPlacement &&
                static_cast<game_data::Phase>(gameSnapshot.phase) ==
                    game_data::Phase::HeroPlacement &&
                !gameSnapshot.hand.empty())
            {
                storyGameKeyboardFocus = StoryGameKeyboardFocus::Hand;
            }
            return;
        }
        if (selectedPieceId && phase == game_data::Phase::Playing &&
            gameSnapshot.activePlayer == gameSnapshot.yourPlayer)
        {
            const game_data::Piece* selectedPiece = gamePieceById(*selectedPieceId);
            if (selectedPiece &&
                selectedPiece->owner == gameSnapshot.yourPlayer &&
                pieceCanTakeGameAction(*selectedPiece))
            {
                requestPieceAction(
                    selectedPiece->id,
                    storyBoardKeyboardCursor.row,
                    storyBoardKeyboardCursor.column);
                selectedPieceId.reset();
                selectedHandIndex.reset();
                return;
            }
            // Enemy and inactive pieces remain available as previews, but can
            // never become keyboard action sources.
            selectedPieceId.reset();
        }

        const game_data::Piece* piece = gamePieceAt(
            storyBoardKeyboardCursor.row, storyBoardKeyboardCursor.column);
        selectedHandIndex.reset();
        if (piece && (gameSnapshot.activePlayer != gameSnapshot.yourPlayer ||
                      pieceCanTakeGameAction(*piece) ||
                      piece->owner != gameSnapshot.yourPlayer))
        {
            selectedPieceId = piece->id;
            const StoryMission& mission = activeStoryMission();
            if (piece->owner == gameSnapshot.yourPlayer &&
                !mission.script.empty() && storyMissionStep >= 0 &&
                storyMissionStep < static_cast<int>(mission.script.size()))
            {
                const StoryScriptAction& step =
                    mission.script[static_cast<std::size_t>(storyMissionStep)];
                if (step.owner == gameSnapshot.yourPlayer &&
                    storyPieceIdForRole(step.actorRole) == piece->id)
                {
                    if ((step.kind == StoryActionKind::Move ||
                         step.kind == StoryActionKind::Attack) &&
                        game_data::inBounds(storyTargetRow, storyTargetColumn))
                    {
                        storyBoardKeyboardCursor = {
                            storyTargetRow, storyTargetColumn};
                    }
                    else if (step.kind == StoryActionKind::UseAbility &&
                             storyAbilityAvailable())
                    {
                        storyGameKeyboardFocus =
                            StoryGameKeyboardFocus::Ability;
                    }
                }
            }
        }
        else
        {
            selectedPieceId.reset();
        }
    };

    auto activateStoryEndTurnKeyboard = [&]() {
        if (storyStage == StoryStage::Complete)
        {
            const bool hasNext = storyMissionIndex + 1 <
                static_cast<int>(storyMissions(storyCampaign).size());
            leaveGame();
            if (hasNext)
            {
                storyComicPage = 0;
                showStoryIntro();
            }
        }
        else if (storyStage == StoryStage::Failed)
        {
            beginStory();
        }
        else if (storyEndTurnAvailable())
        {
            sendEndTurn();
        }
        selectedPieceId.reset();
        selectedHandIndex.reset();
    };

    auto activateStoryGameKeyboardFocus = [&]() {
        switch (storyGameKeyboardFocus)
        {
        case StoryGameKeyboardFocus::Board:
            activateStoryKeyboardBoardSquare();
            break;
        case StoryGameKeyboardFocus::Hand:
            activateStoryKeyboardHand();
            break;
        case StoryGameKeyboardFocus::PlayerOne:
        case StoryGameKeyboardFocus::PlayerTwo:
            if (selectedHandIndex && *selectedHandIndex < gameSnapshot.hand.size())
            {
                const int targetPlayer = storyGameKeyboardFocus ==
                        StoryGameKeyboardFocus::PlayerOne
                    ? 1
                    : 2;
                sendPlayCard(
                    static_cast<int>(*selectedHandIndex), -1, targetPlayer);
                selectedHandIndex.reset();
                selectedPieceId.reset();
            }
            break;
        case StoryGameKeyboardFocus::DrawPile:
            if (playerCanDrawCard())
            {
                sendDrawCard();
            }
            break;
        case StoryGameKeyboardFocus::Ability:
            if (storyAbilityAvailable())
            {
                const int pieceId = *selectedPieceId;
                sendUseAbility(pieceId);
                selectedPieceId.reset();
                selectedHandIndex.reset();
            }
            break;
        case StoryGameKeyboardFocus::EndTurn:
            activateStoryEndTurnKeyboard();
            break;
        case StoryGameKeyboardFocus::Restart:
            if (canContinueStoryWithoutMastery())
            {
                continueStoryWithoutMastery();
            }
            else
            {
                gameConfirmationAction = GameConfirmationAction::RestartStory;
                resignConfirmPopupVisible = true;
            }
            break;
        case StoryGameKeyboardFocus::Exit:
            gameConfirmationAction = GameConfirmationAction::ExitStory;
            resignConfirmPopupVisible = true;
            break;
        }
    };

    auto syncStoryGameKeyboardButtonFocus = [&]() {
        const bool active = storyMode && storyKeyboardNavigationActive;
        storyPopupPreviousButton.setFocused(
            active && !storyPopupPanels.empty() && storyPopupPage > 0 &&
            storyPopupKeyboardFocus == 0);
        storyPopupContinueButton.setFocused(
            active && !storyPopupPanels.empty() &&
            storyPopupKeyboardFocus != 0);
        abilityButton.setFocused(
            active && storyPopupPanels.empty() && storyAbilityAvailable() &&
            storyGameKeyboardFocus == StoryGameKeyboardFocus::Ability);
        endTurnButton.setFocused(
            active && storyPopupPanels.empty() && storyEndTurnAvailable() &&
            storyGameKeyboardFocus == StoryGameKeyboardFocus::EndTurn);
        storyRestartButton.setFocused(
            active && storyPopupPanels.empty() &&
            storyGameKeyboardFocus == StoryGameKeyboardFocus::Restart);
        leaveGameButton.setFocused(
            active && storyPopupPanels.empty() &&
            storyGameKeyboardFocus == StoryGameKeyboardFocus::Exit);
    };

    auto drawStoryKeyboardFocus = [&]() {
        if (!storyMode || !storyKeyboardNavigationActive ||
            pendingPieceActionChoice || resignConfirmPopupVisible ||
            inspectedPieceId || inspectedHandIndex)
        {
            return;
        }

        if (!storyPopupPanels.empty())
        {
            drawCenteredText(
                window,
                font,
                storyPopupPage > 0
                    ? "LEFT/RIGHT: PAGE  |  TAB: BUTTON  |  ENTER: ACTIVATE"
                    : "RIGHT: NEXT  |  ENTER: ACTIVATE",
                12,
                {400.0f, 519.0f},
                sf::Color(190, 198, 214));
            return;
        }

        const auto drawFocusRect = [&](sf::Vector2f position, sf::Vector2f size) {
            sf::RectangleShape outer(size);
            outer.setPosition(position);
            outer.setFillColor(sf::Color::Transparent);
            outer.setOutlineThickness(3.0f);
            outer.setOutlineColor(sf::Color(255, 224, 118, 244));
            window.draw(outer);
            sf::RectangleShape inner(size - sf::Vector2f(6.0f, 6.0f));
            inner.setPosition(position + sf::Vector2f(3.0f, 3.0f));
            inner.setFillColor(sf::Color::Transparent);
            inner.setOutlineThickness(1.0f);
            inner.setOutlineColor(sf::Color(24, 232, 204, 224));
            window.draw(inner);
        };

        std::string keyboardHint =
            "TAB: NEXT CONTROL  |  ENTER: ACTIVATE  |  ESC: CANCEL/EXIT";
        if (storyStage == StoryStage::Failed)
        {
            keyboardHint = canContinueStoryWithoutMastery()
                ? "TAB: RETRY / CONTINUE / EXIT  |  ENTER: CHOOSE"
                : "TAB: RETRY / EXIT  |  ENTER: CHOOSE";
        }
        else if (storyStage == StoryStage::Complete)
        {
            keyboardHint =
                "TAB: CONTINUE / RESTART / EXIT  |  ENTER: CHOOSE";
        }
        else if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                 game_data::Phase::HeroPlacement)
        {
            keyboardHint = selectedHandIndex
                ? "ARROWS: SQUARE  |  ENTER: PLACE  |  TAB: NEXT  |  ESC: CANCEL"
                : "LEFT/RIGHT: HERO  |  ENTER: SELECT  |  TAB: NEXT  |  ESC: BACK";
        }
        else if (!gameSnapshot.foresightChoices.empty())
        {
            if (storyKeyboardForesightIndex < gameSnapshot.foresightChoices.size())
            {
                const std::size_t row =
                    storyKeyboardForesightIndex / ForesightChoiceColumns;
                const std::size_t column =
                    storyKeyboardForesightIndex % ForesightChoiceColumns;
                if (row >= foresightChoiceRowOffset &&
                    row < foresightChoiceRowOffset + ForesightVisibleRows)
                {
                    const std::size_t rowStart = row * ForesightChoiceColumns;
                    const std::size_t rowCount = std::min(
                        ForesightChoiceColumns,
                        gameSnapshot.foresightChoices.size() - rowStart);
                    const float rowWidth = static_cast<float>(rowCount) * HandCardWidth +
                        static_cast<float>(rowCount - 1) * ForesightChoiceGap;
                    const float startX = (ui_canvas::Width - rowWidth) * 0.5f;
                    const float x = startX + static_cast<float>(column) *
                        (HandCardWidth + ForesightChoiceGap);
                    const float y = ForesightChoiceY +
                        static_cast<float>(row - foresightChoiceRowOffset) *
                            ForesightChoiceRowPitch;
                    drawFocusRect(
                        {x - 4.0f, y - 4.0f},
                        {HandCardWidth + 8.0f, HandCardHeight + 38.0f});
                }
            }
            keyboardHint =
                "ARROWS CHOOSE CARD  |  ENTER KEEP CARD";
        }
        else if (storyGameKeyboardFocus == StoryGameKeyboardFocus::Board)
        {
            const BoardCellMetrics metrics = boardCellMetricsForViewer(
                storyBoardKeyboardCursor.row,
                storyBoardKeyboardCursor.column,
                gameSnapshot.yourPlayer);
            drawQuad(
                metrics.corners,
                sf::Color(28, 232, 204, 34),
                3.0f,
                sf::Color(255, 224, 118, 244));
            const std::string cursorSquare =
                std::string(1, static_cast<char>(
                    'A' + storyBoardKeyboardCursor.column)) +
                std::to_string(storyBoardKeyboardCursor.row + 1);
            keyboardHint = selectedPieceId || selectedHandIndex
                ? "TARGET " + cursorSquare +
                    "  |  ARROWS MOVE  |  ENTER ACT  |  ESC CANCEL"
                : "CURSOR " + cursorSquare +
                    "  |  ARROWS MOVE  |  ENTER SELECT  |  TAB NEXT  |  I INFO";
        }
        else if (storyGameKeyboardFocus == StoryGameKeyboardFocus::Hand &&
                 !gameSnapshot.hand.empty())
        {
            ensureStoryKeyboardHandVisible();
            const std::size_t visibleCards = std::min(
                gameSnapshot.hand.size() - gameHandOffset,
                VisibleGameHandCards);
            const std::size_t visibleIndex = storyKeyboardHandIndex - gameHandOffset;
            drawFocusRect(
                {gameHandCardX(visibleIndex, visibleCards) - 4.0f,
                 HandY - HandHoverLift - 4.0f},
                {HandCardWidth + 8.0f,
                 HandCardHeight + HandHoverLift + 8.0f});
            keyboardHint =
                "LEFT/RIGHT CARD  |  ENTER SELECT  |  X DISCARD  |  I INSPECT";
        }
        else if (storyGameKeyboardFocus == StoryGameKeyboardFocus::DrawPile)
        {
            drawFocusRect(
                {GameDeckPileX - 4.0f, GamePileY - 8.0f},
                {GamePileWidth + 8.0f, GamePileHeight + 2.0f});
            keyboardHint = "ENTER / D DRAW CARD  |  TAB NEXT CONTROL";
        }
        else if (storyGameKeyboardFocus == StoryGameKeyboardFocus::PlayerOne ||
                 storyGameKeyboardFocus == StoryGameKeyboardFocus::PlayerTwo)
        {
            const int targetPlayer = storyGameKeyboardFocus ==
                    StoryGameKeyboardFocus::PlayerOne
                ? 1
                : 2;
            drawFocusRect(
                {gamePlayerBannerX(window, targetPlayer) - 4.0f,
                 GameTopBarY - 1.0f},
                {GamePlayerReadoutWidth + 8.0f,
                 gamePlayerBannerHeight(window) + 2.0f});
            keyboardHint =
                "LEFT/RIGHT OR TAB TARGET  |  ENTER PLAY ENCHANTMENT";
        }

        drawBeveledPlate(
            window,
            {110.0f, 576.0f},
            {580.0f, 23.0f},
            sf::Color(8, 14, 15, 242),
            sf::Color(126, 163, 136, 216),
            false,
            4.0f);
        unsigned int keyboardHintSize = 13;
        sf::Text keyboardHintMeasure(font, keyboardHint, keyboardHintSize);
        while (keyboardHintSize > 12u &&
               keyboardHintMeasure.getLocalBounds().size.x > 556.0f)
        {
            keyboardHintMeasure.setCharacterSize(--keyboardHintSize);
        }
        drawCenteredText(
            window,
            font,
            keyboardHint,
            keyboardHintSize,
            {400.0f, 587.5f},
            sf::Color(224, 232, 218));
    };

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
        currentPointer = mousePos;

        if (currentState == GameState::Game)
        {
            pollGameSocket();
            const bool storyClockIsRunning =
                storyMode && storyEngine && storyStage == StoryStage::Objective &&
                activeStoryMission().standardMatch && storyEngine->timersAreEnabled() &&
                (storyEngine->phase() == game_data::Phase::Playing ||
                 storyEngine->phase() == game_data::Phase::HeroPlacement);
            // The briefing is completed before beginStory starts this match.
            // From Hero placement onward, its clocks obey ordinary match rules:
            // inspection and decision overlays never create a Story-only pause.
            storyClockPausedForReading = false;
            if (storyClockIsRunning)
            {
                storyTimerAccumulatorMs += static_cast<double>(deltaTime) * 1000.0;
                const auto elapsedMs = static_cast<std::int64_t>(storyTimerAccumulatorMs);
                if (elapsedMs > 0)
                {
                    storyTimerAccumulatorMs -= static_cast<double>(elapsedMs);
                    if (storyEngine->updateTimers(elapsedMs))
                    {
                        syncStoryEngine();
                    }
                    else
                    {
                        const game_data::Snapshot timedSnapshot = storyEngine->snapshotFor(1);
                        gameSnapshot.turnRemainingMs = timedSnapshot.turnRemainingMs;
                        gameSnapshot.players[0].clockRemainingMs =
                            timedSnapshot.players[0].clockRemainingMs;
                        gameSnapshot.players[1].clockRemainingMs =
                            timedSnapshot.players[1].clockRemainingMs;
                    }
                }
            }
            else
            {
                storyTimerAccumulatorMs = 0.0;
            }
            updateClockWarnings();
            updateStoryAi();
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

        if (pendingAudioSettingsSave &&
            pendingAudioSettingsSave->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AudioSettingsSaveResult result = pendingAudioSettingsSave->get();
            const bool currentAudioSession =
                !activeAccessToken.empty() && activeAccessToken == pendingAudioSettingsSaveToken;
            pendingAudioSettingsSave.reset();
            pendingAudioSettingsSaveToken.clear();

            if (currentAudioSession)
            {
                if (!result.success)
                {
                    setMessage(messageText, result.message, sf::Color::Red);
                }
                if (queuedAudioSettingsSave)
                {
                    startQueuedAudioSettingsSave();
                }
                else if (result.success)
                {
                    audioSettingsDirty = false;
                }
            }
            else if (!activeAccessToken.empty() && queuedAudioSettingsSave)
            {
                startQueuedAudioSettingsSave();
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
                        result.success
                            ? "Spend " + std::to_string(CardPackPrice) + " coins to reveal a random card."
                            : result.message,
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

        if (pendingStoryCardLoad &&
            pendingStoryCardLoad->wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready)
        {
            CardListResult result = pendingStoryCardLoad->get();
            pendingStoryCardLoad.reset();
            const bool usableCatalog = result.success && !result.cards.empty();
            if (usableCatalog && !loggedInUsername.empty())
            {
                allCardLibrary = std::move(result.cards);
            }
            storyBlackthornButton.setEnabled(true);
            storyMirewatchButton.setEnabled(true);
            storySeelieButton.setEnabled(true);
            if (currentState == GameState::StorySelect)
            {
                setMessageY(messageText, 556.0f);
                if (storyCardsReady())
                {
                    setMessage(messageText, "", sf::Color::White);
                }
                else
                {
                    setMessage(
                        messageText,
                        result.success
                            ? "No current game cards were returned. Select a story to retry."
                            : "Card server unavailable. Select a story to retry.",
                        sf::Color::Red);
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
                if (currentState == GameState::StorySelect ||
                    currentState == GameState::StoryMissionSelect ||
                    currentState == GameState::StoryIntro ||
                    (currentState == GameState::Game && storyMode))
                {
                    storyKeyboardNavigationActive = false;
                    storySelectKeyboardFocus = -1;
                    storyMissionKeyboardFocus = -1;
                    storyIntroKeyboardFocus = -1;
                    storyPopupKeyboardFocus = -1;
                }
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

                if (pendingPieceActionChoice)
                {
                    if (const std::optional<int> option =
                            actionChoiceOptionAt(clickPos))
                    {
                        submitPendingPieceActionChoice(*option);
                    }
                    else
                    {
                        const sf::FloatRect cancel = actionChoiceCancelRect();
                        const bool insideDialog = isInsideRect(
                            clickPos,
                            ActionChoiceDialogX,
                            actionChoiceDialogY(),
                            ActionChoiceDialogWidth,
                            actionChoiceDialogHeight());
                        if (cancel.contains(clickPos) || !insideDialog)
                        {
                            pendingPieceActionChoice.reset();
                        }
                    }
                    continue;
                }

                if (resignConfirmPopupVisible)
                {
                    if (confirmResignButton.isClicked(clickPos))
                    {
                        const GameConfirmationAction confirmedAction = gameConfirmationAction;
                        resignConfirmPopupVisible = false;
                        gameConfirmationAction = GameConfirmationAction::Resign;
                        gameConfirmationKeyboardFocus = 0;
                        if (confirmedAction == GameConfirmationAction::RestartStory)
                        {
                            beginStory();
                        }
                        else
                        {
                            leaveGame();
                        }
                    }
                    else if (cancelResignButton.isClicked(clickPos) ||
                             !isInsideRect(clickPos, ResignDialogX, ResignDialogY,
                                          ResignDialogWidth, ResignDialogHeight))
                    {
                        resignConfirmPopupVisible = false;
                        gameConfirmationAction = GameConfirmationAction::Resign;
                        gameConfirmationKeyboardFocus = 0;
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
                else if (currentState == GameState::StorySelect)
                {
                    if (storySpoilerConfirmationVisible)
                    {
                        if (storySpoilerMirewatchButton.isClicked(clickPos))
                        {
                            activateStorySpoilerChoice(false);
                        }
                        else if (storySpoilerContinueButton.isClicked(clickPos))
                        {
                            activateStorySpoilerChoice(true);
                        }
                    }
                    else if (storySelectBackButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (!storyCardsReady())
                    {
                        if (storyBlackthornButton.isClicked(clickPos) ||
                            storyMirewatchButton.isClicked(clickPos) ||
                            storySeelieButton.isClicked(clickPos))
                        {
                            requestStoryCards();
                        }
                    }
                    else if (storyBlackthornButton.isClicked(clickPos))
                    {
                        requestStoryCampaignSelection(StoryCampaign::Blackthorn);
                    }
                    else if (storyMirewatchButton.isClicked(clickPos))
                    {
                        requestStoryCampaignSelection(StoryCampaign::Mirewatch);
                    }
                    else if (storySeelieButton.isClicked(clickPos))
                    {
                        requestStoryCampaignSelection(StoryCampaign::Seelie);
                    }
                }
                else if (currentState == GameState::StoryMissionSelect)
                {
                    if (storyMissionSelectBackButton.isClicked(clickPos))
                    {
                        showStorySelect();
                    }
                    else if (storyRestartCampaignButton.isClicked(clickPos))
                    {
                        const int missionCount =
                            static_cast<int>(storyMissions(storyCampaign).size());
                        const int completed =
                            storyCampaignProgress[storyProgressIndex(storyCampaign)];
                        showStoryIntro(std::min(completed, missionCount - 1));
                    }
                    else if (storyMissionPreviousPageButton.isClicked(clickPos))
                    {
                        storyMissionPage = std::max(0, storyMissionPage - 1);
                    }
                    else if (storyMissionNextPageButton.isClicked(clickPos))
                    {
                        const int missionCount =
                            static_cast<int>(storyMissions(storyCampaign).size());
                        const int lastPage =
                            std::max(0, (missionCount - 1) / StoryMissionPageSize);
                        storyMissionPage = std::min(lastPage, storyMissionPage + 1);
                    }
                    else
                    {
                        const int completed =
                            storyCampaignProgress[storyProgressIndex(storyCampaign)];
                        const int missionCount =
                            static_cast<int>(storyMissions(storyCampaign).size());
                        const int unlockedCount =
                            std::min(missionCount, completed + (completed < missionCount ? 1 : 0));
                        for (int slot = 0; slot < StoryMissionPageSize; ++slot)
                        {
                            const int index = storyMissionPage * StoryMissionPageSize + slot;
                            if (index >= unlockedCount || index >= missionCount)
                            {
                                continue;
                            }
                            if (storyMissionButtons[static_cast<std::size_t>(slot)].isClicked(clickPos))
                            {
                                showStoryIntro(index);
                                break;
                            }
                        }
                    }
                }
                else if (currentState == GameState::StoryIntro)
                {
                    if (storyBackButton.isClicked(clickPos))
                    {
                        if (storyComicPage > 0)
                        {
                            --storyComicPage;
                        }
                        else
                        {
                            showStoryMissionSelect(storyCampaign);
                        }
                    }
                    else if ((activeStoryMission().optionalRehearsal ||
                              activeStoryCatchUpMayBeSkipped()) &&
                        storyComicPage + 1 >= static_cast<int>(
                            activeStoryMission().briefing.size()) &&
                        storySkipDrillButton.isClicked(clickPos))
                    {
                        if (activeStoryCatchUpMayBeSkipped())
                        {
                            beginStory();
                        }
                        else
                        {
                            completeStoryChronicle();
                        }
                    }
                    else if (storyContinueButton.isClicked(clickPos) &&
                        storyComicPage + 1 >= static_cast<int>(
                            storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)]
                                .briefing.size()))
                    {
                        if (activeStoryMission().objectiveSpec.kind ==
                                StoryObjectiveKind::StoryOnly ||
                            activeStoryCatchUpMayBeSkipped())
                        {
                            completeStoryChronicle();
                        }
                        else
                        {
                            beginStory();
                        }
                    }
                    else if (storyContinueButton.isClicked(clickPos))
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
                        queueAudioSettingsSave();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && musicAudioSlider.beginDrag(clickPos))
                    {
                        audioSystem.setMusicVolume(musicAudioSlider.getValue());
                        updateOptionsLabels();
                        queueAudioSettingsSave();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && soundFxAudioSlider.beginDrag(clickPos))
                    {
                        audioSystem.setSoundEffectsVolume(soundFxAudioSlider.getValue());
                        updateOptionsLabels();
                        queueAudioSettingsSave();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && muteAllAudioCheckbox.isClicked(clickPos))
                    {
                        audioSystem.setAllMuted(!audioSystem.isAllMuted());
                        queueAudioSettingsSave();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && muteMusicCheckbox.isClicked(clickPos))
                    {
                        audioSystem.setMusicMuted(!audioSystem.isMusicMuted());
                        queueAudioSettingsSave();
                    }
                    else if (activeOptionsTab == OptionsTab::Audio && muteSoundFxCheckbox.isClicked(clickPos))
                    {
                        audioSystem.setSoundEffectsMuted(!audioSystem.isSoundEffectsMuted());
                        queueAudioSettingsSave();
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
                    authenticatedMenuFocus = -1;
                    syncAuthenticatedMenuFocus();
                    if (authenticatedSettingsButtonClicked(clickPos))
                    {
                        playButtonClickSound();
                        showOptionsScreen(GameState::Authenticated);
                    }
                    else if (storyButton.isClicked(clickPos))
                    {
                        showStorySelect();
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
                        // Resolve the footer before hit-testing it: the dialog
                        // grows and shrinks with the suggestion list.
                        layoutAddCardPopupButtons();
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
                                 clickPos, DeckListX, DeckSelectListY, DeckListWidth, DeckRowHeight,
                                 VisibleDeckRows, deckListOffset, playerDecks.size()))
                    {
                        selectedDeck = *deckIndex;
                    }
                }
                else if (currentState == GameState::Game)
                {
                    if (storyMode && !storyPopupPanels.empty())
                    {
                        if (storyPopupPage > 0 &&
                            storyPopupPreviousButton.isClicked(clickPos))
                        {
                            --storyPopupPage;
                        }
                        else if (storyPopupContinueButton.isClicked(clickPos))
                        {
                            if (storyPopupPage + 1 < storyPopupPanels.size())
                            {
                                ++storyPopupPage;
                            }
                            else
                            {
                                const bool completeAfter = storyCompleteAfterPopup;
                                storyPopupPanels.clear();
                                storyPopupPage = 0;
                                storyCompleteAfterPopup = false;
                                storyScriptActionAt = animationTime + 0.35f;
                                if (completeAfter)
                                {
                                    completeStoryMission(gameSnapshot);
                                }
                            }
                        }
                    }
                    else if (inspectedPieceId || inspectedHandIndex)
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
                        resetGameDrag();
                        const bool matchIsOver = haveSnapshot &&
                            static_cast<game_data::Phase>(gameSnapshot.phase) == game_data::Phase::GameOver;
                        if (storyMode)
                        {
                            gameConfirmationAction = GameConfirmationAction::ExitStory;
                            resignConfirmPopupVisible = true;
                        }
                        else if (!sandboxMode && !conquestBattleMode && !matchIsOver)
                        {
                            gameConfirmationAction = GameConfirmationAction::Resign;
                            resignConfirmPopupVisible = true;
                        }
                        else
                        {
                            leaveGame();
                        }
                    }
                    else if (storyMode && storyRestartButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        resetGameDrag();
                        if (canContinueStoryWithoutMastery())
                        {
                            continueStoryWithoutMastery();
                        }
                        else
                        {
                            gameConfirmationAction = GameConfirmationAction::RestartStory;
                            resignConfirmPopupVisible = true;
                        }
                    }
                    else if (storyMode && endTurnButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        resetGameDrag();
                        if (storyStage == StoryStage::Complete)
                        {
                            const bool hasNext =
                                storyMissionIndex + 1 <
                                    static_cast<int>(storyMissions(storyCampaign).size());
                            leaveGame();
                            if (hasNext)
                            {
                                storyComicPage = 0;
                                showStoryIntro();
                            }
                        }
                        else if (storyStage == StoryStage::Failed)
                        {
                            beginStory();
                        }
                        else
                        {
                            sendEndTurn();
                        }
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
                            !isInsideRect(clickPos, CardPopupX, CardPopupY, CardPopupWidth, CardPopupHeight))
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
                        else if (deckEditorMode == DeckEditorMode::EditDeck && clickCollectionTypeFilter(clickPos))
                        {
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
                        else if (playerCoins < CardPackPrice)
                        {
                            setMessage(
                                messageText,
                                "Need " + std::to_string(CardPackPrice) + " coins to buy a card",
                                sf::Color::Red);
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
                    queueAudioSettingsSave();
                }
                else if (musicAudioSlider.dragTo(dragPos))
                {
                    audioSystem.setMusicVolume(musicAudioSlider.getValue());
                    updateOptionsLabels();
                    queueAudioSettingsSave();
                }
                else if (soundFxAudioSlider.dragTo(dragPos))
                {
                    audioSystem.setSoundEffectsVolume(soundFxAudioSlider.getValue());
                    updateOptionsLabels();
                    queueAudioSettingsSave();
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
                    isInsideRect(wheelPos, CardPopupAbilitiesX, CardPopupAbilitiesY,
                                 CardPopupAbilitiesWidth, CardPopupAbilitiesHeight))
                {
                    const card_data::Card* card = cardByTitle(*inspectedDeckEditorCardTitle);
                    if (!card)
                    {
                        card = cardInAllLibraryByTitle(*inspectedDeckEditorCardTitle);
                    }
                    if (card)
                    {
                        inspectedDeckEditorCardScroll = std::clamp(
                            inspectedDeckEditorCardScroll - wheel->delta * 34.0f,
                            0.0f,
                            deckEditorAbilityMaxScroll(deckEditorAbilityRows(*card)));
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
                     !gameSnapshot.foresightChoices.empty())
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                if (isInsideRect(wheelPos, 24.0f, 54.0f, 752.0f, 524.0f))
                {
                    const std::size_t totalRows =
                        (gameSnapshot.foresightChoices.size() + ForesightChoiceColumns - 1) /
                        ForesightChoiceColumns;
                    scrollList(
                        foresightChoiceRowOffset,
                        totalRows,
                        ForesightVisibleRows,
                        wheel->delta);
                }
            }
            else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>();
                     wheel && currentState == GameState::Game && haveSnapshot &&
                     gameSnapshot.hand.size() > VisibleGameHandCards)
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                const std::size_t visibleCards =
                    std::min(gameSnapshot.hand.size(), VisibleGameHandCards);
                const float handWidth = visibleCards == 0
                    ? HandRightX - HandStartX
                    : gameHandCardPitch(visibleCards) *
                            static_cast<float>(visibleCards - 1) +
                        HandCardWidth;
                if (isInsideRect(
                        wheelPos,
                        HandStartX,
                        HandY - HandHoverLift,
                        handWidth,
                        HandCardHeight + HandHoverLift))
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

                if (pendingPieceActionChoice)
                {
                    const int count = static_cast<int>(
                        pendingPieceActionChoice->actionIndices.size());
                    if (keyPressed->code == sf::Keyboard::Key::Escape)
                    {
                        pendingPieceActionChoice.reset();
                    }
                    else if (count > 0 &&
                        (keyPressed->code == sf::Keyboard::Key::Up ||
                         keyPressed->code == sf::Keyboard::Key::Left))
                    {
                        pendingPieceActionChoice->focusedOption =
                            (pendingPieceActionChoice->focusedOption - 1 + count) % count;
                    }
                    else if (count > 0 &&
                        (keyPressed->code == sf::Keyboard::Key::Down ||
                         keyPressed->code == sf::Keyboard::Key::Right ||
                         keyPressed->code == sf::Keyboard::Key::Tab))
                    {
                        pendingPieceActionChoice->focusedOption =
                            (pendingPieceActionChoice->focusedOption + 1) % count;
                    }
                    else if (count > 0 &&
                        (keyPressed->code == sf::Keyboard::Key::Enter ||
                         keyPressed->code == sf::Keyboard::Key::Space))
                    {
                        submitPendingPieceActionChoice(
                            pendingPieceActionChoice->focusedOption);
                    }
                    continue;
                }

                if (resignConfirmPopupVisible)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Escape)
                    {
                        resignConfirmPopupVisible = false;
                        gameConfirmationAction = GameConfirmationAction::Resign;
                        gameConfirmationKeyboardFocus = 0;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Left ||
                             keyPressed->code == sf::Keyboard::Key::Up ||
                             (keyPressed->code == sf::Keyboard::Key::Tab &&
                              keyPressed->shift))
                    {
                        gameConfirmationKeyboardFocus = 0;
                        storyKeyboardNavigationActive = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Right ||
                             keyPressed->code == sf::Keyboard::Key::Down ||
                             keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        gameConfirmationKeyboardFocus = 1;
                        storyKeyboardNavigationActive = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                             keyPressed->code == sf::Keyboard::Key::Space)
                    {
                        if (gameConfirmationKeyboardFocus == 0)
                        {
                            resignConfirmPopupVisible = false;
                            gameConfirmationAction = GameConfirmationAction::Resign;
                            gameConfirmationKeyboardFocus = 0;
                        }
                        else
                        {
                            const GameConfirmationAction confirmedAction =
                                gameConfirmationAction;
                            resignConfirmPopupVisible = false;
                            gameConfirmationAction = GameConfirmationAction::Resign;
                            gameConfirmationKeyboardFocus = 0;
                            if (confirmedAction ==
                                GameConfirmationAction::RestartStory)
                            {
                                beginStory();
                            }
                            else
                            {
                                leaveGame();
                            }
                        }
                    }
                    continue;
                }

                if (!pendingRequest && !pendingMatchmaking && !pendingSandboxLoad &&
                    currentState == GameState::Authenticated && !exitDesktopPopupVisible)
                {
                    const bool moveUp = keyPressed->code == sf::Keyboard::Key::Up ||
                        (keyPressed->code == sf::Keyboard::Key::Tab && keyPressed->shift);
                    const bool moveDown = keyPressed->code == sf::Keyboard::Key::Down ||
                        (keyPressed->code == sf::Keyboard::Key::Tab && !keyPressed->shift);
                    if (moveUp || moveDown)
                    {
                        const int count = authenticatedMenuButtonCount();
                        if (authenticatedMenuFocus < 0)
                        {
                            authenticatedMenuFocus = moveUp ? count - 1 : 0;
                        }
                        else
                        {
                            const int delta = moveUp ? -1 : 1;
                            authenticatedMenuFocus =
                                (authenticatedMenuFocus + delta + count) % count;
                        }
                        syncAuthenticatedMenuFocus();
                        continue;
                    }
                    if (keyPressed->code == sf::Keyboard::Key::Enter && authenticatedMenuFocus >= 0)
                    {
                        const int index = authenticatedMenuFocus;
                        activateAuthenticatedMenuButton(index);
                        syncAuthenticatedMenuFocus();
                        continue;
                    }
                }

                if (currentState == GameState::StorySelect)
                {
                    bool handled = false;
                    if (storySpoilerConfirmationVisible)
                    {
                        if (keyPressed->code == sf::Keyboard::Key::Left ||
                            keyPressed->code == sf::Keyboard::Key::Up ||
                            (keyPressed->code == sf::Keyboard::Key::Tab && keyPressed->shift))
                        {
                            storySpoilerKeyboardFocus = wrapStoryKeyboardIndex(
                                storySpoilerKeyboardFocus, -1, 2);
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Right ||
                                 keyPressed->code == sf::Keyboard::Key::Down ||
                                 keyPressed->code == sf::Keyboard::Key::Tab)
                        {
                            storySpoilerKeyboardFocus = wrapStoryKeyboardIndex(
                                storySpoilerKeyboardFocus, 1, 2);
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                                 keyPressed->code == sf::Keyboard::Key::Space)
                        {
                            activateStorySpoilerChoice(storySpoilerKeyboardFocus == 1);
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Escape)
                        {
                            activateStorySpoilerChoice(false);
                        }
                        storyKeyboardNavigationActive = true;
                        continue;
                    }
                    if (keyPressed->code == sf::Keyboard::Key::Left ||
                        keyPressed->code == sf::Keyboard::Key::Up ||
                        (keyPressed->code == sf::Keyboard::Key::Tab && keyPressed->shift))
                    {
                        storySelectKeyboardFocus = wrapStoryKeyboardIndex(
                            storySelectKeyboardFocus, -1, 4);
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Right ||
                             keyPressed->code == sf::Keyboard::Key::Down ||
                             keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        storySelectKeyboardFocus = wrapStoryKeyboardIndex(
                            storySelectKeyboardFocus, 1, 4);
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                             keyPressed->code == sf::Keyboard::Key::Space)
                    {
                        if (storySelectKeyboardFocus < 0)
                        {
                            storySelectKeyboardFocus = 1;
                        }
                        activateStorySelectKeyboardFocus();
                        handled = true;
                    }
                    if (handled)
                    {
                        storyKeyboardNavigationActive = true;
                        continue;
                    }
                }

                if (currentState == GameState::StoryMissionSelect)
                {
                    bool handled = false;
                    const bool moveBackward =
                        keyPressed->code == sf::Keyboard::Key::Left ||
                        keyPressed->code == sf::Keyboard::Key::Up ||
                        (keyPressed->code == sf::Keyboard::Key::Tab && keyPressed->shift);
                    const bool moveForward =
                        keyPressed->code == sf::Keyboard::Key::Right ||
                        keyPressed->code == sf::Keyboard::Key::Down ||
                        (keyPressed->code == sf::Keyboard::Key::Tab && !keyPressed->shift);
                    if (moveBackward || moveForward)
                    {
                        moveStoryMissionKeyboardFocus(moveBackward ? -1 : 1);
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::PageUp &&
                             storyMissionPage > 0)
                    {
                        --storyMissionPage;
                        focusFirstStoryMissionOnPage();
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::PageDown)
                    {
                        const int missionCount =
                            static_cast<int>(storyMissions(storyCampaign).size());
                        const int lastPage =
                            std::max(0, (missionCount - 1) / StoryMissionPageSize);
                        if (storyMissionPage < lastPage)
                        {
                            ++storyMissionPage;
                            focusFirstStoryMissionOnPage();
                        }
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                             keyPressed->code == sf::Keyboard::Key::Space)
                    {
                        if (storyMissionKeyboardFocus < 0)
                        {
                            moveStoryMissionKeyboardFocus(1);
                        }
                        activateStoryMissionKeyboardFocus();
                        handled = true;
                    }
                    if (handled)
                    {
                        storyKeyboardNavigationActive = true;
                        continue;
                    }
                }

                if (currentState == GameState::StoryIntro)
                {
                    bool handled = false;
                    if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        moveStoryIntroKeyboardFocus(keyPressed->shift ? -1 : 1);
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Left)
                    {
                        storyIntroBack();
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Right)
                    {
                        storyIntroContinue();
                        handled = true;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                             keyPressed->code == sf::Keyboard::Key::Space)
                    {
                        if (storyIntroKeyboardFocus < 0)
                        {
                            storyIntroKeyboardFocus = 2;
                        }
                        if (storyIntroKeyboardFocus == 0)
                        {
                            storyIntroBack();
                        }
                        else if (storyIntroKeyboardFocus == 1 &&
                                 (activeStoryMission().optionalRehearsal ||
                                  activeStoryCatchUpMayBeSkipped()) &&
                                 storyComicPage + 1 >= static_cast<int>(
                                     activeStoryMission().briefing.size()))
                        {
                            playButtonClickSound();
                            if (activeStoryCatchUpMayBeSkipped())
                            {
                                beginStory();
                            }
                            else
                            {
                                completeStoryChronicle();
                            }
                        }
                        else
                        {
                            storyIntroContinue();
                        }
                        handled = true;
                    }
                    if (handled)
                    {
                        storyKeyboardNavigationActive = true;
                        continue;
                    }
                }

                if (currentState == GameState::Game && storyMode &&
                    !pendingPieceActionChoice && !resignConfirmPopupVisible)
                {
                    storyKeyboardNavigationActive = true;
                    if (!storyPopupPanels.empty())
                    {
                        if (keyPressed->code == sf::Keyboard::Key::Left)
                        {
                            retreatStoryPopup();
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Right)
                        {
                            advanceStoryPopup();
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Tab)
                        {
                            if (storyPopupPage > 0)
                            {
                                storyPopupKeyboardFocus =
                                    storyPopupKeyboardFocus == 0 ? 1 : 0;
                            }
                            else
                            {
                                storyPopupKeyboardFocus = 1;
                            }
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                                 keyPressed->code == sf::Keyboard::Key::Space)
                        {
                            if (storyPopupKeyboardFocus == 0 && storyPopupPage > 0)
                            {
                                retreatStoryPopup();
                            }
                            else
                            {
                                advanceStoryPopup();
                            }
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Escape &&
                                 storyPopupPage > 0)
                        {
                            retreatStoryPopup();
                        }
                        // Story panels are modal. Escape on their first page does
                        // not abandon the mission behind the player's back.
                        continue;
                    }

                    if (!gameSnapshot.foresightChoices.empty())
                    {
                        const int choiceCount = static_cast<int>(
                            gameSnapshot.foresightChoices.size());
                        int delta = 0;
                        if (keyPressed->code == sf::Keyboard::Key::Left ||
                            (keyPressed->code == sf::Keyboard::Key::Tab && keyPressed->shift))
                        {
                            delta = -1;
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Right ||
                                 keyPressed->code == sf::Keyboard::Key::Tab)
                        {
                            delta = 1;
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Up)
                        {
                            delta = -static_cast<int>(ForesightChoiceColumns);
                        }
                        else if (keyPressed->code == sf::Keyboard::Key::Down)
                        {
                            delta = static_cast<int>(ForesightChoiceColumns);
                        }
                        if (delta != 0 && choiceCount > 0)
                        {
                            storyKeyboardForesightIndex = static_cast<std::size_t>(
                                wrapStoryKeyboardIndex(
                                    static_cast<int>(storyKeyboardForesightIndex),
                                    delta,
                                    choiceCount));
                            const std::size_t row = storyKeyboardForesightIndex /
                                ForesightChoiceColumns;
                            if (row < foresightChoiceRowOffset)
                            {
                                foresightChoiceRowOffset = row;
                            }
                            else if (row >= foresightChoiceRowOffset + ForesightVisibleRows)
                            {
                                foresightChoiceRowOffset = row - ForesightVisibleRows + 1;
                            }
                            continue;
                        }
                        if ((keyPressed->code == sf::Keyboard::Key::Enter ||
                             keyPressed->code == sf::Keyboard::Key::Space) &&
                            storyKeyboardForesightIndex <
                                gameSnapshot.foresightChoices.size())
                        {
                            sendChooseForesightCard(static_cast<int>(
                                storyKeyboardForesightIndex));
                        }
                        // The revealed-card choice is modal, just like the mouse
                        // path; no other keyboard command can leak through it.
                        continue;
                    }

                    if (inspectedPieceId || inspectedHandIndex)
                    {
                        if (keyPressed->code == sf::Keyboard::Key::I ||
                            keyPressed->code == sf::Keyboard::Key::Enter ||
                            keyPressed->code == sf::Keyboard::Key::Space)
                        {
                            inspectedPieceId.reset();
                            inspectedHandIndex.reset();
                            inspectedPieceScroll = 0.0f;
                            continue;
                        }
                        // Escape is handled by the common popup-close path below.
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        moveStoryGameKeyboardFocus(keyPressed->shift ? -1 : 1);
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::H &&
                             !gameSnapshot.hand.empty())
                    {
                        if (storyGameKeyboardFocus == StoryGameKeyboardFocus::Hand)
                        {
                            cycleStoryKeyboardHand(1);
                        }
                        else
                        {
                            storyGameKeyboardFocus = StoryGameKeyboardFocus::Hand;
                            ensureStoryKeyboardHandVisible();
                        }
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Left ||
                             keyPressed->code == sf::Keyboard::Key::Right ||
                             keyPressed->code == sf::Keyboard::Key::Up ||
                             keyPressed->code == sf::Keyboard::Key::Down)
                    {
                        if (storyGameKeyboardFocus == StoryGameKeyboardFocus::Board)
                        {
                            const int rowDelta =
                                keyPressed->code == sf::Keyboard::Key::Up ? 1 :
                                keyPressed->code == sf::Keyboard::Key::Down ? -1 : 0;
                            const int columnDelta =
                                keyPressed->code == sf::Keyboard::Key::Right ? 1 :
                                keyPressed->code == sf::Keyboard::Key::Left ? -1 : 0;
                            storyBoardKeyboardCursor = moveStoryBoardCursor(
                                storyBoardKeyboardCursor, rowDelta, columnDelta);
                        }
                        else if (storyGameKeyboardFocus ==
                                     StoryGameKeyboardFocus::Hand &&
                                 (keyPressed->code == sf::Keyboard::Key::Left ||
                                  keyPressed->code == sf::Keyboard::Key::Right))
                        {
                            cycleStoryKeyboardHand(
                                keyPressed->code == sf::Keyboard::Key::Left ? -1 : 1);
                        }
                        else
                        {
                            moveStoryGameKeyboardFocus(
                                keyPressed->code == sf::Keyboard::Key::Left ||
                                        keyPressed->code == sf::Keyboard::Key::Up
                                    ? -1
                                    : 1);
                        }
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter ||
                             keyPressed->code == sf::Keyboard::Key::Space)
                    {
                        activateStoryGameKeyboardFocus();
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::A)
                    {
                        if (storyAbilityAvailable())
                        {
                            storyGameKeyboardFocus = StoryGameKeyboardFocus::Ability;
                            activateStoryGameKeyboardFocus();
                        }
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::D)
                    {
                        if (playerCanDrawCard())
                        {
                            storyGameKeyboardFocus = StoryGameKeyboardFocus::DrawPile;
                            activateStoryGameKeyboardFocus();
                        }
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::E)
                    {
                        if (storyEndTurnAvailable())
                        {
                            storyGameKeyboardFocus = StoryGameKeyboardFocus::EndTurn;
                            activateStoryGameKeyboardFocus();
                        }
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::X)
                    {
                        const std::size_t discardIndex = selectedHandIndex
                            ? *selectedHandIndex
                            : storyKeyboardHandIndex;
                        if (canDiscardHandCard(discardIndex))
                        {
                            sendDiscardCard(static_cast<int>(discardIndex));
                            selectedHandIndex.reset();
                            selectedPieceId.reset();
                            ensureStoryKeyboardHandVisible();
                        }
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::I)
                    {
                        if (storyGameKeyboardFocus == StoryGameKeyboardFocus::Hand &&
                            storyKeyboardHandIndex < gameSnapshot.hand.size())
                        {
                            inspectedHandIndex = storyKeyboardHandIndex;
                            inspectedPieceId.reset();
                        }
                        else
                        {
                            const game_data::Piece* piece = gamePieceAt(
                                storyBoardKeyboardCursor.row,
                                storyBoardKeyboardCursor.column);
                            inspectedPieceId = piece
                                ? std::optional<int>(piece->id)
                                : std::nullopt;
                            inspectedHandIndex.reset();
                        }
                        inspectedPieceScroll = 0.0f;
                        continue;
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Escape)
                    {
                        if (selectedPieceId || selectedHandIndex)
                        {
                            selectedPieceId.reset();
                            selectedHandIndex.reset();
                            storyGameKeyboardFocus = StoryGameKeyboardFocus::Board;
                        }
                        else if (storyGameKeyboardFocus !=
                                 StoryGameKeyboardFocus::Board)
                        {
                            storyGameKeyboardFocus = StoryGameKeyboardFocus::Board;
                        }
                        else
                        {
                            gameConfirmationAction =
                                GameConfirmationAction::ExitStory;
                            resignConfirmPopupVisible = true;
                        }
                        continue;
                    }
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
                    else if (currentState == GameState::StorySelect)
                    {
                        showAuthenticatedScreen();
                    }
                    else if (currentState == GameState::StoryMissionSelect)
                    {
                        showStorySelect();
                    }
                    else if (currentState == GameState::StoryIntro)
                    {
                        showStoryMissionSelect(storyCampaign);
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
        else if (currentState == GameState::StorySelect)
        {
            if (storySpoilerConfirmationVisible)
            {
                storySpoilerMirewatchButton.update(mousePos);
                storySpoilerContinueButton.update(mousePos);
            }
            else
            {
                storyBlackthornButton.update(mousePos);
                storyMirewatchButton.update(mousePos);
                storySeelieButton.update(mousePos);
                storySelectBackButton.update(mousePos);
            }
        }
        else if (currentState == GameState::StoryMissionSelect)
        {
            for (Button& button : storyMissionButtons)
            {
                button.update(mousePos);
            }
            storyMissionPreviousPageButton.update(mousePos);
            storyMissionNextPageButton.update(mousePos);
            storyRestartCampaignButton.update(mousePos);
            storyMissionSelectBackButton.update(mousePos);
        }
        else if (currentState == GameState::StoryIntro)
        {
            storyBackButton.update(mousePos);
            storyContinueButton.update(mousePos);
            if ((activeStoryMission().optionalRehearsal ||
                 activeStoryCatchUpMayBeSkipped()) &&
                storyComicPage + 1 >= static_cast<int>(activeStoryMission().briefing.size()))
            {
                storySkipDrillButton.update(mousePos);
            }
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
            syncAuthenticatedMenuFocus();
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
            layoutDeckSelectControls();
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
            if (pendingPieceActionChoice)
            {
                // Hover is drawn directly from the pointer so the underlying
                // board controls remain inert while a profile is being chosen.
            }
            else if (resignConfirmPopupVisible)
            {
                cancelResignButton.update(mousePos);
                confirmResignButton.update(mousePos);
            }
            else if (storyMode && !storyPopupPanels.empty())
            {
                if (storyPopupPage > 0)
                {
                    storyPopupPreviousButton.update(mousePos);
                }
                storyPopupContinueButton.update(mousePos);
            }
            else if (inspectedPieceId || inspectedHandIndex)
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
                if (storyMode)
                {
                    storyRestartButton.update(mousePos);
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
        else if (currentState == GameState::StorySelect)
        {
            drawStorySelect();
            window.draw(messageText);
        }
        else if (currentState == GameState::StoryMissionSelect)
        {
            drawStoryMissionSelect();
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
                sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
                overlay.setPosition({ui_canvas::Left, 0.0f});
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
            drawAuthenticatedMenuButton(storyButton, mainMenuStoryIconTexture);
            drawAuthenticatedMenuButton(playButton, mainMenuPlayIconTexture);
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
            syncStoryGameKeyboardButtonFocus();
            drawGame();
            drawStoryKeyboardFocus();
            if (pendingPieceActionChoice)
            {
                drawActionChoicePopup();
            }
            else if (resignConfirmPopupVisible)
            {
                drawResignConfirmationPopup();
            }
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
                if (const std::optional<std::string> invariantError =
                        storyActionCaptureInvariantError())
                {
                    failCaptureValidation(
                        "Capture replay error: action screen '" + screen +
                        "' " + *invariantError);
                }
                const std::string captureFile =
                    ui_capture::captureFileName(captureIndex, screen);
                const std::filesystem::path capturePath =
                    captureRequest->outputDirectory / captureFile;
                if (!ui_capture::saveWindow(window, capturePath))
                {
                    failCaptureValidation(
                        "Capture write error: could not save " + capturePath.string());
                }
                else
                {
                    successfulCaptureFiles.push_back(captureFile);
                }

                captureFramesOnScreen = 0;
                ++captureIndex;
                if (captureIndex >= captureRequest->screens.size())
                {
                    if (!captureValidationFailed)
                    {
                        std::string manifestError;
                        const std::filesystem::path executablePath =
                            argc > 0 && argv[0] ? argv[0] : std::filesystem::path{};
                        if (!ui_capture::writeCompletionManifest(
                                *captureRequest,
                                successfulCaptureFiles,
                                executablePath,
                                manifestError))
                        {
                            failCaptureValidation(
                                "Capture completion error: " + manifestError);
                        }
                        else
                        {
                            captureCompleted = true;
                        }
                    }
                    window.close();
                }
                else
                {
                    applyCaptureScreen(captureRequest->screens[captureIndex]);
                }
            }
        }
    }

    return captureRequest && (captureValidationFailed || !captureCompleted) ? 1 : 0;
}
