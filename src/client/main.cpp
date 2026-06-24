#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/game_data.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <shellapi.h>
#endif

import button;
import card_editor_screen;
import inputbox;
import network;

namespace
{
constexpr unsigned short AccountServerPort = 55000;
constexpr unsigned short MatchmakingServerPort = 55001;
constexpr unsigned short CardServerPort = 55004;
constexpr const char* DefaultServerHost = "127.0.0.1";
constexpr const char* DefaultPaymentServerUrl = "http://127.0.0.1:55005";
constexpr bool EnableCoinPurchases = false;
constexpr const char* CoinPackId = "coins_50";
constexpr int CoinPackCoins = 50;
constexpr float CoinPurchasePollIntervalSeconds = 2.0f;
constexpr float CoinPurchasePollTimeoutSeconds = 300.0f;
#ifdef NDEBUG
constexpr const char* ClientConfigFileName = "client_release.cfg";
#else
constexpr const char* ClientConfigFileName = "client_debug.cfg";
#endif

constexpr float DeckPanelX = 20.0f;
constexpr float CurrentDeckPanelX = 290.0f;
constexpr float LibraryPanelX = 560.0f;
constexpr float DeckEditorPanelY = 96.0f;
constexpr float DeckEditorPanelHeight = 400.0f;
constexpr float DeckListX = 34.0f;
constexpr float DeckListY = 194.0f;
constexpr float DeckListWidth = 222.0f;
constexpr float DeckRowHeight = 42.0f;
constexpr std::size_t VisibleDeckRows = 7;

constexpr float DeckCardsX = 304.0f;
constexpr float DeckCardsY = 252.0f;
constexpr float DeckCardsWidth = 222.0f;
constexpr float DeckCardRowHeight = 40.0f;
constexpr std::size_t VisibleDeckCardRows = 6;
constexpr std::uint32_t AdminUsersPageSize = 6;
constexpr float AdminUserRowY = 174.0f;
constexpr float AdminUserRowHeight = 43.0f;

constexpr float LibraryX = 574.0f;
constexpr float LibraryY = 168.0f;
constexpr float LibraryWidth = 192.0f;
constexpr float LibraryRowHeight = 40.0f;
constexpr std::size_t VisibleLibraryRows = 8;

enum class GameState
{
    Menu,
    SandboxLoading,
    Options,
    Login,
    CreateAccount,
    ChangePassword,
    Authenticated,
    DeckSelect,
    Matchmaking,
    DeckEditor,
    Shop,
    AdminUsers,
    CardEditor,
    Game
};

// In-game board layout.
constexpr float BoardOriginX = 24.0f;
constexpr float BoardOriginY = 70.0f;
constexpr float CellSize = 94.0f;
constexpr float BoardBottomWidth = CellSize * static_cast<float>(game_data::BoardSize);
constexpr float BoardTopWidth = 544.0f;
constexpr float BoardHeight = 418.0f;
constexpr float BoardCenterX = BoardOriginX + BoardBottomWidth * 0.5f;
constexpr float BoardPerspectiveExponent = 1.18f;
constexpr float BoardThickness = 14.0f;
constexpr float PieceFarScale = 0.72f;
constexpr float PieceNearScale = 1.22f;
constexpr float PieceBaseWidth = 96.0f;
constexpr float PieceBaseHeight = 100.0f;
constexpr float PieceWalkBaseHeight = 108.0f;
constexpr float WalkAnimationLoopSeconds = 1.0f;
constexpr float GameLabelY = 44.0f;
constexpr float GameActionButtonY = 14.0f;
constexpr float HandY = 512.0f;
constexpr float HandCardWidth = 88.0f;
constexpr float HandCardHeight = 78.0f;
constexpr float HandGap = 6.0f;
constexpr float HandStartX = 28.0f;
constexpr std::size_t VisibleGameHandCards = 8;
constexpr float PiecePopupX = 150.0f;
constexpr float PiecePopupY = 92.0f;
constexpr float PiecePopupWidth = 500.0f;
constexpr float PiecePopupHeight = 416.0f;
constexpr float PiecePopupTextX = PiecePopupX + 24.0f;
constexpr float PiecePopupTextWidth = PiecePopupWidth - 48.0f;
constexpr float PiecePopupActionHeadingY = PiecePopupY + 186.0f;
constexpr float PiecePopupScrollY = PiecePopupActionHeadingY + 26.0f;
constexpr float PiecePopupScrollHeight = PiecePopupHeight - (PiecePopupScrollY - PiecePopupY) - 66.0f;
constexpr float PieceDoubleClickSeconds = 0.38f;
constexpr float DeckCardDoubleClickSeconds = 0.38f;
constexpr float GameDragStartDistanceSquared = 36.0f;
constexpr float LogicalWidth = 800.0f;
constexpr float LogicalHeight = 600.0f;
constexpr const char* DisplaySettingsFileName = "display.cfg";
constexpr const char* RememberTokenFileName = "remember_me.dat";

struct ServerEndpoint
{
    std::string host = DefaultServerHost;
    unsigned short port = 0;
};

struct ClientConfig
{
    ServerEndpoint account{DefaultServerHost, AccountServerPort};
    ServerEndpoint matchmaking{DefaultServerHost, MatchmakingServerPort};
    ServerEndpoint card{DefaultServerHost, CardServerPort};
    std::string gameServerHost = DefaultServerHost;
    std::string paymentServerUrl = DefaultPaymentServerUrl;
};

struct DisplaySettings
{
    bool fullscreen = true;
    unsigned int width = 0;
    unsigned int height = 0;
};

std::string cardRarity(const card_data::Card& card)
{
    const std::string rarity = game_data::cardStr(card, "rarity", "common");
    if (rarity == "rare" || rarity == "legendary")
    {
        return rarity;
    }
    return "common";
}

std::string cardRarityLabel(const card_data::Card& card)
{
    const std::string rarity = cardRarity(card);
    if (rarity == "legendary")
    {
        return "Legendary";
    }
    if (rarity == "rare")
    {
        return "Rare";
    }
    return "Common";
}

sf::Color cardRarityColor(const card_data::Card& card)
{
    const std::string rarity = cardRarity(card);
    if (rarity == "legendary")
    {
        return sf::Color(248, 214, 112);
    }
    if (rarity == "rare")
    {
        return sf::Color(151, 192, 255);
    }
    return sf::Color(190, 198, 214);
}

struct ServerResult
{
    bool success = false;
    std::string message;
    std::shared_ptr<sf::TcpSocket> gameSocket;
    std::string username;
    std::string accessToken;
    std::string rememberToken;
    bool rejectStoredCredential = false;
    bool cancelled = false;
};

struct MatchmakingCancelState
{
    std::atomic<bool> requested{false};
    std::atomic<bool> sent{false};
};

struct CardListResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
};

struct DeckListResult
{
    bool success = false;
    std::string message;
    std::vector<deck_data::Deck> decks;
};

struct AccountStateResult
{
    bool success = false;
    std::string message;
    int coins = 0;
    int rating = 0;
    bool isAdmin = false;
    std::vector<account_data::CollectionCard> collection;
};

struct DeckEditorLoadResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
    std::vector<deck_data::Deck> decks;
    int coins = 0;
    std::vector<account_data::CollectionCard> collection;
};

struct DeckCommandResult
{
    bool success = false;
    std::string message;
    std::string originalName;
    deck_data::Deck deck;
};

struct AccountCommandResult
{
    bool success = false;
    std::string message;
    int coins = 0;
    std::string cardTitle;
};

struct ShopLoadResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
    int coins = 0;
    std::vector<account_data::CollectionCard> collection;
};

struct AdminUsersLoadResult
{
    bool success = false;
    std::string message;
    std::uint32_t totalCount = 0;
    std::uint32_t page = 0;
    std::uint32_t pageSize = 0;
    std::vector<network::AdminUserSummary> users;
};

struct AdminUserPrivilegeResult
{
    bool success = false;
    std::string message;
    bool targetIsAdmin = false;
};

struct AdminUserGoldResult
{
    bool success = false;
    std::string message;
    std::string targetUsername;
    int targetGold = 0;
};

struct AdminUserDeleteResult
{
    bool success = false;
    std::string message;
    std::string targetUsername;
};

struct BoardCellMetrics
{
    std::array<sf::Vector2f, 4> corners{};
    sf::Vector2f center{};
    float height = 0.0f;
    float depthScale = 1.0f;
    int screenRow = 0;
};

std::filesystem::path executableDirectory;

std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (first >= last)
    {
        return "";
    }

    return std::string(first, last);
}

std::string lowerKey(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

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
            L"Steam Tactics remembered login",
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

const ClientConfig& clientConfig()
{
    static const ClientConfig config = loadClientConfig();
    return config;
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

std::string endpointText(const ServerEndpoint& endpoint)
{
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

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

void centerText(sf::Text& text, float x)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, text.getOrigin().y});
    text.setPosition({x, text.getPosition().y});
}

void setMessage(sf::Text& text, const std::string& message, const sf::Color& color)
{
    text.setString(message);
    text.setFillColor(color);
    centerText(text, 400.0f);
}

void setMessageY(sf::Text& text, float y)
{
    text.setPosition({text.getPosition().x, y});
    centerText(text, 400.0f);
}

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    sf::Text text(font, value, size);
    if (text.getLocalBounds().size.x <= maxWidth)
    {
        return value;
    }

    std::string display = value;
    while (!display.empty())
    {
        display.pop_back();
        text.setString(display + "...");
        if (text.getLocalBounds().size.x <= maxWidth)
        {
            return display + "...";
        }
    }

    return "...";
}

void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth = 0.0f)
{
    sf::Text text(font, maxWidth > 0.0f ? elideToWidth(font, value, size, maxWidth) : value, size);
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
}

std::vector<std::string> wrapText(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    std::vector<std::string> lines;
    sf::Text measuringText(font, "", size);
    std::string line;
    std::size_t position = 0;

    auto fits = [&](const std::string& text) {
        measuringText.setString(text);
        return measuringText.getLocalBounds().size.x <= maxWidth;
    };

    auto pushLine = [&]() {
        lines.push_back(line);
        line.clear();
    };

    while (position < value.size())
    {
        if (value[position] == '\n')
        {
            pushLine();
            ++position;
            continue;
        }

        while (position < value.size() && value[position] == ' ')
        {
            ++position;
        }
        if (position >= value.size())
        {
            break;
        }

        const std::size_t wordStart = position;
        while (position < value.size() && value[position] != ' ' && value[position] != '\n')
        {
            ++position;
        }

        const std::string word = value.substr(wordStart, position - wordStart);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (line.empty() || fits(candidate))
        {
            line = candidate;
        }
        else
        {
            pushLine();
            line = word;
        }
    }

    if (!line.empty() || lines.empty())
    {
        lines.push_back(line);
    }
    return lines;
}

float drawWrappedText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth,
    float lineGap = 4.0f)
{
    float y = position.y;
    for (const std::string& line : wrapText(font, value, size, maxWidth))
    {
        drawText(window, font, line, size, {position.x, y}, color);
        y += static_cast<float>(size) + lineGap;
    }
    return y;
}

void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size)
{
    sf::RectangleShape shadow(size);
    shadow.setPosition(position + sf::Vector2f(5.0f, 6.0f));
    shadow.setFillColor(sf::Color(0, 0, 0, 110));
    window.draw(shadow);

    sf::RectangleShape panel(size);
    panel.setPosition(position);
    panel.setFillColor(sf::Color(10, 21, 23, 238));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(158, 111, 56));
    window.draw(panel);

    sf::RectangleShape inner({size.x - 8.0f, size.y - 8.0f});
    inner.setPosition({position.x + 4.0f, position.y + 4.0f});
    inner.setFillColor(sf::Color::Transparent);
    inner.setOutlineThickness(1.0f);
    inner.setOutlineColor(sf::Color(50, 126, 116, 165));
    window.draw(inner);

    sf::RectangleShape topRule({size.x - 12.0f, 2.0f});
    topRule.setPosition({position.x + 6.0f, position.y + 6.0f});
    topRule.setFillColor(sf::Color(213, 157, 76, 85));
    window.draw(topRule);

    for (const sf::Vector2f offset : std::array<sf::Vector2f, 4>{
             sf::Vector2f{8.0f, 8.0f},
             sf::Vector2f{size.x - 8.0f, 8.0f},
             sf::Vector2f{8.0f, size.y - 8.0f},
             sf::Vector2f{size.x - 8.0f, size.y - 8.0f}})
    {
        sf::CircleShape rivet(2.0f);
        rivet.setOrigin({2.0f, 2.0f});
        rivet.setPosition(position + offset);
        rivet.setFillColor(sf::Color(186, 131, 61, 180));
        window.draw(rivet);
    }
}

void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected)
{
    sf::RectangleShape row(size);
    row.setPosition(position);
    row.setFillColor(selected ? sf::Color(42, 112, 103, 230) : sf::Color(28, 39, 42, 224));
    row.setOutlineThickness(1.0f);
    row.setOutlineColor(selected ? sf::Color(111, 226, 200) : sf::Color(102, 76, 46));
    window.draw(row);

    drawText(window, font, primary, 16, {position.x + 8.0f, position.y + 5.0f}, sf::Color(246, 238, 218), size.x - 16.0f);
    if (!secondary.empty())
    {
        drawText(window, font, secondary, 12, {position.x + 8.0f, position.y + 22.0f}, sf::Color(198, 180, 142), size.x - 16.0f);
    }
}

std::optional<std::size_t> rowIndexAt(
    sf::Vector2f mouse,
    float x,
    float y,
    float width,
    float rowHeight,
    std::size_t visibleRows,
    std::size_t offset,
    std::size_t totalRows)
{
    if (mouse.x < x || mouse.x > x + width || mouse.y < y)
    {
        return std::nullopt;
    }

    const std::size_t visibleIndex = static_cast<std::size_t>((mouse.y - y) / rowHeight);
    const std::size_t index = offset + visibleIndex;
    if (visibleIndex < visibleRows && index < totalRows)
    {
        return index;
    }

    return std::nullopt;
}

bool isInsideRect(sf::Vector2f mouse, float x, float y, float width, float height)
{
    return mouse.x >= x && mouse.x <= x + width && mouse.y >= y && mouse.y <= y + height;
}

void scrollList(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows, float delta)
{
    if (delta < 0.0f)
    {
        if (offset + visibleRows < totalRows)
        {
            ++offset;
        }
    }
    else if (offset > 0)
    {
        --offset;
    }
}

void clampListOffset(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows)
{
    if (totalRows <= visibleRows)
    {
        offset = 0;
        return;
    }

    offset = std::min(offset, totalRows - visibleRows);
}

void sendDisconnect(sf::TcpSocket& socket)
{
    sf::Packet disconnectPacket;
    disconnectPacket << static_cast<std::uint8_t>(network::MessageType::Disconnect);
    [[maybe_unused]] auto disconnectResult = socket.send(disconnectPacket);
    socket.disconnect();
}

// Resolves a saved deck's card titles into full card definitions from the
// library so the game server can read their stats.
std::vector<card_data::Card> resolveDeckCards(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library)
{
    std::vector<card_data::Card> resolved;
    resolved.reserve(deck.cardTitles.size());
    for (const std::string& title : deck.cardTitles)
    {
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        if (found != library.end())
        {
            resolved.push_back(*found);
        }
    }
    return resolved;
}

int collectionCopiesFor(const std::vector<account_data::CollectionCard>& collection, const std::string& title)
{
    const auto found = std::find_if(collection.begin(), collection.end(), [&](const account_data::CollectionCard& card) {
        return card.title == title;
    });
    return found == collection.end() ? 0 : found->copies;
}

std::vector<card_data::Card> ownedCardsFromCollection(
    const std::vector<card_data::Card>& library,
    const std::vector<account_data::CollectionCard>& collection)
{
    std::vector<card_data::Card> ownedCards;
    for (const card_data::Card& card : library)
    {
        if (collectionCopiesFor(collection, card.title) > 0)
        {
            ownedCards.push_back(card);
        }
    }
    return ownedCards;
}

int countHeroes(const std::vector<card_data::Card>& cards)
{
    return static_cast<int>(std::count_if(cards.begin(), cards.end(), [](const card_data::Card& card) {
        return game_data::isHeroCard(card);
    }));
}

void sendSubmitDeck(sf::TcpSocket& socket, const std::vector<card_data::Card>& cards)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(network::MessageType::SubmitDeck);
    packet << static_cast<std::uint32_t>(cards.size());
    for (const card_data::Card& card : cards)
    {
        card_data::writeCard(packet, card);
    }
    [[maybe_unused]] auto result = socket.send(packet);
}

ServerResult sendAccountRequest(
    network::MessageType requestType,
    network::MessageType expectedResponseType,
    const std::string& username,
    const std::string& password,
    bool rememberMe = false)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(requestType);
    packet << username;
    packet << password;
    if (requestType == network::MessageType::Login)
    {
        packet << rememberMe;
    }

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send to account server"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from account server"};
    }

    uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;

    if (static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        socket.disconnect();
        return {false, "Unexpected account server response"};
    }

    std::string authenticatedUsername;
    std::string accessToken;
    std::string rememberToken;
    if (requestType == network::MessageType::Login ||
        (requestType == network::MessageType::CreateAccount && success))
    {
        response >> authenticatedUsername >> accessToken >> rememberToken;
        if (!response)
        {
            socket.disconnect();
            return {false, "Invalid account server response"};
        }
    }

    sendDisconnect(socket);
    ServerResult result;
    result.success = success;
    result.message = std::move(message);
    result.username = std::move(authenticatedUsername);
    result.accessToken = std::move(accessToken);
    result.rememberToken = std::move(rememberToken);
    return result;
}

ServerResult sendRememberLogin(const std::string& token)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Could not restore saved login; account server is unavailable"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::RememberLogin) << token;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Could not restore saved login"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Could not restore saved login"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::string username;
    std::string accessToken;
    std::string replacementToken;
    response >> responseType >> success >> message >> username >> accessToken >> replacementToken;
    socket.disconnect();
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::RememberLoginResponse)
    {
        return {false, "Unexpected account server response"};
    }

    ServerResult result;
    result.success = success;
    result.message = std::move(message);
    result.username = std::move(username);
    result.accessToken = std::move(accessToken);
    result.rememberToken = std::move(replacementToken);
    result.rejectStoredCredential = !success;
    return result;
}

void revokeLoginTokens(const std::string& rememberToken, const std::string& accessToken)
{
    if (rememberToken.empty() && accessToken.empty())
    {
        return;
    }

    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::RevokeRememberToken)
            << rememberToken << accessToken;
    if (socket.send(request) == sf::Socket::Status::Done)
    {
        sf::Packet response;
        [[maybe_unused]] auto status = socket.receive(response);
    }
    socket.disconnect();
}

CardListResult fetchCards()
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().card))
    {
        return {false, "Failed to connect to card server " + endpointText(clientConfig().card)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send card list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::CardListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected card list response"};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        card_data::Card card;
        if (!card_data::readCard(response, card))
        {
            socket.disconnect();
            return {false, "Invalid card list payload"};
        }
        cards.push_back(card);
    }

    socket.disconnect();
    return {success, message, cards};
}

DeckListResult fetchDecks(const std::string& accessToken)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckListRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No deck list response from account server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::DeckListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected deck list response"};
    }

    std::vector<deck_data::Deck> decks;
    decks.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        deck_data::Deck deck;
        if (!deck_data::readDeck(response, deck))
        {
            socket.disconnect();
            return {false, "Invalid deck list payload"};
        }
        decks.push_back(deck);
    }

    sendDisconnect(socket);
    return {success, message, decks};
}

AccountStateResult fetchAccountState(const std::string& accessToken)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AccountStateRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send account state request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No account state response from account server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    account_data::AccountState accountState;
    response >> responseType >> success >> message;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::AccountStateResponse)
    {
        socket.disconnect();
        return {false, "Unexpected account state response"};
    }

    if (!account_data::readAccountState(response, accountState))
    {
        socket.disconnect();
        return {false, "Invalid account state payload"};
    }

    sendDisconnect(socket);
    return {
        success,
        message,
        accountState.coins,
        accountState.rating,
        accountState.isAdmin,
        std::move(accountState.collection)};
}

DeckCommandResult readDeckCommandResponse(
    sf::TcpSocket& socket,
    network::MessageType expectedResponseType,
    const std::string& fallbackMessage,
    const std::string& originalName,
    const deck_data::Deck& deck)
{
    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, fallbackMessage, originalName, deck};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response || static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        return {false, "Unexpected deck command response", originalName, deck};
    }

    return {success, message, originalName, deck};
}

DeckCommandResult saveDeckToAccount(
    const std::string& accessToken,
    const std::string& originalName,
    const deck_data::Deck& deck)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), originalName, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckSaveRequest);
    request << accessToken << originalName;
    deck_data::writeDeck(request, deck);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck save request", originalName, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::DeckSaveResponse,
        "No deck save response from account server",
        originalName,
        deck);
    sendDisconnect(socket);
    return result;
}

DeckCommandResult deleteDeckFromAccount(const std::string& accessToken, const std::string& deckName)
{
    deck_data::Deck deck;
    deck.name = deckName;

    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), deckName, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckDeleteRequest);
    request << accessToken << deckName;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck delete request", deckName, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::DeckDeleteResponse,
        "No deck delete response from account server",
        deckName,
        deck);
    sendDisconnect(socket);
    return result;
}

