#include <string>
#include <cstdint>

export module network;

export namespace network {

enum class MessageType : uint8_t
{
    CreateAccount,
    CreateAccountResponse,
    Login,
    LoginResponse,
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

} // namespace network
