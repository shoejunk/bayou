import html
import os
import sqlite3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

try:
    import stripe
except ImportError:
    print("Missing dependency: install the Stripe SDK with `python -m pip install -r requirements-stripe.txt`.")
    raise SystemExit(1)


STRIPE_SIGNATURE_ERRORS = tuple(
    error_type
    for error_type in (
        getattr(stripe, "SignatureVerificationError", None),
        getattr(getattr(stripe, "error", object()), "SignatureVerificationError", None),
    )
    if error_type is not None
)

PRODUCT_TAX_CODE = "txcd_10201001"

COIN_PACKS = {
    "coins_50": {
        "name": "50 Bayou Coins",
        "coins": 50,
        "amount_cents": 499,
        "currency": "usd",
    },
    "coins_120": {
        "name": "120 Bayou Coins",
        "coins": 120,
        "amount_cents": 999,
        "currency": "usd",
    },
    "coins_300": {
        "name": "300 Bayou Coins",
        "coins": 300,
        "amount_cents": 1999,
        "currency": "usd",
    },
}


def env_required(name):
    value = os.environ.get(name, "").strip()
    if not value:
        raise SystemExit(f"{name} is required")
    return value


def connect_db(db_path):
    connection = sqlite3.connect(db_path)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA busy_timeout = 5000")
    connection.execute("PRAGMA foreign_keys = ON")
    return connection


