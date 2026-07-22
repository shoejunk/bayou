#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace bayou::client
{
struct ClockWarning
{
    int playerNumber = 0;
    std::int64_t thresholdMs = 0;
};

class ClockWarningTracker
{
public:
    static constexpr std::array<std::int64_t, 3> ThresholdsMs = {
        120'000,
        60'000,
        30'000};

    void reset()
    {
        previousRemainingMs.fill(-1);
        warned.fill(0);
    }

    std::optional<ClockWarning> observe(int playerNumber, std::int64_t remainingMs)
    {
        if (playerNumber < 1 || playerNumber > 2 || remainingMs < 0)
        {
            return std::nullopt;
        }

        const std::size_t playerIndex = static_cast<std::size_t>(playerNumber - 1);
        const std::int64_t previous = previousRemainingMs[playerIndex];
        previousRemainingMs[playerIndex] = remainingMs;

        // The first observation establishes which warnings have already
        // elapsed. This prevents reconnecting below a threshold from replaying
        // old alerts.
        if (previous < 0)
        {
            for (std::size_t i = 0; i < ThresholdsMs.size(); ++i)
            {
                if (remainingMs <= ThresholdsMs[i])
                {
                    warned[playerIndex] |= static_cast<std::uint8_t>(1u << i);
                }
            }
            return std::nullopt;
        }

        std::optional<ClockWarning> crossed;
        for (std::size_t i = 0; i < ThresholdsMs.size(); ++i)
        {
            const std::uint8_t bit = static_cast<std::uint8_t>(1u << i);
            if (remainingMs > ThresholdsMs[i])
            {
                continue;
            }

            if ((warned[playerIndex] & bit) == 0 && previous > ThresholdsMs[i])
            {
                // If a delayed update spans multiple thresholds, retain the
                // most urgent warning instead of playing several at once.
                crossed = ClockWarning{playerNumber, ThresholdsMs[i]};
            }
            warned[playerIndex] |= bit;
        }
        return crossed;
    }

private:
    std::array<std::int64_t, 2> previousRemainingMs = {-1, -1};
    std::array<std::uint8_t, 2> warned{};
};
}
