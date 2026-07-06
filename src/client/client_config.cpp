#include "client_config.hpp"
#include "client_string.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace bayou::client
{
namespace
{
#ifdef NDEBUG
constexpr const char* ClientConfigFileName = "client_release.cfg";
#else
constexpr const char* ClientConfigFileName = "client_debug.cfg";
#endif
constexpr const char* DisplaySettingsFileName = "display.cfg";
constexpr const char* RememberTokenFileName = "remember_me.dat";

std::filesystem::path executableDirectory;

std::filesystem::path displaySettingsPath()
{
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        return std::filesystem::path(appData) / "SteamTactics" / DisplaySettingsFileName;
    }
#else
    if (const char* home = std::getenv("HOME"); home && *home)
    {
        return std::filesystem::path(home) / ".config" / "SteamTactics" / DisplaySettingsFileName;
    }
#endif

    if (!executableDirectory.empty())
    {
        return executableDirectory / DisplaySettingsFileName;
    }
    return DisplaySettingsFileName;
}

std::filesystem::path userDataPath(const char* fileName)
{
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        return std::filesystem::path(appData) / "SteamTactics" / fileName;
    }
#else
    if (const char* home = std::getenv("HOME"); home && *home)
    {
        return std::filesystem::path(home) / ".config" / "SteamTactics" / fileName;
    }
#endif

    if (!executableDirectory.empty())
    {
        return executableDirectory / fileName;
    }
    return fileName;
}

std::filesystem::path rememberTokenPath()
{
    return userDataPath(RememberTokenFileName);
}

std::optional<unsigned short> parsePort(const std::string& value)
{
    const std::string trimmed = trim(value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    try
    {
        std::size_t parsed = 0;
        const unsigned long port = std::stoul(trimmed, &parsed);
        if (parsed != trimmed.size() || port == 0 || port > std::numeric_limits<unsigned short>::max())
        {
            return std::nullopt;
        }

        return static_cast<unsigned short>(port);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

void applyServerValue(ServerEndpoint& endpoint, const std::string& value)
{
    const std::string server = trim(value);
    if (server.empty())
    {
        return;
    }

    if (server.front() == '[')
    {
        const std::size_t closeBracket = server.find(']');
        if (closeBracket != std::string::npos)
        {
            const std::string host = trim(server.substr(1, closeBracket - 1));
            if (!host.empty())
            {
                endpoint.host = host;
            }

            if (closeBracket + 1 < server.size() && server[closeBracket + 1] == ':')
            {
                if (const std::optional<unsigned short> port = parsePort(server.substr(closeBracket + 2)))
                {
                    endpoint.port = *port;
                }
            }
            return;
        }
    }

    const std::size_t delimiter = server.rfind(':');
    if (delimiter != std::string::npos && server.find(':') == delimiter)
    {
        const std::string host = trim(server.substr(0, delimiter));
        if (!host.empty())
        {
            endpoint.host = host;
        }

        if (const std::optional<unsigned short> port = parsePort(server.substr(delimiter + 1)))
        {
            endpoint.port = *port;
        }
        return;
    }

    endpoint.host = server;
}

void applyConfigEntry(ClientConfig& config, const std::string& key, const std::string& value)
{
    const std::string normalizedKey = lowerKey(trim(key));
    if (normalizedKey == "account_server" || normalizedKey == "accounts_server")
    {
        applyServerValue(config.account, value);
    }
    else if (normalizedKey == "account_server_host" || normalizedKey == "accounts_server_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.account.host = host;
        }
    }
    else if (normalizedKey == "account_server_port" || normalizedKey == "accounts_server_port")
    {
        if (const std::optional<unsigned short> port = parsePort(value))
        {
            config.account.port = *port;
        }
    }
    else if (normalizedKey == "matchmaking_server")
    {
        applyServerValue(config.matchmaking, value);
    }
    else if (normalizedKey == "matchmaking_server_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.matchmaking.host = host;
        }
    }
    else if (normalizedKey == "matchmaking_server_port")
    {
        if (const std::optional<unsigned short> port = parsePort(value))
        {
            config.matchmaking.port = *port;
        }
    }
    else if (normalizedKey == "card_server" || normalizedKey == "cardserver")
    {
        applyServerValue(config.card, value);
    }
    else if (normalizedKey == "card_server_host" || normalizedKey == "cardserver_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.card.host = host;
        }
    }
    else if (normalizedKey == "card_server_port" || normalizedKey == "cardserver_port")
    {
        if (const std::optional<unsigned short> port = parsePort(value))
        {
            config.card.port = *port;
        }
    }
    else if (normalizedKey == "game_server_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.gameServerHost = host;
        }
    }
    else if (normalizedKey == "payment_server_url" || normalizedKey == "stripe_server_url")
    {
        const std::string url = stripTrailingSlashes(trim(value));
        if (!url.empty())
        {
            config.paymentServerUrl = url;
        }
    }
}

