"""Development-only loopback HTTP adapter. Never bind this server publicly."""
import hmac
import json
import os
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from ledger import Ledger, Rejected


def make_server(ledger, tokens, port=17842):
    if not tokens or any(len(token) < 32 or member not in ledger.members for token, member in tokens.items()):
        raise ValueError("Configure distinct >=32 character credentials mapped to members")

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass  # Never log credentials, commands, or private paths.

        def send_json(self, code, data):
            raw = json.dumps(data).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def handle_api(self, write=False):
            self.connection.settimeout(5)
            # No CORS/browser commands or DNS rebinding. API identity is credential-bound.
            if self.headers.get("Origin") or self.headers.get("Host") not in (
                f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"):
                return self.send_json(403, {"error": "Local native clients only"})
            supplied = self.headers.get("Authorization", "").removeprefix("Bearer ")
            actor = next((member for token, member in tokens.items()
                          if hmac.compare_digest(token.encode(), supplied.encode())), None)
            if actor is None:
                return self.send_json(401, {"error": "Authentication required"})
            try:
                if write and self.path == "/v1/commands":
                    length = int(self.headers.get("Content-Length", "0"))
                    if not 0 < length <= 8192 or self.headers.get("Transfer-Encoding"):
                        return self.send_json(413, {"error": "Invalid body size"})
                    if self.headers.get_content_type() != "application/json":
                        return self.send_json(415, {"error": "JSON required"})
                    result = ledger.execute(actor, json.loads(self.rfile.read(length)))
                elif not write and self.path == "/v1/state":
                    result = ledger.state(actor)
                else:
                    return self.send_json(404, {"error": "Unknown endpoint"})
                self.send_json(200, result)
            except (Rejected, ValueError, UnicodeError) as error:
                self.send_json(400, {"error": str(error)})
            except Exception:
                self.send_json(500, {"error": "Storage unavailable; retry the SAME request ID"})

        def do_GET(self):
            self.handle_api()

        def do_POST(self):
            self.handle_api(True)

    class LocalServer(ThreadingHTTPServer):
        allow_reuse_address = False
        def server_bind(self):
            # Windows otherwise permits confusing simultaneous binds by local
            # processes. One port must have exactly one ledger authority.
            if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            super().server_bind()

    return LocalServer(("127.0.0.1", port), Handler)


if __name__ == "__main__":
    # Configuration contains credentials: supply via process environment, never commit.
    config = json.loads(os.environ["GROUP_DAO_AUTH"])
    ledger = Ledger(os.environ.get("GROUP_DAO_DB", "dao-test.sqlite"), config["members"])
    server = make_server(ledger, config["tokens"])
    print("DAO test service on loopback port 17842. No real money. Ctrl+C to stop.", flush=True)
    server.serve_forever()