AccountCommandResult sendCoinCommand(
    network::MessageType requestType,
    network::MessageType expectedResponseType,
    const std::string& accessToken)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(requestType);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send account command"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No account command response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    int coins = 0;
    response >> responseType >> success >> message >> coins;
    if (!response || static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        socket.disconnect();
        return {false, "Unexpected account command response"};
    }

    sendDisconnect(socket);
    return {success, message, coins};
}

AccountCommandResult purchaseRandomCard(const std::string& accessToken)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::ShopPurchaseRequest);
    request << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send shop purchase request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No shop purchase response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    int coins = 0;
    std::string cardTitle;
    response >> responseType >> success >> message >> coins >> cardTitle;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::ShopPurchaseResponse)
    {
        socket.disconnect();
        return {false, "Unexpected shop purchase response"};
    }

    sendDisconnect(socket);
    return {success, message, coins, cardTitle};
}

AccountCommandResult changePassword(
    const std::string& accessToken,
    const std::string& currentPassword,
    const std::string& newPassword)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::ChangePasswordRequest)
            << accessToken << currentPassword << newPassword;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send password change request"};
    }

    socket.setBlocking(false);
    sf::Packet response;
    const auto responseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    sf::Socket::Status receiveStatus = sf::Socket::Status::NotReady;
    while (std::chrono::steady_clock::now() < responseDeadline)
    {
        receiveStatus = socket.receive(response);
        if (receiveStatus != sf::Socket::Status::NotReady)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (receiveStatus != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {
            false,
            receiveStatus == sf::Socket::Status::NotReady
                ? "Password change timed out; restart the account server with the latest build"
                : "No password change response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::ChangePasswordResponse)
    {
        socket.disconnect();
        return {false, "Unexpected password change response"};
    }

    sendDisconnect(socket);
    return {success, message};
}

AdminUsersLoadResult loadAdminUsers(
    const std::string& accessToken,
    const std::string& search,
    std::uint32_t page,
    std::uint32_t pageSize)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserListRequest);
    request << accessToken << search << page << pageSize;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send admin user list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No admin user list response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t totalCount = 0;
    std::uint32_t responsePage = 0;
    std::uint32_t responsePageSize = 0;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> totalCount >> responsePage >> responsePageSize >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected admin user list response"};
    }

    std::vector<network::AdminUserSummary> users;
    users.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        network::AdminUserSummary user;
        response >> user.username >> user.isAdmin >> user.gold;
        if (!response)
        {
            socket.disconnect();
            return {false, "Invalid admin user list payload"};
        }
        users.push_back(std::move(user));
    }

    sendDisconnect(socket);
    return {success, message, totalCount, responsePage, responsePageSize, std::move(users)};
}

AdminUserPrivilegeResult updateAdminUserPrivilege(
    const std::string& accessToken,
    const std::string& targetUsername,
    bool makeAdmin)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserPrivilegeRequest);
    request << accessToken << targetUsername << makeAdmin;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send admin privilege request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No admin privilege response"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    bool targetIsAdmin = false;
    response >> responseType >> success >> message >> targetIsAdmin;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserPrivilegeResponse)
    {
        socket.disconnect();
        return {false, "Unexpected admin privilege response"};
    }

    sendDisconnect(socket);
    return {success, message, targetIsAdmin};
}

AdminUserGoldResult updateAdminUserGold(
    const std::string& accessToken,
    const std::string& targetUsername,
    int amount)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), targetUsername};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserGoldRequest);
    request << accessToken << targetUsername << amount;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send admin gold request", targetUsername};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No admin gold response", targetUsername};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    int targetGold = 0;
    response >> responseType >> success >> message >> targetGold;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserGoldResponse)
    {
        socket.disconnect();
        return {false, "Unexpected admin gold response", targetUsername};
    }

    sendDisconnect(socket);
    return {success, message, targetUsername, targetGold};
}

AdminUserDeleteResult deleteAdminUser(
    const std::string& accessToken,
    const std::string& targetUsername)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), targetUsername};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::AdminUserDeleteRequest);
    request << accessToken << targetUsername;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send delete user request", targetUsername};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No delete user response", targetUsername};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) != network::MessageType::AdminUserDeleteResponse)
    {
        socket.disconnect();
        return {false, "Unexpected delete user response", targetUsername};
    }

    sendDisconnect(socket);
    return {success, message, targetUsername};
}

DeckEditorLoadResult loadDeckEditorData(const std::string& accessToken)
{
    CardListResult cardResult = fetchCards();
    if (!cardResult.success)
    {
        return {false, cardResult.message};
    }

    AccountStateResult accountResult = fetchAccountState(accessToken);
    if (!accountResult.success)
    {
        return {false, accountResult.message};
    }

    DeckListResult deckResult = fetchDecks(accessToken);
    if (!deckResult.success)
    {
        return {
            false,
            deckResult.message,
            ownedCardsFromCollection(cardResult.cards, accountResult.collection),
            {},
            accountResult.coins,
            std::move(accountResult.collection)};
    }

    std::vector<card_data::Card> ownedCards = ownedCardsFromCollection(cardResult.cards, accountResult.collection);
    const std::string message =
        "Loaded " + std::to_string(ownedCards.size()) + " owned cards and " +
        std::to_string(deckResult.decks.size()) + " decks";
    return {
        true,
        message,
        std::move(ownedCards),
        std::move(deckResult.decks),
        accountResult.coins,
        std::move(accountResult.collection)};
}

ShopLoadResult loadShopData(const std::string& accessToken)
{
    CardListResult cardResult = fetchCards();
    if (!cardResult.success)
    {
        return {false, cardResult.message};
    }

    AccountStateResult accountResult = fetchAccountState(accessToken);
    if (!accountResult.success)
    {
        return {false, accountResult.message, std::move(cardResult.cards)};
    }

    return {
        true,
        "Shop loaded",
        std::move(cardResult.cards),
        accountResult.coins,
        std::move(accountResult.collection)};
}

ServerResult joinGameServer(
    int matchId,
    int playerNumber,
    unsigned short gamePort,
    const std::string& accessToken)
{
    if (gamePort == 0)
    {
        return {false, "Game server did not assign a game"};
    }

    auto socket = std::make_shared<sf::TcpSocket>();
    bool connected = false;
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        if (connectToHostPort(*socket, clientConfig().gameServerHost, gamePort))
        {
            connected = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!connected)
    {
        socket->disconnect();
        return {false, "Failed to connect to game server"};
    }

    sf::Packet joinRequest;
    joinRequest << static_cast<uint8_t>(network::MessageType::JoinGame);
    joinRequest << matchId;
    joinRequest << playerNumber;
    joinRequest << accessToken;

    if (socket->send(joinRequest) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "Failed to join game"};
    }

    sf::Packet response;
    if (socket->receive(response) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "No game server response"};
    }

    uint8_t responseType = 0;
    int responseMatchId = 0;
    int responsePlayerNumber = 0;
    std::string message;
    response >> responseType >> responseMatchId >> responsePlayerNumber >> message;
    if (static_cast<network::MessageType>(responseType) != network::MessageType::GameReady ||
        responseMatchId != matchId ||
        responsePlayerNumber != playerNumber)
    {
        socket->disconnect();
        return {false, "Unexpected game server response"};
    }

    return {true, message, socket};
}

