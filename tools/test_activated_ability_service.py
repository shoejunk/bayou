"""Offline TLS/CRUD regression for a temporary local card server; no matches.

Uses only localhost:55004 and refuses to run if that port is already occupied.
Retains its isolated database and log under output/ability-service-tests/.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import socket
import shutil
import sqlite3
import struct
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
U32 = lambda n: struct.pack("!I", n)
I32 = lambda n: struct.pack("!i", n)
STRING = lambda s: U32(len(s.encode())) + s.encode()
STRINGS = lambda items: U32(len(items)) + b"".join(STRING(s) for s in items)


class Reader:
    def __init__(self, data): self.data, self.pos = data, 0
    def take(self, n):
        assert 0 <= n <= len(self.data) - self.pos, "Truncated packet"
        value = self.data[self.pos:self.pos+n]; self.pos += n; return value
    def byte(self): return self.take(1)[0]
    def u32(self): return struct.unpack("!I", self.take(4))[0]
    def integer(self): return struct.unpack("!i", self.take(4))[0]
    def string(self): return self.take(self.u32()).decode()
    def array(self, fn):
        count = self.u32(); assert count <= 4096
        return [fn() for _ in range(count)]
    def strings(self): return self.array(self.string)
    def done(self): assert self.pos == len(self.data), "Unexpected trailing bytes"


def definition_bytes(name):
    return (U32(0x41424431) + STRING(name) + STRING("area heal") +
            b"\1" + I32(2) + b"\1" + I32(3) + b"\1" + I32(1) +
            b"\1" + STRINGS(["Fey", "Mechanical"]) + b"\1" + STRING("") +
            STRING("") + I32(0) + U32(1) + I32(0) + I32(0) + STRING("Repair"))


def read_definition(r):
    assert r.u32() == 0x41424431
    d = {"name": r.string(), "kind": r.string()}
    for key in ("range", "strength", "duration"): d[key] = r.integer() if r.byte() else None
    d["filter"] = r.strings() if r.byte() else None
    d["restriction"] = r.string() if r.byte() else None
    d["summon"] = r.string(); d["uses"] = r.integer()
    d["states"] = r.array(lambda: [r.integer(), r.integer(), r.string()])
    return d


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, default=ROOT / "build/Debug/cardserver.exe")
    args = parser.parse_args()
    # The test must never connect to or stop an existing service.
    with socket.socket() as probe:
        probe.settimeout(0.5)
        if probe.connect_ex(("127.0.0.1", 55004)) == 0:
            raise RuntimeError("localhost:55004 is occupied; no server was started or changed")
    output = ROOT / "output/ability-service-tests"
    output.mkdir(parents=True, exist_ok=True)
    work = output / (datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8])
    work.mkdir(); assert work.resolve().is_relative_to(output.resolve())
    contract = (ROOT / "src/shared/network_messages.inc").read_text()
    body = contract.split("enum class MessageType", 1)[1].split("{", 1)[1].split("}", 1)[0]
    names = [s.strip() for s in re.sub(r"//[^\n]*", "", body).split(",") if s.strip()]
    message = {name: bytes([i]) for i, name in enumerate(names)}
    openssl=shutil.which("openssl") or "C:/Program Files/Git/usr/bin/openssl.exe"
    def cert_command(*args):
        subprocess.run([openssl,*args],cwd=work,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                       creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    # A private test PKI keeps certificate validation strict and does not alter
    # the developer's existing certificates (older ones can lack an AKI).
    cert_command("req","-x509","-newkey","rsa:2048","-nodes","-days","2","-subj","/CN=Ability Test CA",
        "-keyout","ca-key.pem","-out","ca.pem","-addext","basicConstraints=critical,CA:TRUE",
        "-addext","keyUsage=critical,keyCertSign,cRLSign","-addext","subjectKeyIdentifier=hash")
    cert_command("req","-new","-newkey","rsa:2048","-nodes","-subj","/CN=localhost","-keyout","server-key.pem","-out","server.csr")
    (work/"server.ext").write_text("authorityKeyIdentifier=keyid,issuer\nsubjectKeyIdentifier=hash\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost,IP:127.0.0.1\n")
    cert_command("x509","-req","-in","server.csr","-CA","ca.pem","-CAkey","ca-key.pem","-CAcreateserial",
        "-days","2","-extfile","server.ext","-out","server.pem")
    def exchange(payload):
        # Use the same OpenSSL installation that issued the isolated test PKI.
        # Both CA-chain and hostname verification remain mandatory.
        response=subprocess.run([openssl,"s_client","-quiet","-connect","127.0.0.1:55004","-servername","localhost",
            "-CAfile",str(work/"ca.pem"),"-verify_hostname","localhost","-verify_return_error","-ign_eof"],
            input=U32(len(payload))+payload,capture_output=True,timeout=10,
            creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        if b"verify error" in response.stderr.lower(): raise RuntimeError(response.stderr.decode(errors="replace"))
        if len(response.stdout)<4: raise ConnectionError(response.stderr.decode(errors="replace")[-500:])
        size=struct.unpack("!I",response.stdout[:4])[0]
        assert size<=16*1024*1024 and len(response.stdout)==size+4,"Incomplete service response"
        if response.returncode and b"unexpected eof" not in response.stderr.lower():
            raise RuntimeError(response.stderr.decode(errors="replace"))
        return Reader(response.stdout[4:])
    checks = []
    def command(kind, payload=b"", response=None, success=True):
        r = exchange(message[kind] + payload)
        assert r.byte() == message[response or kind.replace("Request", "Response")][0]
        ok, detail = bool(r.byte()), r.string(); assert ok == success, detail
        r.done(); checks.append(kind + (" rejected" if not success else " passed"))
    def library():
        r = exchange(message["ActivatedAbilityListRequest"])
        assert r.byte() == message["ActivatedAbilityListResponse"][0] and r.byte() == 1
        assert r.string() == "OK"
        definitions = r.array(lambda: read_definition(r)); r.done(); return definitions
    def marker_bytes(name, eligibility=0, effect=0, strength=1):
        return STRING(name) + bytes([eligibility, effect]) + I32(strength)
    def read_marker(r, art=False):
        result=dict(name=r.string(), eligibility=r.byte(), effect=r.byte(), strength=r.integer())
        if art: result["image"] = r.string()
        return result
    def marker_library(art=False):
        r = exchange(message["BoardMarkerListRequest"] + (U32(17) if art else b""))
        assert r.byte() == message["BoardMarkerListResponse"][0] and r.byte() == 1
        assert r.string() == "OK"
        definitions = r.array(lambda: read_marker(r,art)); r.done(); return definitions
    def catalog(version):
        r = exchange(message["CardListRequest"] + U32(version))
        assert r.byte() == 11 and r.byte() == 1 and r.string() == "OK" and r.u32() == 0xffffffff
        assert r.u32() == version
        cards = []
        for _ in range(r.u32()):
            start = r.pos
            card = {"title": r.string(), "type": r.string(), "image": r.string(), "traits": r.strings(), "keywords": r.strings()}
            card["integers"] = dict(r.array(lambda: (r.string(), r.integer())))
            card["strings"] = dict(r.array(lambda: (r.string(), r.string())))
            card["lists"] = dict(r.array(lambda: (r.string(), r.strings())))
            card["actions"] = r.strings(); card["labels"] = r.strings()
            assert r.u32() == 0, "This isolated fixture has no printed actions"
            card["legacyBytes"] = r.data[start:r.pos]
            if version >= 14:
                card["abilityName"], card["sacrificeName"] = r.string(), r.string()
                card["ability"] = read_definition(r) if r.byte() else None
                card["sacrifice"] = read_definition(r) if r.byte() else None
            if version >= 15:
                card["explicit"] = bool(r.byte())
                card["passives"] = r.strings()
            if version >= 16:
                card["markerRef"] = r.string()
                card["marker"] = read_marker(r,version>=17) if r.byte() else None
            cards.append(card)
        r.done(); return cards
    def card_body(title, ability, marker_ref=None, marker_override=None, passives=None):
        if passives is None:
            passives = ["Bodyguard", "Traces"] if marker_ref else ["Bodyguard"]
        result = (STRING(title) + STRING("Unit") + STRING("") + STRINGS(["Fey"]) + STRINGS(["Plant", "Undying"]) +
                  U32(1) + STRING("health") + I32(4) + U32(0) + U32(0) + STRINGS([]) + STRINGS([]) + U32(0) +
                  STRING(ability) + STRING("") + b"\0\0\1" + STRINGS(passives))
        if marker_ref is not None:
            result += STRING(marker_ref) + (b"\1" + marker_override if marker_override else b"\0")
        return result
    env = os.environ.copy()
    env.update(BAYOU_TLS_CERT_FILE=str(work / "server.pem"), BAYOU_TLS_KEY_FILE=str(work / "server-key.pem"))
    with (work / "cardserver.log").open("wb") as log:
        process = subprocess.Popen([str(args.server.resolve())], cwd=work, env=env, stdout=log, stderr=log,
                                   creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            for _ in range(25):
                if process.poll() is not None: raise RuntimeError("Isolated server exited; inspect " + str(work / "cardserver.log"))
                try: library(); break
                except OSError as error:
                    last_error=str(error);time.sleep(0.1)
            else: raise RuntimeError("Isolated server did not become ready: "+last_error)
            command("ActivatedAbilityUpsertRequest", definition_bytes("Test Heal"))
            assert library()[0]["filter"] == ["Fey", "Mechanical"] and library()[0]["restriction"] == ""
            command("CardUpsertWithPassivesRequest", card_body("Test Unit", "Test Heal"), "CardUpsertResponse")
            native = catalog(15)[0]
            assert native["abilityName"] == "Test Heal" and native["ability"]["strength"] == 3
            assert not native["strings"] and not native["lists"]
            assert native["explicit"] and native["keywords"] == ["Plant", "Undying"] and native["passives"] == ["Bodyguard"]
            assert catalog(14)[0]["keywords"] == ["Bodyguard", "Plant"]
            old = catalog(13)[0]
            assert old["strings"]["ability"] == "area heal" and old["lists"]["heal filter"] == ["Fey", "Mechanical"]
            command("CardUpsertRequest", old["legacyBytes"], success=False)
            command("CardUpdateWithAbilitiesRequest", STRING("Test Unit"), "CardUpdateResponse", False)
            assert catalog(15)[0]["ability"] == native["ability"] and catalog(15)[0]["keywords"] == native["keywords"]
            command("ActivatedAbilityDeleteRequest", STRING("Test Heal"), success=False)
            command("ActivatedAbilityUpdateRequest", STRING("Test Heal") + definition_bytes("Renamed Heal"))
            assert catalog(14)[0]["abilityName"] == "Renamed Heal" and library()[0]["name"] == "Renamed Heal"
            command("CardUpdateWithPassivesRequest", STRING("Test Unit") + card_body("Renamed Unit", "Renamed Heal"), "CardUpdateResponse")
            command("CardUpsertWithPassivesRequest", card_body("Renamed Unit", "Missing"), "CardUpsertResponse", False)
            assert catalog(14)[0]["abilityName"] == "Renamed Heal"
            command("ActivatedAbilityUpsertRequest", definition_bytes("Invalid")[:-1], success=False)
            command("CardDeleteRequest", STRING("Renamed Unit"))
            command("ActivatedAbilityDeleteRequest", STRING("Renamed Heal"))
            assert not library() and not catalog(14)
            assert not marker_library()
            command("BoardMarkerUpsertRequest", marker_bytes("Trail Heal"))
            command("BoardMarkerUpsertRequest", marker_bytes("trail heal"), success=False)
            command("BoardMarkerUpsertRequest", marker_bytes("Blight", 1, 1), success=False)
            assert marker_library() == [dict(name="Trail Heal", eligibility=0, effect=0, strength=1)]

            command("CardUpsertWithMarkersRequest", card_body("Tracer", "", "Missing Marker"),
                    "CardUpsertResponse", False)
            command("CardUpsertWithMarkersRequest", card_body("Tracer", "", "", passives=["Traces"]),
                    "CardUpsertResponse", False)
            # The service resolves the stored marker and never trusts client-supplied effect details.
            command("CardUpsertWithMarkersRequest",
                    card_body("Tracer", "", "trail heal", marker_bytes("trail heal", 1, 1, 9)),
                    "CardUpsertResponse")
            modern = catalog(16)[0]
            assert modern["markerRef"] == "Trail Heal" and modern["marker"] == marker_library()[0]
            assert modern["passives"] == ["Bodyguard", "Traces"]
            assert catalog(15)[0]["passives"] == ["Bodyguard"]
            for legacy_version in (13, 14):
                assert "Traces" not in catalog(legacy_version)[0]["keywords"]

            command("CardUpdateWithPassivesRequest", STRING("Tracer") + card_body("Tracer", ""),
                    "CardUpdateResponse", False)
            command("CardUpsertWithPassivesRequest", card_body("Tracer", ""), "CardUpsertResponse", False)
            assert catalog(16)[0]["markerRef"] == "Trail Heal"
            command("BoardMarkerDeleteRequest", STRING("TRAIL HEAL"), success=False)
            art="characters/bloomMarker.png"
            command("BoardMarkerUpdateRequest", STRING("Trail Heal") + marker_bytes("Trail Heal") + U32(17) + STRING(art))
            assert marker_library(True)[0]["image"] == art
            assert catalog(17)[0]["marker"]["image"] == art
            assert "image" not in marker_library()[0] and "image" not in catalog(16)[0]["marker"]
            # Export actual service readback for the normal Windows game renderer.
            stored=marker_library(True)[0]
            (work/"marker-art.fixture").write_bytes(marker_bytes(stored["name"],stored["eligibility"],stored["effect"],stored["strength"])+STRING(stored["image"]))
            command("BoardMarkerUpdateRequest", STRING("Trail Heal") + marker_bytes("Trail Heal") + U32(17) + STRING("../outside.png"),success=False)
            assert marker_library(True)[0]["image"] == art
            command("BoardMarkerUpdateRequest", STRING("Trail Heal") + marker_bytes("Trace Gift", 2, 0, 3))
            assert marker_library(True)[0]["image"] == art, "legacy rename must preserve marker art"
            command("BoardMarkerUpdateRequest", STRING("Trace Gift") + marker_bytes("Trace Gift",2,0,3) + U32(17) + STRING(""))
            assert marker_library(True)[0]["image"] == "", "explicit new authoring can clear optional art"
            updated = catalog(16)[0]
            assert updated["markerRef"] == "Trace Gift"
            assert updated["marker"] == dict(name="Trace Gift", eligibility=2, effect=0, strength=3)
            command("BoardMarkerDeleteRequest", STRING("Trace Gift"), success=False)
            command("CardUpdateWithMarkersRequest",
                    STRING("Tracer") + card_body("Tracer", "", ""), "CardUpdateResponse")
            assert catalog(16)[0]["markerRef"] == "" and catalog(16)[0]["marker"] is None
            command("BoardMarkerDeleteRequest", STRING("Trace Gift"))
            command("CardDeleteRequest", STRING("Tracer"))
            assert not marker_library() and not catalog(16)
        finally:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
    connection = sqlite3.connect("file:" + str(work / "cards.db") + "?mode=ro", uri=True)
    assert connection.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
    assert connection.execute("PRAGMA foreign_key_check").fetchall() == []
    connection.close()
    report = {"markerArtFixture": str(work/"marker-art.fixture"), "checks": checks, "databaseIntegrity": "ok", "isolatedDirectory": str(work), "productionContacted": False,"tlsClient":"OpenSSL with CA and hostname verification"}
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__": main()
