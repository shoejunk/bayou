#include "account_rate_limiter.hpp"

#include <chrono>

namespace account_rate_limiter
{
namespace
{
constexpr std::int64_t LoginAttemptWindowSeconds = 15LL * 60LL;
constexpr std::int64_t LoginBlockSeconds = 15LL * 60LL;

std::int64_t unixTime()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

bool AccountRateLimiter::isBlocked(const std::string& key)
{
    const std::int64_t now = unixTime();
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = attempts.find(key);
    if (found == attempts.end())
    {
        return false;
    }

    LoginAttempt& attempt = found->second;
    if (attempt.blockedUntil > now)
    {
        return true;
    }
    if (now - attempt.windowStartedAt >= LoginAttemptWindowSeconds)
    {
        attempts.erase(found);
    }
    return false;
}

void AccountRateLimiter::recordFailure(const std::string& key, int maximumFailures)
{
    const std::int64_t now = unixTime();
    std::lock_guard<std::mutex> lock(mutex);
    LoginAttempt& attempt = attempts[key];
    if (attempt.windowStartedAt == 0 || now - attempt.windowStartedAt >= LoginAttemptWindowSeconds)
    {
        attempt = LoginAttempt{0, now, 0};
    }

    ++attempt.failures;
    if (attempt.failures >= maximumFailures)
    {
        attempt.blockedUntil = now + LoginBlockSeconds;
    }
}

void AccountRateLimiter::clearFailures(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex);
    attempts.erase(key);
}

} // namespace account_rate_limiter
