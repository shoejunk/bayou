#include "conquest_battle_manager.hpp"

#include "game_action.hpp"
#include "game_engine.hpp"

#include "../shared/conquest_event_data.hpp"
#include "../shared/game_data.hpp"
#include "../shared/network.hpp"
#include "../shared/socket_timeout.hpp"

#include <SFML/Network.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr unsigned short AccountServerPort = 55000;
constexpr auto ActivePollInterval = std::chrono::milliseconds(20);
constexpr auto IdlePollInterval = std::chrono::milliseconds(250);
constexpr auto SessionIdleRetention = std::chrono::seconds(2);
constexpr auto CompletedSessionGrace = std::chrono::seconds(2);
constexpr auto ResultRetryInterval = std::chrono::seconds(5);
constexpr auto AccountRpcTimeout = std::chrono::seconds(10);
constexpr auto ClientSendTimeout = std::chrono::seconds(2);
constexpr auto TimerSnapshotInterval = std::chrono::minutes(1);
constexpr int MaxPacketsPerConnectionPerPoll = 16;

struct BattleDataResult
{
    bool success = false;
    std::string message;
    int playerNumber = 0;
    std::string capability;
    conquest_data::BattleData data;
};

struct ActionStoreResult
{
    bool success = false;
    std::string message;
    std::uint32_t acceptedSequence = 0;
};

struct ResultStoreResult
{
    bool success = false;
    std::string message;
};

bool connectToAccountServer(bayou::tls::Socket& socket)
{
    return socket.connect(sf::IpAddress::LocalHost, AccountServerPort) ==
        sf::Socket::Status::Done;
}

