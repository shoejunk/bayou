#include "deck_collection.hpp"

#include "../shared/game_data.hpp"

#include <algorithm>

namespace bayou::client
{
std::vector<card_data::Card> resolveDeckCards(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library)
{
    std::vector<card_data::Card> resolved;
    resolved.reserve(deck.cardTitles.size());
    for (const std::string& title : deck.cardTitles)
    {
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        if (found != library.end())
        {
            resolved.push_back(*found);
        }
    }
    return resolved;
}

int collectionCopiesFor(const std::vector<account_data::CollectionCard>& collection, const std::string& title)
{
    const auto found = std::find_if(collection.begin(), collection.end(), [&](const account_data::CollectionCard& card) {
        return card.title == title;
    });
    return found == collection.end() ? 0 : found->copies;
}

std::vector<card_data::Card> ownedCardsFromCollection(
    const std::vector<card_data::Card>& library,
    const std::vector<account_data::CollectionCard>& collection)
{
    std::vector<card_data::Card> ownedCards;
    for (const card_data::Card& card : library)
    {
        if (collectionCopiesFor(collection, card.title) > 0)
        {
            ownedCards.push_back(card);
        }
    }
    return ownedCards;
}

int countHeroes(const std::vector<card_data::Card>& cards)
{
    return static_cast<int>(std::count_if(cards.begin(), cards.end(), [](const card_data::Card& card) {
        return game_data::isHeroCard(card);
    }));
}
}
