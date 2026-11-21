#pragma once

#include <SFML/Network.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace marker_data
{
enum class Eligibility : std::uint8_t { Friendly, Enemy, Any };
enum class Effect : std::uint8_t { Heal, Damage };
struct Definition
{
    std::string name;
    Eligibility eligibility = Eligibility::Friendly;
    Effect effect = Effect::Heal;
    int strength = 1;
    bool operator==(const Definition&) const = default;
};

inline std::string nameKey(std::string_view value)
{
    std::string result(value);
    for (char& ch : result) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch-'A'+'a');
    return result;
}

inline bool validName(std::string_view name)
{
    if (name.empty() || name.size() > 128 || name.front() == ' ' || name.back() == ' ' || nameKey(name)=="blight")
        return false;
    for (unsigned char ch : name) if (ch < 32 || ch == 127) return false;
    return true;
}

inline bool valid(const Definition& value)
{
    return validName(value.name) && value.eligibility <= Eligibility::Any &&
           value.effect <= Effect::Damage && value.strength > 0;
}

inline void write(sf::Packet& packet, const Definition& value)
{
    packet << value.name << static_cast<std::uint8_t>(value.eligibility)
           << static_cast<std::uint8_t>(value.effect) << value.strength;
}

inline bool read(sf::Packet& packet, Definition& value)
{
    std::uint8_t eligibility=0,effect=0;
    if (!(packet >> value.name >> eligibility >> effect >> value.strength)) return false;
    value.eligibility=static_cast<Eligibility>(eligibility);
    value.effect=static_cast<Effect>(effect);
    return valid(value);
}

inline void writeOptional(sf::Packet& packet, const std::optional<Definition>& value)
{
    packet << static_cast<std::uint8_t>(value.has_value());
    if (value) write(packet,*value);
}

inline bool readOptional(sf::Packet& packet, std::optional<Definition>& value)
{
    value.reset();
    std::uint8_t present=0;
    return (packet >> present) && present <= 1 && (present==0 || read(packet,value.emplace()));
}
} // namespace marker_data