def ensure_database(db_path):
    with connect_db(db_path) as connection:
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS accounts (
                username TEXT PRIMARY KEY NOT NULL,
                password_hash TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
            """
        )
        columns = {row["name"] for row in connection.execute("PRAGMA table_info(accounts)")}
        if "coins" not in columns:
            connection.execute("ALTER TABLE accounts ADD COLUMN coins INTEGER NOT NULL DEFAULT 0")

        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS coin_purchase_sessions (
                stripe_session_id TEXT PRIMARY KEY NOT NULL,
                username TEXT NOT NULL,
                pack_id TEXT NOT NULL,
                coins INTEGER NOT NULL,
                amount_cents INTEGER NOT NULL,
                currency TEXT NOT NULL,
                status TEXT NOT NULL DEFAULT 'open',
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                credited_at TEXT,
                FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE
            )
            """
        )
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS stripe_events (
                event_id TEXT PRIMARY KEY NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
            """
        )


def account_exists(connection, username):
    row = connection.execute(
        "SELECT 1 FROM accounts WHERE username = ? LIMIT 1",
        (username,),
    ).fetchone()
    return row is not None


def insert_purchase_session(connection, session_id, username, pack_id, pack):
    connection.execute(
        """
        INSERT OR IGNORE INTO coin_purchase_sessions (
            stripe_session_id,
            username,
            pack_id,
            coins,
            amount_cents,
            currency,
            status
        ) VALUES (?, ?, ?, ?, ?, ?, 'open')
        """,
        (
            session_id,
            username,
            pack_id,
            int(pack["coins"]),
            int(pack["amount_cents"]),
            str(pack["currency"]),
        ),
    )


def pack_from_metadata(metadata):
    pack_id = str(metadata.get("pack_id", ""))
    pack = COIN_PACKS.get(pack_id)
    if pack is None:
        raise ValueError(f"Unknown coin pack metadata: {pack_id}")
    return pack_id, pack


def stripe_object_to_dict(value):
    if hasattr(value, "to_dict_recursive"):
        return value.to_dict_recursive()
    if hasattr(value, "to_dict"):
        return value.to_dict()
    return value


def ensure_recorded_session(connection, session):
    session_id = str(session.get("id", ""))
    if not session_id:
        raise ValueError("Stripe session payload is missing id")

    row = connection.execute(
        "SELECT * FROM coin_purchase_sessions WHERE stripe_session_id = ? LIMIT 1",
        (session_id,),
    ).fetchone()
    if row is not None:
        return row

    metadata = dict(session.get("metadata") or {})
    username = str(metadata.get("username", "")).strip()
    if not username:
        raise ValueError(f"Stripe session {session_id} is missing username metadata")
    if not account_exists(connection, username):
        raise ValueError(f"Stripe session {session_id} references unknown account {username}")

    pack_id, pack = pack_from_metadata(metadata)
    insert_purchase_session(connection, session_id, username, pack_id, pack)
    return connection.execute(
        "SELECT * FROM coin_purchase_sessions WHERE stripe_session_id = ? LIMIT 1",
        (session_id,),
    ).fetchone()


def credit_checkout_session(connection, session):
    row = ensure_recorded_session(connection, session)
    if row["status"] == "credited":
        return "already credited"

    payment_status = str(session.get("payment_status", ""))
    if payment_status and payment_status != "paid":
        connection.execute(
            "UPDATE coin_purchase_sessions SET status = ? WHERE stripe_session_id = ? AND status <> 'credited'",
            (payment_status, row["stripe_session_id"]),
        )
        return f"ignored payment_status={payment_status}"

    updated = connection.execute(
        "UPDATE accounts SET coins = coins + ? WHERE username = ?",
        (row["coins"], row["username"]),
    )
    if updated.rowcount != 1:
        connection.execute(
            "UPDATE coin_purchase_sessions SET status = 'failed' WHERE stripe_session_id = ?",
            (row["stripe_session_id"],),
        )
        raise ValueError(f"Could not credit account {row['username']}")

    connection.execute(
        """
        UPDATE coin_purchase_sessions
        SET status = 'credited', credited_at = CURRENT_TIMESTAMP
        WHERE stripe_session_id = ?
        """,
        (row["stripe_session_id"],),
    )
    return f"credited {row['coins']} coins to {row['username']}"


def mark_checkout_session(connection, session, status):
    session_id = str(session.get("id", ""))
    if not session_id:
        return "missing session id"
    connection.execute(
        """
        UPDATE coin_purchase_sessions
        SET status = ?
        WHERE stripe_session_id = ? AND status <> 'credited'
        """,
        (status, session_id),
    )
    return f"marked {session_id} {status}"


class StripeCoinHandler(BaseHTTPRequestHandler):
    db_path = "accounts.db"
    public_base_url = "http://127.0.0.1:55005"
    webhook_secret = ""

    def log_message(self, message_format, *args):
        print(f"{self.address_string()} - {message_format % args}")

    def send_text(self, status, body, content_type="text/plain; charset=utf-8"):
        encoded = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def send_html(self, status, title, body):
        self.send_text(
            status,
            (
                "<!doctype html><html><head><meta charset='utf-8'>"
                f"<title>{html.escape(title)}</title></head>"
                f"<body><h1>{html.escape(title)}</h1><p>{html.escape(body)}</p></body></html>"
            ),
            "text/html; charset=utf-8",
        )

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/health":
            self.send_text(200, "ok\n")
        elif parsed.path == "/checkout":
            self.handle_checkout(parse_qs(parsed.query))
        elif parsed.path == "/checkout/success":
            self.send_html(200, "Payment complete", "Return to Bayou and refresh the shop to see your coins.")
        elif parsed.path == "/checkout/cancel":
            self.send_html(200, "Checkout canceled", "No coins were purchased.")
        else:
            self.send_html(404, "Not found", "Unknown Stripe coin service endpoint.")

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/stripe/webhook":
            self.send_html(404, "Not found", "Unknown Stripe coin service endpoint.")
            return
        self.handle_webhook()

    def handle_checkout(self, query):
        username = query.get("username", [""])[0].strip()
        pack_id = query.get("pack", ["coins_50"])[0].strip()
        pack = COIN_PACKS.get(pack_id)

        if not username:
            self.send_html(400, "Missing username", "Sign in before purchasing coins.")
            return
        if pack is None:
            self.send_html(400, "Unknown coin pack", "The requested coin pack is not available.")
            return

        with connect_db(self.db_path) as connection:
            if not account_exists(connection, username):
                self.send_html(404, "Account not found", "Create or sign in to the account before purchasing coins.")
                return

        try:
            session = stripe.checkout.Session.create(
                mode="payment",
                managed_payments={"enabled": True},
                client_reference_id=username,
                success_url=self.public_base_url + "/checkout/success?session_id={CHECKOUT_SESSION_ID}",
                cancel_url=self.public_base_url + "/checkout/cancel",
                line_items=[
                    {
                        "quantity": 1,
                        "price_data": {
                            "currency": pack["currency"],
                            "unit_amount": pack["amount_cents"],
                            "product_data": {
                                "name": pack["name"],
                                "tax_code": PRODUCT_TAX_CODE,
                            },
                        },
                    }
                ],
                metadata={
                    "username": username,
                    "pack_id": pack_id,
                    "coins": str(pack["coins"]),
                },
            )
        except Exception as error:
            print(f"Stripe checkout creation failed: {error}")
            self.send_html(502, "Stripe error", "Could not start checkout.")
            return

        with connect_db(self.db_path) as connection:
            insert_purchase_session(connection, session["id"], username, pack_id, pack)

        self.send_response(303)
        self.send_header("Location", session["url"])
        self.end_headers()

    def handle_webhook(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_text(400, "invalid content length\n")
            return

        payload = self.rfile.read(length)
        signature = self.headers.get("Stripe-Signature", "")

        try:
            event = stripe.Webhook.construct_event(payload, signature, self.webhook_secret)
        except ValueError:
            self.send_text(400, "invalid payload\n")
            return
        except Exception as error:
            if STRIPE_SIGNATURE_ERRORS and isinstance(error, STRIPE_SIGNATURE_ERRORS):
                self.send_text(400, "invalid signature\n")
                return
            print(f"Webhook verification failed: {error}")
            self.send_text(400, "invalid signature\n")
            return

        event_data = stripe_object_to_dict(event)
        event_id = str(event_data.get("id", ""))
        event_type = str(event_data.get("type", ""))
        session = event_data.get("data", {}).get("object", {})

        if not event_id:
            self.send_text(400, "missing event id\n")
            return

        try:
            with connect_db(self.db_path) as connection:
                connection.execute("INSERT INTO stripe_events(event_id) VALUES (?)", (event_id,))
                if event_type in {"checkout.session.completed", "checkout.session.async_payment_succeeded"}:
                    result = credit_checkout_session(connection, session)
                elif event_type == "checkout.session.expired":
                    result = mark_checkout_session(connection, session, "expired")
                elif event_type == "checkout.session.async_payment_failed":
                    result = mark_checkout_session(connection, session, "failed")
                else:
                    result = f"ignored {event_type}"
        except sqlite3.IntegrityError:
            self.send_text(200, "duplicate event\n")
            return
        except Exception as error:
            print(f"Webhook handling failed for {event_id}: {error}")
            self.send_text(500, "webhook handling failed\n")
            return

        print(f"Stripe event {event_id}: {result}")
        self.send_text(200, "ok\n")


def main():
    stripe.api_key = env_required("STRIPE_SECRET_KEY")
    webhook_secret = env_required("STRIPE_WEBHOOK_SECRET")

    db_path = os.environ.get("BAYOU_ACCOUNTS_DB", "accounts.db")
    host = os.environ.get("BAYOU_STRIPE_HOST", "127.0.0.1")
    port = int(os.environ.get("BAYOU_STRIPE_PORT", "55005"))
    public_base_url = os.environ.get("BAYOU_STRIPE_PUBLIC_BASE_URL", f"http://{host}:{port}").rstrip("/")

    ensure_database(db_path)

    StripeCoinHandler.db_path = db_path
    StripeCoinHandler.public_base_url = public_base_url
    StripeCoinHandler.webhook_secret = webhook_secret

    server = ThreadingHTTPServer((host, port), StripeCoinHandler)
    print(f"Stripe coin service listening on http://{host}:{port}")
    print(f"Checkout URL base: {public_base_url}")
    print(f"Webhook endpoint: {public_base_url}/stripe/webhook")
    server.serve_forever()


if __name__ == "__main__":
    main()
