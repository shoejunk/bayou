#include "account_tokens.hpp"

#include "account_security.hpp"

#include <chrono>
#include <cstdint>

namespace account_tokens
{
namespace
{
constexpr std::int64_t RememberTokenLifetimeSeconds = 30LL * 24LL * 60LL * 60LL;
constexpr std::int64_t AccessTokenLifetimeSeconds = 12LL * 60LL * 60LL;

std::int64_t unixTime()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void deleteExpiredRememberTokens(SQLite::Database& database)
{
    SQLite::Statement cleanup(database, "DELETE FROM remember_tokens WHERE expires_at <= ?");
    cleanup.bind(1, unixTime());
    cleanup.exec();
}

void deleteExpiredAccessTokens(SQLite::Database& database)
{
    SQLite::Statement cleanup(database, "DELETE FROM access_tokens WHERE expires_at <= ?");
    cleanup.bind(1, unixTime());
    cleanup.exec();
}

} // namespace

std::string issueRememberToken(SQLite::Database& database, const std::string& username)
{
    deleteExpiredRememberTokens(database);
    const std::string token = account_security::generateToken();
    const std::int64_t now = unixTime();
    SQLite::Statement insert(
        database,
        "INSERT INTO remember_tokens "
        "(token_hash, username, expires_at, created_at, last_used_at) "
        "VALUES (?, ?, ?, ?, ?)");
    insert.bind(1, account_security::hashToken(token));
    insert.bind(2, username);
    insert.bind(3, now + RememberTokenLifetimeSeconds);
    insert.bind(4, now);
    insert.bind(5, now);
    insert.exec();
    return token;
}

std::string issueAccessToken(SQLite::Database& database, const std::string& username)
{
    deleteExpiredAccessTokens(database);
    const std::string token = account_security::generateToken();
    const std::int64_t now = unixTime();
    SQLite::Statement insert(
        database,
        "INSERT INTO access_tokens "
        "(token_hash, username, expires_at, created_at, last_used_at) "
        "VALUES (?, ?, ?, ?, ?)");
    insert.bind(1, account_security::hashToken(token));
    insert.bind(2, username);
    insert.bind(3, now + AccessTokenLifetimeSeconds);
    insert.bind(4, now);
    insert.bind(5, now);
    insert.exec();
    return token;
}

std::optional<std::string> authenticateAccessToken(SQLite::Database& database, const std::string& token)
{
    if (token.empty())
    {
        return std::nullopt;
    }

    const std::int64_t now = unixTime();
    const std::string tokenHash = account_security::hashToken(token);
    std::optional<std::string> username;
    {
        SQLite::Statement query(
            database,
            "SELECT username FROM access_tokens WHERE token_hash = ? AND expires_at > ?");
        query.bind(1, tokenHash);
        query.bind(2, now);
        if (!query.executeStep())
        {
            return std::nullopt;
        }
        username = query.getColumn(0).getString();
    }

    SQLite::Statement touch(
        database,
        "UPDATE access_tokens SET last_used_at = ? WHERE token_hash = ?");
    touch.bind(1, now);
    touch.bind(2, tokenHash);
    touch.exec();
    return username;
}

std::optional<RememberLogin> rotateRememberToken(SQLite::Database& database, const std::string& token)
{
    if (token.empty())
    {
        return std::nullopt;
    }

    deleteExpiredRememberTokens(database);
    const std::string oldHash = account_security::hashToken(token);
    SQLite::Statement query(
        database,
        "SELECT username FROM remember_tokens "
        "WHERE token_hash = ? AND expires_at > ?");
    query.bind(1, oldHash);
    query.bind(2, unixTime());
    if (!query.executeStep())
    {
        return std::nullopt;
    }

    const std::string username = query.getColumn(0).getString();
    const std::string replacement = account_security::generateToken();
    const std::int64_t now = unixTime();
    SQLite::Statement rotate(
        database,
        "UPDATE remember_tokens "
        "SET token_hash = ?, expires_at = ?, last_used_at = ? "
        "WHERE token_hash = ?");
    rotate.bind(1, account_security::hashToken(replacement));
    rotate.bind(2, now + RememberTokenLifetimeSeconds);
    rotate.bind(3, now);
    rotate.bind(4, oldHash);
    rotate.exec();

    return RememberLogin{username, replacement};
}

void revokeRememberToken(SQLite::Database& database, const std::string& token)
{
    if (token.empty())
    {
        return;
    }

    SQLite::Statement revoke(database, "DELETE FROM remember_tokens WHERE token_hash = ?");
    revoke.bind(1, account_security::hashToken(token));
    revoke.exec();
}

void revokeAccessToken(SQLite::Database& database, const std::string& token)
{
    if (token.empty())
    {
        return;
    }

    SQLite::Statement revoke(database, "DELETE FROM access_tokens WHERE token_hash = ?");
    revoke.bind(1, account_security::hashToken(token));
    revoke.exec();
}

} // namespace account_tokens
