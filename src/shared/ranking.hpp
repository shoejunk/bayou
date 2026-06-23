#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace ranking
{
constexpr int EloOffset = 2000;
constexpr int EloKFactor = 32;
constexpr int InitialMatchmakingRange = 10;
constexpr int MatchmakingExpansionSeconds = 5;

struct MatchRewards
{
    std::array<int, 2> ratings{};
    std::array<int, 2> ratingChanges{};
    int winnerCoins = 0;
};

inline std::pair<int, int> ratingsAfterMatch(int playerOneRating, int playerTwoRating, int winner)
{
    const double shiftedOne = static_cast<double>(playerOneRating) + EloOffset;
    const double shiftedTwo = static_cast<double>(playerTwoRating) + EloOffset;
    const double expectedOne = 1.0 / (1.0 + std::pow(10.0, (shiftedTwo - shiftedOne) / 400.0));
    const double scoreOne = winner == 1 ? 1.0 : 0.0;
    const int adjustmentOne = static_cast<int>(
        std::lround(EloKFactor * (scoreOne - expectedOne)));
    const int adjustmentTwo = static_cast<int>(
        std::lround(EloKFactor * ((1.0 - scoreOne) - (1.0 - expectedOne))));

    return {
        std::max(0, playerOneRating + adjustmentOne),
        std::max(0, playerTwoRating + adjustmentTwo)};
}

inline MatchRewards rewardsAfterMatch(
    int playerOneRating,
    int playerTwoRating,
    int winner,
    bool selfMatch,
    int winRewardCoins)
{
    MatchRewards rewards;
    rewards.ratings = {playerOneRating, playerTwoRating};
    if (!selfMatch)
    {
        const auto [newRatingOne, newRatingTwo] =
            ratingsAfterMatch(playerOneRating, playerTwoRating, winner);
        rewards.ratings = {newRatingOne, newRatingTwo};
        rewards.ratingChanges = {
            newRatingOne - playerOneRating,
            newRatingTwo - playerTwoRating};
        rewards.winnerCoins = winRewardCoins;
    }
    return rewards;
}

inline int matchmakingRange(std::chrono::steady_clock::duration waitTime)
{
    const auto elapsedSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(waitTime).count();
    const std::int64_t doublings = std::max<std::int64_t>(
        0, elapsedSeconds / MatchmakingExpansionSeconds);
    if (doublings >= 27)
    {
        return std::numeric_limits<int>::max();
    }
    return InitialMatchmakingRange << doublings;
}
}
