#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace conquest_map
{

inline constexpr std::string_view DarkRealmsId = "dark-realms";
inline constexpr std::string_view DarkRealmsName = "Dark Realms";
inline constexpr std::string_view DarkRealmsAsset = "conquest/maps/dark-realms.png";
inline constexpr int DarkRealmsImageWidth = 1536;
inline constexpr int DarkRealmsImageHeight = 1024;

struct RegionDefinition
{
    int id = 0;
    std::string_view name;
    int centerX = 0;
    int centerY = 0;
    bool edge = false;
    // Zeroes pad regions with fewer than six neighbours.
    std::array<int, 6> neighbours{};
};

// This compiled form mirrors assets/conquest/maps/dark-realms.json. Keeping
// the graph in a header lets both authoritative services and the graphical
// client validate moves without introducing a runtime JSON dependency.
inline constexpr std::array<RegionDefinition, 20> DarkRealmsRegions{{
    {1,  "Northpoint",    430,  128, true,  {2, 4, 5, 0, 0, 0}},
    {2,  "Frostbourne",   732,  140, true,  {1, 3, 5, 6, 0, 0}},
    {3,  "Grimhold",      1036, 146, true,  {2, 7, 8, 0, 0, 0}},
    {4,  "Blackreach",    289,  237, true,  {1, 5, 9, 0, 0, 0}},
    {5,  "Stonegate",     506,  264, false, {1, 2, 4, 6, 10, 0}},
    {6,  "Ironwood",      783,  267, false, {2, 5, 7, 11, 0, 0}},
    {7,  "Dreadmoor",     1006, 275, false, {3, 6, 8, 11, 12, 0}},
    {8,  "Bloodfen",      1253, 295, true,  {3, 7, 13, 0, 0, 0}},
    {9,  "Shadowmere",    272,  385, true,  {4, 10, 14, 0, 0, 0}},
    {10, "Wraithlands",   530,  413, false, {5, 9, 11, 14, 15, 0}},
    {11, "Emberfall",     768,  453, false, {6, 7, 10, 12, 15, 16}},
    {12, "Darkest Pass",  992,  456, false, {7, 11, 13, 16, 17, 0}},
    {13, "Skull Coast",   1263, 453, true,  {8, 12, 17, 18, 0, 0}},
    {14, "Sable Dunes",   272,  576, true,  {9, 10, 15, 19, 0, 0}},
    {15, "Oathwatch",     531,  578, false, {10, 11, 14, 16, 19, 0}},
    {16, "Deadcross",     776,  612, false, {11, 12, 15, 17, 19, 20}},
    {17, "Blackhollow",   1026, 608, false, {12, 13, 16, 18, 20, 0}},
    {18, "Tempest Bay",   1236, 680, true,  {13, 17, 20, 0, 0, 0}},
    {19, "Sunken Reef",   506,  749, true,  {14, 15, 16, 20, 0, 0}},
    {20, "Doomstone",     793,  812, true,  {16, 17, 18, 19, 0, 0}}
}};

inline constexpr std::array<int, 11> DarkRealmsEdgeRegions{
    1, 2, 3, 8, 13, 18, 20, 19, 14, 9, 4};

struct PlayerColor
{
    std::uint8_t red = 255;
    std::uint8_t green = 255;
    std::uint8_t blue = 255;
};

inline constexpr std::array<PlayerColor, 12> PlayerColors{{
    {224, 73, 73},
    {66, 145, 235},
    {72, 190, 112},
    {232, 177, 59},
    {166, 104, 224},
    {44, 196, 196},
    {238, 116, 184},
    {233, 126, 53},
    {143, 196, 61},
    {103, 109, 224},
    {191, 83, 120},
    {157, 127, 85}
}};

inline constexpr const RegionDefinition* region(int id)
{
    return id >= 1 && id <= static_cast<int>(DarkRealmsRegions.size())
        ? &DarkRealmsRegions[static_cast<std::size_t>(id - 1)]
        : nullptr;
}

inline constexpr bool isEdgeRegion(int id)
{
    const RegionDefinition* found = region(id);
    return found && found->edge;
}

inline constexpr bool areAdjacent(int first, int second)
{
    const RegionDefinition* found = region(first);
    return found && region(second) &&
        std::find(found->neighbours.begin(), found->neighbours.end(), second) !=
        found->neighbours.end();
}

} // namespace conquest_map
