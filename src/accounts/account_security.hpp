#pragma once

#include <cstddef>
#include <string>

namespace account_security
{
inline constexpr std::size_t MinimumPasswordLength = 15;
inline constexpr std::size_t MaximumPasswordLength = 128;
inline constexpr std::size_t MinimumUsernameLength = 3;
inline constexpr std::size_t MaximumUsernameLength = 32;

std::string legacyPasswordHash(const std::string& password);
bool isModernPasswordHash(const std::string& hash);
bool passwordHashNeedsUpgrade(const std::string& hash);
bool isValidUsername(const std::string& username);
bool isValidNewPassword(const std::string& password);
std::string hashPassword(const std::string& password);
const std::string& dummyPasswordHash();
bool verifyPassword(const std::string& storedHash, const std::string& password);
std::string generateToken();
std::string hashToken(const std::string& token);
}