std::optional<ClientConfig> loadClientConfigFrom(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream)
    {
        return std::nullopt;
    }

    ClientConfig config;
    std::string line;
    while (std::getline(stream, line))
    {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';')
        {
            continue;
        }

        const std::size_t delimiter = line.find('=');
        if (delimiter == std::string::npos)
        {
            continue;
        }

        applyConfigEntry(config, line.substr(0, delimiter), line.substr(delimiter + 1));
    }

    return config;
}

ClientConfig loadClientConfig()
{
    if (const std::optional<ClientConfig> config = loadClientConfigFrom(ClientConfigFileName))
    {
        return *config;
    }

    if (!executableDirectory.empty())
    {
        if (const std::optional<ClientConfig> config = loadClientConfigFrom(executableDirectory / ClientConfigFileName))
        {
            return *config;
        }
    }

    return {};
}
}

void setExecutableDirectory(const char* executablePath)
{
    if (executablePath == nullptr)
    {
        return;
    }

    const std::filesystem::path path(executablePath);
    if (path.has_parent_path())
    {
        executableDirectory = path.parent_path();
    }
}

const ClientConfig& clientConfig()
{
    static const ClientConfig config = loadClientConfig();
    return config;
}

bool saveRememberToken(const std::string& token)
{
    if (token.empty())
    {
        return false;
    }

    std::vector<std::uint8_t> stored;
#ifdef _WIN32
    DATA_BLOB plaintext{
        static_cast<DWORD>(token.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(token.data()))};
    DATA_BLOB protectedData{};
    if (!CryptProtectData(
            &plaintext,
            L"Gloomthorn remembered login",
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &protectedData))
    {
        return false;
    }
    stored.assign(protectedData.pbData, protectedData.pbData + protectedData.cbData);
    LocalFree(protectedData.pbData);
#else
    stored.assign(token.begin(), token.end());
#endif

    const std::filesystem::path path = rememberTokenPath();
    std::error_code error;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            return false;
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }
    stream.write(
        reinterpret_cast<const char*>(stored.data()),
        static_cast<std::streamsize>(stored.size()));
    stream.close();
    if (!stream)
    {
        return false;
    }

#ifndef _WIN32
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        error);
#endif
    return true;
}

