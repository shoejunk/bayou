#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <optional>
#include <string>

namespace account_tokens
{

struct RememberLogin
{
    std::string username;
    std::string replacementToken;
};

std::string issueRememberToken(SQLite::Database& database, const std::string& username);
std::string issueAccessToken(SQLite::Database& database, const std::string& username);
std::optional<std::string> authenticateAccessToken(SQLite::Database& database, const std::string& token);
std::optional<RememberLogin> rotateRememberToken(SQLite::Database& database, const std::string& token);
void revokeRememberToken(SQLite::Database& database, const std::string& token);
void revokeAccessToken(SQLite::Database& database, const std::string& token);

} // namespace account_tokens
