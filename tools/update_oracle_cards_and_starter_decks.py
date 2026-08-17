#!/usr/bin/env python3
"""Update production card traits and starter deck contents.

Run on the Oracle VM:

    python3 tools/update_oracle_cards_and_starter_decks.py \
      --cards-db /var/lib/bayou/shared/cards.db \
      --starter-db /var/lib/bayou/shared/starter_decks.db

The script preserves actions, stats, images, keywords, and other card metadata.
It updates only card_traits, starter-card rarity/deck limits, and starter decks.
"""

from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path


TRAITS = {
    "civilized",
    "wild",
    "corrupt",
    "honorable",
    "fey",
    "arcane",
    "mechanical",
    "undead",
    "ancient",
}


@dataclass(frozen=True)
class CardSpec:
    title: str
    traits: tuple[str, ...]
    aliases: tuple[str, ...] = ()


def c(title: str, *traits: str, aliases: tuple[str, ...] = ()) -> CardSpec:
    return CardSpec(title, traits, aliases)


CARD_SPECS = [
    c("Thaeron Baelstone", "civilized", "corrupt", "arcane"),
    c("Ashenfang", "wild", "corrupt"),
    c("Blackthorn Debt Collector", "civilized", "corrupt"),
    c("Blackthorn Alchemist", "civilized", "arcane"),
    c("Blackthorn Foreman", "civilized", "corrupt"),
    c("Blackthorn Lumberjack", "civilized"),
    c("Grove Sister", "wild", "corrupt"),
    c("Mog", "wild", "corrupt"),
    c("Grask", "wild", "corrupt"),
    c("Goblin Ambusher", "wild"),
    c("Braun Stonefist", "civilized", "corrupt"),
    c("Goblin Sharpshooter", "corrupt"),
    c("Victor Greyshard", "civilized", "corrupt"),
    c("Fizzlewick Gearwright", "corrupt", "mechanical"),
    c("Lumber Automaton", "corrupt", "mechanical"),
    c("Clockwork Guardian", "mechanical"),
    c("Gearjaw", "mechanical", aliases=("GearJaw",)),
    c("Remy Croche", "corrupt"),
    c("Camp Physician", "civilized", "arcane"),
    c("Blackthorn Engineer", "civilized", "mechanical"),
    c("Black Powder Keg", "mechanical"),
    c("Gilded Cage", "arcane"),
    c("Joni Pumpernickel", "civilized", "arcane", aliases=("Joni Pumpernickle",)),
    c("Vanya Bluewater", "civilized", "honorable"),
    c("Birdie the Wise", "wild", "honorable"),
    c("Donella of the Marsh", "wild", "arcane"),
    c("Juniper Flash", "honorable", "arcane"),
    c("Scooter", "wild"),
    c("Erevan the Shadow", "civilized", "arcane"),
    c("Reed Baelstone", "civilized", "honorable"),
    c("Bog Spearman", "wild"),
    c("Marshland Veteran", "honorable"),
    c("Resistance Smuggler", "civilized"),
    c("Mirewatch Informant", "civilized"),
    c("Swamp Tracker", "wild", "honorable"),
    c("Maggie Mudroot", "wild", "arcane", "undead", aliases=("Maggie Mudroot (mounted)",)),
    c("Rowan Leafbound", "civilized", "honorable"),
    c("Telos the Merchant", "civilized", "arcane"),
    c("Maggie's Recruit", "civilized", "undead", aliases=("Maggies Recruit", "Maggie's Recruit (skeleton)")),
    c("Nibsy", "undead", "arcane"),
    c("Elliot Greentide", "honorable", "undead", aliases=("The Ghost of Elliot Greentide", "Ghost of Elliot Greentide")),
    c("Bull Gator", "wild"),
    c("Delphine Nettle", "wild", "arcane"),
    c("Hidden Camp", "wild"),
    c("Resistance Cache", "honorable"),
    c("Prince Vesper", "arcane", "honorable"),
    c("Caltheriel", "fey", "honorable"),
    c("Sylvara", "ancient", "wild", "fey"),
    c("Pavo Quickstep", "arcane", "fey"),
    c("Nettle Starbright", "wild", "honorable", aliases=("Nettle Starbright (mounted on Dash)",)),
    c("Archivist Mosswake", "ancient", "wild"),
    c("Duchess Dewbell", "ancient"),
    c("Fey Messenger", "fey"),
    c("Starbloom Knight", "fey", "honorable"),
    c("Crystal Unicorn", "wild", "honorable", aliases=("Crystalline Unicorn",)),
    c("Heartwood Sister", "ancient", "fey"),
    c("Thorn Griffin", "wild"),
    c("Quinberry Lark", "arcane"),
    c("Lady Mirrorglace", "ancient", "arcane", aliases=("Lady Mirrorglass",)),
    c("Sylvan Enchantress", "arcane", "honorable"),
    c("Sylvan Champion", "fey", "honorable"),
    c("Marrowind", "ancient", "honorable", aliases=("Morrowind",)),
    c("Vaelthorn", "fey", "wild", aliases=("Vaeloren",)),
    c("Glimmer Stag", "fey"),
    c("Sun Sprite", "arcane", "fey"),
    c("Mossy Giant", "wild", "ancient"),
    c("Fairy Ring", "fey"),
    c("Heartwood", "ancient"),
    c("Queen Nyxara", "ancient", "fey", "corrupt"),
    c("Archduchess Vespara", "undead", "fey", "corrupt"),
    c("Briar Whisperthorn", "fey"),
    c("Gloom Fairy", "undead", "fey"),
    c("Pipistrel Vane", "ancient", "corrupt"),
    c("Dusk Harvester", "undead", "fey", "corrupt"),
    c("Lady Silkmaw", "corrupt"),
    c("Duke Lanternwing", "ancient", "corrupt"),
    c("Bristlejack", "corrupt"),
    c("Mourning Light", "undead", "fey", aliases=("Mourning Lights",)),
    c("Moonshade Stalker", "fey"),
    c("Widowroot", "ancient"),
    c("The Tockman", "mechanical", "fey", "arcane"),
    c("Sister Honeygrave", "ancient", "arcane"),
    c("Father Grub", "undead", "arcane"),
    c("The Weaver", "fey", "mechanical"),
    c("The Choir", "fey", "corrupt"),
    c("Hushkeeper", "mechanical", "arcane"),
    c("Warden", "mechanical", aliases=("The Warden",)),
    c("Lord Pallid", "arcane", "corrupt"),
    c("Sir Chitinous Vale", "undead", "fey"),
    c("Echo Lantern", "mechanical"),
    c("Eyeblight", "corrupt"),
    c("Ambush", "wild"),
    c("Rapid Assembly", "mechanical"),
    c("Supply Shortage", "civilized"),
    c("Survival Instinct", "wild"),
    c("Hidden Path", "fey"),
    c("Spell Surge", "arcane"),
    c("Ancient Awakening", "ancient"),
    c("Cursed Ground", "corrupt"),
    c("Raise the Fallen", "undead"),
    c("Last Stand", "honorable"),
]


