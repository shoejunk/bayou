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
    std::string imagePath; // Optional asset relative to assets/, owned by this object.
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

inline bool validImagePath(std::string_view path)
{
    if (path.size() > 512 || (!path.empty() && (path.front() == '/' || path.front() == '\\')) ||
        path.find(':') != path.npos || path.find('\\') != path.npos) return false;
    std::size_t start = 0;
    while (start < path.size()) {
        const auto end = path.find('/',start);
        const auto part = path.substr(start,end == path.npos ? path.size()-start : end-start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == path.npos) break;
        start = end+1;
    }
    for (unsigned char c : path) if (c < 32 || c == 127) return false;
    return true;
}

inline bool valid(const Definition& value)
{
    return validName(value.name) && validImagePath(value.imagePath) && value.eligibility <= Eligibility::Any &&
           value.effect <= Effect::Damage && value.strength > 0;
}

inline void write(sf::Packet& packet, const Definition& value, bool includesArt = false)
{
    packet << value.name << static_cast<std::uint8_t>(value.eligibility)
           << static_cast<std::uint8_t>(value.effect) << value.strength;
    if (includesArt) packet << value.imagePath;
}

inline bool read(sf::Packet& packet, Definition& value, bool includesArt = false)
{
    value.imagePath.clear();
    std::uint8_t eligibility=0,effect=0;
    if (!(packet >> value.name >> eligibility >> effect >> value.strength)) return false;
    value.eligibility=static_cast<Eligibility>(eligibility);
    value.effect=static_cast<Effect>(effect);
    if (includesArt && !(packet >> value.imagePath)) return false;
    return valid(value);
}

inline void writeOptional(sf::Packet& packet, const std::optional<Definition>& value, bool includesArt = false)
{
    packet << static_cast<std::uint8_t>(value.has_value());
    if (value) write(packet,*value,includesArt);
}

inline bool readOptional(sf::Packet& packet, std::optional<Definition>& value, bool includesArt = false)
{
    value.reset();
    std::uint8_t present=0;
    return (packet >> present) && present <= 1 && (present==0 || read(packet,value.emplace(),includesArt));
}
} // namespace marker_data
