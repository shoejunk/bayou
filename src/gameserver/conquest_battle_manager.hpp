#pragma once

#include "../shared/card_data.hpp"
#include "../shared/tls_socket.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Owns reconnectable tactical sessions for Conquest battles. The coordinator
// authenticates no Conquest details itself: it passes the accepted socket and
// JoinConquestBattle payload here, and the manager obtains the authoritative
// players, frozen decks, seed, and action log from the account server.
class ConquestBattleManager
{
public:
    // Fetches the authoritative catalog, returning an empty vector and setting
    // `error` when the card source is unavailable.
    using CardCatalogLoader = std::function<std::vector<card_data::Card>(std::string& error)>;

    ConquestBattleManager();
    explicit ConquestBattleManager(std::vector<card_data::Card> cardCatalog);
    // Legacy battles (stored before frozen card definitions) replay against the
    // coordinator's own catalog, so `loader` re-pulls it and card edits made
    // while the coordinator runs apply without a restart.
    ConquestBattleManager(std::vector<card_data::Card> cardCatalog, CardCatalogLoader loader);
    ~ConquestBattleManager();

    ConquestBattleManager(const ConquestBattleManager&) = delete;
    ConquestBattleManager& operator=(const ConquestBattleManager&) = delete;
    ConquestBattleManager(ConquestBattleManager&&) = delete;
    ConquestBattleManager& operator=(ConquestBattleManager&&) = delete;

    // May be called concurrently. Ownership of client always transfers to the
    // manager. On success a per-battle worker becomes the only thread touching
    // the TLS socket, which is necessary because bayou::tls::Socket toggles its
    // underlying blocking mode during send.
    void handleClient(
        std::unique_ptr<bayou::tls::Socket> client,
        std::uint64_t battleId,
        std::string accessToken);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
