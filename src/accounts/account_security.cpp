#include "account_security.hpp"

#include <fmt/core.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace account_security
{
std::string legacyPasswordHash(const std::string& password)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : password)
    {
        hash ^= c;
        hash *= 1099511628211ull;
    }

    return fmt::format("{:016x}", hash);
}

bool isModernPasswordHash(const std::string& hash)
{
    return hash.starts_with("$argon2");
}

bool passwordHashNeedsUpgrade(const std::string& hash)
{
    if (!isModernPasswordHash(hash))
    {
        return true;
    }

    return crypto_pwhash_str_needs_rehash(
        hash.c_str(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0;
}

bool isValidUsername(const std::string& username)
{
    if (username.size() < MinimumUsernameLength || username.size() > MaximumUsernameLength)
    {
        return false;
    }

    return std::all_of(username.begin(), username.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '-';
    });
}

bool isValidNewPassword(const std::string& password)
{
    if (password.size() < MinimumPasswordLength ||
        password.size() > MaximumPasswordLength)
    {
        return false;
    }

    bool hasLowercase = false;
    bool hasUppercase = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (unsigned char ch : password)
    {
        hasLowercase = hasLowercase || std::islower(ch) != 0;
        hasUppercase = hasUppercase || std::isupper(ch) != 0;
        hasDigit = hasDigit || std::isdigit(ch) != 0;
        hasSpecial = hasSpecial || std::ispunct(ch) != 0;
    }

    return hasLowercase && hasUppercase && hasDigit && hasSpecial;
}

std::string hashPassword(const std::string& password)
{
    std::array<char, crypto_pwhash_STRBYTES> hash{};
    if (crypto_pwhash_str(
            hash.data(),
            password.data(),
            static_cast<unsigned long long>(password.size()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        throw std::runtime_error("Unable to hash password");
    }
    return hash.data();
}

const std::string& dummyPasswordHash()
{
    static const std::string hash = hashPassword(
        "not-a-real-account-password-used-only-to-equalize-login-work");
    return hash;
}

bool verifyPassword(const std::string& storedHash, const std::string& password)
{
    if (!isModernPasswordHash(storedHash))
    {
        const std::string expected = legacyPasswordHash(password);
        if (storedHash.size() != expected.size())
        {
            return false;
        }
        return sodium_memcmp(
            storedHash.data(),
            expected.data(),
            expected.size()) == 0;
    }

    return crypto_pwhash_str_verify(
        storedHash.c_str(),
        password.data(),
        static_cast<unsigned long long>(password.size())) == 0;
}

std::string generateToken()
{
    std::array<unsigned char, 32> bytes{};
    std::array<char, 65> encoded{};
    randombytes_buf(bytes.data(), bytes.size());
    sodium_bin2hex(encoded.data(), encoded.size(), bytes.data(), bytes.size());
    return encoded.data();
}

std::string hashToken(const std::string& token)
{
    std::array<unsigned char, crypto_generichash_BYTES> hash{};
    std::array<char, crypto_generichash_BYTES * 2 + 1> encoded{};
    crypto_generichash(
        hash.data(),
        hash.size(),
        reinterpret_cast<const unsigned char*>(token.data()),
        static_cast<unsigned long long>(token.size()),
        nullptr,
        0);
    sodium_bin2hex(encoded.data(), encoded.size(), hash.data(), hash.size());
    return encoded.data();
}
}
