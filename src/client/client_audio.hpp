#pragma once

#include "client_audio_events.hpp"
#include "client_audio_plan.hpp"
#include "../shared/account_data.hpp"

#include <filesystem>
#include <memory>
#include <string_view>

namespace bayou::client
{
enum class AudioCue { ButtonClick, PiecePlace, UnitMove, UnitAttack, UnitDeath,
    Dematerialize, Victory, Defeat, ClockWarning };

class AudioSystem
{
public:
    explicit AudioSystem(bool startMusic = true);
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    void play(AudioCue cue, float volumeScale = 1.0f);
    void play(std::string_view id, float gain = 1.0f, audio::Bus bus = audio::Bus::UI);
    void submit(const std::vector<audio::Request>& requests);
    void transition(const game_data::Snapshot& before, const game_data::Snapshot& after, bool initialized);
    void expect(audio::Intent intent);
    void resetMatch(bool preserveInterface = false);
    void update();
    void setEnabled(bool value);
    void setAmbient(bool value);
    void setAllVolume(float value);
    void setMusicVolume(float value);
    void setSoundEffectsVolume(float value);
    float getAllVolume() const;
    float getMusicVolume() const;
    float getSoundEffectsVolume() const;
    void setAllMuted(bool value);
    void setMusicMuted(bool value);
    void setSoundEffectsMuted(bool value);
    bool isAllMuted() const;
    bool isMusicMuted() const;
    bool isSoundEffectsMuted() const;
    account_data::AudioSettings getSettings() const;
    void applySettings(const account_data::AudioSettings& settings);
    bool smokeTest(const std::filesystem::path& report);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