std::optional<std::string> loadRememberToken()
{
    std::ifstream stream(rememberTokenPath(), std::ios::binary);
    if (!stream)
    {
        return std::nullopt;
    }

    std::vector<std::uint8_t> stored(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    if (stored.empty())
    {
        return std::nullopt;
    }

#ifdef _WIN32
    DATA_BLOB protectedData{
        static_cast<DWORD>(stored.size()),
        reinterpret_cast<BYTE*>(stored.data())};
    DATA_BLOB plaintext{};
    if (!CryptUnprotectData(
            &protectedData,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &plaintext))
    {
        return std::nullopt;
    }
    std::string token(
        reinterpret_cast<const char*>(plaintext.pbData),
        plaintext.cbData);
    SecureZeroMemory(plaintext.pbData, plaintext.cbData);
    LocalFree(plaintext.pbData);
    return token.empty() ? std::nullopt : std::optional<std::string>(std::move(token));
#else
    return std::string(stored.begin(), stored.end());
#endif
}

void clearRememberToken()
{
    std::error_code error;
    std::filesystem::remove(rememberTokenPath(), error);
}

DisplaySettings loadDisplaySettings()
{
    DisplaySettings settings;
    std::ifstream stream(displaySettingsPath());
    std::string line;
    while (std::getline(stream, line))
    {
        const std::size_t delimiter = line.find('=');
        if (delimiter == std::string::npos)
        {
            continue;
        }

        const std::string key = lowerKey(trim(line.substr(0, delimiter)));
        const std::string value = lowerKey(trim(line.substr(delimiter + 1)));
        try
        {
            if (key == "fullscreen")
            {
                settings.fullscreen = value == "1" || value == "true" || value == "yes";
            }
            else if (key == "width")
            {
                settings.width = static_cast<unsigned int>(std::stoul(value));
            }
            else if (key == "height")
            {
                settings.height = static_cast<unsigned int>(std::stoul(value));
            }
        }
        catch (const std::exception&)
        {
            // Ignore malformed values and retain safe defaults.
        }
    }
    return settings;
}

bool saveDisplaySettings(const DisplaySettings& settings)
{
    const std::filesystem::path path = displaySettingsPath();
    std::error_code error;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            return false;
        }
    }

    std::ofstream stream(path, std::ios::trunc);
    if (!stream)
    {
        return false;
    }

    stream << "fullscreen=" << (settings.fullscreen ? "true" : "false") << '\n'
           << "width=" << settings.width << '\n'
           << "height=" << settings.height << '\n';
    return static_cast<bool>(stream);
}

std::string stripTrailingSlashes(std::string value)
{
    while (!value.empty() && value.back() == '/')
    {
        value.pop_back();
    }
    return value;
}

std::string assetRelativePath(const std::string& value)
{
    const std::string trimmed = trim(value);
    if (trimmed.empty())
    {
        return "";
    }

    std::filesystem::path path(trimmed);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    {
        return path.lexically_normal().generic_string();
    }

    std::filesystem::path normalizedPath;
    bool checkedFirstComponent = false;
    for (const std::filesystem::path& component : path)
    {
        if (!checkedFirstComponent)
        {
            checkedFirstComponent = true;
            if (lowerKey(component.string()) == "assets")
            {
                continue;
            }
        }

        normalizedPath /= component;
    }

    return normalizedPath.lexically_normal().generic_string();
}

std::optional<std::filesystem::path> resolveAssetPath(const std::string& value)
{
    const std::string relativeValue = assetRelativePath(value);
    if (relativeValue.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path relativePath(relativeValue);
    if (relativePath.is_absolute())
    {
        return relativePath;
    }

    const std::filesystem::path cwdCandidate = (std::filesystem::path("assets") / relativePath).lexically_normal();
    if (std::filesystem::exists(cwdCandidate))
    {
        return cwdCandidate;
    }

    if (!executableDirectory.empty())
    {
        const std::filesystem::path exeCandidate = (executableDirectory / "assets" / relativePath).lexically_normal();
        if (std::filesystem::exists(exeCandidate))
        {
            return exeCandidate;
        }
    }

    return cwdCandidate;
}

std::string endpointText(const ServerEndpoint& endpoint)
{
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

bool connectToHostPort(sf::TcpSocket& socket, const std::string& host, unsigned short port)
{
    const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(host);
    if (!address)
    {
        return false;
    }

    return socket.connect(*address, port) == sf::Socket::Status::Done;
}

bool connectToEndpoint(sf::TcpSocket& socket, const ServerEndpoint& endpoint)
{
    return connectToHostPort(socket, endpoint.host, endpoint.port);
}
}
