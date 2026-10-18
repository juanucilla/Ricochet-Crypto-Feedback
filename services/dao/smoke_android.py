"""Android UI -> same test ledger contract, on an explicitly selected emulator.

Requires the preview APK already installed. Never runs on a physical device.
Uses ephemeral credentials and database; does not print or persist credentials.
"""
import argparse
from pathlib import Path
import re
import secrets
import subprocess
import tempfile
import threading
import time
import xml.etree.ElementTree as ET
from ledger import Ledger
from server import make_server

parser = argparse.ArgumentParser()
parser.add_argument("--serial", required=True)
parser.add_argument("--adb", default=r"C:\Users\G\AppData\Local\Android\Sdk\platform-tools\adb.exe")
args = parser.parse_args()
if not re.fullmatch(r"emulator-[0-9]+", args.serial):
    raise SystemExit("This automated smoke test is restricted to emulators")


def adb(*parts):
    result = subprocess.run([args.adb, "-s", args.serial, *parts], capture_output=True, timeout=30)
    if result.returncode:
        raise RuntimeError("Emulator command failed")
    return result.stdout


def nodes():
    adb("shell", "uiautomator", "dump", "/sdcard/group-ricochet-smoke.xml")
    return list(ET.fromstring(adb("shell", "cat", "/sdcard/group-ricochet-smoke.xml")).iter("node"))


def find(text):
    for node in nodes():
        if node.get("text", "").casefold() == text.casefold() or node.get("hint") == text:
            return node
    raise AssertionError("Expected UI control not found: " + text)


def tap(node):
    coords = [int(n) for n in re.findall(r"[0-9]+", node.get("bounds"))]
    adb("shell", "input", "tap", str((coords[0] + coords[2]) // 2), str((coords[1] + coords[3]) // 2))


def fill(hint, text):
    tap(find(hint))
    # Let the emulator finish switching focus/IME before injecting key events.
    time.sleep(1)
    for offset in range(0, len(text), 8):
        adb("shell", "input", "text", text[offset:offset + 8])
        time.sleep(0.1)
    # BACK can finish the Activity when an emulator's soft keyboard has not
    # opened yet. ENTER triggers the single-line field's IME action instead.
    adb("shell", "input", "keyevent", "KEYCODE_ENTER")


with tempfile.TemporaryDirectory() as directory:
    ledger = Ledger(str(Path(directory) / "test.sqlite"), {"alice": "issuer", "bob": "member"})
    ledger.execute("alice", dict(kind="create", request_id="create", asset="DAO", name="Test credits", cap=1000))
    ledger.execute("alice", dict(kind="mint", request_id="mint", asset="DAO", to="alice", amount=100))
    token = secrets.token_hex(32)
    service = make_server(ledger, {token: "alice"})
    thread = threading.Thread(target=service.serve_forever, daemon=True)
    thread.start()
    try:
        adb("reverse", "tcp:17842", "tcp:17842")
        adb("shell", "am", "force-stop", "org.groupricochet.preview")
        adb("shell", "am", "start", "-W", "-n", "org.groupricochet.preview/.MainActivity")
        fill("Credenziale personale del servizio locale", token)
        tap(find("CONNETTI / AGGIORNA"))
        # UI hierarchy queries wait for idle; then verify real authenticated data.
        snapshot = nodes()
        assert any("saldo 100" in n.get("text", "") for n in snapshot), "Wallet state did not load"
        fill("ID credito (es. DAO)", "DAO")
        fill("ID destinatario accreditato", "bob")
        fill("Quantità in unità intere", "25")
        tap(find("TRASFERISCI"))
        tap(find("Conferma"))
        snapshot = nodes()
        assert any("saldo 75" in n.get("text", "") for n in snapshot), "Updated balance not rendered"
        assert ledger.state("bob")["assets"][0]["balance"] == 25
        assert ledger.state("alice")["assets"][0]["balance"] == 75
        print("PASS: Android confirmation UI -> authenticated API -> ledger; balances 75/25.")
    finally:
        # Remove only our development reverse mapping; keep the emulator available.
        adb("reverse", "--remove", "tcp:17842")
        service.shutdown()
        service.server_close()
        thread.join()
