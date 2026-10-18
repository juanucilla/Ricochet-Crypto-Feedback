"""Local development DAO ledger. Not a production or LiquidFeedback server.

Only Python's standard library is required. Amounts are integer base units.
Identity/role comes from server configuration, never from a submitted command.
"""
import json
import re
import sqlite3
from contextlib import contextmanager

MAX_UNITS = 2**53 - 1  # Exact in QML/JSON clients as well as SQLite.


class Rejected(ValueError):
    pass


def identifier(value):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", value):
        raise Rejected("Invalid identifier")
    return value


def units(value):
    if type(value) is not int or not 0 < value <= MAX_UNITS:
        raise Rejected("Amount must be positive integer base units")
    return value


class Ledger:
    def __init__(self, path, members):
        self.path = path
        self.members = dict(members)
        if not self.members or any(role not in ("member", "issuer") for role in self.members.values()):
            raise Rejected("Configure member/issuer roles explicitly")
        for member in self.members:
            identifier(member)
        with self.connect() as db:
            db.executescript('''
                PRAGMA journal_mode=WAL;
                CREATE TABLE IF NOT EXISTS assets(
                    id TEXT PRIMARY KEY, name TEXT NOT NULL, cap INTEGER NOT NULL,
                    supply INTEGER NOT NULL DEFAULT 0 CHECK(supply >= 0 AND supply <= cap));
                CREATE TABLE IF NOT EXISTS balances(
                    asset TEXT NOT NULL REFERENCES assets(id), member TEXT NOT NULL,
                    amount INTEGER NOT NULL CHECK(amount >= 0), PRIMARY KEY(asset, member));
                CREATE TABLE IF NOT EXISTS commands(
                    seq INTEGER PRIMARY KEY AUTOINCREMENT, actor TEXT NOT NULL,
                    request_id TEXT NOT NULL, body TEXT NOT NULL, receipt TEXT NOT NULL,
                    created TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
                    UNIQUE(actor, request_id));
                CREATE TABLE IF NOT EXISTS entries(
                    seq INTEGER NOT NULL REFERENCES commands(seq), asset TEXT NOT NULL,
                    account TEXT NOT NULL, delta INTEGER NOT NULL);
                CREATE TRIGGER IF NOT EXISTS immutable_commands_update BEFORE UPDATE ON commands
                    BEGIN SELECT RAISE(ABORT, 'immutable audit'); END;
                CREATE TRIGGER IF NOT EXISTS immutable_commands_delete BEFORE DELETE ON commands
                    BEGIN SELECT RAISE(ABORT, 'immutable audit'); END;
                CREATE TRIGGER IF NOT EXISTS immutable_entries_update BEFORE UPDATE ON entries
                    BEGIN SELECT RAISE(ABORT, 'immutable audit'); END;
                CREATE TRIGGER IF NOT EXISTS immutable_entries_delete BEFORE DELETE ON entries
                    BEGIN SELECT RAISE(ABORT, 'immutable audit'); END;
            ''')

    @contextmanager
    def connect(self):
        db = sqlite3.connect(self.path, timeout=10, isolation_level=None)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA foreign_keys=ON")
        try:
            yield db
        finally:
            db.close()

    def role(self, actor):
        if actor not in self.members:
            raise Rejected("Unknown member")
        return self.members[actor]

    def state(self, actor):
        role = self.role(actor)
        with self.connect() as db:
            db.execute("BEGIN")
            assets = [dict(row) for row in db.execute('''SELECT a.*,
                COALESCE(b.amount,0) AS balance FROM assets a LEFT JOIN balances b
                ON a.id=b.asset AND b.member=? ORDER BY a.id''', (actor,))]
            history = [dict(row) for row in db.execute('''SELECT seq,actor,body,receipt,created
                FROM commands WHERE actor=? ORDER BY seq DESC LIMIT 50''', (actor,))]
            # Recipients can see their receipts, not other members' full balances/audit.
            incoming = [dict(row) for row in db.execute('''SELECT c.seq,c.created,e.asset,e.delta
                FROM entries e JOIN commands c ON e.seq=c.seq
                WHERE e.account=? AND e.delta>0 ORDER BY c.seq DESC LIMIT 50''', (actor,))]
            seq = db.execute("SELECT COALESCE(MAX(seq),0) FROM commands").fetchone()[0]
            db.commit()
        return dict(mode="LOCAL TEST CREDITS", actor=actor, role=role, assets=assets,
                    members=sorted(self.members), history=history, incoming=incoming,
                    sequence=seq, governance="LiquidFeedback integration pending; voting disabled")

    def execute(self, actor, command):
        role = self.role(actor)
        if not isinstance(command, dict):
            raise Rejected("Command must be an object")
        kind = command.get("kind")
        fields = {"create": {"asset", "name", "cap"},
                  "mint": {"asset", "to", "amount"},
                  "transfer": {"asset", "to", "amount"},
                  "burn": {"asset", "amount"}}
        if not isinstance(kind, str) or kind not in fields or set(command) != fields[kind] | {"kind", "request_id"}:
            raise Rejected("Unsupported command or fields")
        request_id = identifier(command["request_id"])
        asset = identifier(command["asset"])
        body = json.dumps(command, sort_keys=True, separators=(",", ":"), ensure_ascii=True)
        with self.connect() as db:
            db.execute("BEGIN IMMEDIATE")
            try:
                previous = db.execute("SELECT body,receipt FROM commands WHERE actor=? AND request_id=?",
                                      (actor, request_id)).fetchone()
                if previous:
                    if previous["body"] != body:
                        raise Rejected("Idempotency key already used with different command")
                    db.rollback()
                    return json.loads(previous["receipt"])
                if kind in ("create", "mint") and role != "issuer":
                    raise Rejected("Issuer authority required")
                postings = []
                if kind == "create":
                    name = command["name"]
                    if not isinstance(name, str) or not name.strip() or len(name) > 80:
                        raise Rejected("Asset name required (max 80 characters)")
                    cap = units(command["cap"])
                    if db.execute("SELECT 1 FROM assets WHERE id=?", (asset,)).fetchone():
                        raise Rejected("Asset already exists")
                    db.execute("INSERT INTO assets(id,name,cap) VALUES(?,?,?)", (asset, name, cap))
                else:
                    amount = units(command["amount"])
                    row = db.execute("SELECT * FROM assets WHERE id=?", (asset,)).fetchone()
                    if row is None:
                        raise Rejected("Unknown asset")
                    recipient = command.get("to", actor)
                    if not isinstance(recipient, str) or recipient not in self.members:
                        raise Rejected("Recipient is not an accredited member")
                    if kind == "transfer" and recipient == actor:
                        raise Rejected("Self transfer is not supported")
                    if kind == "mint":
                        if amount > row["cap"] - row["supply"]:
                            raise Rejected("Supply cap exceeded")
                        db.execute("UPDATE assets SET supply=supply+? WHERE id=?", (amount, asset))
                        postings = [("@issuance", -amount), (recipient, amount)]
                    else:
                        balance = db.execute("SELECT amount FROM balances WHERE asset=? AND member=?",
                                             (asset, actor)).fetchone()
                        if not balance or balance[0] < amount:
                            raise Rejected("Insufficient balance")
                        postings = [(actor, -amount), (recipient if kind == "transfer" else "@issuance", amount)]
                        if kind == "burn":
                            db.execute("UPDATE assets SET supply=supply-? WHERE id=?", (amount, asset))
                    for account, delta in postings:
                        if account != "@issuance":
                            # Apply debits through UPDATE so the nonnegative CHECK never sees
                            # a negative tentative INSERT in SQLite's UPSERT implementation.
                            db.execute("INSERT OR IGNORE INTO balances VALUES(?,?,0)", (asset, account))
                            db.execute("UPDATE balances SET amount=amount+? WHERE asset=? AND member=?",
                                       (delta, asset, account))
                seq = db.execute("SELECT COALESCE(MAX(seq),0)+1 FROM commands").fetchone()[0]
                receipt = dict(sequence=seq, request_id=request_id, kind=kind, asset=asset, status="booked")
                db.execute("INSERT INTO commands(seq,actor,request_id,body,receipt) VALUES(?,?,?,?,?)",
                           (seq, actor, request_id, body, json.dumps(receipt)))
                db.executemany("INSERT INTO entries VALUES(?,?,?,?)",
                               [(seq, asset, account, delta) for account, delta in postings])
                db.commit()
                return receipt
            except Exception:
                db.rollback()
                raise