STARTER_DECKS = {
    "The Blackthorns": [
        ("Thaeron Baelstone", 1),
        ("Ashenfang", 1),
        ("Blackthorn Debt Collector", 2),
        ("Blackthorn Alchemist", 2),
        ("Blackthorn Foreman", 2),
        ("Blackthorn Lumberjack", 4),
        ("Grove Sister", 3),
        ("Mog", 1),
        ("Grask", 1),
        ("Goblin Ambusher", 2),
        ("Braun Stonefist", 1),
        ("Goblin Sharpshooter", 2),
    ],
    "The Mirewatch Resistance": [
        ("Joni Pumpernickel", 1),
        ("Vanya Bluewater", 1),
        ("Birdie the Wise", 1),
        ("Donella of the Marsh", 1),
        ("Juniper Flash", 1),
        ("Scooter", 1),
        ("Erevan the Shadow", 1),
        ("Reed Baelstone", 1),
        ("Bog Spearman", 4),
        ("Marshland Veteran", 4),
        ("Resistance Smuggler", 3),
        ("Mirewatch Informant", 2),
        ("Swamp Tracker", 2),
    ],
    "The Seelie Court": [
        ("Prince Vesper", 1),
        ("Caltheriel", 1),
        ("Sylvara", 1),
        ("Pavo Quickstep", 1),
        ("Nettle Starbright", 1),
        ("Archivist Mosswake", 1),
        ("Duchess Dewbell", 1),
        ("Fey Messenger", 3),
        ("Starbloom Knight", 4),
        ("Crystal Unicorn", 2),
        ("Heartwood Sister", 3),
        ("Thorn Griffin", 2),
        ("Quinberry Lark", 2),
    ],
    "The Unseelie Court": [
        ("Queen Nyxara", 1),
        ("Archduchess Vespara", 1),
        ("Briar Whisperthorn", 1),
        ("Gloom Fairy", 4),
        ("Pipistrel Vane", 1),
        ("Dusk Harvester", 2),
        ("Lady Silkmaw", 1),
        ("Duke Lanternwing", 1),
        ("Bristlejack", 2),
        ("Mourning Light", 4),
        ("Moonshade Stalker", 2),
        ("Widowroot", 2),
    ],
}