BattleDataResult loadBattleData(
    std::uint64_t battleId,
    const std::string& accessToken)
{
    bayou::tls::Socket socket;
    if (!connectToAccountServer(socket))
    {
        return {false, "Could not reach the account server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::ConquestBattleDataRequest)
            << battleId << accessToken;
    if (socket.send(request, AccountRpcTimeout) != sf::Socket::Status::Done)
    {
        return {false, "Could not request Conquest battle data"};
    }

    sf::Packet response;
    if (socket_timeout::receivePacket(socket, response, AccountRpcTimeout) !=
        sf::Socket::Status::Done)
    {
        return {false, "No Conquest battle data response"};
    }

    std::uint8_t rawType = 0;
    BattleDataResult result;
    response >> rawType >> result.success >> result.message >> result.playerNumber
             >> result.capability;
    if (!response ||
        static_cast<network::MessageType>(rawType) !=
            network::MessageType::ConquestBattleDataResponse)
    {
        return {false, "Unexpected Conquest battle data response"};
    }
    if (!result.success)
    {
        return result;
    }
    if ((result.playerNumber != 1 && result.playerNumber != 2) ||
        result.capability.size() != conquest_data::ConquestBattleCapabilityHexLength ||
        !conquest_data::readBattleData(response, result.data) ||
        result.data.battleId != battleId)
    {
        return {false, "Invalid Conquest battle data response"};
    }
    return result;
}

BattleDataResult reloadBattleData(
    std::uint64_t battleId,
    const std::string& capability)
{
    bayou::tls::Socket socket;
    if (!connectToAccountServer(socket))
    {
        return {false, "Could not reach the account server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(
                   network::MessageType::ConquestBattleReloadRequest)
            << battleId << capability;
    if (socket.send(request, AccountRpcTimeout) != sf::Socket::Status::Done)
    {
        return {false, "Could not reload Conquest battle data"};
    }

    sf::Packet response;
    if (socket_timeout::receivePacket(socket, response, AccountRpcTimeout) !=
        sf::Socket::Status::Done)
    {
        return {false, "No Conquest battle reload response"};
    }

    std::uint8_t rawType = 0;
    BattleDataResult result;
    response >> rawType >> result.success >> result.message;
    if (!response ||
        static_cast<network::MessageType>(rawType) !=
            network::MessageType::ConquestBattleReloadResponse)
    {
        return {false, "Unexpected Conquest battle reload response"};
    }
    if (!result.success)
    {
        return result;
    }
    if (!conquest_data::readBattleData(response, result.data) ||
        result.data.battleId != battleId)
    {
        return {false, "Invalid Conquest battle reload response"};
    }
    result.capability = capability;
    return result;
}

ActionStoreResult storeBattleActionOnce(
    std::uint64_t battleId,
    const std::string& capability,
    const conquest_data::BattleAction& action)
{
    bayou::tls::Socket socket;
    if (!connectToAccountServer(socket))
    {
        return {false, "Could not reach the account server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::ConquestBattleActionRequest)
            << battleId << capability;
    conquest_data::writeBattleAction(request, action);
    if (socket.send(request, AccountRpcTimeout) != sf::Socket::Status::Done)
    {
        return {false, "Could not persist the Conquest battle action"};
    }

    sf::Packet response;
    if (socket_timeout::receivePacket(socket, response, AccountRpcTimeout) !=
        sf::Socket::Status::Done)
    {
        return {false, "No Conquest battle action response"};
    }

    std::uint8_t rawType = 0;
    ActionStoreResult result;
    response >> rawType >> result.success >> result.message >> result.acceptedSequence;
    if (!response ||
        static_cast<network::MessageType>(rawType) !=
            network::MessageType::ConquestBattleActionResponse)
    {
        return {false, "Unexpected Conquest battle action response"};
    }
    if (result.success && result.acceptedSequence != action.sequence)
    {
        return {
            false,
            "Account server acknowledged an unexpected Conquest action sequence",
            result.acceptedSequence};
    }
    return result;
}

ActionStoreResult storeBattleAction(
    std::uint64_t battleId,
    const std::string& capability,
    const conquest_data::BattleAction& action)
{
    ActionStoreResult result = storeBattleActionOnce(battleId, capability, action);
    if (result.success)
    {
        return result;
    }

    // An append is idempotent when the same action and sequence are retried.
    // Retrying here closes the common commit-with-lost-response window before
    // the session falls back to a full authoritative resync.
    return storeBattleActionOnce(battleId, capability, action);
}

ResultStoreResult storeBattleResult(
    std::uint64_t battleId,
    const std::string& capability,
    int winnerPlayerNumber)
{
    bayou::tls::Socket socket;
    if (!connectToAccountServer(socket))
    {
        return {false, "Could not reach the account server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::SubmitConquestBattleResult)
            << battleId << capability << winnerPlayerNumber;
    if (socket.send(request, AccountRpcTimeout) != sf::Socket::Status::Done)
    {
        return {false, "Could not submit the Conquest battle result"};
    }

    sf::Packet response;
    if (socket_timeout::receivePacket(socket, response, AccountRpcTimeout) !=
        sf::Socket::Status::Done)
    {
        return {false, "No Conquest battle result response"};
    }

    std::uint8_t rawType = 0;
    ResultStoreResult result;
    response >> rawType >> result.success >> result.message;
    if (!response ||
        static_cast<network::MessageType>(rawType) !=
            network::MessageType::SubmitConquestBattleResultResponse)
    {
        return {false, "Unexpected Conquest battle result response"};
    }
    return result;
}

void sendReadyFailure(
    bayou::tls::Socket& client,
    std::uint64_t battleId,
    const std::string& message)
{
    sf::Packet response;
    response << static_cast<std::uint8_t>(network::MessageType::ConquestBattleReady)
             << false << battleId << 0 << message;
    [[maybe_unused]] const sf::Socket::Status sent =
        client.send(response, ClientSendTimeout);
    client.disconnect();
}

bool permanentResultFailure(const std::string& message)
{
    return message == "Conquest battle not found" ||
        message == "Invalid Conquest battle capability" ||
        message == "Conquest battle is not ready for a result" ||
        message == "Conquest event for battle is missing" ||
        message == "A different conquest battle result is already stored";
}

bool permanentActionFailure(const std::string& message)
{
    return message == "Invalid Conquest battle capability" ||
        message == "Conquest battle not found" ||
        message == "Conquest battle is not accepting actions";
}

bool sameDeck(const deck_data::Deck& one, const deck_data::Deck& two)
{
    return one.name == two.name && one.cardTitles == two.cardTitles;
}

bool sameCards(
    const std::vector<card_data::Card>& one,
    const std::vector<card_data::Card>& two)
{
    if (one.size() != two.size())
    {
        return false;
    }
    sf::Packet onePacket;
    sf::Packet twoPacket;
    for (const card_data::Card& card : one)
    {
        card_data::writeCard(onePacket, card);
    }
    for (const card_data::Card& card : two)
    {
        card_data::writeCard(twoPacket, card);
    }
    return onePacket.getDataSize() == twoPacket.getDataSize() &&
        (onePacket.getDataSize() == 0 ||
         std::memcmp(
             onePacket.getData(),
             twoPacket.getData(),
             onePacket.getDataSize()) == 0);
}

bool sameAction(
    const conquest_data::BattleAction& one,
    const conquest_data::BattleAction& two)
{
    return one.sequence == two.sequence &&
        one.playerNumber == two.playerNumber &&
        one.actionType == two.actionType &&
        one.argumentOne == two.argumentOne &&
        one.argumentTwo == two.argumentTwo &&
        one.argumentThree == two.argumentThree;
}

} // namespace

class ConquestBattleManager::Impl
{
    class Session;

public:
    explicit Impl(std::vector<card_data::Card> catalog)
        : cardCatalog(std::move(catalog))
    {
        for (const card_data::Card& card : cardCatalog)
        {
            cardsByTitle.emplace(card.title, card);
        }
    }

    ~Impl();

    void handleClient(
        std::unique_ptr<bayou::tls::Socket> client,
        std::uint64_t battleId,
        std::string accessToken)
    {
        reapRetiredSessions();
        if (!client)
        {
            return;
        }
        if (battleId == 0 || accessToken.empty())
        {
            sendReadyFailure(*client, battleId, "Invalid Conquest battle join request");
            return;
        }

        BattleDataResult loaded = loadBattleData(battleId, accessToken);
        if (!loaded.success)
        {
            sendReadyFailure(
                *client,
                battleId,
                loaded.message.empty() ? "Could not load Conquest battle" : loaded.message);
            return;
        }

        std::shared_ptr<Session> session;
        std::string error;
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            const auto found = sessions.find(battleId);
            if (found != sessions.end())
            {
                session = found->second;
            }
            else
            {
                std::unique_ptr<GameEngine> engine = makeEngine(loaded.data, error);
                if (engine)
                {
                    session = std::make_shared<Session>(
                        *this,
                        std::move(loaded.data),
                        std::move(loaded.capability),
                        std::move(engine));
                    sessions.emplace(battleId, session);
                    session->start();
                }
            }
        }

        if (!session || !error.empty())
        {
            sendReadyFailure(
                *client,
                battleId,
                error.empty() ? "Could not start Conquest battle" : error);
            return;
        }

        session->addClient(
            std::move(client),
            loaded.playerNumber);
    }

private:
    void retireSession(std::uint64_t battleId, Session* retiring);
    void reapRetiredSessions();

    struct Connection
    {
        std::unique_ptr<bayou::tls::Socket> socket;
        int playerNumber = 0;
        bool connected = true;
    };

    class Session
    {
    public:
        Session(
            Impl& owner,
            conquest_data::BattleData data,
            std::string capability,
            std::unique_ptr<GameEngine> engine)
            : owner(owner),
              battleId(data.battleId),
              capability(std::move(capability)),
              data(std::move(data)),
              engine(std::move(engine))
        {
            data.actions.reserve(conquest_data::MaxConquestBattleActions);
        }

        ~Session()
        {
            requestStop();
            join();
        }

        void start()
        {
            worker = std::thread(&Session::run, this);
        }

        void requestStop()
        {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                stopping = true;
            }
            wake.notify_all();
        }

        void join()
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        bool isFinished() const
        {
            return finished.load();
        }

        bool sameIdentity(const conquest_data::BattleData& candidate) const
        {
            return data.battleId == candidate.battleId &&
                data.seed == candidate.seed &&
                data.createdAt == candidate.createdAt &&
                data.timerStartedAt == candidate.timerStartedAt &&
                data.playerOne == candidate.playerOne &&
                data.playerTwo == candidate.playerTwo &&
                sameDeck(data.deckOne, candidate.deckOne) &&
                sameDeck(data.deckTwo, candidate.deckTwo) &&
                sameCards(data.cardsOne, candidate.cardsOne) &&
                sameCards(data.cardsTwo, candidate.cardsTwo) &&
                sameCards(data.catalog, candidate.catalog);
        }

        void addClient(
            std::unique_ptr<bayou::tls::Socket> socket,
            int playerNumber)
        {
            if (!socket || (playerNumber != 1 && playerNumber != 2))
            {
                if (socket)
                {
                    sendReadyFailure(
                        *socket,
                        battleId,
                        "Account server returned an invalid Conquest player");
                }
                return;
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (stopping)
                {
                    sendReadyFailure(*socket, battleId, "Conquest battle server is stopping");
                    return;
                }
                for (Connection& pending : pendingConnections)
                {
                    if (pending.playerNumber == playerNumber)
                    {
                        pending.socket->disconnect();
                        pending.connected = false;
                    }
                }
                pendingConnections.erase(
                    std::remove_if(
                        pendingConnections.begin(),
                        pendingConnections.end(),
                        [](const Connection& pending) {
                            return !pending.connected;
                        }),
                    pendingConnections.end());
                pendingConnections.push_back({
                    std::move(socket),
                    playerNumber,
                    true});
            }
            wake.notify_one();
        }

    private:
        Impl& owner;
        // Mutable replay data belongs exclusively to the worker thread. Keep
        // the database identity separately for addClient(), which runs on
        // coordinator request threads.
        const std::uint64_t battleId;
        const std::string capability;
        conquest_data::BattleData data;
        std::unique_ptr<GameEngine> engine;
        std::mutex queueMutex;
        std::condition_variable wake;
        std::vector<Connection> pendingConnections;
        std::vector<Connection> connections;
        std::thread worker;
        bool stopping = false;
        bool resultSubmitted = false;
        bool resultTerminalFailure = false;
        bool actionTerminalFailure = false;
        std::atomic<bool> finished{false};
        std::chrono::steady_clock::time_point nextResultAttempt{};
        std::chrono::steady_clock::time_point completedRetireAt{};
        std::chrono::steady_clock::time_point lastTimerUpdate =
            std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point lastTimerBroadcast = lastTimerUpdate;

        void run()
        {
            std::chrono::steady_clock::time_point idleSince{};
            while (true)
            {
                std::vector<Connection> arriving;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    std::chrono::milliseconds waitTime = ActivePollInterval;
                    if (connections.empty())
                    {
                        const std::int64_t untilTimerEvent =
                            engine->timeUntilNextTimerEventMs();
                        waitTime = untilTimerEvent > 0
                            ? std::chrono::milliseconds(untilTimerEvent)
                            : std::chrono::duration_cast<std::chrono::milliseconds>(
                                  IdlePollInterval);
                    }
                    wake.wait_for(lock, waitTime, [&]() {
                        return stopping || !pendingConnections.empty();
                    });
                    if (stopping)
                    {
                        break;
                    }
                    arriving.swap(pendingConnections);
                }

                const auto timerNow = std::chrono::steady_clock::now();
                const auto timerElapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        timerNow - lastTimerUpdate);
                const bool timerTransition = engine->updateTimers(timerElapsed.count());
                lastTimerUpdate += timerElapsed;
                if (timerTransition)
                {
                    if (engine->phase() == game_data::Phase::GameOver)
                    {
                        nextResultAttempt = {};
                    }
                    broadcastSnapshots();
                }
                else if (!connections.empty() &&
                         engine->phase() == game_data::Phase::Playing &&
                         timerNow - lastTimerBroadcast >= TimerSnapshotInterval)
                {
                    broadcastSnapshots();
                }

                for (Connection& connection : arriving)
                {
                    adoptConnection(std::move(connection));
                }
                removeDisconnected();

                for (Connection& connection : connections)
                {
                    pollConnection(connection);
                }
                removeDisconnected();
                trySubmitResult();

                const auto lifecycleNow = std::chrono::steady_clock::now();
                if (completedRetireAt != std::chrono::steady_clock::time_point{} &&
                    lifecycleNow >= completedRetireAt)
                {
                    // Completion has already been durably accepted and every
                    // connected player received the final snapshot. Retire on
                    // a fixed schedule even if a client leaves its socket
                    // open indefinitely.
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        stopping = true;
                    }
                    break;
                }

                const bool awaitingDurableResult =
                    engine->phase() == game_data::Phase::GameOver &&
                    !resultSubmitted && !resultTerminalFailure;
                const bool awaitingTimer =
                    engine->phase() == game_data::Phase::Playing &&
                    engine->timersAreEnabled();
                if (connections.empty() && !awaitingDurableResult && !awaitingTimer)
                {
                    const auto now = std::chrono::steady_clock::now();
                    if (idleSince == std::chrono::steady_clock::time_point{})
                    {
                        idleSince = now;
                    }
                    else if (now - idleSince >= SessionIdleRetention)
                    {
                        bool retire = false;
                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            if (pendingConnections.empty())
                            {
                                stopping = true;
                                retire = true;
                            }
                        }
                        if (retire)
                        {
                            break;
                        }
                        idleSince = {};
                    }
                }
                else
                {
                    idleSince = {};
                }
            }

            for (Connection& connection : connections)
            {
                connection.socket->disconnect();
            }
            connections.clear();

            std::vector<Connection> abandoned;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                abandoned.swap(pendingConnections);
            }
            for (Connection& connection : abandoned)
            {
                connection.socket->disconnect();
            }
            finished.store(true);
            owner.retireSession(data.battleId, this);
        }

        void adoptConnection(Connection connection)
        {
            connection.socket->setBlocking(false);

            // A battle has exactly one authoritative view per player. A fresh
            // login replaces an older tab/device instead of allowing an
            // unbounded subscriber list or competing same-player actions.
            for (Connection& existing : connections)
            {
                if (existing.playerNumber == connection.playerNumber)
                {
                    existing.socket->disconnect();
                    existing.connected = false;
                }
            }
            removeDisconnected();

            sf::Packet ready;
            ready << static_cast<std::uint8_t>(network::MessageType::ConquestBattleReady)
                  << true << data.battleId << connection.playerNumber
                  << std::string("Conquest battle ready");
            if (connection.socket->send(ready, ClientSendTimeout) !=
                sf::Socket::Status::Done)
            {
                connection.socket->disconnect();
                return;
            }

            connections.push_back(std::move(connection));
            sendSnapshot(connections.back());
            lastTimerBroadcast = std::chrono::steady_clock::now();
        }

        void pollConnection(Connection& connection)
        {
            if (!connection.connected)
            {
                return;
            }

            for (int packetIndex = 0;
                 packetIndex < MaxPacketsPerConnectionPerPoll && connection.connected;
                 ++packetIndex)
            {
                sf::Packet packet;
                const sf::Socket::Status status = connection.socket->receive(packet);
                if (status == sf::Socket::Status::NotReady ||
                    status == sf::Socket::Status::Partial)
                {
                    return;
                }
                if (status != sf::Socket::Status::Done)
                {
                    connection.connected = false;
                    return;
                }

                std::uint8_t rawType = 0;
                packet >> rawType;
                if (!packet)
                {
                    sendSnapshot(connection);
                    continue;
                }
                const auto type = static_cast<network::MessageType>(rawType);
                if (type == network::MessageType::Disconnect ||
                    type == network::MessageType::LeaveConquestBattle)
                {
                    connection.connected = false;
                    return;
                }
                processAction(connection, type, packet);
            }
        }

        void processAction(
            Connection& connection,
            network::MessageType type,
            sf::Packet& packet)
        {
            if (actionTerminalFailure)
            {
                sendSnapshot(connection);
                return;
            }
            if (engine->phase() == game_data::Phase::GameOver)
            {
                sendSnapshot(connection);
                return;
            }
            if (data.actions.size() >= conquest_data::MaxConquestBattleActions ||
                data.actions.size() >=
                    static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
            {
                fmt::println(
                    "Conquest battle {} reached its action limit",
                    data.battleId);
                sendSnapshot(connection);
                return;
            }

            conquest_data::BattleAction action;
            std::string error;
            const std::uint32_t sequence =
                static_cast<std::uint32_t>(data.actions.size() + 1);
            if (!game_action::decodePayload(
                    type,
                    packet,
                    connection.playerNumber,
                    sequence,
                    action,
                    error))
            {
                fmt::println(
                    "Conquest battle {} rejected player {} packet: {}",
                    data.battleId,
                    connection.playerNumber,
                    error);
                sendSnapshot(connection);
                return;
            }

            // Validate against an isolated candidate. game_action::apply
            // returning false rejects legal-shape/no-op packets before they
            // consume the durable action-log limit. The live engine is not
            // mutated yet.
            GameEngine candidate = *engine;
            if (!game_action::apply(candidate, action, &error))
            {
                fmt::println(
                    "Conquest battle {} rejected player {} action: {}",
                    data.battleId,
                    connection.playerNumber,
                    error);
                sendSnapshot(connection);
                return;
            }
            const bool adjudicatedAtLimit =
                game_action::adjudicateConquestActionLimit(
                    candidate,
                    static_cast<std::size_t>(action.sequence),
                    data.seed);

            // Durability is deliberately ordered before committing the
            // candidate. If the account service cannot append the expected
            // sequence, the live engine remains reconstructible.
            const ActionStoreResult stored = storeBattleAction(
                data.battleId,
                capability,
                action);
            if (!stored.success)
            {
                fmt::println(
                    "Conquest battle {} could not persist action {}: {}",
                    data.battleId,
                    action.sequence,
                    stored.message);
                if (permanentActionFailure(stored.message))
                {
                    actionTerminalFailure = true;
                    if (completedRetireAt == std::chrono::steady_clock::time_point{})
                    {
                        completedRetireAt =
                            std::chrono::steady_clock::now() + CompletedSessionGrace;
                    }
                    sendSnapshot(connection);
                    return;
                }
                resyncAfterStoreFailure(connection, action);
                sendSnapshot(connection);
                return;
            }

            data.actions.push_back(action);
            *engine = std::move(candidate);

            if (adjudicatedAtLimit)
            {
                fmt::println(
                    "Conquest battle {} reached its action limit; player {} wins by board adjudication",
                    data.battleId,
                    engine->winner());
            }

            if (engine->phase() == game_data::Phase::GameOver)
            {
                nextResultAttempt = {};
            }
            broadcastSnapshots();
            trySubmitResult();
        }

        void resyncAfterStoreFailure(
            Connection& connection,
            const conquest_data::BattleAction& attempted)
        {
            BattleDataResult refreshed = reloadBattleData(
                data.battleId,
                capability);
            if (!refreshed.success ||
                !sameIdentity(refreshed.data) ||
                refreshed.data.actions.size() < data.actions.size())
            {
                return;
            }

            std::string error;
            std::unique_ptr<GameEngine> refreshedEngine =
                owner.makeEngine(refreshed.data, error);
            if (!refreshedEngine)
            {
                fmt::println(
                    "Conquest battle {} could not resync after an action-store failure: {}",
                    data.battleId,
                    error);
                return;
            }

            const bool attemptedActionWasStored =
                attempted.sequence > 0 &&
                refreshed.data.actions.size() >= attempted.sequence &&
                sameAction(
                    refreshed.data.actions[static_cast<std::size_t>(attempted.sequence - 1)],
                    attempted);
            data = std::move(refreshed.data);
            data.actions.reserve(conquest_data::MaxConquestBattleActions);
            engine = std::move(refreshedEngine);
            if (engine->phase() == game_data::Phase::GameOver)
            {
                nextResultAttempt = {};
            }
            broadcastSnapshots();
            trySubmitResult();

            fmt::println(
                "Conquest battle {} resynced at action {}{}",
                data.battleId,
                data.actions.size(),
                attemptedActionWasStored ? " (retry was already committed)" : "");
        }

        void sendSnapshot(Connection& connection)
        {
            if (!connection.connected)
            {
                return;
            }
            sf::Packet packet;
            packet << static_cast<std::uint8_t>(network::MessageType::GameStateUpdate);
            game_data::writeSnapshot(
                packet,
                engine->snapshotFor(connection.playerNumber));
            if (connection.socket->send(packet, ClientSendTimeout) !=
                sf::Socket::Status::Done)
            {
                connection.connected = false;
            }
        }

        void broadcastSnapshots()
        {
            for (Connection& connection : connections)
            {
                sendSnapshot(connection);
            }
            if (!connections.empty())
            {
                lastTimerBroadcast = std::chrono::steady_clock::now();
            }
        }

        void removeDisconnected()
        {
            for (Connection& connection : connections)
            {
                if (!connection.connected)
                {
                    connection.socket->disconnect();
                }
            }
            connections.erase(
                std::remove_if(
                    connections.begin(),
                    connections.end(),
                    [](const Connection& connection) {
                        return !connection.connected;
                    }),
                connections.end());
        }

        void trySubmitResult()
        {
            if (resultSubmitted || resultTerminalFailure ||
                engine->phase() != game_data::Phase::GameOver ||
                (engine->winner() != 1 && engine->winner() != 2))
            {
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (nextResultAttempt != std::chrono::steady_clock::time_point{} &&
                now < nextResultAttempt)
            {
                return;
            }
            nextResultAttempt = now + ResultRetryInterval;

            const ResultStoreResult stored = storeBattleResult(
                data.battleId,
                capability,
                engine->winner());
            if (stored.success)
            {
                resultSubmitted = true;
                completedRetireAt = now + CompletedSessionGrace;
                // The first GameOver snapshot can precede the result RPC. Send
                // it once more after the strategic result is durable so the
                // final client-visible state has a clear persistence barrier.
                broadcastSnapshots();
                fmt::println(
                    "Conquest battle {} completed; player {} won",
                    data.battleId,
                    engine->winner());
            }
            else
            {
                if (permanentResultFailure(stored.message))
                {
                    resultTerminalFailure = true;
                    completedRetireAt = now + CompletedSessionGrace;
                    fmt::println(
                        "Conquest battle {} can no longer store its result; retiring session: {}",
                        data.battleId,
                        stored.message);
                }
                else
                {
                    fmt::println(
                        "Conquest battle {} result submission will retry: {}",
                        data.battleId,
                        stored.message);
                }
            }
        }
    };

    std::vector<card_data::Card> cardCatalog;
    std::unordered_map<std::string, card_data::Card> cardsByTitle;
    std::mutex sessionsMutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<Session>> sessions;
    std::vector<std::shared_ptr<Session>> retiredSessions;

    std::vector<card_data::Card> resolveDeck(
        const deck_data::Deck& deck,
        const std::vector<card_data::Card>& frozenCards,
        std::string& error) const
    {
        std::vector<card_data::Card> cards;
        if (!frozenCards.empty())
        {
            if (frozenCards.size() != deck.cardTitles.size())
            {
                error = "Frozen Conquest card definitions do not match the deck size";
                return {};
            }
            for (std::size_t index = 0; index < frozenCards.size(); ++index)
            {
                if (frozenCards[index].title != deck.cardTitles[index])
                {
                    error = "Frozen Conquest card definitions do not match deck title order";
                    return {};
                }
            }
            cards = frozenCards;
        }
        else
        {
            // Legacy battles created before frozen definitions were stored can
            // still be resumed from the current authoritative catalog.
            cards.reserve(deck.cardTitles.size());
            for (const std::string& title : deck.cardTitles)
            {
                const auto found = cardsByTitle.find(title);
                if (found == cardsByTitle.end())
                {
                    error = "Frozen Conquest deck references unknown card " + title;
                    return {};
                }
                cards.push_back(found->second);
            }
        }
        if (const std::optional<std::string> rulesError =
                game_data::deckRulesError(cards))
        {
            error = "Frozen Conquest deck is invalid: " + *rulesError;
            return {};
        }
        return cards;
    }

    std::unique_ptr<GameEngine> makeEngine(
        const conquest_data::BattleData& battle,
        std::string& error) const
    {
        if (battle.battleId == 0 || battle.playerOne.empty() ||
            battle.playerTwo.empty() || battle.playerOne == battle.playerTwo)
        {
            error = "Invalid Conquest battle participants";
            return nullptr;
        }
        if (battle.catalog.empty() && cardCatalog.empty())
        {
            error = "Conquest battle card catalog is unavailable";
            return nullptr;
        }

        std::vector<card_data::Card> deckOne =
            resolveDeck(battle.deckOne, battle.cardsOne, error);
        if (!error.empty())
        {
            return nullptr;
        }
        std::vector<card_data::Card> deckTwo =
            resolveDeck(battle.deckTwo, battle.cardsTwo, error);
        if (!error.empty())
        {
            return nullptr;
        }

        const std::vector<card_data::Card>& replayCatalog =
            battle.catalog.empty() ? cardCatalog : battle.catalog;
        auto engine = std::make_unique<GameEngine>(battle.seed, replayCatalog);
        engine->enableTimers(
            GameEngine::ConquestClockMs,
            GameEngine::ConquestFullTurnTimerMs,
            GameEngine::ConquestReducedTurnTimerMs,
            GameEngine::ConquestMinimumTurnTimerMs);
        engine->submitDeck(1, deckOne);
        engine->submitDeck(2, deckTwo);

        std::uint32_t expectedSequence = 1;
        std::int64_t previousActionTimestamp = battle.createdAt;
        std::int64_t timerTimestamp = battle.timerStartedAt;
        for (const conquest_data::BattleAction& action : battle.actions)
        {
            if (action.sequence != expectedSequence)
            {
                error = "Conquest battle action log has a sequence gap";
                return nullptr;
            }
            if (action.createdAt < previousActionTimestamp ||
                action.createdAt > battle.loadedAt)
            {
                error = "Conquest battle action log has invalid timestamps";
                return nullptr;
            }
            const std::int64_t timedActionAt = std::max(
                action.createdAt,
                battle.timerStartedAt);
            engine->updateTimers((timedActionAt - timerTimestamp) * 1000);
            if (engine->phase() == game_data::Phase::GameOver)
            {
                error = "Conquest battle action log continues after GameOver";
                return nullptr;
            }
            if (!game_action::apply(*engine, action, &error))
            {
                error = "Conquest battle action log is invalid: " + error;
                return nullptr;
            }
            previousActionTimestamp = action.createdAt;
            timerTimestamp = timedActionAt;
            ++expectedSequence;
        }
        engine->updateTimers((battle.loadedAt - timerTimestamp) * 1000);
        game_action::adjudicateConquestActionLimit(
            *engine,
            battle.actions.size(),
            battle.seed);
        error.clear();
        return engine;
    }
};

void ConquestBattleManager::Impl::retireSession(
    std::uint64_t battleId,
    Session* retiring)
{
    std::lock_guard<std::mutex> lock(sessionsMutex);
    const auto found = sessions.find(battleId);
    if (found == sessions.end() || found->second.get() != retiring)
    {
        return;
    }
    retiredSessions.push_back(std::move(found->second));
    sessions.erase(found);
}

void ConquestBattleManager::Impl::reapRetiredSessions()
{
    std::vector<std::shared_ptr<Session>> completed;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto iterator = retiredSessions.begin();
             iterator != retiredSessions.end();)
        {
            if ((*iterator)->isFinished())
            {
                completed.push_back(std::move(*iterator));
                iterator = retiredSessions.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }
    for (const std::shared_ptr<Session>& session : completed)
    {
        session->join();
    }
}

ConquestBattleManager::Impl::~Impl()
{
    std::vector<std::shared_ptr<Session>> activeSessions;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        activeSessions.reserve(sessions.size());
        for (auto& [battleId, session] : sessions)
        {
            (void)battleId;
            activeSessions.push_back(session);
        }
        for (const std::shared_ptr<Session>& session : retiredSessions)
        {
            activeSessions.push_back(session);
        }
    }
    for (const std::shared_ptr<Session>& session : activeSessions)
    {
        session->requestStop();
    }
    for (const std::shared_ptr<Session>& session : activeSessions)
    {
        session->join();
    }
}

ConquestBattleManager::ConquestBattleManager()
    : impl(std::make_unique<Impl>(std::vector<card_data::Card>{}))
{
}

ConquestBattleManager::ConquestBattleManager(
    std::vector<card_data::Card> cardCatalog)
    : impl(std::make_unique<Impl>(std::move(cardCatalog)))
{
}

ConquestBattleManager::~ConquestBattleManager() = default;

void ConquestBattleManager::handleClient(
    std::unique_ptr<bayou::tls::Socket> client,
    std::uint64_t battleId,
    std::string accessToken)
{
    impl->handleClient(std::move(client), battleId, std::move(accessToken));
}
