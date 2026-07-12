#include "card_source_config.hpp"

#include <cctype>
#include <fstream>
#include <limits>
#include <utility>

namespace card_source_config
{
namespace
{
std::string trim(const std::string& value)
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
    {
        --last;
    }
    return value.substr(first, last - first);
}

std::optional<unsigned short> parsePort(const std::string& value)
{
    try
    {
        std::size_t parsed = 0;
        const unsigned long port = std::stoul(value, &parsed);
        if (parsed != value.size() || port == 0 || port > std::numeric_limits<unsigned short>::max())
        {
            return std::nullopt;
        }
        return static_cast<unsigned short>(port);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool parseEndpoint(const std::string& value, std::string& host, unsigned short& port)
{
    const std::size_t delimiter = value.rfind(':');
    if (delimiter == std::string::npos)
    {
        host = value;
        port = 55004;
        return !host.empty();
    }

    host = trim(value.substr(0, delimiter));
    const std::optional<unsigned short> parsedPort = parsePort(trim(value.substr(delimiter + 1)));
    if (host.empty() || !parsedPort)
    {
        return false;
    }
    port = *parsedPort;
    return true;
}
}

std::optional<Config> load(const std::filesystem::path& configPath, std::string& error)
{
    std::ifstream stream(configPath);
    if (!stream)
    {
        error = "could not open server card-source config " + configPath.string();
        return std::nullopt;
    }

    std::string cardsDatabaseValue;
    std::string cardServerValue;
    std::string cardServerHost;
    unsigned short cardServerPort = 0;
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';')
        {
            continue;
        }

        const std::size_t delimiter = trimmed.find('=');
        if (delimiter == std::string::npos)
        {
            error = "invalid server card-source config line: " + trimmed;
            return std::nullopt;
        }

        const std::string key = trim(trimmed.substr(0, delimiter));
        const std::string value = trim(trimmed.substr(delimiter + 1));
        if (key == "cards_database" || key == "card_database")
        {
            cardsDatabaseValue = value;
        }
        else if (key == "card_server")
        {
            cardServerValue = value;
        }
        else if (key == "card_server_host")
        {
            cardServerHost = value;
        }
        else if (key == "card_server_port")
        {
            const std::optional<unsigned short> parsedPort = parsePort(value);
            if (!parsedPort)
            {
                error = "invalid card_server_port in server card-source config";
                return std::nullopt;
            }
            cardServerPort = *parsedPort;
        }
    }

    if (!cardServerValue.empty() && (!cardServerHost.empty() || cardServerPort != 0))
    {
        error = "use either card_server or card_server_host/card_server_port, not both";
        return std::nullopt;
    }

    if (!cardServerValue.empty())
    {
        if (!parseEndpoint(cardServerValue, cardServerHost, cardServerPort))
        {
            error = "invalid card_server in server card-source config";
            return std::nullopt;
        }
    }
    else if (!cardServerHost.empty() && cardServerPort == 0)
    {
        cardServerPort = 55004;
    }

    if (cardServerHost.empty() && cardsDatabaseValue.empty())
    {
        error = "server card-source config is missing card_server or cards_database";
        return std::nullopt;
    }

    std::optional<std::filesystem::path> cardsDatabasePath;
    if (!cardServerHost.empty())
    {
        cardsDatabaseValue.clear();
    }
    if (!cardsDatabaseValue.empty())
    {
        std::filesystem::path resolvedPath(cardsDatabaseValue);
        if (resolvedPath.is_relative())
        {
            resolvedPath = configPath.parent_path() / resolvedPath;
        }
        resolvedPath = resolvedPath.lexically_normal();

        std::error_code filesystemError;
        if (!std::filesystem::is_regular_file(resolvedPath, filesystemError))
        {
            error = "configured cards database does not exist: " + resolvedPath.string();
            return std::nullopt;
        }
        cardsDatabasePath = std::move(resolvedPath);
    }

    return Config{std::move(cardsDatabasePath), std::move(cardServerHost), cardServerPort};
}

} // namespace card_source_config
