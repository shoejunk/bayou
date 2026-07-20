#pragma once

#include "../shared/conquest_event_data.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace account_conquest_events
{

struct CommandResult
{
    bool success = false;
    std::string message;
};

// Passing zero for now uses the current Unix time. The explicit clock input is
// useful to both the account server and deterministic integration tests.
void initializeSchema(SQLite::Database& database, std::int64_t now = 0);

std::vector<conquest_data::EventSummary> listEvents(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t now = 0);

std::optional<conquest_data::EventState> loadEventState(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::string& error,
    std::int64_t now = 0);

CommandResult joinEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    const std::vector<conquest_data::StartingPlacement>& placements,
    std::int64_t now = 0);

CommandResult submitOrders(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    const std::vector<conquest_data::MoveOrder>& orders,
    std::int64_t now = 0);

// Starts planning after registration and resolves a planning turn once its
// deadline has passed or every participant has submitted orders.
CommandResult resolveEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    std::int64_t now = 0);

// Immediately starts a registration-phase event after verifying the requesting
// account's admin privilege. At least two participants are still required.
CommandResult forceStartEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::int64_t now = 0);

// Creates a Dark Realms registration event with a frozen copy of the current
// card catalog after verifying the requesting account's admin privilege.
CommandResult createEvent(
    SQLite::Database& database,
    const std::string& username,
    const std::string& name,
    std::int64_t registrationSeconds,
    std::int64_t turnSeconds,
    std::int64_t reinforcementCooldownSeconds,
    std::int64_t now = 0);

// Immediately completes an event without a winner reward after verifying the
// requesting account's admin privilege. Every participant receives their
// fixed entry fee back exactly once.
CommandResult forceEndEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::int64_t now = 0);

CommandResult deployReinforcement(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::uint64_t eventDeckId,
    int regionId,
    std::int64_t now = 0);

// Participant-facing data loader used after the account server authenticates
// an access token. playerNumber is populated only on a successful load; deck
// selection and numbering always come from the stored conflict.
std::optional<conquest_data::BattleData> loadBattleData(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    int& playerNumber,
    std::string& error);

// Coordinator RPCs use a durable, per-battle capability after this initial
// participant-authenticated load. The capability is never sent to a player.
std::optional<conquest_data::BattleData> loadBattleDataForCoordinator(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    int& playerNumber,
    std::string& capability,
    std::string& error);

std::optional<conquest_data::BattleData> reloadBattleDataForCoordinator(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    std::string& error);

// Trusted participant form retained for deterministic store tests.
CommandResult appendBattleAction(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    const conquest_data::BattleAction& action);

// Production coordinator mutation path. Participant identity is derived from
// the frozen battle row and the action's player number after capability auth.
CommandResult appendBattleActionWithCapability(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    const conquest_data::BattleAction& action);

// Trusted participant form retained for deterministic store tests. Production
// RPCs use the capability form below. Applying the same winning result more
// than once is a successful no-op.
CommandResult applyBattleResult(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    int winnerPlayerNumber,
    std::int64_t now = 0);

CommandResult applyBattleResultWithCapability(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    int winnerPlayerNumber,
    std::int64_t now = 0);

} // namespace account_conquest_events
