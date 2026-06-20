module;

#include <cstdint>
#include <string>
#include <vector>

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
    Disconnect,
    CardUpdateRequest,
    CardUpdateResponse,
    CardDeleteRequest,
    CardDeleteResponse,
    DeckListRequest,
    DeckListResponse,
    DeckSaveRequest,
    DeckSaveResponse,
    DeckDeleteRequest,
    DeckDeleteResponse,
    AccountStateRequest,
    AccountStateResponse,
    WinRewardRequest,
    WinRewardResponse,
    ShopPurchaseRequest,
    ShopPurchaseResponse,
    AdminUserListRequest,
    AdminUserListResponse,
    AdminUserPrivilegeRequest,
    AdminUserPrivilegeResponse,
    SubmitDeck,
    GameStateUpdate,
    PlaceHero,
    PlayCard,
    MovePiece,
    AttackPiece,
    EndTurn,
    GameOver,
    RememberLogin,
    RememberLoginResponse,
    RevokeRememberToken,
    RevokeRememberTokenResponse
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
    std::string username;
    std::string accessToken;
    std::string rememberToken;
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

struct AdminUserSummary
{
    std::string username;
    bool isAdmin;
};

struct AdminUserListResponse
{
    bool success;
    std::string message;
    std::uint32_t totalCount;
    std::uint32_t page;
    std::uint32_t pageSize;
    std::vector<AdminUserSummary> users;
};

struct AdminUserPrivilegeRequest
{
    std::string targetUsername;
    bool makeAdmin;
};

struct AdminUserPrivilegeResponse
{
    bool success;
    std::string message;
    bool targetIsAdmin;
};

} // namespace network
