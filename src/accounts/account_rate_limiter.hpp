#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace account_rate_limiter
{

constexpr int MaximumLoginFailures = 5;
constexpr int MaximumLoginFailuresPerAddress = 25;

class AccountRateLimiter
{
public:
    bool isBlocked(const std::string& key);
    void recordFailure(const std::string& key, int maximumFailures = MaximumLoginFailures);
    void clearFailures(const std::string& key);

private:
    struct LoginAttempt
    {
        int failures = 0;
        std::int64_t windowStartedAt = 0;
        std::int64_t blockedUntil = 0;
    };

    std::mutex mutex;
    std::unordered_map<std::string, LoginAttempt> attempts;
};

} // namespace account_rate_limiter