NON_STARTER_TITLES = {
    "Victor Greyshard",
    "Fizzlewick Gearwright",
    "Lumber Automaton",
    "Clockwork Guardian",
    "Gearjaw",
    "Remy Croche",
    "Camp Physician",
    "Blackthorn Engineer",
    "Black Powder Keg",
    "Gilded Cage",
    "Maggie Mudroot",
    "Rowan Leafbound",
    "Telos the Merchant",
    "Maggie's Recruit",
    "Nibsy",
    "Elliot Greentide",
    "Bull Gator",
    "Delphine Nettle",
    "Hidden Camp",
    "Resistance Cache",
    "Lady Mirrorglace",
    "Sylvan Enchantress",
    "Sylvan Champion",
    "Marrowind",
    "Vaelthorn",
    "Glimmer Stag",
    "Sun Sprite",
    "Mossy Giant",
    "Fairy Ring",
    "Heartwood",
    "The Tockman",
    "Sister Honeygrave",
    "Father Grub",
    "The Weaver",
    "The Choir",
    "Hushkeeper",
    "Warden",
    "Lord Pallid",
    "Sir Chitinous Vale",
    "Echo Lantern",
    "Eyeblight",
    "Ambush",
    "Rapid Assembly",
    "Supply Shortage",
    "Survival Instinct",
    "Hidden Path",
    "Spell Surge",
    "Ancient Awakening",
    "Cursed Ground",
    "Raise the Fallen",
    "Last Stand",
}


