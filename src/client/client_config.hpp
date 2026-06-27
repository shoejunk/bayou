#pragma once

#include <SFML/Network.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace bayou::client
{
inline constexpr unsigned short AccountServerPort = 55000;
inline constexpr unsigned short MatchmakingServerPort = 55001;
inline constexpr unsigned short CardServerPort = 55004;
inline constexpr const char* DefaultServerHost = "127.0.0.1";
inline constexpr const char* DefaultPaymentServerUrl = "http://127.0.0.1:55005";

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

void setExecutableDirectory(const char* executablePath);
const ClientConfig& clientConfig();

DisplaySettings loadDisplaySettings();
bool saveDisplaySettings(const DisplaySettings& settings);

bool saveRememberToken(const std::string& token);
std::optional<std::string> loadRememberToken();
void clearRememberToken();

std::string stripTrailingSlashes(std::string value);
std::string assetRelativePath(const std::string& value);
std::optional<std::filesystem::path> resolveAssetPath(const std::string& value);
std::string endpointText(const ServerEndpoint& endpoint);

bool connectToHostPort(sf::TcpSocket& socket, const std::string& host, unsigned short port);
bool connectToEndpoint(sf::TcpSocket& socket, const ServerEndpoint& endpoint);
}
