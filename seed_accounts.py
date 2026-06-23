import os
import sqlite3

from argon2 import PasswordHasher
from argon2.low_level import Type

password = os.environ.get("BAYOU_SEED_PASSWORD", "")
if not 15 <= len(password) <= 128:
    raise SystemExit("Set BAYOU_SEED_PASSWORD to a 15-128 character development password.")

admin_username = os.environ.get("BAYOU_SEED_ADMIN_USERNAME", "").strip()
password_hasher = PasswordHasher(
    time_cost=2,
    memory_cost=65536,
    parallelism=1,
    hash_len=32,
    salt_len=16,
    type=Type.ID,
)
pw = password_hasher.hash(password)

con = sqlite3.connect("accounts.db")
cur = con.cursor()

cur.execute("""
CREATE TABLE IF NOT EXISTS accounts (
    username TEXT PRIMARY KEY NOT NULL,
    password_hash TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
)
""")
cur.execute("PRAGMA table_info(accounts)")
columns = [row[1] for row in cur.fetchall()]
if "coins" not in columns:
    cur.execute("ALTER TABLE accounts ADD COLUMN coins INTEGER NOT NULL DEFAULT 0")
if "is_admin" not in columns:
    cur.execute("ALTER TABLE accounts ADD COLUMN is_admin INTEGER NOT NULL DEFAULT 0")
if "rating" not in columns:
    cur.execute("ALTER TABLE accounts ADD COLUMN rating INTEGER NOT NULL DEFAULT 0")
cur.execute("""
CREATE TABLE IF NOT EXISTS decks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    name TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(username, name),
    FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE
)
""")
cur.execute("""
CREATE TABLE IF NOT EXISTS deck_cards (
    deck_id INTEGER NOT NULL,
    card_index INTEGER NOT NULL,
    card_title TEXT NOT NULL,
    PRIMARY KEY(deck_id, card_index),
    FOREIGN KEY(deck_id) REFERENCES decks(id) ON DELETE CASCADE
)
""")
cur.execute("""
CREATE TABLE IF NOT EXISTS card_collections (
    username TEXT NOT NULL,
    card_title TEXT NOT NULL,
    copies INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(username, card_title),
    FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE
)
""")

def ensure_account(user, is_admin=False):
    cur.execute(
        "INSERT INTO accounts(username, password_hash, coins, is_admin) VALUES(?,?,0,?) "
        "ON CONFLICT(username) DO UPDATE SET password_hash=excluded.password_hash, is_admin=excluded.is_admin",
        (user, pw, 1 if is_admin else 0))

def set_deck(user, name, cards):
    cur.execute("SELECT id FROM decks WHERE username=? AND name=?", (user, name))
    row = cur.fetchone()
    if row:
        deck_id = row[0]
        cur.execute("DELETE FROM deck_cards WHERE deck_id=?", (deck_id,))
    else:
        cur.execute("INSERT INTO decks(username, name) VALUES(?,?)", (user, name))
        deck_id = cur.lastrowid
    for i, title in enumerate(cards):
        cur.execute("INSERT INTO deck_cards(deck_id, card_index, card_title) VALUES(?,?,?)", (deck_id, i, title))

def set_collection(user, cards):
    counts = {}
    for title in cards:
        counts[title] = counts.get(title, 0) + 1
    cur.execute("DELETE FROM card_collections WHERE username=?", (user,))
    for title, copies in sorted(counts.items()):
        cur.execute(
            "INSERT INTO card_collections(username, card_title, copies) VALUES(?,?,?)",
            (user, title, copies))

starter_nonheroes = [
    "Brass Pawn", "Rifleman", "Clockwork Rook", "Steam Bishop", "Automaton Knight",
    "Dredger", "Spark Drone", "Smoke Bomb", "Cannon Blast", "Repair Kit",
    "Overpressure", "Gearwright", "Brass Medic", "Boiler Imp", "Railgunner",
    "Swamp Skiff", "Arc Lantern", "Sprocket Swarm", "Chain Harpoon", "Mudslide",
]
cards20 = [title for title in starter_nonheroes[:10] for _ in range(2)]
assert len(cards20) == 20, len(cards20)

ensure_account("alpha")
alpha_deck = ["Steam Baron"] + cards20
set_collection("alpha", alpha_deck)
set_deck("alpha", "Baron Brawl", alpha_deck)  # 1 hero, cost 100

ensure_account("bravo")
bravo_deck = ["Gear Knight", "Marsh Witch"] + cards20
set_collection("bravo", bravo_deck)
set_deck("bravo", "Knight Coven", bravo_deck)  # 2 heroes, cost 90

if admin_username:
    ensure_account(admin_username, is_admin=True)

con.commit()
con.close()
print("Seeded development accounts alpha/bravo with collections, coins, and valid decks.")
if admin_username:
    print(f"Seeded admin account: {admin_username}")