def normalized(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", value.lower())


def existing_titles(connection: sqlite3.Connection) -> list[str]:
    return [row[0] for row in connection.execute("SELECT title FROM cards")]


def build_resolver(titles: list[str]) -> dict[str, str]:
    resolved: dict[str, str] = {}
    for title in titles:
        resolved[normalized(title)] = title
    return resolved


def resolve_title(name: str, specs: dict[str, CardSpec], resolver: dict[str, str]) -> str | None:
    candidates = [name]
    spec = specs.get(name)
    if spec:
        candidates.extend(spec.aliases)
    for candidate in candidates:
        exact = resolver.get(normalized(candidate))
        if exact:
            return exact
    return None


def validate_traits() -> None:
    for spec in CARD_SPECS:
        unknown = set(spec.traits) - TRAITS
        if unknown:
            raise ValueError(f"{spec.title} has unknown traits: {sorted(unknown)}")


def set_rarity(connection: sqlite3.Connection, title: str, rarity: str) -> None:
    connection.execute("DELETE FROM card_string_values WHERE title = ? AND key = 'rarity'", (title,))
    connection.execute(
        "INSERT INTO card_string_values (title, key, value) VALUES (?, 'rarity', ?)",
        (title, rarity),
    )


def clear_starter_rarity(connection: sqlite3.Connection, title: str) -> None:
    connection.execute(
        "DELETE FROM card_string_values WHERE title = ? AND key = 'rarity' AND value = 'starter'",
        (title,),
    )


def set_deck_limit(connection: sqlite3.Connection, title: str, limit: int) -> None:
    connection.execute(
        "DELETE FROM card_integer_values WHERE title = ? AND key IN ('Deck Limit', 'deckLimit')",
        (title,),
    )
    connection.execute(
        "INSERT INTO card_integer_values (title, key, value) VALUES (?, 'Deck Limit', ?)",
        (title, limit),
    )


def update_cards(connection: sqlite3.Connection, dry_run: bool) -> dict[str, str]:
    specs = {spec.title: spec for spec in CARD_SPECS}
    resolver = build_resolver(existing_titles(connection))
    resolved_by_spec: dict[str, str] = {}
    missing: list[str] = []
    starter_quantities: dict[str, int] = {}

    for deck in STARTER_DECKS.values():
        for title, quantity in deck:
            starter_quantities[title] = max(starter_quantities.get(title, 0), quantity)

    for spec in CARD_SPECS:
        resolved = resolve_title(spec.title, specs, resolver)
        if not resolved:
            missing.append(spec.title)
            continue
        resolved_by_spec[spec.title] = resolved

    if missing:
        raise RuntimeError("Missing cards in cards.db: " + ", ".join(missing))

    if dry_run:
        return resolved_by_spec

    with connection:
        for spec in CARD_SPECS:
            title = resolved_by_spec[spec.title]
            connection.execute("DELETE FROM card_traits WHERE title = ?", (title,))
            connection.executemany(
                "INSERT INTO card_traits (title, trait) VALUES (?, ?)",
                [(title, trait) for trait in spec.traits],
            )

        for title, quantity in starter_quantities.items():
            resolved = resolved_by_spec[title]
            set_rarity(connection, resolved, "starter")
            set_deck_limit(connection, resolved, max(1, quantity))

        for title in NON_STARTER_TITLES:
            resolved = resolved_by_spec[title]
            clear_starter_rarity(connection, resolved)

    return resolved_by_spec


def update_starter_decks(connection: sqlite3.Connection, resolved: dict[str, str], dry_run: bool) -> None:
    if dry_run:
        return
    with connection:
        connection.execute(
            "CREATE TABLE IF NOT EXISTS starter_deck_cards ("
            "deck_name TEXT NOT NULL,"
            "card_index INTEGER NOT NULL,"
            "card_title TEXT NOT NULL,"
            "PRIMARY KEY(deck_name, card_index)"
            ")"
        )
        for deck_name, entries in STARTER_DECKS.items():
            connection.execute("DELETE FROM starter_deck_cards WHERE deck_name = ?", (deck_name,))
            card_index = 0
            for title, quantity in entries:
                for _ in range(quantity):
                    connection.execute(
                        "INSERT INTO starter_deck_cards (deck_name, card_index, card_title) VALUES (?, ?, ?)",
                        (deck_name, card_index, resolved[title]),
                    )
                    card_index += 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cards-db", default="/var/lib/bayou/shared/cards.db")
    parser.add_argument("--starter-db", default="/var/lib/bayou/shared/starter_decks.db")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    validate_traits()
    cards_db = Path(args.cards_db)
    starter_db = Path(args.starter_db)
    if not cards_db.exists():
        raise SystemExit(f"cards database not found: {cards_db}")
    if not starter_db.exists() and not args.dry_run:
        starter_db.parent.mkdir(parents=True, exist_ok=True)

    cards = sqlite3.connect(cards_db)
    starter = sqlite3.connect(starter_db)
    try:
        resolved = update_cards(cards, args.dry_run)
        update_starter_decks(starter, resolved, args.dry_run)
    finally:
        cards.close()
        starter.close()

    mode = "would update" if args.dry_run else "updated"
    print(f"{mode} {len(CARD_SPECS)} card trait records")
    for deck_name, entries in STARTER_DECKS.items():
        total = sum(quantity for _, quantity in entries)
        print(f"{mode} {deck_name}: {total} cards")
    return 0


if __name__ == "__main__":
    sys.exit(main())