ServerResult joinMatchmaking(
    const std::string& accessToken,
    std::shared_ptr<MatchmakingCancelState> cancelState)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().matchmaking))
    {
        return {false, "Failed to connect to matchmaking " + endpointText(clientConfig().matchmaking)};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(network::MessageType::JoinMatchmaking);
    packet << accessToken;

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to join matchmaking"};
    }

    socket.setBlocking(false);
    sf::Packet cancelPacket;
    cancelPacket << static_cast<uint8_t>(network::MessageType::CancelMatchmaking);

    while (true)
    {
        if (cancelState &&
            cancelState->requested.load() &&
            !cancelState->sent.load())
        {
            const sf::Socket::Status cancelStatus = socket.send(cancelPacket);
            if (cancelStatus == sf::Socket::Status::Done)
            {
                cancelState->sent.store(true);
            }
            else if (cancelStatus != sf::Socket::Status::NotReady &&
                     cancelStatus != sf::Socket::Status::Partial)
            {
                socket.disconnect();
                return {false, "Failed to cancel matchmaking"};
            }
        }

        sf::Packet response;
        const sf::Socket::Status receiveStatus = socket.receive(response);
        if (receiveStatus == sf::Socket::Status::NotReady)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }

        if (receiveStatus != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Disconnected from matchmaking"};
        }

        uint8_t responseType = 0;
        response >> responseType;
        const auto messageType = static_cast<network::MessageType>(responseType);
        if (messageType == network::MessageType::CancelMatchmakingResponse)
        {
            bool cancelled = false;
            std::string message;
            response >> cancelled >> message;
            socket.disconnect();
            if (!response || !cancelled)
            {
                return {false, message.empty() ? "Matchmaking cancel was rejected" : message};
            }

            ServerResult result;
            result.message = message.empty() ? "Matchmaking cancelled." : message;
            result.cancelled = true;
            return result;
        }

        if (messageType != network::MessageType::MatchFound)
        {
            socket.disconnect();
            return {false, "Unexpected matchmaking response"};
        }

        int matchId = 0;
        int playerNumber = 0;
        unsigned short gamePort = 0;
        response >> matchId >> playerNumber >> gamePort;
        socket.disconnect();
        return joinGameServer(matchId, playerNumber, gamePort, accessToken);
    }
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

    const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    const std::vector<sf::VideoMode>& fullscreenModes = sf::VideoMode::getFullscreenModes();
    std::vector<sf::Vector2u> displayResolutions;
    auto addResolution = [&](sf::Vector2u size) {
        if (size.x < 1024u ||
            size.y < 720u ||
            size.x > desktopMode.size.x ||
            size.y > desktopMode.size.y)
        {
            return;
        }
        if (std::find(displayResolutions.begin(), displayResolutions.end(), size) == displayResolutions.end())
        {
            displayResolutions.push_back(size);
        }
    };

    addResolution({1024, 768});
    addResolution({1280, 720});
    addResolution({1280, 800});
    addResolution({1366, 768});
    addResolution({1600, 900});
    addResolution({1920, 1080});
    addResolution({2560, 1440});
    addResolution(desktopMode.size);
    for (const sf::VideoMode& mode : fullscreenModes)
    {
        addResolution(mode.size);
    }
    std::sort(displayResolutions.begin(), displayResolutions.end(), [](sf::Vector2u left, sf::Vector2u right) {
        const std::uint64_t leftPixels = static_cast<std::uint64_t>(left.x) * left.y;
        const std::uint64_t rightPixels = static_cast<std::uint64_t>(right.x) * right.y;
        return leftPixels == rightPixels ? left.x < right.x : leftPixels < rightPixels;
    });

    DisplaySettings displaySettings = loadDisplaySettings();
    if (displaySettings.width == 0 || displaySettings.height == 0)
    {
        displaySettings.width = desktopMode.size.x;
        displaySettings.height = desktopMode.size.y;
    }
    sf::Vector2u configuredSize{displaySettings.width, displaySettings.height};
    if (std::find(displayResolutions.begin(), displayResolutions.end(), configuredSize) == displayResolutions.end())
    {
        configuredSize = desktopMode.size;
        displaySettings.width = configuredSize.x;
        displaySettings.height = configuredSize.y;
    }

    sf::RenderWindow window;
    auto applyLogicalView = [&]() {
        const sf::Vector2u windowSize = window.getSize();
        if (windowSize.x == 0 || windowSize.y == 0)
        {
            return;
        }

        const float windowAspect = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
        const float logicalAspect = LogicalWidth / LogicalHeight;
        sf::FloatRect viewport({0.0f, 0.0f}, {1.0f, 1.0f});
        if (windowAspect > logicalAspect)
        {
            viewport.size.x = logicalAspect / windowAspect;
            viewport.position.x = (1.0f - viewport.size.x) * 0.5f;
        }
        else if (windowAspect < logicalAspect)
        {
            viewport.size.y = windowAspect / logicalAspect;
            viewport.position.y = (1.0f - viewport.size.y) * 0.5f;
        }

        sf::View view(sf::FloatRect({0.0f, 0.0f}, {LogicalWidth, LogicalHeight}));
        view.setViewport(viewport);
        window.setView(view);
    };

    auto createDisplayWindow = [&](DisplaySettings& settings) {
        sf::VideoMode mode({settings.width, settings.height});
        if (settings.fullscreen)
        {
            const auto matchingMode = std::find_if(fullscreenModes.begin(), fullscreenModes.end(), [&](const sf::VideoMode& candidate) {
                return candidate.size == mode.size;
            });
            mode = matchingMode != fullscreenModes.end() ? *matchingMode : desktopMode;
            settings.width = mode.size.x;
            settings.height = mode.size.y;
            window.create(mode, "Steam Tactics", sf::State::Fullscreen);
        }
        else
        {
            window.create(mode, "Steam Tactics", sf::Style::Titlebar | sf::Style::Close, sf::State::Windowed);
        }
        window.setFramerateLimit(60);
        applyLogicalView();
    };

    createDisplayWindow(displaySettings);

    sf::Font font;
    const std::optional<std::filesystem::path> fontPath = resolveAssetPath("Roboto.ttf");
    if (!fontPath || !font.openFromFile(*fontPath))
    {
        return 1;
    }

    std::unordered_map<std::string, std::shared_ptr<sf::Texture>> textureCache;
    auto loadTexture = [&](const std::string& assetPath) -> sf::Texture* {
        const std::string key = assetRelativePath(assetPath);
        if (key.empty())
        {
            return nullptr;
        }

        if (const auto found = textureCache.find(key); found != textureCache.end())
        {
            return found->second.get();
        }

        const std::optional<std::filesystem::path> resolvedPath = resolveAssetPath(key);
        auto texture = std::make_shared<sf::Texture>();
        if (!resolvedPath || !texture->loadFromFile(*resolvedPath))
        {
            return nullptr;
        }

        texture->setSmooth(true);
        sf::Texture* loaded = texture.get();
        textureCache.emplace(key, std::move(texture));
        return loaded;
    };

    sf::Texture* backdropTexture = loadTexture("ui/steampunk-bayou-backdrop.png");

    auto drawCoverSprite = [&](sf::Texture& texture, sf::FloatRect target, sf::Color color = sf::Color::White) {
        sf::Sprite sprite(texture);
        const sf::Vector2u imageSize = texture.getSize();
        const float scale = std::max(target.size.x / static_cast<float>(imageSize.x),
                                     target.size.y / static_cast<float>(imageSize.y));
        sprite.setScale({scale, scale});
        sprite.setColor(color);
        sprite.setPosition({
            target.position.x + (target.size.x - static_cast<float>(imageSize.x) * scale) * 0.5f,
            target.position.y + (target.size.y - static_cast<float>(imageSize.y) * scale) * 0.5f});
        window.draw(sprite);
    };

    auto drawContainSprite = [&](sf::Texture& texture,
                                 sf::FloatRect target,
                                 sf::Color color = sf::Color::White,
                                 bool flipX = false) {
        sf::Sprite sprite(texture);
        const sf::Vector2u imageSize = texture.getSize();
        const float sourceWidth = static_cast<float>(imageSize.x);
        const float sourceHeight = static_cast<float>(imageSize.y);
        const float scale = std::min(target.size.x / static_cast<float>(imageSize.x),
                                     target.size.y / static_cast<float>(imageSize.y));
        sprite.setScale({flipX ? -scale : scale, scale});
        sprite.setColor(color);
        sprite.setPosition({
            target.position.x + (target.size.x + (flipX ? sourceWidth * scale : -sourceWidth * scale)) * 0.5f,
            target.position.y + (target.size.y - sourceHeight * scale) * 0.5f});
        window.draw(sprite);
    };

    auto drawTextureRectContain = [&](sf::Texture& texture,
                                      sf::IntRect textureRect,
                                      sf::FloatRect target,
                                      sf::Color color = sf::Color::White,
                                      bool flipX = false) {
        sf::Sprite sprite(texture);
        sprite.setTextureRect(textureRect);
        const float sourceWidth = static_cast<float>(textureRect.size.x);
        const float sourceHeight = static_cast<float>(textureRect.size.y);
        const float scale = std::min(target.size.x / sourceWidth, target.size.y / sourceHeight);
        sprite.setScale({flipX ? -scale : scale, scale});
        sprite.setColor(color);
        sprite.setPosition({
            target.position.x + (target.size.x + (flipX ? sourceWidth * scale : -sourceWidth * scale)) * 0.5f,
            target.position.y + (target.size.y - sourceHeight * scale) * 0.5f});
        window.draw(sprite);
    };

    auto drawBackdrop = [&]() {
        if (backdropTexture)
        {
            drawCoverSprite(*backdropTexture, {{0.0f, 0.0f}, {800.0f, 600.0f}});
        }
        else
        {
            window.clear(sf::Color(9, 17, 19));
        }

        sf::RectangleShape wash({800.0f, 600.0f});
        wash.setFillColor(sf::Color(5, 12, 15, 118));
        window.draw(wash);

        sf::RectangleShape topShade({800.0f, 124.0f});
        topShade.setFillColor(sf::Color(4, 9, 11, 156));
        window.draw(topShade);
    };

    sf::Text title(font, "Steam Tactics", 48);
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
    InputBox deckNameInput({304.0f, 154.0f}, {222.0f, 40.0f}, "Deck Name", font);
    InputBox adminSearchInput({120.0f, 94.0f}, {520.0f, 36.0f}, "", font);
    InputBox adminGoldInput({234.0f, 460.0f}, {130.0f, 36.0f}, "Gold amount", font);

    Button rememberMeButton({300.0f, 280.0f}, {200.0f, 42.0f}, "Remember Me: Off", font);
    Button passwordVisibilityButton({520.0f, 220.0f}, {180.0f, 40.0f}, "Show Password", font);
    Button loginSubmitButton({300.0f, 342.0f}, {200.0f, 50.0f}, "Login", font);
    Button createSubmitButton({300.0f, 380.0f}, {200.0f, 50.0f}, "Create Account", font);
    Button backButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Back", font);
    Button exitDesktopButton({20.0f, 520.0f}, {200.0f, 45.0f}, "Exit to Desktop", font);
    Button cancelMatchmakingButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Cancel", font);
    Button playButton({300.0f, 136.0f}, {200.0f, 40.0f}, "Play", font);
    Button sandboxButton({300.0f, 184.0f}, {200.0f, 40.0f}, "Sandbox", font);
    Button deckEditorButton({300.0f, 232.0f}, {200.0f, 40.0f}, "Deck Editor", font);
    Button shopButton({300.0f, 280.0f}, {200.0f, 40.0f}, "Shop", font);
    Button adminCardEditorButton({300.0f, 328.0f}, {200.0f, 40.0f}, "Card Editor", font);
    Button adminUsersButton({300.0f, 376.0f}, {200.0f, 40.0f}, "Admin", font);
    Button authenticatedOptionsButton({300.0f, 424.0f}, {200.0f, 40.0f}, "Options", font);
    Button logoutButton({300.0f, 472.0f}, {200.0f, 40.0f}, "Log Out", font);

    Button displayModeButton({270.0f, 178.0f}, {260.0f, 54.0f}, "", font);
    Button previousResolutionButton({210.0f, 276.0f}, {64.0f, 54.0f}, "<", font);
    Button resolutionButton({290.0f, 276.0f}, {220.0f, 54.0f}, "", font);
    Button nextResolutionButton({526.0f, 276.0f}, {64.0f, 54.0f}, ">", font);
    Button applyOptionsButton({300.0f, 350.0f}, {200.0f, 54.0f}, "Apply", font);
    Button changePasswordOptionButton({300.0f, 414.0f}, {200.0f, 54.0f}, "Change Password", font);
    Button optionsBackButton({300.0f, 478.0f}, {200.0f, 54.0f}, "Back", font);
    Button changePasswordVisibilityButton({520.0f, 230.0f}, {180.0f, 40.0f}, "Show Passwords", font);
    Button changePasswordSubmitButton({300.0f, 390.0f}, {200.0f, 50.0f}, "Change Password", font);
    Button changePasswordBackButton({300.0f, 470.0f}, {200.0f, 50.0f}, "Back", font);
    Button dismissPasswordChangedButton({320.0f, 344.0f}, {160.0f, 46.0f}, "OK", font);

    Button deckBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button newDeckButton({34.0f, 140.0f}, {102.0f, 38.0f}, "New", font);
    Button refreshDeckButton({146.0f, 140.0f}, {110.0f, 38.0f}, "Refresh", font);
    Button deleteDeckButton({34.0f, 508.0f}, {110.0f, 38.0f}, "Delete", font);
    Button removeCardButton({304.0f, 508.0f}, {110.0f, 38.0f}, "Remove", font);
    Button addCardButton({574.0f, 508.0f}, {88.0f, 38.0f}, "Add", font);
    Button saveDeckButton({668.0f, 508.0f}, {108.0f, 38.0f}, "Save", font);
    Button shopBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button buyCoinPackButton({94.0f, 492.0f}, {180.0f, 46.0f}, "Buy " + std::to_string(CoinPackCoins) + " Coins", font);
    Button refreshShopButton({310.0f, 492.0f}, {180.0f, 46.0f}, "Refresh", font);
    Button buyCardButton(
        {EnableCoinPurchases ? 526.0f : 300.0f, 492.0f},
        {EnableCoinPurchases ? 180.0f : 200.0f, 46.0f},
        "Buy Card",
        font);
    Button dismissRevealedCardButton({300.0f, 492.0f}, {200.0f, 46.0f}, "Dismiss", font);
    Button adminBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button adminPrevPageButton({270.0f, 516.0f}, {52.0f, 38.0f}, "<", font);
    Button adminNextPageButton({478.0f, 516.0f}, {52.0f, 38.0f}, ">", font);
    Button adminRefreshButton({332.0f, 516.0f}, {104.0f, 38.0f}, "Refresh", font);
    Button adminGrantButton({42.0f, 458.0f}, {150.0f, 42.0f}, "Grant Admin", font);
    Button adminRevokeButton({42.0f, 458.0f}, {150.0f, 42.0f}, "Revoke Admin", font);
    Button adminGrantGoldButton({378.0f, 458.0f}, {150.0f, 42.0f}, "Grant Gold", font);
    Button adminRemoveGoldButton({542.0f, 458.0f}, {150.0f, 42.0f}, "Remove Gold", font);
    Button adminDeleteButton({600.0f, 514.0f}, {176.0f, 40.0f}, "Delete User", font);
    Button cancelDeleteUserButton({250.0f, 366.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmDeleteUserButton({420.0f, 366.0f}, {130.0f, 42.0f}, "Delete", font);
    Button cancelExitDesktopButton({250.0f, 356.0f}, {130.0f, 42.0f}, "Cancel", font);
    Button confirmExitDesktopButton({420.0f, 356.0f}, {130.0f, 42.0f}, "Exit", font);
    Button closeDeckCardPopupButton({PiecePopupX + 190.0f, PiecePopupY + PiecePopupHeight - 54.0f}, {120.0f, 38.0f}, "Close", font);

    sf::Text messageText(font, "", 20);
    messageText.setFillColor(sf::Color::Red);
    messageText.setPosition({400.0f, 450.0f});
    CardEditorScreen cardEditorScreen(
        font,
        {clientConfig().card.host, clientConfig().card.port},
        fontPath->parent_path());

    sf::Clock clock;
    float animationTime = 0.0f;
    GameState currentState = GameState::Menu;
    GameState optionsReturnState = GameState::Menu;
    DisplaySettings pendingDisplaySettings = displaySettings;
    configuredSize = {displaySettings.width, displaySettings.height};
    std::size_t selectedResolution = static_cast<std::size_t>(std::distance(
        displayResolutions.begin(),
        std::find(displayResolutions.begin(), displayResolutions.end(), configuredSize)));
    std::optional<std::future<ServerResult>> pendingRequest;
    std::optional<std::future<ServerResult>> pendingMatchmaking;
    std::optional<std::future<CardListResult>> pendingSandboxLoad;
    std::shared_ptr<MatchmakingCancelState> activeMatchmakingCancel;
    bool matchmakingCancelRequested = false;
    std::optional<std::future<void>> pendingLogout;
    std::optional<std::future<DeckEditorLoadResult>> pendingDeckEditorLoad;
    std::optional<std::future<DeckCommandResult>> pendingDeckSave;
    std::optional<std::future<DeckCommandResult>> pendingDeckDelete;
    std::optional<std::future<AccountStateResult>> pendingAccountState;
    std::optional<std::future<ShopLoadResult>> pendingShopLoad;
    std::optional<std::future<AccountCommandResult>> pendingShopPurchase;
    std::optional<std::future<AdminUsersLoadResult>> pendingAdminUsersLoad;
    std::optional<std::future<AdminUserPrivilegeResult>> pendingAdminPrivilege;
    std::optional<std::future<AdminUserGoldResult>> pendingAdminGold;
    std::optional<std::future<AdminUserDeleteResult>> pendingAdminUserDelete;
    std::optional<std::future<AccountCommandResult>> pendingPasswordChange;
    bool coinPurchasePolling = false;
    int coinPurchaseStartingCoins = 0;
    float nextCoinPurchasePollAt = 0.0f;
    float coinPurchasePollDeadline = 0.0f;
    std::shared_ptr<sf::TcpSocket> activeGameSocket;
    std::string loggedInUsername;
    std::string activeAccessToken;
    std::string activeRememberToken;
    bool rememberMeChecked = false;
    bool passwordVisible = false;
    bool changePasswordsVisible = false;
    bool passwordChangedPopupVisible = false;
    bool exitDesktopPopupVisible = false;
    bool exitDesktopCloseHovered = false;
    bool pendingAutoLogin = false;
    bool pendingRememberRequested = false;
    std::vector<card_data::Card> cardLibrary;
    std::vector<card_data::Card> allCardLibrary;
    std::vector<deck_data::Deck> playerDecks;
    std::vector<account_data::CollectionCard> playerCollection;
    std::vector<network::AdminUserSummary> adminUsers;
    deck_data::Deck editingDeck;
    std::string activeDeckOriginalName;
    int playerCoins = 0;
    int playerRating = 0;
    bool loggedInIsAdmin = false;
    std::string adminSearchQuery;
    std::uint32_t adminUsersPage = 0;
    std::uint32_t adminUsersPageSize = AdminUsersPageSize;
    std::uint32_t adminUsersTotalCount = 0;
    std::optional<std::size_t> selectedAdminUser;
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
    int gameRatingChange = 0;
    std::string gameRewardText;
    std::optional<std::size_t> draggingLibraryCard;
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
    bool haveSnapshot = false;
    bool sandboxMode = false;
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

    Button findMatchButton({300.0f, 458.0f}, {200.0f, 52.0f}, "Find Match", font);
    Button abilityButton({392.0f, GameActionButtonY}, {138.0f, 36.0f}, "Use Ability", font);
    Button endTurnButton({540.0f, GameActionButtonY}, {132.0f, 36.0f}, "Pass Turn", font);
    Button sandboxPlayerButton({540.0f, GameActionButtonY}, {132.0f, 36.0f}, "Place P1", font);
    Button leaveGameButton({684.0f, 14.0f}, {100.0f, 36.0f}, "Leave", font);
    Button closePiecePopupButton({PiecePopupX + 358.0f, PiecePopupY + PiecePopupHeight - 54.0f}, {120.0f, 38.0f}, "Close", font);
    Button discardCardButton({PiecePopupX + 22.0f, PiecePopupY + PiecePopupHeight - 54.0f}, {220.0f, 38.0f},
                             "Discard (+" + std::to_string(game_data::DiscardSteamGain) + " steam)", font);

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

    auto layoutAuthenticatedButtons = [&]() {
        float y = 140.0f;
        constexpr float x = 300.0f;
        constexpr float gap = 10.0f;
        constexpr float height = 40.0f;

        auto place = [&](Button& button) {
            button.setPosition({x, y});
            y += height + gap;
        };

        place(playButton);
        place(sandboxButton);
        place(deckEditorButton);
        place(shopButton);
        if (loggedInIsAdmin)
        {
            place(adminCardEditorButton);
            place(adminUsersButton);
        }
        place(authenticatedOptionsButton);
        place(logoutButton);
    };

    auto drawCoinIcon = [&](sf::Vector2f position, float radius) {
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

    auto drawExitDesktopCloseButton = [&]() {
        const sf::Vector2f position{724.0f, 18.0f};
        const sf::Vector2f size{52.0f, 52.0f};

        sf::RectangleShape shadow(size);
        shadow.setPosition(position + sf::Vector2f(3.0f, 4.0f));
        shadow.setFillColor(sf::Color(0, 0, 0, 110));
        window.draw(shadow);

        sf::RectangleShape button(size);
        button.setPosition(position);
        button.setFillColor(exitDesktopCloseHovered ? sf::Color(205, 35, 35, 248) : sf::Color(152, 22, 28, 244));
        button.setOutlineThickness(2.0f);
        button.setOutlineColor(exitDesktopCloseHovered ? sf::Color(255, 178, 178) : sf::Color(255, 92, 92));
        window.draw(button);

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

    auto applyAccountState = [&](const AccountStateResult& result) {
        playerCoins = result.coins;
        playerRating = result.rating;
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
        return collectionCopiesFor(playerCollection, title);
    };

    auto deckCopies = [&](const std::string& title) {
        return static_cast<int>(std::count(editingDeck.cardTitles.begin(), editingDeck.cardTitles.end(), title));
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
        allCardLibrary.clear();
        playerDecks.clear();
        playerCollection.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        playerCoins = 0;
        playerRating = 0;
        loggedInIsAdmin = false;
        adminUsers.clear();
        adminSearchQuery.clear();
        adminUsersPage = 0;
        adminUsersTotalCount = 0;
        selectedAdminUser.reset();
        deleteUserPopupVisible = false;
        exitDesktopPopupVisible = false;
        adminUserDeleteTarget.clear();
        adminSearchInput.clear();
        adminGoldInput.clear();
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
        gameRatingChange = 0;
        gameRewardText.clear();
        draggingLibraryCard.reset();
        dragActive = false;
        inspectedPieceId.reset();
        inspectedHandIndex.reset();
        lastClickedPieceId.reset();
        pendingHandClickIndex.reset();
        inspectedPieceScroll = 0.0f;
        sandboxMode = false;
        sandboxPlacementPlayer = 1;
        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        gameDragKind = GameDragKind::None;
        draggingHandIndex.reset();
        draggingPieceId.reset();
        gameDragActive = false;
        title.setString("Steam Tactics");
        centerText(title, 400.0f);
        setMessageY(messageText, 450.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        deckNameInput.clear();
        draggingLibraryCard.reset();
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
        title.setString("");
        centerText(title, 400.0f);
        clearFocus();
        adminSearchInput.setContent(adminSearchQuery);
        adminSearchInput.setActive(true);
        adminUsers.clear();
        selectedAdminUser.reset();
        deleteUserPopupVisible = false;
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
    };

    auto searchAdminUsers = [&]() {
        adminSearchQuery = trim(adminSearchInput.getContent());
        adminUsersPage = 0;
        loadAdminUsersScreen();
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
    };

    auto showOptionsScreen = [&](GameState returnState) {
        optionsReturnState = returnState;
        currentState = GameState::Options;
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
        title.setString(optionsReturnState == GameState::Authenticated ? "" : "Steam Tactics");
        centerText(title, 400.0f);
        setMessageY(messageText, optionsReturnState == GameState::Authenticated ? 500.0f : 450.0f);
        setMessage(messageText, "", sf::Color::White);
    };

    auto updateChangePasswordVisibility = [&]() {
        currentPasswordInput.setPasswordMode(!changePasswordsVisible);
        newPasswordInput.setPasswordMode(!changePasswordsVisible);
        confirmNewPasswordInput.setPasswordMode(!changePasswordsVisible);
        changePasswordVisibilityButton.setLabel(
            changePasswordsVisible ? "Hide Passwords" : "Show Passwords");
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

    auto showGameScreen = [&](std::shared_ptr<sf::TcpSocket> gameSocket) {
        activeGameSocket = std::move(gameSocket);
        currentState = GameState::Game;
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();

        haveSnapshot = false;
        sandboxMode = false;
        sandboxPlacementPlayer = 1;
        gameHandOffset = 0;
        nextSandboxPieceId = 1;
        gameSnapshot = {};
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
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();

        // Submit our deck, then switch the socket to non-blocking polling.
        if (activeGameSocket)
        {
            sendSubmitDeck(*activeGameSocket, matchDeck);
            activeGameSocket->setBlocking(false);
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
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "Loading deck editor...", sf::Color::Yellow);
        clearFocus();
        cardLibrary.clear();
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
        dragActive = false;
        deckListOffset = 0;
        deckCardListOffset = 0;
        libraryOffset = 0;
        deckNameInput.clear();
        pendingDeckEditorLoad = std::async(std::launch::async, loadDeckEditorData, activeAccessToken);
    };

    auto deckEditorBusy = [&]() {
        return pendingDeckEditorLoad.has_value() || pendingDeckSave.has_value() || pendingDeckDelete.has_value();
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

    auto updateRememberMeLabel = [&]() {
        rememberMeButton.setLabel(rememberMeChecked ? "Remember Me: On" : "Remember Me: Off");
    };

    auto updatePasswordVisibility = [&]() {
        passwordInput.setPasswordMode(!passwordVisible);
        confirmInput.setPasswordMode(!passwordVisible);
        passwordVisibilityButton.setLabel(passwordVisible ? "Hide Password" : "Show Password");
    };

    auto submitCreateAccount = [&]() {
        if (usernameInput.getContent().empty() || passwordInput.getContent().empty())
        {
            setMessage(messageText, "Username and password cannot be empty", sf::Color::Red);
        }
        else if (passwordInput.getContent().size() < 15 || passwordInput.getContent().size() > 128)
        {
            setMessage(messageText, "Password must be 15-128 characters", sf::Color::Red);
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
        else if (newPasswordInput.getContent().size() < 15 || newPasswordInput.getContent().size() > 128)
        {
            setMessage(messageText, "New password must be 15-128 characters", sf::Color::Red);
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
    };

    auto computeDeckStats = [&]() {
        DeckStats stats;
        for (const std::string& title : editingDeck.cardTitles)
        {
            const card_data::Card* card = cardByTitle(title);
            if (card && game_data::isHeroCard(*card))
            {
                ++stats.heroCount;
                stats.heroCost += game_data::cardInt(*card, "heroCost", 0);
            }
            else
            {
                ++stats.cardCount;
            }
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
        deck.name = trim(deckNameInput.getContent());
        if (deck.name.empty())
        {
            setMessage(messageText, "Deck name cannot be empty", sf::Color::Red);
            return;
        }

        const std::string collectionError = deckCollectionError();
        if (!collectionError.empty())
        {
            setMessage(messageText, collectionError, sf::Color::Red);
            return;
        }

        const std::string validationError = deckValidationError(deck);
        if (!validationError.empty())
        {
            setMessage(messageText, validationError, sf::Color::Red);
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
        if (libraryIndex >= cardLibrary.size())
        {
            return;
        }

        const std::string& title = cardLibrary[libraryIndex].title;
        const bool isHero = game_data::isHeroCard(cardLibrary[libraryIndex]);
        const int copyLimit = isHero ? game_data::MaxHeroCopies : game_data::MaxCardCopies;
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
        selectedDeckCard = editingDeck.cardTitles.size() - 1;
        clampListOffset(deckCardListOffset, editingDeck.cardTitles.size(), VisibleDeckCardRows);
        if (*selectedDeckCard >= deckCardListOffset + VisibleDeckCardRows)
        {
            deckCardListOffset = *selectedDeckCard - VisibleDeckCardRows + 1;
        }
        setMessage(messageText, message, sf::Color::Yellow);
    };

    auto addSelectedCard = [&]() {
        if (!selectedLibraryCard || *selectedLibraryCard >= cardLibrary.size())
        {
            setMessage(messageText, "Select a card from the library first", sf::Color::Red);
            return;
        }

        addLibraryCardToDeck(*selectedLibraryCard, "Card added. Save to keep changes.");
    };

    auto removeSelectedCard = [&]() {
        if (!selectedDeckCard || *selectedDeckCard >= editingDeck.cardTitles.size())
        {
            setMessage(messageText, "Select a card in the deck first", sf::Color::Red);
            return;
        }

        editingDeck.cardTitles.erase(editingDeck.cardTitles.begin() + static_cast<std::ptrdiff_t>(*selectedDeckCard));
        if (editingDeck.cardTitles.empty())
        {
            selectedDeckCard.reset();
        }
        else if (*selectedDeckCard >= editingDeck.cardTitles.size())
        {
            selectedDeckCard = editingDeck.cardTitles.size() - 1;
        }
        clampListOffset(deckCardListOffset, editingDeck.cardTitles.size(), VisibleDeckCardRows);
        setMessage(messageText, "Card removed. Save to keep changes.", sf::Color::Yellow);
    };

    auto drawDeckEditor = [&]() {
        drawText(window, font, "Deck Editor", 30, {24.0f, 18.0f}, sf::Color::White);
        drawText(window, font, "Signed in as " + signedInLabel(), 14, {270.0f, 22.0f}, sf::Color(178, 186, 202), 360.0f);
        drawText(window, font, "Coins " + std::to_string(playerCoins), 13, {270.0f, 45.0f}, sf::Color(248, 214, 112), 160.0f);
        drawText(window, font, "Card server " + endpointText(clientConfig().card), 13, {390.0f, 45.0f}, sf::Color(148, 158, 176), 240.0f);
        deckBackButton.draw(window);

        drawPanel(window, {DeckPanelX, DeckEditorPanelY}, {250.0f, DeckEditorPanelHeight});
        drawText(window, font, "Decks", 22, {34.0f, 107.0f}, sf::Color::White);
        newDeckButton.draw(window);
        refreshDeckButton.draw(window);

        const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
        for (std::size_t i = deckListOffset; i < lastDeck; ++i)
        {
            const float y = DeckListY + static_cast<float>(i - deckListOffset) * DeckRowHeight;
            drawRow(
                window,
                font,
                {DeckListX, y},
                {DeckListWidth, DeckRowHeight - 4.0f},
                playerDecks[i].name,
                std::to_string(playerDecks[i].cardTitles.size()) + " cards",
                selectedDeck && *selectedDeck == i);
        }
        if (playerDecks.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No saved decks", 16, {56.0f, 296.0f}, sf::Color(178, 186, 202));
        }
        deleteDeckButton.draw(window);

        drawPanel(window, {CurrentDeckPanelX, DeckEditorPanelY}, {250.0f, DeckEditorPanelHeight});
        drawText(window, font, "Current Deck", 22, {304.0f, 107.0f}, sf::Color::White);
        deckNameInput.draw(window);

        const DeckStats stats = computeDeckStats();
        const bool cardsOk = stats.cardCount == game_data::DeckCardCount;
        const bool heroesOk = stats.heroCount >= game_data::MinHeroes && stats.heroCount <= game_data::MaxHeroes;
        const bool costOk = stats.heroCost <= game_data::HeroCostLimit;
        const sf::Color okColor(120, 220, 150);
        const sf::Color badColor(224, 130, 110);
        drawText(window, font,
                 "Cards " + std::to_string(stats.cardCount) + "/" + std::to_string(game_data::DeckCardCount),
                 14, {304.0f, 200.0f}, cardsOk ? okColor : badColor);
        drawText(window, font,
                 "Heroes " + std::to_string(stats.heroCount) + " (" + std::to_string(game_data::MinHeroes) +
                     "-" + std::to_string(game_data::MaxHeroes) + ")   Hero cost " +
                     std::to_string(stats.heroCost) + "/" + std::to_string(game_data::HeroCostLimit),
                 13, {304.0f, 220.0f}, (heroesOk && costOk) ? okColor : badColor);

        const std::size_t lastDeckCard = std::min(editingDeck.cardTitles.size(), deckCardListOffset + VisibleDeckCardRows);
        for (std::size_t i = deckCardListOffset; i < lastDeckCard; ++i)
        {
            const float y = DeckCardsY + static_cast<float>(i - deckCardListOffset) * DeckCardRowHeight;
            const card_data::Card* card = cardByTitle(editingDeck.cardTitles[i]);
            std::string secondary;
            if (card)
            {
                const std::string copies = "  " + std::to_string(deckCopies(editingDeck.cardTitles[i])) +
                    "/" + std::to_string(ownedCopies(editingDeck.cardTitles[i]));
                secondary = game_data::isHeroCard(*card)
                    ? "Hero  cost " + std::to_string(game_data::cardInt(*card, "heroCost", 0)) + copies
                    : card->type + "  " + std::to_string(game_data::cardInt(*card, "cost", 0)) + " steam" + copies;
            }
            drawRow(
                window,
                font,
                {DeckCardsX, y},
                {DeckCardsWidth, DeckCardRowHeight - 4.0f},
                editingDeck.cardTitles[i],
                secondary,
                selectedDeckCard && *selectedDeckCard == i);
        }
        if (editingDeck.cardTitles.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No cards in this deck", 15, {328.0f, 320.0f}, sf::Color(178, 186, 202), 180.0f);
        }
        removeCardButton.draw(window);

        drawPanel(window, {LibraryPanelX, DeckEditorPanelY}, {220.0f, DeckEditorPanelHeight});
        drawText(window, font, "Collection", 22, {574.0f, 107.0f}, sf::Color::White);
        drawText(
            window,
            font,
            std::to_string(cardLibrary.size()) + " owned card types",
            14,
            {574.0f, 138.0f},
            sf::Color(178, 186, 202));

        const std::size_t lastCard = std::min(cardLibrary.size(), libraryOffset + VisibleLibraryRows);
        for (std::size_t i = libraryOffset; i < lastCard; ++i)
        {
            const float y = LibraryY + static_cast<float>(i - libraryOffset) * LibraryRowHeight;
            const card_data::Card& libCard = cardLibrary[i];
            const std::string secondary = game_data::isHeroCard(libCard)
                ? "Hero  hc " + std::to_string(game_data::cardInt(libCard, "heroCost", 0)) +
                    "  owned " + std::to_string(ownedCopies(libCard.title))
                : libCard.type + "  " + std::to_string(game_data::cardInt(libCard, "cost", 0)) +
                    " steam  owned " + std::to_string(ownedCopies(libCard.title));
            drawRow(
                window,
                font,
                {LibraryX, y},
                {LibraryWidth, LibraryRowHeight - 4.0f},
                libCard.title,
                secondary,
                selectedLibraryCard && *selectedLibraryCard == i);
        }
        if (cardLibrary.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No owned cards", 16, {592.0f, 296.0f}, sf::Color(178, 186, 202));
        }
        addCardButton.draw(window);
        saveDeckButton.draw(window);

        const bool hoveringDropTarget = dragActive && draggingLibraryCard &&
            isInsideRect(dragCurrentPos, CurrentDeckPanelX, DeckEditorPanelY, 250.0f, DeckEditorPanelHeight);
        if (hoveringDropTarget)
        {
            sf::RectangleShape dropTarget({250.0f, DeckEditorPanelHeight});
            dropTarget.setPosition({CurrentDeckPanelX, DeckEditorPanelY});
            dropTarget.setFillColor(sf::Color(80, 140, 130, 45));
            dropTarget.setOutlineThickness(3.0f);
            dropTarget.setOutlineColor(sf::Color(103, 198, 184));
            window.draw(dropTarget);
        }

        if (dragActive && draggingLibraryCard && *draggingLibraryCard < cardLibrary.size())
        {
            const sf::Vector2f ghostPosition{dragCurrentPos.x - 96.0f, dragCurrentPos.y - 15.0f};
            sf::RectangleShape ghost({192.0f, 30.0f});
            ghost.setPosition(ghostPosition);
            ghost.setFillColor(sf::Color(60, 88, 102, 220));
            ghost.setOutlineThickness(1.0f);
            ghost.setOutlineColor(sf::Color(130, 220, 205));
            window.draw(ghost);
            drawText(
                window,
                font,
                cardLibrary[*draggingLibraryCard].title,
                15,
                {ghostPosition.x + 10.0f, ghostPosition.y + 6.0f},
                sf::Color::White,
                172.0f);
        }

        window.draw(messageText);
    };

    auto showDeckSelect = [&]() {
        currentState = GameState::DeckSelect;
        title.setString("Select Deck");
        centerText(title, 400.0f);
        clearFocus();
        playerDecks.clear();
        cardLibrary.clear();
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

    auto ownerColor = [](int owner) -> sf::Color {
        if (owner == 1) return sf::Color(80, 132, 214);
        if (owner == 2) return sf::Color(214, 102, 74);
        return sf::Color(120, 124, 134);
    };

    auto ownerTint = [](int owner) -> sf::Color {
        if (owner == 1) return sf::Color(24, 64, 72, 226);
        if (owner == 2) return sf::Color(88, 48, 36, 226);
        return sf::Color(38, 48, 43, 214);
    };

    // Fixed 2.5D camera: row 1 is nearest at the bottom, row 8 is farthest at the top.
    auto screenRowForViewer = [](int row, int /*viewer*/) {
        return game_data::BoardSize - 1 - row;
    };

    auto rowForScreenRow = [](int screenRow, int /*viewer*/) {
        return game_data::BoardSize - 1 - screenRow;
    };

    auto boardEdgePoint = [](int screenEdge, int columnEdge) -> sf::Vector2f {
        const float t = static_cast<float>(screenEdge) / static_cast<float>(game_data::BoardSize);
        const float y = BoardOriginY + BoardHeight * std::pow(t, BoardPerspectiveExponent);
        const float width = BoardTopWidth + (BoardBottomWidth - BoardTopWidth) * t;
        const float left = BoardCenterX - width * 0.5f;
        return {
            left + width * static_cast<float>(columnEdge) / static_cast<float>(game_data::BoardSize),
            y};
    };

    auto pieceScaleForScreenRow = [](int screenRow) {
        const float t = static_cast<float>(screenRow) / static_cast<float>(game_data::BoardSize - 1);
        return PieceFarScale + (PieceNearScale - PieceFarScale) * t;
    };

    auto boardCellMetricsForViewer = [&](int row, int column, int viewer) {
        BoardCellMetrics metrics;
        metrics.screenRow = screenRowForViewer(row, viewer);
        metrics.corners = {
            boardEdgePoint(metrics.screenRow, column),
            boardEdgePoint(metrics.screenRow, column + 1),
            boardEdgePoint(metrics.screenRow + 1, column + 1),
            boardEdgePoint(metrics.screenRow + 1, column)};
        metrics.center = {
            (metrics.corners[0].x + metrics.corners[1].x + metrics.corners[2].x + metrics.corners[3].x) * 0.25f,
            (metrics.corners[0].y + metrics.corners[1].y + metrics.corners[2].y + metrics.corners[3].y) * 0.25f};
        metrics.height = metrics.corners[3].y - metrics.corners[0].y;
        metrics.depthScale = pieceScaleForScreenRow(metrics.screenRow);
        return metrics;
    };

    auto boardCellMetrics = [&](int row, int column) {
        return boardCellMetricsForViewer(row, column, gameSnapshot.yourPlayer);
    };

    auto boardCellAnchor = [](const BoardCellMetrics& metrics) {
        return sf::Vector2f{metrics.center.x, metrics.center.y + metrics.height * 0.36f};
    };

    auto pointInConvex = [](sf::Vector2f point, const std::array<sf::Vector2f, 4>& corners) {
        bool hasNegative = false;
        bool hasPositive = false;
        for (std::size_t i = 0; i < corners.size(); ++i)
        {
            const sf::Vector2f a = corners[i];
            const sf::Vector2f b = corners[(i + 1) % corners.size()];
            const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
            hasNegative = hasNegative || cross < -0.01f;
            hasPositive = hasPositive || cross > 0.01f;
            if (hasNegative && hasPositive)
            {
                return false;
            }
        }
        return true;
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

    auto offsetQuad = [](std::array<sf::Vector2f, 4> corners, sf::Vector2f offset) {
        for (sf::Vector2f& corner : corners)
        {
            corner += offset;
        }
        return corners;
    };

    auto pieceTargetRect = [](sf::Vector2f anchor, float scale, bool walkSheet) {
        const float width = PieceBaseWidth * scale;
        const float height = (walkSheet ? PieceWalkBaseHeight : PieceBaseHeight) * scale;
        return sf::FloatRect{{anchor.x - width * 0.5f, anchor.y - height}, {width, height}};
    };

    auto pieceByIdInSnapshot = [](const game_data::Snapshot& snapshot, int id) -> const game_data::Piece* {
        for (const game_data::Piece& piece : snapshot.pieces)
        {
            if (piece.id == id)
            {
                return &piece;
            }
        }
        return nullptr;
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

        if (!haveSnapshot)
        {
            return;
        }

        for (const game_data::Piece& nextPiece : nextSnapshot.pieces)
        {
            const game_data::Piece* currentPiece = pieceByIdInSnapshot(gameSnapshot, nextPiece.id);
            if (!currentPiece)
            {
                continue;
            }

            if (currentPiece->row != nextPiece.row || currentPiece->column != nextPiece.column)
            {
                pieceMoveAnimations[nextPiece.id] = {
                    currentPiece->row,
                    currentPiece->column,
                    nextPiece.row,
                    nextPiece.column,
                    animationTime,
                    0.95f};
            }
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

    auto pieceAtInSnapshot = [](const game_data::Snapshot& snapshot, int row, int column) -> const game_data::Piece* {
        return game_data::findPieceAt(snapshot.pieces, row, column);
    };

    auto pieceByIdInSnapshotMutable = [](game_data::Snapshot& snapshot, int id) -> game_data::Piece* {
        for (game_data::Piece& piece : snapshot.pieces)
        {
            if (piece.id == id)
            {
                return &piece;
            }
        }
        return nullptr;
    };

    auto removePieceFromSnapshot = [](game_data::Snapshot& snapshot, int id) {
        snapshot.pieces.erase(
            std::remove_if(snapshot.pieces.begin(), snapshot.pieces.end(), [id](const game_data::Piece& piece) {
                return piece.id == id;
            }),
            snapshot.pieces.end());
    };

    auto controlledCountInSnapshot = [](const game_data::Snapshot& snapshot, int playerNumber) {
        return static_cast<int>(std::count(
            snapshot.control.begin(),
            snapshot.control.end(),
            static_cast<std::uint8_t>(playerNumber)));
    };

    auto heroesAliveInSnapshot = [](const game_data::Snapshot& snapshot, int playerNumber) {
        return static_cast<int>(std::count_if(snapshot.pieces.begin(), snapshot.pieces.end(), [playerNumber](const game_data::Piece& piece) {
            return piece.owner == playerNumber && piece.isHero;
        }));
    };

    auto refreshSandboxPlayerSnapshots = [&](game_data::Snapshot& snapshot) {
        snapshot.players[0].steam = 999;
        snapshot.players[0].controlledSquares = controlledCountInSnapshot(snapshot, 1);
        snapshot.players[0].handCount = static_cast<int>(snapshot.hand.size());
        snapshot.players[0].heroesToPlace = 0;
        snapshot.players[0].heroesAlive = heroesAliveInSnapshot(snapshot, 1);
        snapshot.players[0].drawPileCount = 0;

        snapshot.players[1].steam = 0;
        snapshot.players[1].controlledSquares = controlledCountInSnapshot(snapshot, 2);
        snapshot.players[1].handCount = 0;
        snapshot.players[1].heroesToPlace = 0;
        snapshot.players[1].heroesAlive = heroesAliveInSnapshot(snapshot, 2);
        snapshot.players[1].drawPileCount = 0;
    };

    auto recomputeSandboxControl = [&](game_data::Snapshot& snapshot) {
        std::array<std::uint8_t, game_data::BoardSquares> next = snapshot.control;
        for (int row = 0; row < game_data::BoardSize; ++row)
        {
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const std::size_t index = static_cast<std::size_t>(game_data::squareIndex(row, column));
                if (const game_data::Piece* occupant = pieceAtInSnapshot(snapshot, row, column))
                {
                    if (occupant->canControl)
                    {
                        next[index] = static_cast<std::uint8_t>(occupant->owner);
                    }
                    continue;
                }

                int influence1 = 0;
                int influence2 = 0;
                for (int dr = -1; dr <= 1; ++dr)
                {
                    for (int dc = -1; dc <= 1; ++dc)
                    {
                        if (dr == 0 && dc == 0)
                        {
                            continue;
                        }
                        const game_data::Piece* neighbor = game_data::inBounds(row + dr, column + dc)
                            ? pieceAtInSnapshot(snapshot, row + dr, column + dc)
                            : nullptr;
                        if (!neighbor || !neighbor->canControl)
                        {
                            continue;
                        }
                        if (neighbor->owner == 1)
                        {
                            ++influence1;
                        }
                        else if (neighbor->owner == 2)
                        {
                            ++influence2;
                        }
                    }
                }

                if (influence1 > influence2)
                {
                    next[index] = 1;
                }
                else if (influence2 > influence1)
                {
                    next[index] = 2;
                }
            }
        }
        snapshot.control = next;
    };

    auto commitSandboxSnapshot = [&](game_data::Snapshot nextSnapshot) {
        recomputeSandboxControl(nextSnapshot);
        refreshSandboxPlayerSnapshots(nextSnapshot);
        updatePieceMoveAnimations(nextSnapshot);
        gameSnapshot = std::move(nextSnapshot);
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

    auto spawnSandboxPiece = [&](game_data::Snapshot& snapshot, int owner, const game_data::GameCard& card, int row, int column, bool isHero) {
        game_data::Piece piece;
        piece.id = nextSandboxPieceId++;
        piece.owner = owner;
        piece.row = row;
        piece.column = column;
        piece.name = card.title;
        piece.keywords = card.keywords;
        piece.imagePath = card.imagePath;
        piece.walkAnimPath = card.walkAnimPath;
        piece.blueTokenPath = card.blueTokenPath;
        piece.redTokenPath = card.redTokenPath;
        piece.blueWalkAnimPath = card.blueWalkAnimPath;
        piece.redWalkAnimPath = card.redWalkAnimPath;
        piece.walkAnimFrames = card.walkAnimFrames;
        piece.maxHealth = card.health;
        piece.health = card.health;
        piece.attack = card.attack;
        piece.attackRange = card.attackRange;
        piece.movePattern = card.movePattern;
        piece.moveRange = card.moveRange;
        piece.attackingMove = card.attackingMove;
        piece.canControl = card.canControl;
        piece.growTurnsRemaining = card.growTurns;
        piece.actions = card.actions;
        piece.ability = card.ability;
        piece.abilityLabels = card.abilityLabels;
        piece.abilityUses = card.abilityUses;
        piece.isHero = isHero;
        piece.hasActed = false;
        snapshot.pieces.push_back(std::move(piece));
    };

    auto beginSandbox = [&](std::vector<card_data::Card> cards) {
        sandboxMode = true;
        sandboxPlacementPlayer = 1;
        sandboxPlayerButton.setLabel("Place P1");
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
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();

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
        currentState = GameState::SandboxLoading;
        title.setString("Sandbox");
        centerText(title, 400.0f);
        setMessageY(messageText, 450.0f);
        setMessage(messageText, "Loading card database...", sf::Color::Yellow);
        pendingSandboxLoad = std::async(std::launch::async, fetchCards);
    };

    auto updateSandboxPlayerButton = [&]() {
        sandboxPlayerButton.setLabel("Place P" + std::to_string(sandboxPlacementPlayer));
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
        return loadTexture(imagePath);
    };

    auto walkAnimTexture = [&](const std::string& walkAnimPath) -> sf::Texture* {
        return loadTexture(walkAnimPath);
    };

    auto pieceTokenPath = [](const game_data::Piece& piece) -> const std::string& {
        return piece.owner == 1 ? piece.blueTokenPath : piece.redTokenPath;
    };

    auto pieceWalkAnimPath = [](const game_data::Piece& piece) -> const std::string& {
        return piece.owner == 1 ? piece.blueWalkAnimPath : piece.redWalkAnimPath;
    };

    auto cardTokenPath = [](const game_data::GameCard& card, int owner) -> const std::string& {
        return owner == 1 ? card.blueTokenPath : card.redTokenPath;
    };

    auto cardWalkAnimPath = [](const game_data::GameCard& card, int owner) -> const std::string& {
        return owner == 1 ? card.blueWalkAnimPath : card.redWalkAnimPath;
    };

    auto drawCardPiecePreview = [&](const game_data::GameCard& card,
                                    int owner,
                                    sf::Vector2f anchor,
                                    float scale,
                                    bool valid) {
        const sf::Color tint = valid ? sf::Color(255, 255, 255, 220) : sf::Color(220, 120, 110, 190);
        const std::string& tokenPath = cardTokenPath(card, owner);
        const std::string& walkPath = cardWalkAnimPath(card, owner);

        bool drewPiece = false;
        if (sf::Texture* token = loadTexture(tokenPath))
        {
            drawContainSprite(*token, pieceTargetRect(anchor, scale, true), tint);
            drewPiece = true;
        }
        else if (sf::Texture* walkSheet = walkAnimTexture(walkPath))
        {
            const int frameCount = std::max(1, card.walkAnimFrames);
            const sf::Vector2u sheetSize = walkSheet->getSize();
            const int frameWidth = static_cast<int>(sheetSize.x / static_cast<unsigned int>(frameCount));
            const int frameHeight = static_cast<int>(sheetSize.y);
            if (frameWidth > 0 && frameHeight > 0)
            {
                drawTextureRectContain(
                    *walkSheet,
                    sf::IntRect({0, 0}, {frameWidth, frameHeight}),
                    pieceTargetRect(anchor, scale, true),
                    tint);
                drewPiece = true;
            }
        }
        else if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawContainSprite(*art, pieceTargetRect(anchor, scale, false), tint);
            drewPiece = true;
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
        sf::RectangleShape frame(size);
        frame.setPosition(position);
        frame.setFillColor(sf::Color(22, 29, 32, 244));
        frame.setOutlineThickness(3.0f);
        frame.setOutlineColor(game_data::isHeroCard(card) ? sf::Color(232, 187, 83) : sf::Color(116, 220, 202));
        window.draw(frame);

        sf::RectangleShape artFrame({size.x - 30.0f, 150.0f});
        artFrame.setPosition({position.x + 15.0f, position.y + 16.0f});
        artFrame.setFillColor(sf::Color(8, 14, 15));
        artFrame.setOutlineThickness(1.0f);
        artFrame.setOutlineColor(sf::Color(116, 86, 52));
        window.draw(artFrame);
        if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawContainSprite(*art, {{position.x + 20.0f, position.y + 20.0f}, {size.x - 40.0f, 142.0f}});
        }

        drawText(window, font, card.title, 22, {position.x + 18.0f, position.y + 178.0f}, sf::Color(248, 239, 216), size.x - 36.0f);
        const std::string typeLine = game_data::isHeroCard(card)
            ? "Hero cost " + std::to_string(game_data::cardInt(card, "heroCost", 0))
            : card.type + "  " + std::to_string(game_data::cardInt(card, "cost", 0)) + " steam";
        drawText(window, font, cardRarityLabel(card) + "  " + typeLine, 16, {position.x + 18.0f, position.y + 210.0f}, cardRarityColor(card), size.x - 36.0f);

        std::string statLine;
        if (card.type == "Unit" || game_data::isHeroCard(card))
        {
            statLine = "HP " + std::to_string(game_data::cardInt(card, "health", 0)) +
                "  Actions " + std::to_string(card.actions.size());
        }
        else
        {
            statLine = "Spell  " + game_data::cardStr(card, "effect", "effect") +
                " " + std::to_string(game_data::cardInt(card, "power", 0));
        }
        drawText(window, font, statLine, 15, {position.x + 18.0f, position.y + 236.0f}, sf::Color(224, 210, 176), size.x - 36.0f);
        drawText(
            window,
            font,
            "Owned " + std::to_string(ownedCopies(card.title)),
            15,
            {position.x + 18.0f, position.y + 264.0f},
            sf::Color(248, 214, 112),
            size.x - 36.0f);
    };

    auto joinStrings = [](const std::vector<std::string>& values, const std::string& separator) {
        std::string result;
        for (const std::string& value : values)
        {
            if (!result.empty())
            {
                result += separator;
            }
            result += value;
        }
        return result;
    };

    auto actionKindName = [](std::uint8_t kind) {
        switch (static_cast<game_data::ActionKind>(kind))
        {
            case game_data::ActionKind::Ranged: return std::string("Ranged");
            case game_data::ActionKind::Hop: return std::string("Hop");
            case game_data::ActionKind::Teleport: return std::string("Teleport");
            case game_data::ActionKind::Tunnel: return std::string("Tunnel");
            default: return std::string("Slide");
        }
    };

    auto actionPatternName = [](const game_data::ActionProfile& action) {
        if (static_cast<game_data::MovePattern>(action.pattern) == game_data::MovePattern::None)
        {
            return static_cast<game_data::ActionKind>(action.kind) == game_data::ActionKind::Ranged
                ? std::string("Any direction")
                : std::string("No pattern");
        }
        return game_data::movePatternName(action.pattern);
    };

    auto actionRangeText = [](const game_data::ActionProfile& action) {
        if (action.minRange == action.maxRange)
        {
            return "range " + std::to_string(action.maxRange);
        }
        return "range " + std::to_string(action.minRange) + "-" + std::to_string(action.maxRange);
    };

    auto actionDescription = [&](const game_data::ActionProfile& action, std::size_t index) {
        std::vector<std::string> parts;
        if (action.state != 0)
        {
            parts.push_back("state " + std::to_string(action.state));
        }
        parts.push_back(actionKindName(action.kind));
        parts.push_back(actionPatternName(action));
        parts.push_back(actionRangeText(action));
        if (action.canMove)
        {
            parts.push_back("moves");
        }
        if (action.canAttack)
        {
            parts.push_back("attacks for " + std::to_string(action.damage));
        }
        if (action.statusTurns > 0)
        {
            parts.push_back("disables " + std::to_string(action.statusTurns) + " turn(s)");
        }
        if (action.cooldownTurns > 0)
        {
            parts.push_back("cooldown " + std::to_string(action.cooldownTurns));
        }
        if (action.passThrough)
        {
            parts.push_back("passes through blockers");
        }
        if (action.lineOfSight)
        {
            parts.push_back("line of sight");
        }

        const std::string label = action.name.empty()
            ? "Action " + std::to_string(index + 1)
            : action.name;
        return label + ": " + joinStrings(parts, ", ");
    };

    auto deckEditorCardDetails = [&](const card_data::Card& card) {
        std::vector<std::pair<std::string, sf::Color>> details;
        const game_data::GameCard gameCard = game_data::toGameCard(card);
        const bool hero = game_data::isHeroCard(card);
        const bool unit = card.type == "Unit" || hero;

        if (hero)
        {
            details.push_back({"Rarity: " + cardRarityLabel(card), cardRarityColor(card)});
            details.push_back({"Hero cost: " + std::to_string(game_data::cardInt(card, "heroCost", 0)),
                               sf::Color(248, 214, 112)});
        }
        else
        {
            details.push_back({"Rarity: " + cardRarityLabel(card), cardRarityColor(card)});
            details.push_back({"Cost: " + std::to_string(game_data::cardInt(card, "cost", 0)) + " steam",
                               sf::Color(150, 210, 235)});
        }

        if (unit)
        {
            details.push_back({"Health: " + std::to_string(gameCard.health), sf::Color(224, 210, 176)});
            if (gameCard.actions.empty())
            {
                details.push_back({"Actions: none", sf::Color(225, 170, 150)});
            }
            for (std::size_t i = 0; i < gameCard.actions.size(); ++i)
            {
                details.push_back({actionDescription(gameCard.actions[i], i), sf::Color(143, 220, 205)});
            }
            details.push_back({"Territory: occupied square + adjacent influence", sf::Color(198, 180, 142)});
        }
        else
        {
            details.push_back({"Effect: " + game_data::cardStr(card, "effect", "none"),
                               sf::Color(224, 210, 176)});
            details.push_back({"Power: " + std::to_string(game_data::cardInt(card, "power", 0)),
                               sf::Color(224, 210, 176)});
            details.push_back({"Target: " + game_data::cardStr(card, "target", "none"),
                               sf::Color(143, 220, 205)});
        }

        if (!card.keywords.empty())
        {
            details.push_back({"Keywords: " + joinStrings(card.keywords, ", "), sf::Color(210, 216, 228)});
        }

        auto isHiddenCardDetailKey = [](const std::string& key) {
            return key == "cost" || key == "heroCost" || key == "health" || key == "attack" ||
                key == "range" || key == "move" || key == "attackingMove" || key == "power" ||
                key == "canControl" || key == "growTurns" || key == "abilityUses" ||
                key == "WalkAnimFrames" || key == "rarity" || key == "effect" || key == "target" ||
                key == "movement" || key == "WalkAnim" || key == "WalkAnimBlue" ||
                key == "WalkAnimRed" || key == "TokenBlue" || key == "TokenRed";
        };

        for (const card_data::KeyIntPair& item : card.integerValues)
        {
            if (isHiddenCardDetailKey(item.key))
            {
                continue;
            }
            details.push_back({item.key + ": " + std::to_string(item.value), sf::Color(190, 198, 214)});
        }
        for (const card_data::KeyStringPair& item : card.stringValues)
        {
            if (isHiddenCardDetailKey(item.key))
            {
                continue;
            }
            details.push_back({item.key + ": " + item.value, sf::Color(190, 198, 214)});
        }
        for (const card_data::KeyStringList& item : card.stringLists)
        {
            details.push_back({item.key + ": " + joinStrings(item.values, ", "), sf::Color(190, 198, 214)});
        }

        return details;
    };

    auto deckEditorCardDetailsHeight = [&](const std::vector<std::pair<std::string, sf::Color>>& details) {
        float height = 0.0f;
        for (const auto& [description, color] : details)
        {
            (void)color;
            height += static_cast<float>(wrapText(font, description, 14, PiecePopupTextWidth - 24.0f).size()) * 18.0f;
            height += 8.0f;
        }
        return height;
    };

    auto deckEditorCardDetailsMaxScroll = [&](const std::vector<std::pair<std::string, sf::Color>>& details) {
        return std::max(0.0f, deckEditorCardDetailsHeight(details) - PiecePopupScrollHeight);
    };

    auto drawDeckEditorCardPopup = [&]() {
        if (!inspectedDeckEditorCardTitle)
        {
            return;
        }

        const card_data::Card* card = cardByTitle(*inspectedDeckEditorCardTitle);
        if (!card)
        {
            card = cardInAllLibraryByTitle(*inspectedDeckEditorCardTitle);
        }
        if (!card)
        {
            inspectedDeckEditorCardTitle.reset();
            inspectedDeckEditorCardScroll = 0.0f;
            return;
        }

        const std::vector<std::pair<std::string, sf::Color>> details = deckEditorCardDetails(*card);

        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(overlay);

        drawPanel(window, {PiecePopupX, PiecePopupY}, {PiecePopupWidth, PiecePopupHeight});
        drawText(window, font, card->title, 24, {PiecePopupX + 22.0f, PiecePopupY + 18.0f},
                 sf::Color(248, 239, 216), PiecePopupWidth - 44.0f);

        sf::RectangleShape artFrame({104.0f, 104.0f});
        artFrame.setPosition({PiecePopupX + 22.0f, PiecePopupY + 62.0f});
        artFrame.setFillColor(sf::Color(8, 14, 15));
        artFrame.setOutlineThickness(1.0f);
        artFrame.setOutlineColor(sf::Color(155, 111, 59));
        window.draw(artFrame);
        if (sf::Texture* art = cardArtTexture(card->imagePath))
        {
            drawContainSprite(*art, {{PiecePopupX + 30.0f, PiecePopupY + 70.0f}, {88.0f, 88.0f}});
        }

        float y = PiecePopupY + 66.0f;
        const float statX = PiecePopupX + 146.0f;
        const bool hero = game_data::isHeroCard(*card);
        drawText(window, font, "Type: " + card->type + "   Location: Collection", 15, {statX, y},
                 hero ? sf::Color(248, 214, 112) : sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        y += 24.0f;
        drawText(window, font, "Owned: " + std::to_string(ownedCopies(card->title)), 14, {statX, y},
                 sf::Color(248, 214, 112));
        y += 24.0f;
        if (hero)
        {
            drawText(window, font, "Hero cost: " + std::to_string(game_data::cardInt(*card, "heroCost", 0)),
                     14, {statX, y}, sf::Color(224, 210, 176));
        }
        else
        {
            drawText(window, font, "Cost: " + std::to_string(game_data::cardInt(*card, "cost", 0)) + " steam",
                     14, {statX, y}, sf::Color(150, 210, 235));
        }
        y += 22.0f;
        if (card->type == "Unit" || hero)
        {
            const game_data::GameCard gameCard = game_data::toGameCard(*card);
            drawText(window, font, "Health: " + std::to_string(gameCard.health),
                     14, {statX, y}, sf::Color(224, 210, 176));
            y += 22.0f;
            drawText(window, font, "Actions: " + std::to_string(gameCard.actions.size()),
                     14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        }
        else
        {
            drawText(window, font, "Effect: " + game_data::cardStr(*card, "effect", "none") +
                         "   Power: " + std::to_string(game_data::cardInt(*card, "power", 0)),
                     14, {statX, y}, sf::Color(224, 210, 176), PiecePopupWidth - 174.0f);
            y += 22.0f;
            drawText(window, font, "Target: " + game_data::cardStr(*card, "target", "none"),
                     14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        }

        inspectedDeckEditorCardScroll = std::clamp(
            inspectedDeckEditorCardScroll,
            0.0f,
            deckEditorCardDetailsMaxScroll(details));

        drawText(window, font, "Details", 17, {PiecePopupTextX, PiecePopupActionHeadingY}, sf::Color::White);

        sf::RectangleShape scrollBack({PiecePopupTextWidth, PiecePopupScrollHeight});
        scrollBack.setPosition({PiecePopupTextX, PiecePopupScrollY});
        scrollBack.setFillColor(sf::Color(8, 14, 15, 132));
        scrollBack.setOutlineThickness(1.0f);
        scrollBack.setOutlineColor(sf::Color(44, 108, 101, 120));
        window.draw(scrollBack);

        const sf::View previousView = window.getView();
        sf::View detailView(sf::FloatRect(
            {PiecePopupTextX, PiecePopupScrollY + inspectedDeckEditorCardScroll},
            {PiecePopupTextWidth, PiecePopupScrollHeight}));
        detailView.setViewport(sf::FloatRect(
            {PiecePopupTextX / 800.0f, PiecePopupScrollY / 600.0f},
            {PiecePopupTextWidth / 800.0f, PiecePopupScrollHeight / 600.0f}));
        window.setView(detailView);

        y = PiecePopupScrollY + 8.0f;
        for (const auto& [description, color] : details)
        {
            y = drawWrappedText(window, font, description, 14, {PiecePopupTextX + 8.0f, y}, color, PiecePopupTextWidth - 24.0f);
            y += 8.0f;
        }

        window.setView(previousView);

        const float maxScroll = deckEditorCardDetailsMaxScroll(details);
        if (maxScroll > 0.0f)
        {
            const float trackX = PiecePopupX + PiecePopupWidth - 22.0f;
            sf::RectangleShape track({4.0f, PiecePopupScrollHeight - 12.0f});
            track.setPosition({trackX, PiecePopupScrollY + 6.0f});
            track.setFillColor(sf::Color(73, 96, 98, 170));
            window.draw(track);

            const float thumbHeight = std::max(28.0f, track.getSize().y * (PiecePopupScrollHeight / (PiecePopupScrollHeight + maxScroll)));
            const float thumbY = track.getPosition().y +
                (track.getSize().y - thumbHeight) * (inspectedDeckEditorCardScroll / maxScroll);
            sf::RectangleShape thumb({4.0f, thumbHeight});
            thumb.setPosition({trackX, thumbY});
            thumb.setFillColor(sf::Color(143, 220, 205, 230));
            window.draw(thumb);
        }

        closeDeckCardPopupButton.draw(window);
    };

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
        dragActive = false;
        clearFocus();
        return true;
    };

    auto drawShop = [&]() {
        drawText(window, font, "Shop", 30, {24.0f, 18.0f}, sf::Color::White);
        drawText(window, font, "Signed in as " + signedInLabel(), 14, {220.0f, 24.0f}, sf::Color(178, 186, 202), 280.0f);

        sf::CircleShape coin(14.0f);
        coin.setPosition({534.0f, 24.0f});
        coin.setFillColor(sf::Color(214, 158, 48));
        coin.setOutlineThickness(2.0f);
        coin.setOutlineColor(sf::Color(255, 225, 132));
        window.draw(coin);
        drawText(window, font, std::to_string(playerCoins), 18, {570.0f, 22.0f}, sf::Color(248, 239, 216), 80.0f);
        shopBackButton.draw(window);

        drawPanel(window, {170.0f, 96.0f}, {460.0f, 378.0f});

        const sf::Vector2f center{400.0f, 286.0f};
        if (pendingShopLoad)
        {
            drawText(window, font, "Loading shop...", 24, {306.0f, 270.0f}, sf::Color(248, 239, 216), 220.0f);
        }
        else if (revealedCardTitle)
        {
            const float t = animationTime - revealStartedAt;
            for (int i = 0; i < 4; ++i)
            {
                const float radius = 86.0f + static_cast<float>(i) * 24.0f + std::sin(t * 4.0f + static_cast<float>(i)) * 6.0f;
                sf::CircleShape glow(radius);
                glow.setOrigin({radius, radius});
                glow.setPosition(center);
                glow.setFillColor(sf::Color(229, 183, 83, static_cast<std::uint8_t>(34 - i * 6)));
                window.draw(glow);
            }

            for (int i = 0; i < 14; ++i)
            {
                const float angle = static_cast<float>(i) * 0.72f + t * 1.8f;
                const float radius = 132.0f + std::sin(t * 3.0f + static_cast<float>(i)) * 18.0f;
                sf::CircleShape sparkle(3.0f + static_cast<float>(i % 3));
                sparkle.setPosition({
                    center.x + std::cos(angle) * radius,
                    center.y + std::sin(angle) * radius * 0.72f});
                sparkle.setFillColor(sf::Color(248, 230, 150, 190));
                window.draw(sparkle);
            }

            drawText(window, font, "Revealed", 22, {350.0f, 112.0f}, sf::Color(248, 214, 112), 120.0f);
            if (const card_data::Card* card = cardInAllLibraryByTitle(*revealedCardTitle))
            {
                drawLargeCollectionCard(*card, {290.0f, 144.0f}, {220.0f, 300.0f});
            }
            else
            {
                sf::RectangleShape fallback({220.0f, 300.0f});
                fallback.setPosition({290.0f, 144.0f});
                fallback.setFillColor(sf::Color(22, 29, 32, 244));
                fallback.setOutlineThickness(3.0f);
                fallback.setOutlineColor(sf::Color(232, 187, 83));
                window.draw(fallback);
                drawText(window, font, *revealedCardTitle, 22, {310.0f, 270.0f}, sf::Color(248, 239, 216), 180.0f);
            }
        }
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                sf::CircleShape glow(86.0f + static_cast<float>(i) * 28.0f);
                glow.setOrigin({glow.getRadius(), glow.getRadius()});
                glow.setPosition(center);
                glow.setFillColor(sf::Color(42, 120, 112, static_cast<std::uint8_t>(34 - i * 8)));
                window.draw(glow);
            }

            sf::RectangleShape pack({190.0f, 250.0f});
            pack.setPosition({305.0f, 152.0f});
            pack.setFillColor(sf::Color(43, 57, 60, 245));
            pack.setOutlineThickness(3.0f);
            pack.setOutlineColor(sf::Color(210, 154, 74));
            window.draw(pack);

            sf::RectangleShape band({190.0f, 54.0f});
            band.setPosition({305.0f, 252.0f});
            band.setFillColor(sf::Color(93, 64, 39, 230));
            window.draw(band);

            drawText(window, font, "Mystery", 26, {345.0f, 190.0f}, sf::Color(248, 239, 216), 120.0f);
            drawText(window, font, "Card", 26, {370.0f, 222.0f}, sf::Color(248, 239, 216), 90.0f);
            drawText(window, font, "5 coins", 22, {362.0f, 265.0f}, sf::Color(248, 214, 112), 100.0f);
            drawText(window, font, "Odds: Common 70%  Rare 25%  Legendary 5%", 14, {248.0f, 412.0f}, sf::Color(248, 239, 216), 304.0f);
            drawText(window, font, "Cards inside each rarity are equally likely", 13, {278.0f, 436.0f}, sf::Color(190, 198, 214), 244.0f);
        }

        if (revealedCardTitle)
        {
            dismissRevealedCardButton.draw(window);
        }
        else
        {
            if (EnableCoinPurchases)
            {
                buyCoinPackButton.draw(window);
                refreshShopButton.draw(window);
            }
            buyCardButton.draw(window);
        }
        window.draw(messageText);
    };

    auto drawAdminUsers = [&]() {
        drawText(window, font, "Admin Users", 30, {24.0f, 18.0f}, sf::Color::White);
        drawText(
            window,
            font,
            "Signed in as " + signedInLabel(),
            14,
            {250.0f, 22.0f},
            sf::Color(178, 186, 202),
            300.0f);
        drawText(
            window,
            font,
            "Users " + std::to_string(adminUsersTotalCount),
            14,
            {250.0f, 44.0f},
            sf::Color(248, 214, 112),
            150.0f);
        adminBackButton.draw(window);

        drawPanel(window, {24.0f, 78.0f}, {752.0f, 68.0f});
        drawText(window, font, "Search", 16, {42.0f, 98.0f}, sf::Color::White);
        adminSearchInput.draw(window);

        drawPanel(window, {24.0f, 160.0f}, {752.0f, 278.0f});
        const std::size_t lastUser =
            std::min(adminUsers.size(), static_cast<std::size_t>(AdminUsersPageSize));
        for (std::size_t i = 0; i < lastUser; ++i)
        {
            const float y = AdminUserRowY + static_cast<float>(i) * AdminUserRowHeight;
            const bool selected = selectedAdminUser && *selectedAdminUser == i;
            drawRow(
                window,
                font,
                {38.0f, y},
                {704.0f, 40.0f},
                adminUsers[i].username,
                std::string(adminUsers[i].isAdmin ? "Admin" : "Player") +
                    "  |  Gold: " + std::to_string(adminUsers[i].gold),
                selected);
        }
        if (adminUsers.empty() && !pendingAdminUsersLoad)
        {
            drawText(window, font, "No matching users", 18, {292.0f, 294.0f}, sf::Color(178, 186, 202));
        }

        adminPrevPageButton.draw(window);
        adminRefreshButton.draw(window);
        adminNextPageButton.draw(window);
        if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
        {
            adminGoldInput.draw(window);
            adminGrantGoldButton.draw(window);
            adminRemoveGoldButton.draw(window);
            const bool targetIsAdmin = adminUsers[*selectedAdminUser].isAdmin;
            if (targetIsAdmin)
            {
                if (adminUsers[*selectedAdminUser].username == loggedInUsername)
                {
                    drawText(
                        window,
                        font,
                        "You cannot revoke your own admin status",
                        14,
                        {42.0f, 466.0f},
                        sf::Color(248, 214, 112),
                        190.0f);
                }
                else
                {
                    adminRevokeButton.draw(window);
                }
            }
            else
            {
                adminGrantButton.draw(window);
            }
            if (adminUsers[*selectedAdminUser].username != loggedInUsername)
            {
                adminDeleteButton.draw(window);
            }
        }
        window.draw(messageText);

        if (deleteUserPopupVisible)
        {
            sf::RectangleShape overlay({800.0f, 600.0f});
            overlay.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(overlay);
            drawPanel(window, {220.0f, 176.0f}, {360.0f, 248.0f});
            drawText(window, font, "Delete User", 28, {250.0f, 200.0f}, sf::Color(248, 224, 172), 300.0f);
            drawText(window, font, "Permanently delete account:", 16, {250.0f, 252.0f}, sf::Color(220, 224, 230), 300.0f);
            drawText(window, font, adminUserDeleteTarget, 20, {250.0f, 276.0f}, sf::Color(248, 214, 112), 300.0f);
            drawText(window, font, "This also removes their decks and", 13, {250.0f, 314.0f}, sf::Color(214, 150, 140), 300.0f);
            drawText(window, font, "cannot be undone.", 13, {250.0f, 332.0f}, sf::Color(214, 150, 140), 300.0f);
            cancelDeleteUserButton.draw(window);
            confirmDeleteUserButton.draw(window);
        }
    };

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

    auto sandboxPlayCard = [&](int handIndex, int row, int column) {
        if (!sandboxMode || !haveSnapshot ||
            static_cast<game_data::Phase>(gameSnapshot.phase) != game_data::Phase::Playing ||
            handIndex < 0 || handIndex >= static_cast<int>(gameSnapshot.hand.size()))
        {
            return;
        }

        game_data::Snapshot next = gameSnapshot;
        const game_data::GameCard card = next.hand[static_cast<std::size_t>(handIndex)];
        const int actingPlayer = sandboxPlacementPlayer;
        if (card.type == "Unit")
        {
            if (!game_data::inBounds(row, column) ||
                next.control[static_cast<std::size_t>(game_data::squareIndex(row, column))] != actingPlayer ||
                pieceAtInSnapshot(next, row, column) != nullptr)
            {
                next.status = "Sandbox pieces deploy onto an empty square controlled by the selected player.";
                commitSandboxSnapshot(std::move(next));
                return;
            }

            spawnSandboxPiece(next, actingPlayer, card, row, column, card.type == "Hero");
            next.status = "Sandbox played " + card.title + " for Player " + std::to_string(actingPlayer) + ".";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        if (card.effect == "steam")
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
            target->health -= card.power;
            if (card.power > 0)
            {
                target->sleepTurnsRemaining = std::max(target->sleepTurnsRemaining, 1);
            }
            if (target->health <= 0)
            {
                removePieceFromSnapshot(next, target->id);
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

        game_data::Snapshot next = gameSnapshot;
        game_data::Piece* piece = pieceByIdInSnapshotMutable(next, pieceId);
        if (!piece)
        {
            return;
        }

        const game_data::ActionResolution action =
            game_data::resolvePieceAction(next.pieces, next.holes, *piece, row, column);
        if (!action.legal)
        {
            next.status = "That piece cannot act there.";
            commitSandboxSnapshot(std::move(next));
            return;
        }

        const int attackerId = piece->id;
        const std::string attackerName = piece->name;
        std::string targetName;
        bool targetDestroyed = false;
        bool targetAtDestination = false;

        if (action.attacks)
        {
            game_data::Piece* target = pieceByIdInSnapshotMutable(next, action.targetId);
            if (!target)
            {
                return;
            }
            targetName = target->name;
            targetAtDestination = target->row == row && target->column == column;
            target->health -= action.damage;
            if (action.damage > 0)
            {
                target->sleepTurnsRemaining = std::max(target->sleepTurnsRemaining, 1);
            }
            target->disabledTurns = std::max(target->disabledTurns, action.statusTurns);
            if (target->health <= 0)
            {
                targetDestroyed = true;
                removePieceFromSnapshot(next, target->id);
            }
        }

        game_data::Piece* acting = pieceByIdInSnapshotMutable(next, attackerId);
        if (!acting)
        {
            return;
        }

        if (action.moves)
        {
            if (!action.attacks || !targetAtDestination || targetDestroyed)
            {
                acting->row = row;
                acting->column = column;
            }
            else
            {
                acting->row = action.stagingRow;
                acting->column = action.stagingColumn;
            }
        }
        acting->disabledTurns = std::max(acting->disabledTurns, action.cooldownTurns);
        acting->hasActed = false;

        next.status = action.attacks
            ? attackerName + " hit " + targetName + " for " + std::to_string(action.damage) +
                (targetDestroyed ? " and destroyed it." : ".")
            : attackerName + " moved.";
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
        if (!piece ||
            piece->ability.empty() || piece->growTurnsRemaining > 0 || piece->disabledTurns > 0)
        {
            return;
        }

        const std::string abilityLabel = game_data::pieceAbilityLabel(*piece);
        if (piece->ability == "dig")
        {
            if (piece->abilityUses <= 0)
            {
                next.status = "That piece has already dug its hole.";
                commitSandboxSnapshot(std::move(next));
                return;
            }
            next.holes[static_cast<std::size_t>(game_data::squareIndex(piece->row, piece->column))] = 1;
            --piece->abilityUses;
        }
        else if (piece->ability == "transform" || piece->ability == "dematerialize")
        {
            int stateCount = 1;
            for (const game_data::ActionProfile& action : piece->actions)
            {
                stateCount = std::max(stateCount, action.state + 1);
            }
            piece->actionState = (piece->actionState + 1) % stateCount;
            piece->hidden = piece->ability == "dematerialize" && piece->actionState != 0;
        }
        else
        {
            return;
        }

        piece->hasActed = false;
        next.status = piece->name + " used " + abilityLabel + ".";
        commitSandboxSnapshot(std::move(next));
    };

    auto sandboxEndTurn = [&]() {
        if (!sandboxMode || !haveSnapshot)
        {
            return;
        }
        game_data::Snapshot next = gameSnapshot;
        for (game_data::Piece& piece : next.pieces)
        {
            if (piece.owner == 1)
            {
                piece.hasActed = false;
                if (piece.growTurnsRemaining > 0)
                {
                    --piece.growTurnsRemaining;
                    piece.hasActed = piece.growTurnsRemaining > 0;
                }
                if (piece.disabledTurns > 0)
                {
                    --piece.disabledTurns;
                    piece.hasActed = true;
                }
                if (piece.sleepTurnsRemaining > 0)
                {
                    --piece.sleepTurnsRemaining;
                }
            }
        }
        next.status = "Sandbox turn reset.";
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

    auto canDiscardInspectedHandCard = [&]() {
        return !sandboxMode && haveSnapshot && inspectedHandIndex &&
            *inspectedHandIndex < gameSnapshot.hand.size() &&
            static_cast<game_data::Phase>(gameSnapshot.phase) == game_data::Phase::Playing &&
            gameSnapshot.activePlayer == gameSnapshot.yourPlayer;
    };

    auto handleHandCardClick = [&](std::size_t handIndex) {
        if (handIndex >= gameSnapshot.hand.size())
        {
            return false;
        }

        const game_data::GameCard& card = gameSnapshot.hand[handIndex];
        selectedPieceId.reset();
        if (card.type != "Unit" && card.effect == "steam" &&
            (sandboxMode ||
             game_data::heroKeywordsAllowCard(
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
            gameDragKind = GameDragKind::HandCard;
            draggingHandIndex = *handIndex;
            gameDragStartPos = clickPos;
            gameDragCurrentPos = clickPos;
            return;
        }

        if (const game_data::Piece* piece = gamePieceAtPixel(clickPos);
            piece && (sandboxMode || (piece->owner == me && !piece->hasActed)))
        {
            gameDragKind = GameDragKind::Piece;
            draggingPieceId = piece->id;
            gameDragStartPos = clickPos;
            gameDragCurrentPos = clickPos;
        }
    };

    auto finishGameDrag = [&](sf::Vector2f releasePos) {
        if (!gameDragActive || !haveSnapshot)
        {
            resetGameDrag();
            return false;
        }

        const std::optional<std::pair<int, int>> square = squareAtPixel(releasePos);
        if (!square)
        {
            resetGameDrag();
            return true;
        }

        const auto [row, column] = *square;
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
            if (const game_data::Piece* piece = gamePieceById(*draggingPieceId))
            {
                const game_data::ActionResolution action = game_data::resolvePieceAction(
                    gameSnapshot.pieces, gameSnapshot.holes, *piece, row, column);
                if (action.legal)
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
        while (activeGameSocket->receive(packet) == sf::Socket::Status::Done)
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
                    haveSnapshot = true;
                    clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
                    if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                            game_data::Phase::GameOver &&
                        !gameResultReceived)
                    {
                        gameRewardText = "Finalizing match rewards...";
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
        }
    };

    auto leaveGame = [&]() {
        const bool wasSandbox = sandboxMode;
        if (activeGameSocket)
        {
            sendDisconnect(*activeGameSocket);
            activeGameSocket.reset();
        }
        haveSnapshot = false;
        gameSnapshot = {};
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
        gameRatingChange = 0;
        gameRewardText.clear();
        pieceMoveAnimations.clear();
        sandboxMode = false;
        sandboxPlacementPlayer = 1;
        nextSandboxPieceId = 1;
        gameHandOffset = 0;
        if (wasSandbox)
        {
            showAuthenticatedScreen();
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

        // Playing phase — only the active player may act.
        if (!sandboxMode && gameSnapshot.activePlayer != me)
        {
            return;
        }

        if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
        {
            handleHandCardClick(*handIndex);
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
                if (clicked && clicked->owner != (sandboxMode ? selected->owner : me))
                {
                    const game_data::ActionResolution action = game_data::resolvePieceAction(
                        gameSnapshot.pieces, gameSnapshot.holes, *selected, row, column);
                    if (action.legal)
                    {
                        sendMovePiece(selected->id, row, column);
                    }
                    selectedPieceId.reset();
                    return;
                }
                if (clicked && (sandboxMode || clicked->owner == me))
                {
                    selectedPieceId = (!sandboxMode && clicked->hasActed) ? std::nullopt : std::optional<int>(clicked->id);
                    return;
                }
                const game_data::ActionResolution action = game_data::resolvePieceAction(
                    gameSnapshot.pieces, gameSnapshot.holes, *selected, row, column);
                if (action.legal)
                {
                    sendMovePiece(selected->id, row, column);
                }
                selectedPieceId.reset();
                return;
            }
            selectedPieceId.reset();
            return;
        }

        if (clicked && (sandboxMode || (clicked->owner == me && !clicked->hasActed)))
        {
            selectedPieceId = clicked->id;
        }
        else
        {
            selectedPieceId.reset();
        }
    };

    auto drawGameCardFace = [&](sf::Vector2f position, const game_data::GameCard& card, bool selected, bool affordable) {
        sf::RectangleShape rect({HandCardWidth, HandCardHeight});
        rect.setPosition(position);
        rect.setFillColor(selected ? sf::Color(35, 97, 92, 238) : sf::Color(20, 28, 30, 236));
        rect.setOutlineThickness(selected ? 2.0f : 1.0f);
        rect.setOutlineColor(selected ? sf::Color(121, 238, 207) : sf::Color(155, 111, 59));
        window.draw(rect);

        sf::RectangleShape artFrame({34.0f, 34.0f});
        artFrame.setPosition({position.x + 5.0f, position.y + 5.0f});
        artFrame.setFillColor(sf::Color(8, 14, 15));
        artFrame.setOutlineThickness(1.0f);
        artFrame.setOutlineColor(sf::Color(114, 83, 47));
        window.draw(artFrame);
        if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawContainSprite(*art, {{position.x + 7.0f, position.y + 7.0f}, {30.0f, 30.0f}},
                              affordable ? sf::Color::White : sf::Color(120, 112, 104));
        }

        sf::CircleShape costBadge(11.0f);
        costBadge.setPosition({position.x + HandCardWidth - 24.0f, position.y + 3.0f});
        costBadge.setFillColor(affordable ? sf::Color(39, 126, 139) : sf::Color(91, 66, 58));
        costBadge.setOutlineThickness(1.0f);
        costBadge.setOutlineColor(sf::Color(224, 174, 83));
        window.draw(costBadge);
        const int displayedCost = card.type == "Hero" ? card.heroCost : card.cost;
        drawText(window, font, std::to_string(displayedCost), 13, {position.x + HandCardWidth - 21.0f, position.y + 4.0f}, sf::Color(248, 239, 216));

        const sf::Color titleColor = affordable ? sf::Color(248, 239, 216) : sf::Color(158, 128, 118);
        drawText(window, font, card.title, 12, {position.x + 6.0f, position.y + 37.0f}, titleColor, HandCardWidth - 12.0f);

        std::string line2;
        std::string line3;
        if (card.type == "Unit" || card.type == "Hero")
        {
            line2 = "HP " + std::to_string(card.health);
            line3 = "Actions " + std::to_string(card.actions.size());
        }
        else
        {
            line2 = "Spell";
            if (card.effect == "damage") line3 = "Deal " + std::to_string(card.power);
            else if (card.effect == "heal") line3 = "Heal " + std::to_string(card.power);
            else if (card.effect == "steam") line3 = "+" + std::to_string(card.power) + " steam";
        }
        drawText(window, font, line2, 11, {position.x + 6.0f, position.y + 53.0f}, sf::Color(224, 210, 176), HandCardWidth - 12.0f);
        drawText(window, font, line3, 11, {position.x + 6.0f, position.y + 65.0f}, sf::Color(143, 220, 205), HandCardWidth - 12.0f);
    };

    auto readinessDescription = [&](const game_data::Piece& piece) {
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        if (sandboxMode)
        {
            return std::string("Status: sandbox ready");
        }
        if (phase == game_data::Phase::HeroPlacement)
        {
            return std::string("Status: waiting for hero placement");
        }
        if (phase == game_data::Phase::GameOver)
        {
            return std::string("Status: game over");
        }
        if (gameSnapshot.activePlayer != piece.owner)
        {
            return "Status: waiting for Player " + std::to_string(piece.owner);
        }
        if (piece.hasActed)
        {
            return std::string("Status: acted this turn");
        }
        if (piece.sleepTurnsRemaining > 0)
        {
            return std::string("Status: sleeping, cannot move");
        }
        return std::string("Status: ready");
    };

    auto piecePopupActionDescriptions = [&](const game_data::Piece& piece) {
        std::vector<std::pair<std::string, sf::Color>> descriptions;
        descriptions.push_back({"Position: row " + std::to_string(piece.row + 1) +
                                    ", column " + std::to_string(piece.column + 1),
                                sf::Color(190, 198, 214)});
        descriptions.push_back({
            std::string("Territory: ") +
                (piece.canControl ? "controls occupied square + adjacent influence" : "no control"),
            sf::Color(198, 180, 142)});
        if (piece.growTurnsRemaining > 0)
        {
            descriptions.push_back({"Growing: " + std::to_string(piece.growTurnsRemaining) + " turns",
                                    sf::Color(210, 180, 105)});
        }
        if (piece.disabledTurns > 0)
        {
            descriptions.push_back({"Disabled: " + std::to_string(piece.disabledTurns) + " turns",
                                    sf::Color(225, 130, 110)});
        }
        if (piece.sleepTurnsRemaining > 0)
        {
            descriptions.push_back({"Sleeping: " + std::to_string(piece.sleepTurnsRemaining) + " turns",
                                    sf::Color(120, 190, 230)});
        }
        if (!piece.ability.empty())
        {
            descriptions.push_back({"Ability: " + game_data::pieceAbilityLabel(piece), sf::Color(210, 216, 228)});
            if (piece.abilityUses > 0)
            {
                descriptions.push_back({"Ability uses: " + std::to_string(piece.abilityUses),
                                        sf::Color(190, 198, 214)});
            }
        }
        if (piece.actions.empty())
        {
            descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
        }
        for (std::size_t i = 0; i < piece.actions.size(); ++i)
        {
            descriptions.push_back({
                actionDescription(piece.actions[i], i),
                piece.actions[i].state == piece.actionState ? sf::Color(143, 220, 205) : sf::Color(190, 198, 214)});
        }
        if (piece.isHero)
        {
            descriptions.push_back({sandboxMode ? "Hero: sandbox rules" : "Hero: defeat all enemy heroes to win",
                                    sf::Color(225, 170, 150)});
            if (!piece.keywords.empty())
            {
                descriptions.push_back({"Keywords: " + joinStrings(piece.keywords, ", "), sf::Color(198, 180, 142)});
            }
        }
        else if (!piece.keywords.empty())
        {
            descriptions.push_back({"Keywords: " + joinStrings(piece.keywords, ", "), sf::Color(198, 180, 142)});
        }
        return descriptions;
    };

    auto cardPlayDescription = [&](const game_data::GameCard& card) {
        if (card.type == "Hero")
        {
            return std::string("Play: hero placement");
        }
        if (card.type == "Unit")
        {
            return "Play: " + std::to_string(card.cost) + " steam, controlled empty square";
        }
        if (card.effect == "damage")
        {
            return "Play: " + std::to_string(card.cost) + " steam, deal " +
                std::to_string(card.power) + " damage";
        }
        if (card.effect == "heal")
        {
            return "Play: " + std::to_string(card.cost) + " steam, restore " +
                std::to_string(card.power) + " health";
        }
        if (card.effect == "steam")
        {
            return "Play: " + std::to_string(card.cost) + " steam, gain " +
                std::to_string(card.power) + " steam";
        }
        return std::string("Play: ") + std::to_string(card.cost) + " steam";
    };

    auto cardPopupActionDescriptions = [&](const game_data::GameCard& card) {
        std::vector<std::pair<std::string, sf::Color>> descriptions;
        const int me = gameSnapshot.yourPlayer;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        const bool yourTurn = sandboxMode ||
            (phase == game_data::Phase::Playing && gameSnapshot.activePlayer == me);
        const bool affordable = sandboxMode || card.cost <= gameSnapshot.players[static_cast<std::size_t>(me - 1)].steam;
        const std::vector<std::string> missingKeywords =
            sandboxMode ? std::vector<std::string>{} : game_data::missingHeroKeywords(gameSnapshot.pieces, me, card);

        if (phase == game_data::Phase::GameOver)
        {
            descriptions.push_back({"Status: game over", sf::Color(210, 216, 228)});
        }
        else if (!yourTurn)
        {
            descriptions.push_back({"Status: playable on your battle turn", sf::Color(210, 216, 228)});
        }
        else if (!affordable)
        {
            descriptions.push_back({"Status: not enough steam", sf::Color(225, 170, 150)});
        }
        else if (!missingKeywords.empty())
        {
            descriptions.push_back({
                "Status: requires " + joinStrings(missingKeywords, ", "),
                sf::Color(225, 170, 150)});
        }
        else
        {
            descriptions.push_back({
                sandboxMode
                    ? "Status: playable now, free in sandbox"
                    : "Status: playable now, ends turn",
                sf::Color(120, 220, 150)});
        }

        descriptions.push_back({cardPlayDescription(card), sf::Color(210, 216, 228)});
        if (!card.keywords.empty())
        {
            descriptions.push_back({
                "Required keywords: " + joinStrings(card.keywords, ", "),
                sf::Color(198, 180, 142)});
        }
        if (card.type == "Unit" || card.type == "Hero")
        {
            if (card.actions.empty())
            {
                descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
            }
            for (std::size_t i = 0; i < card.actions.size(); ++i)
            {
                descriptions.push_back({actionDescription(card.actions[i], i), sf::Color(143, 220, 205)});
            }
            descriptions.push_back({
                "Territory: occupied square + adjacent influence",
                sf::Color(198, 180, 142)});
        }
        return descriptions;
    };

    auto popupActionContentHeight = [&](const std::vector<std::pair<std::string, sf::Color>>& descriptions) {
        float height = 0.0f;
        for (const auto& [description, color] : descriptions)
        {
            (void)color;
            height += static_cast<float>(wrapText(font, description, 14, PiecePopupTextWidth - 24.0f).size()) * 18.0f;
            height += 8.0f;
        }
        return height;
    };

    auto popupMaxScroll = [&](const std::vector<std::pair<std::string, sf::Color>>& descriptions) {
        return std::max(0.0f, popupActionContentHeight(descriptions) - PiecePopupScrollHeight);
    };

    auto drawPiecePopup = [&]() {
        if (!inspectedPieceId && !inspectedHandIndex)
        {
            return;
        }

        const game_data::Piece* piece = nullptr;
        const game_data::GameCard* card = nullptr;
        if (inspectedHandIndex)
        {
            if (*inspectedHandIndex >= gameSnapshot.hand.size())
            {
                inspectedHandIndex.reset();
                inspectedPieceScroll = 0.0f;
                return;
            }
            card = &gameSnapshot.hand[*inspectedHandIndex];
        }
        else if (inspectedPieceId)
        {
            piece = gamePieceById(*inspectedPieceId);
            if (!piece)
            {
                inspectedPieceId.reset();
                inspectedPieceScroll = 0.0f;
                return;
            }
        }

        const std::vector<std::pair<std::string, sf::Color>> actionDescriptions =
            piece ? piecePopupActionDescriptions(*piece) : cardPopupActionDescriptions(*card);

        if (!piece && !card)
        {
            return;
        }

        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(overlay);

        drawPanel(window, {PiecePopupX, PiecePopupY}, {PiecePopupWidth, PiecePopupHeight});
        drawText(window, font, piece ? piece->name : card->title, 24, {PiecePopupX + 22.0f, PiecePopupY + 18.0f},
                 sf::Color(248, 239, 216), PiecePopupWidth - 44.0f);

        sf::RectangleShape artFrame({104.0f, 104.0f});
        artFrame.setPosition({PiecePopupX + 22.0f, PiecePopupY + 62.0f});
        artFrame.setFillColor(sf::Color(8, 14, 15));
        artFrame.setOutlineThickness(1.0f);
        artFrame.setOutlineColor(sf::Color(155, 111, 59));
        window.draw(artFrame);

        bool drewArt = false;
        if (sf::Texture* art = cardArtTexture(piece ? piece->imagePath : card->imagePath))
        {
            drawContainSprite(
                *art,
                {{PiecePopupX + 30.0f, PiecePopupY + 70.0f}, {88.0f, 88.0f}});
            drewArt = true;
        }
        if (piece)
        {
            if (!drewArt)
            {
                if (sf::Texture* token = loadTexture(pieceTokenPath(*piece)))
                {
                    drawContainSprite(
                        *token,
                        {{PiecePopupX + 30.0f, PiecePopupY + 68.0f}, {88.0f, 92.0f}});
                    drewArt = true;
                }
                else if (sf::Texture* walkSheet = walkAnimTexture(pieceWalkAnimPath(*piece)))
                {
                    const int walkFrameCount = std::max(1, piece->walkAnimFrames);
                    const sf::Vector2u sheetSize = walkSheet->getSize();
                    const int frameWidth = static_cast<int>(sheetSize.x / static_cast<unsigned int>(walkFrameCount));
                    const int frameHeight = static_cast<int>(sheetSize.y);
                    if (frameWidth > 0 && frameHeight > 0)
                    {
                        drawTextureRectContain(
                            *walkSheet,
                            sf::IntRect({0, 0}, {frameWidth, frameHeight}),
                            {{PiecePopupX + 30.0f, PiecePopupY + 68.0f}, {88.0f, 92.0f}},
                            sf::Color::White);
                        drewArt = true;
                    }
                }
            }
        }

        float y = PiecePopupY + 66.0f;
        const float statX = PiecePopupX + 146.0f;
        if (piece)
        {
            const std::string ownerLabel = piece->owner == gameSnapshot.yourPlayer ? "Yours" : "Opponent";
            const std::string typeLabel = piece->isHero ? "Hero" : "Unit";
            drawText(window, font, "Type: " + typeLabel + "   Owner: " + ownerLabel +
                         " (P" + std::to_string(piece->owner) + ")",
                     15, {statX, y}, ownerColor(piece->owner), PiecePopupWidth - 174.0f);
            y += 22.0f;
            drawText(window, font, readinessDescription(*piece), 14, {statX, y}, sf::Color(210, 216, 228),
                     PiecePopupWidth - 174.0f);
            y += 22.0f;
            drawText(window, font, "Health: " + std::to_string(piece->health) + "/" + std::to_string(piece->maxHealth),
                     14, {statX, y}, sf::Color(224, 210, 176));
            y += 22.0f;
            drawText(window, font, "Actions: " + std::to_string(piece->actions.size()),
                     14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        }
        else
        {
            drawText(window, font, "Type: " + card->type + "   Location: Hand", 15, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
            y += 24.0f;
            if (card->type == "Hero")
            {
                drawText(window, font, "Hero cost: " + std::to_string(card->heroCost), 14, {statX, y}, sf::Color(248, 214, 112));
            }
            else
            {
                drawText(window, font, "Cost: " + std::to_string(card->cost) + " steam", 14, {statX, y}, sf::Color(150, 210, 235));
            }
            y += 24.0f;
            if (card->type == "Unit" || card->type == "Hero")
            {
                drawText(window, font, "Health: " + std::to_string(card->health), 14, {statX, y}, sf::Color(224, 210, 176));
                y += 22.0f;
                drawText(window, font, "Actions: " + std::to_string(card->actions.size()),
                         14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
            }
            else
            {
                drawText(window, font, "Effect: " + card->effect, 14, {statX, y}, sf::Color(224, 210, 176));
                y += 22.0f;
                drawText(window, font, "Power: " + std::to_string(card->power), 14, {statX, y}, sf::Color(224, 210, 176));
                y += 22.0f;
                drawText(window, font, "Target: " + card->target, 14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
            }
        }

        inspectedPieceScroll = std::clamp(inspectedPieceScroll, 0.0f, popupMaxScroll(actionDescriptions));

        drawText(window, font, piece ? "Details" : "Actions", 17, {PiecePopupTextX, PiecePopupActionHeadingY}, sf::Color::White);

        sf::RectangleShape scrollBack({PiecePopupTextWidth, PiecePopupScrollHeight});
        scrollBack.setPosition({PiecePopupTextX, PiecePopupScrollY});
        scrollBack.setFillColor(sf::Color(8, 14, 15, 132));
        scrollBack.setOutlineThickness(1.0f);
        scrollBack.setOutlineColor(sf::Color(44, 108, 101, 120));
        window.draw(scrollBack);

        const sf::View previousView = window.getView();
        sf::View actionView(sf::FloatRect(
            {PiecePopupTextX, PiecePopupScrollY + inspectedPieceScroll},
            {PiecePopupTextWidth, PiecePopupScrollHeight}));
        actionView.setViewport(sf::FloatRect(
            {PiecePopupTextX / 800.0f, PiecePopupScrollY / 600.0f},
            {PiecePopupTextWidth / 800.0f, PiecePopupScrollHeight / 600.0f}));
        window.setView(actionView);

        y = PiecePopupScrollY + 8.0f;
        for (const auto& [description, color] : actionDescriptions)
        {
            y = drawWrappedText(window, font, description, 14, {PiecePopupTextX + 8.0f, y}, color, PiecePopupTextWidth - 24.0f);
            y += 8.0f;
        }

        window.setView(previousView);

        const float maxScroll = popupMaxScroll(actionDescriptions);
        if (maxScroll > 0.0f)
        {
            const float trackX = PiecePopupX + PiecePopupWidth - 22.0f;
            sf::RectangleShape track({4.0f, PiecePopupScrollHeight - 12.0f});
            track.setPosition({trackX, PiecePopupScrollY + 6.0f});
            track.setFillColor(sf::Color(73, 96, 98, 170));
            window.draw(track);

            const float thumbHeight = std::max(28.0f, track.getSize().y * (PiecePopupScrollHeight / (PiecePopupScrollHeight + maxScroll)));
            const float thumbY = track.getPosition().y +
                (track.getSize().y - thumbHeight) * (inspectedPieceScroll / maxScroll);
            sf::RectangleShape thumb({4.0f, thumbHeight});
            thumb.setPosition({trackX, thumbY});
            thumb.setFillColor(sf::Color(143, 220, 205, 230));
            window.draw(thumb);
        }

        if (canDiscardInspectedHandCard())
        {
            discardCardButton.draw(window);
        }
        closePiecePopupButton.draw(window);
    };

    auto drawGame = [&]() {
        if (!haveSnapshot)
        {
            drawText(
                window,
                font,
                sandboxMode ? "Loading sandbox..." : "Connecting to match...",
                24,
                {260.0f, 280.0f},
                sf::Color(200, 208, 222));
            leaveGameButton.draw(window);
            return;
        }

        const int me = gameSnapshot.yourPlayer;
        const int sandboxPlayer = sandboxMode ? sandboxPlacementPlayer : me;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        const game_data::Piece* selectedPiece = selectedPieceId ? gamePieceById(*selectedPieceId) : nullptr;
        const game_data::Piece* draggedPiece =
            gameDragKind == GameDragKind::Piece && draggingPieceId ? gamePieceById(*draggingPieceId) : nullptr;
        const game_data::Piece* actingPiece = draggedPiece ? draggedPiece : selectedPiece;
        const std::optional<std::pair<int, int>> draggedPieceSquare =
            gameDragActive && draggedPiece ? squareAtPixel(gameDragCurrentPos) : std::nullopt;
        bool draggedPieceDropValid = false;
        if (draggedPiece && draggedPieceSquare)
        {
            const game_data::ActionResolution action = game_data::resolvePieceAction(
                gameSnapshot.pieces,
                gameSnapshot.holes,
                *draggedPiece,
                draggedPieceSquare->first,
                draggedPieceSquare->second);
            draggedPieceDropValid = phase == game_data::Phase::Playing &&
                (sandboxMode ||
                 (gameSnapshot.activePlayer == me &&
                  draggedPiece->owner == me &&
                  !draggedPiece->hasActed)) &&
                action.legal;
        }
        const std::optional<std::size_t> actingHandIndex =
            gameDragKind == GameDragKind::HandCard && draggingHandIndex ? draggingHandIndex : selectedHandIndex;
        const game_data::GameCard* draggedHandCard =
            gameDragActive && gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
                *draggingHandIndex < gameSnapshot.hand.size()
            ? &gameSnapshot.hand[*draggingHandIndex]
            : nullptr;
        const bool draggingPieceCard = draggedHandCard &&
            (draggedHandCard->type == "Unit" || draggedHandCard->type == "Hero");
        const std::optional<std::pair<int, int>> draggedHandSquare =
            draggingPieceCard ? squareAtPixel(gameDragCurrentPos) : std::nullopt;
        bool draggedHandDropValid = false;
        if (draggedHandCard && draggedHandSquare)
        {
            const auto [row, column] = *draggedHandSquare;
            const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
            if (phase == game_data::Phase::HeroPlacement && draggedHandCard->type == "Hero")
            {
                const auto home = game_data::homeSquares(me);
                draggedHandDropValid = !gamePieceAt(row, column) &&
                    std::find(home.begin(), home.end(), std::pair<int, int>{row, column}) != home.end();
            }
            else if (phase == game_data::Phase::Playing &&
                     (draggedHandCard->type == "Unit" || (sandboxMode && draggedHandCard->type == "Hero")))
            {
                draggedHandDropValid = (sandboxMode || gameSnapshot.activePlayer == me) &&
                    (sandboxMode || draggedHandCard->cost <= gameSnapshot.players[static_cast<std::size_t>(me - 1)].steam) &&
                    (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, *draggedHandCard)) &&
                    !gamePieceAt(row, column) &&
                    gameSnapshot.control[idx] == sandboxPlayer;
            }
        }

        // Precompute highlight masks for the current selection.
        std::array<int, game_data::BoardSquares> highlight{};  // 0 none,1 move,2 attack,3 place,4 spell
        if (phase == game_data::Phase::HeroPlacement &&
            gameSnapshot.players[static_cast<std::size_t>(me - 1)].heroesToPlace > 0)
        {
            for (const auto& [r, c] : game_data::homeSquares(me))
            {
                if (!gamePieceAt(r, c))
                {
                    highlight[static_cast<std::size_t>(game_data::squareIndex(r, c))] = 3;
                }
            }
        }
        else if (phase == game_data::Phase::Playing && (sandboxMode || gameSnapshot.activePlayer == me))
        {
            if (actingPiece && (sandboxMode || !actingPiece->hasActed))
            {
                for (int r = 0; r < game_data::BoardSize; ++r)
                {
                    for (int c = 0; c < game_data::BoardSize; ++c)
                    {
                        const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                        const game_data::ActionResolution action = game_data::resolvePieceAction(
                            gameSnapshot.pieces, gameSnapshot.holes, *actingPiece, r, c);
                        if (action.legal)
                        {
                            highlight[idx] = action.attacks ? 2 : 1;
                        }
                    }
                }
            }
            else if (actingHandIndex && *actingHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& card = gameSnapshot.hand[*actingHandIndex];
                if (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, card))
                {
                    for (int r = 0; r < game_data::BoardSize; ++r)
                    {
                        for (int c = 0; c < game_data::BoardSize; ++c)
                        {
                            const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                            const game_data::Piece* occupant = gamePieceAt(r, c);
                            if (card.type == "Unit" || (sandboxMode && card.type == "Hero"))
                            {
                                if (!occupant && gameSnapshot.control[idx] == sandboxPlayer)
                                {
                                    highlight[idx] = 3;
                                }
                            }
                            else if (card.effect == "damage" && occupant && occupant->owner != sandboxPlayer)
                            {
                                highlight[idx] = 2;
                            }
                            else if (card.effect == "heal" && occupant && occupant->owner == sandboxPlayer)
                            {
                                highlight[idx] = 4;
                            }
                        }
                    }
                }
            }
        }

        const std::array<sf::Vector2f, 4> boardTop = {
            boardEdgePoint(0, 0),
            boardEdgePoint(0, game_data::BoardSize),
            boardEdgePoint(game_data::BoardSize, game_data::BoardSize),
            boardEdgePoint(game_data::BoardSize, 0)};
        drawQuad(offsetQuad(boardTop, {7.0f, 15.0f}), sf::Color(0, 0, 0, 95));

        const sf::Vector2f topLeft = boardTop[0];
        const sf::Vector2f topRight = boardTop[1];
        const sf::Vector2f bottomRight = boardTop[2];
        const sf::Vector2f bottomLeft = boardTop[3];
        drawQuad(
            {topRight, bottomRight, {bottomRight.x + 10.0f, bottomRight.y + BoardThickness},
             {topRight.x + 5.0f, topRight.y + BoardThickness * 0.42f}},
            sf::Color(7, 28, 31, 238),
            1.0f,
            sf::Color(35, 83, 77, 170));
        drawQuad(
            {topLeft, {topLeft.x - 5.0f, topLeft.y + BoardThickness * 0.42f},
             {bottomLeft.x - 10.0f, bottomLeft.y + BoardThickness}, bottomLeft},
            sf::Color(8, 24, 27, 238),
            1.0f,
            sf::Color(35, 83, 77, 170));
        drawQuad(
            {bottomLeft, bottomRight, {bottomRight.x + 10.0f, bottomRight.y + BoardThickness},
             {bottomLeft.x - 10.0f, bottomLeft.y + BoardThickness}},
            sf::Color(77, 49, 28, 246),
            1.0f,
            sf::Color(167, 112, 56, 190));
        drawQuad(boardTop, sf::Color(9, 20, 21, 232), 3.0f, sf::Color(153, 105, 51));

        // Board squares.
        for (int screenRow = 0; screenRow < game_data::BoardSize; ++screenRow)
        {
            const int row = rowForScreenRow(screenRow, me);
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
                const BoardCellMetrics metrics = boardCellMetrics(row, column);
                drawQuad(metrics.corners, ownerTint(gameSnapshot.control[idx]), 1.0f, sf::Color(81, 63, 37));

                if ((row + column) % 2 == 0)
                {
                    drawQuad(metrics.corners, sf::Color(255, 239, 190, 16));
                }

                if (gameSnapshot.holes[idx] != 0)
                {
                    const sf::Vector2f anchor = boardCellAnchor(metrics);
                    const float radius = 8.0f * metrics.depthScale;
                    sf::CircleShape hole(radius);
                    hole.setScale({1.0f, 0.48f});
                    hole.setPosition({anchor.x - radius, anchor.y - radius * 0.42f});
                    hole.setFillColor(sf::Color(3, 7, 8, 225));
                    hole.setOutlineThickness(1.5f);
                    hole.setOutlineColor(sf::Color(108, 78, 46));
                    window.draw(hole);
                }

                if (highlight[idx] != 0)
                {
                    sf::Color colors[5] = {
                        sf::Color::Transparent,
                        sf::Color(90, 200, 120, 90),
                        sf::Color(220, 90, 80, 110),
                        sf::Color(90, 200, 210, 90),
                        sf::Color(110, 200, 150, 90)};
                    drawQuad(metrics.corners, colors[highlight[idx]]);
                }

                if (draggedHandSquare &&
                    draggedHandSquare->first == row &&
                    draggedHandSquare->second == column)
                {
                    drawQuad(
                        metrics.corners,
                        draggedHandDropValid
                            ? sf::Color(90, 225, 170, 125)
                            : sf::Color(225, 75, 65, 125),
                        2.5f,
                        draggedHandDropValid
                            ? sf::Color(145, 255, 215, 235)
                            : sf::Color(255, 135, 120, 235));
                }

                if (draggedPieceSquare &&
                    draggedPieceSquare->first == row &&
                    draggedPieceSquare->second == column)
                {
                    drawQuad(
                        metrics.corners,
                        draggedPieceDropValid
                            ? sf::Color(90, 225, 170, 125)
                            : sf::Color(225, 75, 65, 125),
                        2.5f,
                        draggedPieceDropValid
                            ? sf::Color(145, 255, 215, 235)
                            : sf::Color(255, 135, 120, 235));
                }
            }
        }

        // Pieces.
        std::vector<const game_data::Piece*> pieceDrawOrder;
        pieceDrawOrder.reserve(gameSnapshot.pieces.size());
        for (const game_data::Piece& piece : gameSnapshot.pieces)
        {
            if (gameDragActive && draggedPiece && piece.id == draggedPiece->id)
            {
                continue;
            }
            pieceDrawOrder.push_back(&piece);
        }
        std::sort(pieceDrawOrder.begin(), pieceDrawOrder.end(), [&](const game_data::Piece* a, const game_data::Piece* b) {
            const BoardCellMetrics aCell = boardCellMetrics(a->row, a->column);
            const BoardCellMetrics bCell = boardCellMetrics(b->row, b->column);
            if (aCell.screenRow != bCell.screenRow)
            {
                return aCell.screenRow < bCell.screenRow;
            }
            return a->column < b->column;
        });

        for (const game_data::Piece* piecePtr : pieceDrawOrder)
        {
            const game_data::Piece& piece = *piecePtr;
            BoardCellMetrics cell = boardCellMetrics(piece.row, piece.column);
            sf::Vector2f anchor = boardCellAnchor(cell);
            float pieceScale = cell.depthScale;
            bool isMoving = false;
            float walkAnimationElapsed = 0.0f;
            if (const auto animation = pieceMoveAnimations.find(piece.id); animation != pieceMoveAnimations.end())
            {
                walkAnimationElapsed = std::max(0.0f, animationTime - animation->second.startTime);
                const float progress = std::min(walkAnimationElapsed / animation->second.duration, 1.0f);
                if (progress < 1.0f)
                {
                    isMoving = true;
                    const BoardCellMetrics startCell = boardCellMetricsForViewer(
                        animation->second.fromRow, animation->second.fromColumn, gameSnapshot.yourPlayer);
                    const BoardCellMetrics endCell = boardCellMetricsForViewer(
                        animation->second.toRow, animation->second.toColumn, gameSnapshot.yourPlayer);
                    const sf::Vector2f start = boardCellAnchor(startCell);
                    const sf::Vector2f end = boardCellAnchor(endCell);
                    anchor = {
                        start.x + (end.x - start.x) * progress,
                        start.y + (end.y - start.y) * progress};
                    pieceScale = startCell.depthScale + (endCell.depthScale - startCell.depthScale) * progress;
                }
                else
                {
                    pieceMoveAnimations.erase(piece.id);
                }
            }

            const bool pieceUnavailable =
                (piece.hasActed && piece.owner == gameSnapshot.activePlayer) || piece.disabledTurns > 0;
            sf::Color color = ownerColor(piece.owner);
            if (pieceUnavailable)
            {
                color = sf::Color(static_cast<std::uint8_t>(color.r * 0.55f),
                                  static_cast<std::uint8_t>(color.g * 0.55f),
                                  static_cast<std::uint8_t>(color.b * 0.55f));
            }

            const std::string& walkPath = pieceWalkAnimPath(piece);
            const std::string& tokenPath = pieceTokenPath(piece);
            if (isMoving && !walkPath.empty() && walkAnimTexture(walkPath) != nullptr)
            {
                if (sf::Texture* walkSheet = walkAnimTexture(walkPath))
                {
                    const int walkFrameCount = std::max(1, piece.walkAnimFrames);
                    const sf::Vector2u sheetSize = walkSheet->getSize();
                    const int frameWidth = static_cast<int>(
                        sheetSize.x / static_cast<unsigned int>(walkFrameCount));
                    const int frameHeight = static_cast<int>(sheetSize.y);
                    if (frameWidth > 0 && frameHeight > 0)
                    {
                        const float loopProgress =
                            std::fmod(walkAnimationElapsed, WalkAnimationLoopSeconds) /
                            WalkAnimationLoopSeconds;
                        const int frame = std::min(
                            static_cast<int>(loopProgress * static_cast<float>(walkFrameCount)),
                            walkFrameCount - 1);
                        drawTextureRectContain(
                            *walkSheet,
                            sf::IntRect({frame * frameWidth, 0}, {frameWidth, frameHeight}),
                            pieceTargetRect(anchor, pieceScale, true),
                            pieceUnavailable
                                ? sf::Color(150, 150, 150, 215)
                                : sf::Color::White);
                    }
                }
            }
            else if (sf::Texture* token = loadTexture(tokenPath))
            {
                drawContainSprite(
                    *token,
                    pieceTargetRect(anchor, pieceScale, true),
                    piece.hasActed && piece.owner == gameSnapshot.activePlayer
                        ? sf::Color(150, 150, 150, 215)
                        : sf::Color::White);
            }
            else if (sf::Texture* walkSheet = walkAnimTexture(walkPath))
            {
                const int walkFrameCount = std::max(1, piece.walkAnimFrames);
                const sf::Vector2u sheetSize = walkSheet->getSize();
                const int frameWidth = static_cast<int>(
                    sheetSize.x / static_cast<unsigned int>(walkFrameCount));
                const int frameHeight = static_cast<int>(sheetSize.y);
                if (frameWidth > 0 && frameHeight > 0)
                {
                    drawTextureRectContain(
                        *walkSheet,
                        sf::IntRect({0, 0}, {frameWidth, frameHeight}),
                        pieceTargetRect(anchor, pieceScale, true),
                        pieceUnavailable
                            ? sf::Color(150, 150, 150, 215)
                            : sf::Color::White);
                }
            }
            else if (sf::Texture* art = cardArtTexture(piece.imagePath))
            {
                drawContainSprite(*art, pieceTargetRect(anchor, pieceScale, false),
                                  pieceUnavailable
                                      ? sf::Color(130, 130, 130)
                                      : sf::Color::White);
            }
            else
            {
                const float radius = PieceBaseWidth * 0.28f * pieceScale;
                sf::CircleShape body(radius);
                body.setPosition({anchor.x - radius, anchor.y - radius * 2.0f});
                body.setFillColor(color);
                window.draw(body);
            }
            const unsigned int healthSize = static_cast<unsigned int>(std::clamp(12.0f * pieceScale, 10.0f, 17.0f));
            drawText(window, font, std::to_string(piece.health), healthSize,
                     {anchor.x - 5.0f * pieceScale, anchor.y - 21.0f * pieceScale}, sf::Color(248, 239, 216));
        }

        // Compact game readout.
        const game_data::PlayerSnapshot& mine = gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        const int activePlayer = std::clamp(gameSnapshot.activePlayer, 1, 2);
        const game_data::PlayerSnapshot& activePlayerSnapshot =
            gameSnapshot.players[static_cast<std::size_t>(activePlayer - 1)];
        const std::string activePlayerName = sandboxMode
            ? "Player " + std::to_string(activePlayer)
            : (activePlayer == me ? loggedInUsername : "Opponent");
        const std::string steamText = sandboxMode ? "free" : std::to_string(activePlayerSnapshot.steam);
        drawText(window, font, "Turn: " + activePlayerName, 16, {BoardOriginX, GameLabelY},
                 ownerColor(activePlayer), 240.0f);
        drawText(window, font, "Steam: " + steamText, 16, {282.0f, GameLabelY},
                 sf::Color(150, 210, 235), 100.0f);

        if (phase == game_data::Phase::Playing && (sandboxMode || gameSnapshot.activePlayer == me))
        {
            if (selectedPiece && (sandboxMode || (selectedPiece->owner == me && !selectedPiece->hasActed)) &&
                !selectedPiece->ability.empty() && selectedPiece->growTurnsRemaining == 0 &&
                selectedPiece->disabledTurns == 0 &&
                (selectedPiece->ability != "dig" || selectedPiece->abilityUses > 0))
            {
                abilityButton.setLabel(game_data::pieceAbilityLabel(*selectedPiece));
                abilityButton.draw(window);
            }
            if (sandboxMode)
            {
                sandboxPlayerButton.draw(window);
            }
            else
            {
                endTurnButton.draw(window);
            }
        }
        leaveGameButton.draw(window);

        // Hand.
        clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
        const std::size_t lastHandCard = std::min(gameSnapshot.hand.size(), gameHandOffset + VisibleGameHandCards);
        if (gameSnapshot.hand.size() > VisibleGameHandCards)
        {
            drawText(
                window,
                font,
                "Cards " + std::to_string(gameHandOffset + 1) + "-" +
                    std::to_string(lastHandCard) + "/" + std::to_string(gameSnapshot.hand.size()),
                12,
                {HandStartX, HandY - 18.0f},
                sf::Color(190, 198, 214),
                240.0f);
        }
        for (std::size_t i = gameHandOffset; i < lastHandCard; ++i)
        {
            const float x = HandStartX + static_cast<float>(i - gameHandOffset) * (HandCardWidth + HandGap);
            const game_data::GameCard& card = gameSnapshot.hand[i];
            const bool affordable = phase == game_data::Phase::HeroPlacement ||
                ((sandboxMode || card.cost <= mine.steam) && (sandboxMode || gameSnapshot.activePlayer == me) &&
                 phase == game_data::Phase::Playing &&
                 (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, card)));
            drawGameCardFace({x, HandY}, card, selectedHandIndex && *selectedHandIndex == i, affordable);
        }

        if (gameDragActive)
        {
            if (gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
                *draggingHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& draggedCard = gameSnapshot.hand[*draggingHandIndex];
                if (draggedCard.type == "Unit" || draggedCard.type == "Hero")
                {
                    sf::Vector2f anchor = gameDragCurrentPos;
                    float scale = 1.0f;
                    if (draggedHandSquare)
                    {
                        const BoardCellMetrics metrics =
                            boardCellMetrics(draggedHandSquare->first, draggedHandSquare->second);
                        anchor = boardCellAnchor(metrics);
                        scale = metrics.depthScale;
                    }
                    drawCardPiecePreview(draggedCard, sandboxPlayer, anchor, scale, draggedHandDropValid);
                }
                else
                {
                    const bool affordable = (sandboxMode || draggedCard.cost <= mine.steam) &&
                        (sandboxMode || gameSnapshot.activePlayer == me) && phase == game_data::Phase::Playing &&
                        (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, draggedCard));
                    drawGameCardFace(
                        {gameDragCurrentPos.x - HandCardWidth / 2.0f,
                         gameDragCurrentPos.y - HandCardHeight / 2.0f},
                        draggedCard,
                        true,
                        affordable);
                }
            }
            else if (gameDragKind == GameDragKind::Piece && draggedPiece)
            {
                sf::Vector2f anchor = gameDragCurrentPos;
                float scale = 1.0f;
                if (draggedPieceSquare)
                {
                    const BoardCellMetrics metrics =
                        boardCellMetrics(draggedPieceSquare->first, draggedPieceSquare->second);
                    anchor = boardCellAnchor(metrics);
                    scale = metrics.depthScale;
                }
                const sf::Color tint = draggedPieceDropValid
                    ? sf::Color(255, 255, 255, 220)
                    : sf::Color(220, 120, 110, 190);
                bool drewPiece = false;

                if (sf::Texture* token = loadTexture(pieceTokenPath(*draggedPiece)))
                {
                    drawContainSprite(*token, pieceTargetRect(anchor, scale, true), tint);
                    drewPiece = true;
                }
                else if (sf::Texture* walkSheet = walkAnimTexture(pieceWalkAnimPath(*draggedPiece)))
                {
                    const int walkFrameCount = std::max(1, draggedPiece->walkAnimFrames);
                    const sf::Vector2u sheetSize = walkSheet->getSize();
                    const int frameWidth = static_cast<int>(
                        sheetSize.x / static_cast<unsigned int>(walkFrameCount));
                    const int frameHeight = static_cast<int>(sheetSize.y);
                    if (frameWidth > 0 && frameHeight > 0)
                    {
                        drawTextureRectContain(
                            *walkSheet,
                            sf::IntRect({0, 0}, {frameWidth, frameHeight}),
                            pieceTargetRect(anchor, scale, true),
                            tint);
                        drewPiece = true;
                    }
                }
                else if (sf::Texture* art = cardArtTexture(draggedPiece->imagePath))
                {
                    drawContainSprite(*art, pieceTargetRect(anchor, scale, false), tint);
                    drewPiece = true;
                }

                if (!drewPiece)
                {
                    const float radius = PieceBaseWidth * 0.28f * scale;
                    sf::CircleShape body(radius);
                    body.setPosition({anchor.x - radius, anchor.y - radius * 2.0f});
                    body.setFillColor(
                        draggedPieceDropValid
                            ? ownerColor(draggedPiece->owner)
                            : sf::Color(180, 75, 65, 210));
                    window.draw(body);
                }

                const unsigned int healthSize =
                    static_cast<unsigned int>(std::clamp(12.0f * scale, 10.0f, 17.0f));
                drawText(
                    window,
                    font,
                    std::to_string(draggedPiece->health),
                    healthSize,
                    {anchor.x - 5.0f * scale, anchor.y - 21.0f * scale},
                    sf::Color(248, 239, 216, 220));
            }
        }

        // Game-over banner.
        if (phase == game_data::Phase::GameOver)
        {
            sf::RectangleShape banner({420.0f, 126.0f});
            banner.setPosition({40.0f, 210.0f});
            banner.setFillColor(sf::Color(20, 24, 32, 235));
            banner.setOutlineThickness(2.0f);
            banner.setOutlineColor(gameSnapshot.winner == me ? sf::Color(120, 220, 150) : sf::Color(220, 110, 90));
            window.draw(banner);
            const std::string result = gameSnapshot.winner == me ? "Victory!" : "Defeat";
            drawText(window, font, result, 34, {60.0f, 224.0f}, gameSnapshot.winner == me ? sf::Color(140, 230, 160) : sf::Color(230, 130, 110));
            const std::string ratingText = gameResultReceived && gameResultSuccess
                ? "Rating " +
                    std::string(gameRatingChange >= 0 ? "+" : "") +
                    std::to_string(gameRatingChange)
                : (gameResultReceived
                    ? "Rating update unavailable"
                    : "Rating update pending...");
            drawText(
                window,
                font,
                ratingText,
                18,
                {60.0f, 264.0f},
                sf::Color(151, 192, 255),
                360.0f);
            if (!gameRewardText.empty())
            {
                drawText(window, font, gameRewardText, 16, {60.0f, 288.0f}, sf::Color(248, 214, 112), 360.0f);
            }
            drawText(window, font, "Press Leave to return.", 14, {60.0f, 314.0f}, sf::Color(200, 206, 220));
        }

        drawPiecePopup();
    };

    auto drawDeckSelect = [&]() {
        drawPanel(window, {250.0f, 120.0f}, {300.0f, 312.0f});
        drawText(window, font, "Your Decks", 22, {266.0f, 132.0f}, sf::Color::White);
        drawText(window, font, "Coins " + std::to_string(playerCoins), 14, {430.0f, 138.0f}, sf::Color(248, 214, 112), 100.0f);

        const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
        for (std::size_t i = deckListOffset; i < lastDeck; ++i)
        {
            const float rowY = 172.0f + static_cast<float>(i - deckListOffset) * DeckRowHeight;
            drawRow(window, font, {266.0f, rowY}, {268.0f, DeckRowHeight - 4.0f},
                    playerDecks[i].name,
                    std::to_string(playerDecks[i].cardTitles.size()) + " cards",
                    selectedDeck && *selectedDeck == i);
        }
        if (playerDecks.empty() && !pendingPlayLoad)
        {
            drawText(window, font, "No decks. Build one in the", 15, {268.0f, 220.0f}, sf::Color(190, 198, 214));
            drawText(window, font, "Deck Editor first.", 15, {268.0f, 242.0f}, sf::Color(190, 198, 214));
        }

        findMatchButton.draw(window);
        backButton.draw(window);
        window.draw(messageText);
    };

    if (const std::optional<std::string> savedToken = loadRememberToken())
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
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (currentState == GameState::Game)
        {
            pollGameSocket();
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
                    title.setString("Steam Tactics");
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
                    currentState = GameState::Menu;
                    title.setString("Steam Tactics");
                    centerText(title, 400.0f);
                    setMessageY(messageText, 450.0f);
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
            sortDecks();
            deckListOffset = 0;
            selectedDeck = playerDecks.empty() ? std::nullopt : std::optional<std::size_t>(0);
            if (result.success)
            {
                setMessage(messageText,
                           playerDecks.empty() ? "No decks yet. Build one in the Deck Editor."
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
                selectedLibraryCard = cardLibrary.empty() ? std::nullopt : std::optional<std::size_t>(0);
                if (!playerDecks.empty())
                {
                    selectDeck(0);
                }
                else
                {
                    createNewDeck();
                    deckNameInput.setActive(false);
                }
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                cardLibrary = std::move(result.cards);
                playerCoins = result.coins;
                playerCollection = std::move(result.collection);
                playerDecks.clear();
                createNewDeck();
                deckNameInput.setActive(false);
                setMessage(messageText, result.message, sf::Color::Red);
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
                    selectDeck(0);
                }
                else
                {
                    createNewDeck();
                    deckNameInput.setActive(false);
                }
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
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
                        updateRememberMeLabel();
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
                else if (currentState == GameState::Options)
                {
                    if (displayModeButton.isClicked(clickPos))
                    {
                        pendingDisplaySettings.fullscreen = !pendingDisplaySettings.fullscreen;
                        updateOptionsLabels();
                    }
                    else if (previousResolutionButton.isClicked(clickPos))
                    {
                        selectedResolution = selectedResolution == 0
                            ? displayResolutions.size() - 1
                            : selectedResolution - 1;
                        updateOptionsLabels();
                    }
                    else if (nextResolutionButton.isClicked(clickPos))
                    {
                        selectedResolution = (selectedResolution + 1) % displayResolutions.size();
                        updateOptionsLabels();
                    }
                    else if (applyOptionsButton.isClicked(clickPos))
                    {
                        const sf::Vector2u size = displayResolutions[selectedResolution];
                        pendingDisplaySettings.width = size.x;
                        pendingDisplaySettings.height = size.y;
                        createDisplayWindow(pendingDisplaySettings);
                        displaySettings = pendingDisplaySettings;
                        const sf::Vector2u appliedSize{displaySettings.width, displaySettings.height};
                        if (const auto applied = std::find(displayResolutions.begin(), displayResolutions.end(), appliedSize);
                            applied != displayResolutions.end())
                        {
                            selectedResolution = static_cast<std::size_t>(std::distance(displayResolutions.begin(), applied));
                        }
                        updateOptionsLabels();
                        const bool saved = saveDisplaySettings(displaySettings);
                        setMessage(
                            messageText,
                            saved ? "Display settings applied and saved." : "Settings applied, but could not be saved.",
                            saved ? sf::Color(120, 220, 150) : sf::Color::Red);
                    }
                    else if (optionsReturnState == GameState::Authenticated &&
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
                    else if (changePasswordVisibilityButton.isClicked(clickPos))
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
                    else if (rememberMeButton.isClicked(clickPos))
                    {
                        rememberMeChecked = !rememberMeChecked;
                        updateRememberMeLabel();
                    }
                    else if (passwordVisibilityButton.isClicked(clickPos))
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
                    else if (passwordVisibilityButton.isClicked(clickPos))
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
                    if (playButton.isClicked(clickPos))
                    {
                        showDeckSelect();
                    }
                    else if (sandboxButton.isClicked(clickPos))
                    {
                        loadSandbox();
                    }
                    else if (deckEditorButton.isClicked(clickPos))
                    {
                        loadDeckEditor();
                    }
                    else if (shopButton.isClicked(clickPos))
                    {
                        loadShop();
                    }
                    else if (loggedInIsAdmin && adminCardEditorButton.isClicked(clickPos))
                    {
                        showCardEditorScreen();
                    }
                    else if (loggedInIsAdmin && adminUsersButton.isClicked(clickPos))
                    {
                        adminUsersPage = 0;
                        loadAdminUsersScreen();
                    }
                    else if (authenticatedOptionsButton.isClicked(clickPos))
                    {
                        showOptionsScreen(GameState::Authenticated);
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
                    else if (adminBackButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
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
                            piece && (sandboxMode || (piece->owner == gameSnapshot.yourPlayer && !piece->hasActed)) &&
                            !piece->ability.empty() && piece->growTurnsRemaining == 0 &&
                            piece->disabledTurns == 0 &&
                            (piece->ability != "dig" || piece->abilityUses > 0))
                        {
                            pendingHandClickIndex.reset();
                            sendUseAbility(piece->id);
                            selectedPieceId.reset();
                            selectedHandIndex.reset();
                        }
                    }
                    else if (sandboxMode && sandboxPlayerButton.isClicked(clickPos))
                    {
                        pendingHandClickIndex.reset();
                        toggleSandboxPlacementPlayer();
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
                        showAuthenticatedScreen();
                    }
                    else if (!deckEditorBusy())
                    {
                        if (newDeckButton.isClicked(clickPos))
                        {
                            createNewDeck();
                            setMessage(messageText, "Editing a new deck. Save to store it.", sf::Color::Yellow);
                        }
                        else if (refreshDeckButton.isClicked(clickPos))
                        {
                            loadDeckEditor();
                        }
                        else if (deleteDeckButton.isClicked(clickPos))
                        {
                            deleteCurrentDeck();
                        }
                        else if (removeCardButton.isClicked(clickPos))
                        {
                            removeSelectedCard();
                        }
                        else if (addCardButton.isClicked(clickPos))
                        {
                            addSelectedCard();
                        }
                        else if (saveDeckButton.isClicked(clickPos))
                        {
                            saveCurrentDeck();
                        }
                        else if (deckNameInput.contains(clickPos))
                        {
                            clearFocus();
                            deckNameInput.setActive(true);
                        }
                        else if (const std::optional<std::size_t> deckIndex = rowIndexAt(
                                     clickPos,
                                     DeckListX,
                                     DeckListY,
                                     DeckListWidth,
                                     DeckRowHeight,
                                     VisibleDeckRows,
                                     deckListOffset,
                                     playerDecks.size()))
                        {
                            selectDeck(*deckIndex);
                        }
                        else if (const std::optional<std::size_t> cardIndex = rowIndexAt(
                                     clickPos,
                                     DeckCardsX,
                                     DeckCardsY,
                                     DeckCardsWidth,
                                     DeckCardRowHeight,
                                     VisibleDeckCardRows,
                                     deckCardListOffset,
                                     editingDeck.cardTitles.size()))
                        {
                            clearFocus();
                            selectedDeckCard = *cardIndex;
                            showDeckEditorCardPopupIfDoubleClick(editingDeck.cardTitles[*cardIndex], clickPos);
                        }
                        else if (const std::optional<std::size_t> libraryIndex = rowIndexAt(
                                     clickPos,
                                     LibraryX,
                                     LibraryY,
                                     LibraryWidth,
                                     LibraryRowHeight,
                                     VisibleLibraryRows,
                                     libraryOffset,
                                     cardLibrary.size()))
                        {
                            clearFocus();
                            selectedLibraryCard = *libraryIndex;
                            if (!showDeckEditorCardPopupIfDoubleClick(cardLibrary[*libraryIndex].title, clickPos))
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
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();
                mouseMoved && currentState == GameState::DeckEditor && draggingLibraryCard)
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
                mouseReleased && mouseReleased->button == sf::Mouse::Button::Left && currentState == GameState::DeckEditor)
            {
                const sf::Vector2f releasePos = window.mapPixelToCoords(mouseReleased->position);
                if (draggingLibraryCard && dragActive &&
                    isInsideRect(releasePos, CurrentDeckPanelX, DeckEditorPanelY, 250.0f, DeckEditorPanelHeight))
                {
                    addLibraryCardToDeck(*draggingLibraryCard, "Card dropped into deck. Save to keep changes.");
                }

                draggingLibraryCard.reset();
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

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>(); wheel && currentState == GameState::DeckEditor)
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
                        const std::vector<std::pair<std::string, sf::Color>> details = deckEditorCardDetails(*card);
                        inspectedDeckEditorCardScroll = std::clamp(
                            inspectedDeckEditorCardScroll - wheel->delta * 34.0f,
                            0.0f,
                            deckEditorCardDetailsMaxScroll(details));
                    }
                }
                else if (!inspectedDeckEditorCardTitle &&
                         isInsideRect(wheelPos, DeckListX, DeckListY, DeckListWidth, DeckRowHeight * VisibleDeckRows))
                {
                    scrollList(deckListOffset, playerDecks.size(), VisibleDeckRows, wheel->delta);
                }
                else if (!inspectedDeckEditorCardTitle &&
                         isInsideRect(wheelPos, DeckCardsX, DeckCardsY, DeckCardsWidth, DeckCardRowHeight * VisibleDeckCardRows))
                {
                    scrollList(deckCardListOffset, editingDeck.cardTitles.size(), VisibleDeckCardRows, wheel->delta);
                }
                else if (!inspectedDeckEditorCardTitle &&
                         isInsideRect(wheelPos, LibraryX, LibraryY, LibraryWidth, LibraryRowHeight * VisibleLibraryRows))
                {
                    scrollList(libraryOffset, cardLibrary.size(), VisibleLibraryRows, wheel->delta);
                }
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>();
                wheel && currentState == GameState::Game && (inspectedPieceId || inspectedHandIndex))
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                if (isInsideRect(wheelPos, PiecePopupTextX, PiecePopupScrollY, PiecePopupTextWidth, PiecePopupScrollHeight))
                {
                    std::vector<std::pair<std::string, sf::Color>> actionDescriptions;
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
            if (currentState == GameState::AdminUsers && !deleteUserPopupVisible)
            {
                adminSearchInput.handleEvent(*event, window);
                adminGoldInput.handleEvent(*event, window);
            }

            if (currentState == GameState::CardEditor)
            {
                if (cardEditorScreen.handleEvent(*event, window))
                {
                    showAuthenticatedScreen();
                }
                continue;
            }

            if (currentState == GameState::CreateAccount)
            {
                confirmInput.handleEvent(*event, window);
            }

            if (currentState == GameState::DeckEditor && !deckEditorBusy() && !inspectedDeckEditorCardTitle)
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
                    else if (currentState == GameState::AdminUsers)
                    {
                        showAuthenticatedScreen();
                    }
                    else if (currentState == GameState::DeckEditor && !deckEditorBusy())
                    {
                        showAuthenticatedScreen();
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
                    else if (currentState == GameState::DeckEditor || currentState == GameState::Shop)
                    {
                        // Busy editor/shop requests keep their screen until they finish.
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
                else if (currentState == GameState::AdminUsers && !deleteUserPopupVisible)
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
                    if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        saveCurrentDeck();
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Delete && !deckNameInput.active)
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
            displayModeButton.update(mousePos);
            previousResolutionButton.update(mousePos);
            resolutionButton.update(mousePos);
            nextResolutionButton.update(mousePos);
            applyOptionsButton.update(mousePos);
            if (optionsReturnState == GameState::Authenticated)
            {
                changePasswordOptionButton.update(mousePos);
            }
            optionsBackButton.update(mousePos);
        }
        else if (currentState == GameState::ChangePassword)
        {
            if (passwordChangedPopupVisible)
            {
                dismissPasswordChangedButton.update(mousePos);
            }
            else
            {
                changePasswordVisibilityButton.update(mousePos);
                changePasswordSubmitButton.update(mousePos);
                changePasswordBackButton.update(mousePos);
                currentPasswordInput.updateCursor(deltaTime);
                newPasswordInput.updateCursor(deltaTime);
                confirmNewPasswordInput.updateCursor(deltaTime);
            }
        }
        else if (currentState == GameState::Login)
        {
            rememberMeButton.update(mousePos);
            passwordVisibilityButton.update(mousePos);
            loginSubmitButton.update(mousePos);
            backButton.update(mousePos);
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            passwordVisibilityButton.update(mousePos);
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
            if (exitDesktopPopupVisible)
            {
                cancelExitDesktopButton.update(mousePos);
                confirmExitDesktopButton.update(mousePos);
            }
            else
            {
                playButton.update(mousePos);
                sandboxButton.update(mousePos);
                deckEditorButton.update(mousePos);
                shopButton.update(mousePos);
                if (loggedInIsAdmin)
                {
                    adminCardEditorButton.update(mousePos);
                    adminUsersButton.update(mousePos);
                }
                authenticatedOptionsButton.update(mousePos);
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
            else
            {
                adminBackButton.update(mousePos);
                adminPrevPageButton.update(mousePos);
                adminRefreshButton.update(mousePos);
                adminNextPageButton.update(mousePos);
                if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
                {
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
        }
        else if (currentState == GameState::DeckSelect)
        {
            findMatchButton.update(mousePos);
            backButton.update(mousePos);
        }
        else if (currentState == GameState::Matchmaking)
        {
            cancelMatchmakingButton.update(mousePos);
        }
        else if (currentState == GameState::DeckEditor)
        {
            deckBackButton.update(mousePos);
            newDeckButton.update(mousePos);
            refreshDeckButton.update(mousePos);
            deleteDeckButton.update(mousePos);
            removeCardButton.update(mousePos);
            addCardButton.update(mousePos);
            saveDeckButton.update(mousePos);
            if (inspectedDeckEditorCardTitle)
            {
                closeDeckCardPopupButton.update(mousePos);
            }
            deckNameInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CardEditor)
        {
            cardEditorScreen.update(window, deltaTime);
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
                buyCardButton.update(mousePos);
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
                        piece && (sandboxMode || (piece->owner == gameSnapshot.yourPlayer && !piece->hasActed)) &&
                        !piece->ability.empty() && piece->growTurnsRemaining == 0 &&
                        piece->disabledTurns == 0 &&
                        (piece->ability != "dig" || piece->abilityUses > 0))
                    {
                        abilityButton.update(mousePos);
                    }
                }
                if (sandboxMode)
                {
                    sandboxPlayerButton.update(mousePos);
                }
                else
                {
                    endTurnButton.update(mousePos);
                }
                leaveGameButton.update(mousePos);
            }
        }

        window.clear(sf::Color(9, 17, 19));
        drawBackdrop();
        if (currentState != GameState::DeckEditor &&
            currentState != GameState::Shop &&
            currentState != GameState::AdminUsers &&
            currentState != GameState::CardEditor &&
            currentState != GameState::Game)
        {
            window.draw(title);
        }

        if (currentState == GameState::Menu)
        {
            loginButton.draw(window);
            createButton.draw(window);
            menuOptionsButton.draw(window);
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
            drawText(window, font, "Display Mode", 18, {332.0f, 148.0f}, sf::Color(220, 224, 230));
            displayModeButton.draw(window);
            drawText(window, font, "Resolution", 18, {350.0f, 246.0f}, sf::Color(220, 224, 230));
            previousResolutionButton.draw(window);
            resolutionButton.draw(window);
            nextResolutionButton.draw(window);
            applyOptionsButton.draw(window);
            if (optionsReturnState == GameState::Authenticated)
            {
                changePasswordOptionButton.draw(window);
            }
            optionsBackButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::ChangePassword)
        {
            currentPasswordInput.draw(window);
            newPasswordInput.draw(window);
            confirmNewPasswordInput.draw(window);
            changePasswordVisibilityButton.draw(window);
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
            usernameInput.draw(window);
            passwordInput.draw(window);
            passwordVisibilityButton.draw(window);
            rememberMeButton.draw(window);
            loginSubmitButton.draw(window);
            backButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::CreateAccount)
        {
            usernameInput.draw(window);
            passwordInput.draw(window);
            confirmInput.draw(window);
            passwordVisibilityButton.draw(window);
            createSubmitButton.draw(window);
            backButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::Authenticated)
        {
            drawText(window, font, signedInLabel(), 18, {24.0f, 20.0f}, sf::Color(246, 238, 218), 240.0f);
            drawText(window, font, "Rating: " + std::to_string(playerRating), 16, {24.0f, 48.0f}, sf::Color(151, 192, 255), 180.0f);
            drawCoinIcon({24.0f, 76.0f}, 13.0f);
            drawText(window, font, std::to_string(playerCoins), 18, {58.0f, 75.0f}, sf::Color(248, 239, 216), 120.0f);
            playButton.draw(window);
            sandboxButton.draw(window);
            deckEditorButton.draw(window);
            shopButton.draw(window);
            if (loggedInIsAdmin)
            {
                adminCardEditorButton.draw(window);
                adminUsersButton.draw(window);
            }
            authenticatedOptionsButton.draw(window);
            logoutButton.draw(window);
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
        else if (currentState == GameState::DeckSelect)
        {
            drawDeckSelect();
        }
        else if (currentState == GameState::Matchmaking)
        {
            window.draw(messageText);
            cancelMatchmakingButton.draw(window);
        }
        else if (currentState == GameState::DeckEditor)
        {
            drawDeckEditor();
            drawDeckEditorCardPopup();
        }
        else if (currentState == GameState::Shop)
        {
            drawShop();
        }
        else if (currentState == GameState::CardEditor)
        {
            cardEditorScreen.render(window);
        }
        else if (currentState == GameState::Game)
        {
            drawGame();
        }

        window.display();
    }

    return 0;
}
