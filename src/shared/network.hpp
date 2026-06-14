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
    SubmitDeck,
    GameStateUpdate,
    PlaceHero,
    PlayCard,
    MovePiece,
    AttackPiece,
    EndTurn,
    GameOver
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
} // namespace network
