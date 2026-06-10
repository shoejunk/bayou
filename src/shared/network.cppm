module;

#include <cstdint>
#include <string>

export module network;

export namespace network {

enum class MessageType : uint8_t
{
    CreateAccount,
    CreateAccountResponse,
    Login,
    LoginResponse,
    JoinMatchmaking,
    MatchFound,
    CreateGameSession,
    GameSessionCreated,
    JoinGame,
    GameReady,
    CardListRequest,
    CardListResponse,
    CardUpsertRequest,
    CardUpsertResponse,
    Disconnect
};

struct CreateAccountRequest
{
    std::string username;
    std::string password;
};

struct CreateAccountResponse
{
    bool success;
    std::string message;
};

struct LoginRequest
{
    std::string username;
    std::string password;
};

struct LoginResponse
{
    bool success;
    std::string message;
};

struct MatchFoundResponse
{
    int matchId;
    int playerNumber;
    unsigned short gamePort;
};

struct GameSessionCreatedResponse
{
    bool success;
    int matchId;
    unsigned short port;
    std::string message;
};

struct GameReadyResponse
{
    int matchId;
    int playerNumber;
    std::string message;
};

} // namespace network
