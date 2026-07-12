#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace card_source_config
{

struct Config
{
    std::optional<std::filesystem::path> cardsDatabasePath;
    std::string cardServerHost;
    unsigned short cardServerPort = 0;

    bool usesCardServer() const
    {
        return !cardServerHost.empty();
    }
};

std::optional<Config> load(const std::filesystem::path& configPath, std::string& error);

} // namespace card_source_config
