#pragma once

#include <cstdint>
#include <string>

namespace network
{
enum class MessageType : std::uint8_t
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
    UseAbility,
    EndTurn,
    GameOver,
    RememberLogin,
    RememberLoginResponse,
    RevokeRememberToken,
    RevokeRememberTokenResponse,
    ChangePasswordRequest,
    ChangePasswordResponse,
    AdminUserGoldRequest,
    AdminUserGoldResponse,
    DiscardCard
};

struct CreateAccountRequest
{
    std::string username;
    std::string password;
};

struct CreateAccountResponse
{
    bool success = false;
    std::string message;
};

struct LoginRequest
{
    std::string username;
    std::string password;
};

struct LoginResponse
{
    bool success = false;
    std::string message;
    std::string username;
    std::string accessToken;
    std::string rememberToken;
};

struct ChangePasswordRequest
{
    std::string currentPassword;
    std::string newPassword;
};

struct ChangePasswordResponse
{
    bool success = false;
    std::string message;
};

struct MatchFoundResponse
{
    int matchId = 0;
    int playerNumber = 0;
    unsigned short gamePort = 0;
};

struct GameSessionCreatedResponse
{
    bool success = false;
    int matchId = 0;
    unsigned short port = 0;
    std::string message;
};

struct GameReadyResponse
{
    int matchId = 0;
    int playerNumber = 0;
    std::string message;
};

struct AdminUserSummary
{
    std::string username;
    bool isAdmin = false;
    int gold = 0;
};

struct AdminUserListResponse
{
    bool success = false;
    std::string message;
    std::uint32_t totalCount = 0;
    std::uint32_t page = 0;
    std::uint32_t pageSize = 0;
    std::vector<AdminUserSummary> users;
};

struct AdminUserPrivilegeRequest
{
    std::string targetUsername;
    bool makeAdmin = false;
};

struct AdminUserPrivilegeResponse
{
    bool success = false;
    std::string message;
    bool targetIsAdmin = false;
};

struct AdminUserGoldRequest
{
    std::string targetUsername;
    int amount = 0;
};

struct AdminUserGoldResponse
{
    bool success = false;
    std::string message;
    int targetGold = 0;
};
} // namespace network
