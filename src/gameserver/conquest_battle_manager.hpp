#pragma once

#include "../shared/card_data.hpp"
#include "../shared/tls_socket.hpp"

#include <cstdint>
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
    ConquestBattleManager();
    explicit ConquestBattleManager(std::vector<card_data::Card> cardCatalog);
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
