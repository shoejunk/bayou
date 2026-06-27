#pragma once

#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <string>
#include <vector>

namespace bayou::client
{
std::vector<card_data::Card> resolveDeckCards(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library);
int collectionCopiesFor(const std::vector<account_data::CollectionCard>& collection, const std::string& title);
std::vector<card_data::Card> ownedCardsFromCollection(
    const std::vector<card_data::Card>& library,
    const std::vector<account_data::CollectionCard>& collection);
int countHeroes(const std::vector<card_data::Card>& cards);
}
