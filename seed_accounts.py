import sqlite3

def fnv1a(password: str) -> str:
    h = 14695981039346656037
    mask = 0xFFFFFFFFFFFFFFFF
    for ch in password.encode('utf-8'):
        h ^= ch
        h = (h * 1099511628211) & mask
    return f"{h:016x}"

pw = fnv1a("test1234")
print("hash(test1234) =", pw)

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
if "coins" not in [row[1] for row in cur.fetchall()]:
    cur.execute("ALTER TABLE accounts ADD COLUMN coins INTEGER NOT NULL DEFAULT 0")
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

def ensure_account(user):
    cur.execute(
        "INSERT INTO accounts(username, password_hash, coins) VALUES(?,?,0) "
        "ON CONFLICT(username) DO UPDATE SET password_hash=excluded.password_hash",
        (user, pw))

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
cards40 = [title for title in starter_nonheroes for _ in range(2)]
assert len(cards40) == 40, len(cards40)

ensure_account("alpha")
alpha_deck = ["Steam Baron"] + cards40
set_collection("alpha", alpha_deck)
set_deck("alpha", "Baron Brawl", alpha_deck)  # 1 hero, cost 10

ensure_account("bravo")
bravo_deck = ["Gear Knight", "Marsh Witch"] + cards40
set_collection("bravo", bravo_deck)
set_deck("bravo", "Knight Coven", bravo_deck)  # 2 heroes, cost 9

con.commit()
con.close()
print("Seeded accounts alpha/bravo (password test1234) with collections, coins, and valid decks.")
