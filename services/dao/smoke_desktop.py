"""Runs an isolated local API + actual Qt client transfer. No real profile/credits."""
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import threading
from ledger import Ledger
from server import make_server

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    ledger = Ledger(str(Path(directory) / "test.sqlite"), {"alice": "issuer", "bob": "member"})
    ledger.execute("alice", dict(kind="create", request_id="create", asset="DAO", name="Test credits", cap=1000))
    ledger.execute("alice", dict(kind="mint", request_id="mint", asset="DAO", to="alice", amount=100))
    token = secrets.token_urlsafe(32)
    server = make_server(ledger, {token: "alice"})
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    env = dict(os.environ, GROUP_DAO_TOKEN=token, DAO_TEST_TRANSFER="1", QT_QPA_PLATFORM="windows", QT_QUICK_BACKEND="software",
               DAO_SCREENSHOT=str(root / "build-group-tests" / "dao-ui.png"))
    env["PATH"] = str(root / ".qt-sdk/5.15.2/msvc2019_64/bin") + os.pathsep + env["PATH"]
    try:
        result = subprocess.run([str(root / "build-group-tests/Release/dao_ui_smoke.exe")], env=env,
                                capture_output=True, text=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:
            print(result.stderr)
            raise SystemExit(result.returncode)
        assert ledger.state("bob")["assets"][0]["balance"] == 25
        assert ledger.state("alice")["assets"][0]["balance"] == 75
        print("PASS: Qt native client -> authenticated API -> ledger; 25 units booked, balances 75/25.")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
