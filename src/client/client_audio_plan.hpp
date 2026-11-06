#pragma once

#include "../shared/game_data.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bayou::client::audio
{
struct Layer { std::string_view id; float gain = 0.7f; float delay = 0.0f; };
struct MappedAction { std::string_view name, internalName; std::vector<Layer> layers; };
struct SoundProfile
{
    std::string_view title;
    float identitySemitones = 0.0f;
    std::vector<Layer> movement, deploy, damage, death;
    std::vector<MappedAction> actions;
};
struct SoundDefinition
{
    std::string_view id, file;
    std::vector<std::string_view> slices;
    bool loop = false;
};
enum class Bus { UI, Combat, Accent, Ambient, Alert, Hover };
struct Request
{
    std::string id;
    float gain = 1.0f, semitones = 0.0f, delay = 0.0f;
    bool slice = false, reverse = false;
    Bus bus = Bus::Combat;
    int actorId = 0;
};
struct Intent
{
    enum class Kind { None, Action, Ability, PlayCard, Discard };
    Kind kind = Kind::None;
    int actorId = 0, row = -1, column = -1, actionIndex = -1, handIndex = -1;
    bool sacrifice = false;
};
const std::vector<SoundDefinition>& definitions();
const std::vector<SoundProfile>& profiles();
const SoundProfile* profile(std::string_view title);
std::vector<Layer> abilityRecipe(std::string_view ability);
std::vector<Layer> effectRecipe(const game_data::GameCard& card);
std::vector<Request> planTransition(const game_data::Snapshot& before,
    const game_data::Snapshot& after, const Intent& intent = {});
bool gameplayChanged(const game_data::Snapshot& before, const game_data::Snapshot& after);
}
