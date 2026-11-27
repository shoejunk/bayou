#!/usr/bin/env python3
"""Prepare a new isolated Crystal Unicorn catalog; never edits source/live data."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sqlite3


def prepare(source, output):
    source, output = source.resolve(strict=True), output.resolve()
    if source == output or output.exists():
        raise ValueError("Output must be a new isolated database")
    before = sqlite3.connect(source.as_uri()+"?mode=ro", uri=True)
    try:
        tables=[r[0] for r in before.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name")]
        old={t: Counter(before.execute('SELECT * FROM "'+t.replace('"','""')+'"').fetchall()) for t in tables}
        assert before.execute("SELECT type FROM cards WHERE title='Crystal Unicorn'").fetchall()==[("Unit",)]
        links=before.execute("SELECT action_name,item_index,display_name FROM card_actions WHERE title='Crystal Unicorn' ORDER BY item_index").fetchall()
        assert links in [[('RookMoveD2R4',0,'Prismatic Charge')],[('RookMoveD2R4',0,'Prismatic Charge'),('Purifying Horn',1,'Purifying Horn')]], "Unexpected action baseline"
        values=dict(name="Purifying Horn",state=0,next_state=0,kind="ranged",pattern="omni",min_range=1,max_range=2,
                    damage=0,heal=0,can_move=0,can_attack=1,pass_through=0,line_of_sight=0,status_turns=0,
                    cooldown_turns=0,push=0,control=0,repeat=0,infest="",pull=0,lifedrain=0,slow=0)
        columns=[r[1] for r in before.execute("PRAGMA table_info(actions)")]
        assert set(columns)==set(values), "Unrecognized action schema"
        action=tuple(values[c] for c in columns)
        existing=before.execute("SELECT * FROM actions WHERE name='Purifying Horn'").fetchall()
        assert not existing or existing==[action], "Purifying Horn name collision"
        filters=before.execute("SELECT * FROM action_target_filters WHERE action_name='Purifying Horn'").fetchall()
        assert not filters or filters==[('Purifying Horn',0,'@purify')], "Unexpected action filters"
        with output.open("xb"): pass
        db=sqlite3.connect(output)
        try:
            before.backup(db)
            expected={t: old[t].copy() for t in tables}
            with db:
                if not existing:
                    db.execute('INSERT INTO actions('+','.join(columns)+') VALUES('+','.join('?' for _ in columns)+')',action)
                    expected['actions'][action]+=1
                if not filters:
                    db.execute("INSERT INTO action_target_filters(action_name,item_index,value) VALUES('Purifying Horn',0,'@purify')")
                    expected['action_target_filters'][('Purifying Horn',0,'@purify')]+=1
                if len(links)==1:
                    db.execute("INSERT INTO card_actions(title,action_name,item_index,display_name) VALUES('Crystal Unicorn','Purifying Horn',1,'Purifying Horn')")
                    expected['card_actions'][('Crystal Unicorn','Purifying Horn',1,'Purifying Horn')]+=1
                for table in tables:
                    assert Counter(db.execute('SELECT * FROM "'+table.replace('"','""')+'"').fetchall())==expected[table], table
                assert db.execute('PRAGMA integrity_check').fetchall()==[('ok',)]
                assert not db.execute('PRAGMA foreign_key_check').fetchall()
            return dict(isolatedCatalog=str(output),tablesCompared=len(tables),onlyApprovedChanges=True,
                        regularActions=['Prismatic Charge','Purifying Horn'],sourceModified=False,liveAssigned=False,integrity='ok')
        finally: db.close()
    finally: before.close()


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();print(json.dumps(prepare(a.source,a.output),indent=2))
