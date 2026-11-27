#!/usr/bin/env python3
"""Prepare a new isolated catalog with Fey Messenger's existing Foresight passive.

Never modifies the source or replaces an existing output. The server installer
never deploys this database; use it only for rehearsal/review, not replacement
of an authoritative catalog. Supports native passives and legacy keyword storage.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import sqlite3


def rows(db, table):
    return db.execute('SELECT * FROM "' + table.replace('"', '""') + '"').fetchall()


def prepare(source, output):
    source, output = source.resolve(strict=True), output.resolve()
    if source == output or output.exists():
        raise ValueError("Output must be a new isolated database, distinct from source")
    original = sqlite3.connect(source.as_uri() + "?mode=ro", uri=True)
    try:
        tables = [r[0] for r in original.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name")]
        table, column = ("card_passives", "passive") if "card_passives" in tables else ("card_keywords", "keyword")
        canonical = original.execute(f"SELECT {column} FROM {table} WHERE title=? AND lower({column})='foresight'", ("Archivist Mosswake",)).fetchall()
        if len(canonical) != 1:
            raise ValueError("Archivist Mosswake must have exactly one canonical Foresight assignment")
        if original.execute("SELECT type FROM cards WHERE title='Fey Messenger'").fetchall() != [("Unit",)]:
            raise ValueError("Expected exactly one Fey Messenger Unit")
        actions = original.execute("SELECT display_name FROM card_actions WHERE title='Fey Messenger' ORDER BY item_index").fetchall()
        if actions != [("Courier Flight",), ("Star Dart",)]:
            raise ValueError("Fey Messenger action baseline differs; review before applying")
        before = {name: rows(original, name) for name in tables}
        existing = original.execute(f"SELECT {column} FROM {table} WHERE title='Fey Messenger' AND lower({column})='foresight'").fetchall()
        if existing and existing != canonical:
            raise ValueError("Existing Foresight assignment needs review")
        # Exclusive creation ensures no catalog can be overwritten accidentally.
        with output.open("xb"):
            pass
        db = sqlite3.connect(output)
        try:
            original.backup(db)
            with db:
                if not existing:
                    db.execute(f"INSERT INTO {table}(title,{column}) VALUES (?,?)", ("Fey Messenger", canonical[0][0]))
                for name, old in before.items():
                    expected = Counter(old)
                    if name == table and not existing:
                        columns = [r[1] for r in db.execute(f"PRAGMA table_info({table})")]
                        if columns != ["title", column]:
                            raise ValueError("Unexpected passive table shape; refuse unverified changes")
                        expected[("Fey Messenger", canonical[0][0])] += 1
                    if Counter(rows(db, name)) != expected:
                        raise ValueError("Unexpected data difference in " + name)
                if db.execute("PRAGMA integrity_check").fetchall() != [("ok",)] or db.execute("PRAGMA foreign_key_check").fetchall():
                    raise ValueError("Catalog integrity/foreign key check failed")
            return {"source": str(source), "isolatedCatalog": str(output), "storage": table,
                    "passive": canonical[0][0], "rowsAdded": int(not existing),
                    "tablesCompared": len(tables), "onlyApprovedDifference": True,
                    "integrity": "ok", "sourceModified": False, "liveAssigned": False}
        finally:
            db.close()
    finally:
        original.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.output), indent=2))
