import concurrent.futures
import json
import sqlite3
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from ledger import Ledger, Rejected, MAX_UNITS
from server import make_server


class LedgerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = str(Path(self.temp.name) / "ledger.sqlite")
        self.ledger = Ledger(self.path, {"alice": "issuer", "bob": "member", "carol": "member"})
        self.run_command("alice", "create", "create", asset="DAO", name="Test credits", cap=1000)

    def tearDown(self):
        self.temp.cleanup()

    def run_command(self, actor, request_id, kind, **fields):
        return self.ledger.execute(actor, dict(request_id=request_id, kind=kind, **fields))

    def mint(self, amount=100):
        return self.run_command("alice", "mint", "mint", asset="DAO", to="alice", amount=amount)

    def balance(self, actor):
        return self.ledger.state(actor)["assets"][0]["balance"]

    def test_transfer_burn_conservation_and_restart(self):
        self.mint()
        self.run_command("alice", "pay", "transfer", asset="DAO", to="bob", amount=30)
        self.run_command("bob", "burn", "burn", asset="DAO", amount=5)
        self.assertEqual((self.balance("alice"), self.balance("bob")), (70, 25))
        self.ledger = Ledger(self.path, self.ledger.members)
        self.assertEqual(self.ledger.state("bob")["assets"][0]["supply"], 95)
        with self.ledger.connect() as db:
            self.assertEqual(db.execute("SELECT SUM(delta) FROM entries").fetchone()[0], 0)
            self.assertEqual(db.execute("SELECT SUM(amount) FROM balances").fetchone()[0], 95)

    def test_unauthorised_mint_and_spoofed_sender(self):
        for command in [dict(kind="mint", to="bob", amount=1),
                        dict(kind="transfer", to="bob", amount=1, actor="alice")]:
            with self.assertRaises(Rejected):
                self.ledger.execute("bob", dict(request_id="bad", asset="DAO", **command))
        self.assertEqual(self.balance("bob"), 0)

    def test_idempotency_and_conflicting_replay(self):
        receipt = self.mint()
        self.assertEqual(self.mint(), receipt)
        with self.assertRaises(Rejected):
            self.mint(101)
        self.assertEqual(self.balance("alice"), 100)

    def test_integer_validation_cap_and_rollback(self):
        for amount in [0, -1, True, 1.5, "1", MAX_UNITS + 1, 1001]:
            with self.assertRaises(Rejected):
                self.mint(amount)
        self.assertEqual(self.ledger.state("alice")["sequence"], 1)
        self.assertEqual(self.balance("alice"), 0)

    def test_concurrent_double_spend(self):
        self.mint()
        def spend(index):
            try:
                self.run_command("alice", f"spend{index}", "transfer", asset="DAO", to="bob", amount=80)
                return True
            except Rejected:
                return False
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(spend, range(8)))
        self.assertEqual(sum(results), 1)
        self.assertEqual((self.balance("alice"), self.balance("bob")), (20, 80))

    def test_audit_immutable_and_history_private(self):
        self.mint()
        self.assertEqual(self.ledger.state("bob")["history"], [])
        with self.ledger.connect() as db:
            for sql in ["DELETE FROM commands", "UPDATE entries SET delta=1"]:
                with self.assertRaises(sqlite3.IntegrityError):
                    db.execute(sql)

    def test_http_auth_role_and_origin(self):
        token = "a" * 32
        service = make_server(self.ledger, {token: "bob"}, 0)
        with self.assertRaises(OSError):
            make_server(self.ledger, {token: "bob"}, service.server_port)
        thread = threading.Thread(target=service.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{service.server_port}"
        try:
            for headers, status in [({}, 401), ({"Authorization": "Bearer " + token, "Origin": "https://evil.invalid"}, 403)]:
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(urllib.request.Request(base + "/v1/state", headers=headers))
                self.assertEqual(error.exception.code, status)
                error.exception.close()
            req = urllib.request.Request(base + "/v1/state", headers={"Authorization": "Bearer " + token})
            with urllib.request.urlopen(req) as response:
                self.assertEqual(json.load(response)["actor"], "bob")
        finally:
            service.shutdown()
            service.server_close()
            thread.join()


if __name__ == "__main__":
    unittest.main()
