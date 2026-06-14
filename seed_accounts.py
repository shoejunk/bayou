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

def ensure_account(user):
    cur.execute("INSERT OR REPLACE INTO accounts(username, password_hash) VALUES(?,?)", (user, pw))

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

# 20 non-hero cards
units20 = (["Brass Pawn"]*6 + ["Rifleman"]*3 + ["Clockwork Rook"]*2 + ["Steam Bishop"]*2 +
           ["Automaton Knight"]*2 + ["Dredger"]*1 + ["Spark Drone"]*2 + ["Smoke Bomb"]*1 +
           ["Repair Kit"]*1)
assert len(units20) == 20, len(units20)

ensure_account("alpha")
set_deck("alpha", "Baron Brawl", ["Steam Baron"] + units20)  # 1 hero, cost 10

ensure_account("bravo")
set_deck("bravo", "Knight Coven", ["Gear Knight", "Marsh Witch"] + units20)  # 2 heroes, cost 9

con.commit()
con.close()
print("Seeded accounts alpha/bravo (password test1234) with valid decks.")
