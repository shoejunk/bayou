import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path


WEBHOOK_SECRET_PATTERN = re.compile(r"whsec_[A-Za-z0-9_]+")
SECRET_PATTERN = re.compile(r"(?:sk|rk|pk)_(?:test|live)_[A-Za-z0-9_]+|whsec_[A-Za-z0-9_]+")


def load_env_file(path):
    if not path.exists():
        return

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        os.environ.setdefault(name.strip(), value.strip())


def redact_secrets(message):
    return SECRET_PATTERN.sub("[redacted]", message)


def stop_process(process):
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def stream_output(process, prefix, output_queue=None):
    for line in process.stdout:
        if output_queue is not None:
            output_queue.put(line)
        else:
            print(f"{prefix}{redact_secrets(line.rstrip())}", flush=True)


def main():
    project_root = Path(__file__).resolve().parents[2]
    load_env_file(project_root / ".env.stripe")

    stripe_secret_key = os.environ.get("STRIPE_SECRET_KEY", "").strip()
    if not stripe_secret_key:
        raise SystemExit("STRIPE_SECRET_KEY is required in .env.stripe")
    if shutil.which("stripe") is None:
        raise SystemExit("Stripe CLI is required. Install it before running run_stripe.bat.")

    host = os.environ.get("BAYOU_STRIPE_HOST", "127.0.0.1")
    port = int(os.environ.get("BAYOU_STRIPE_PORT", "55005"))
    webhook_url = f"http://{host}:{port}/stripe/webhook"

    cli_env = os.environ.copy()
    cli_env["STRIPE_API_KEY"] = stripe_secret_key
    listener = subprocess.Popen(
        [
            "stripe",
            "listen",
            "--skip-update",
            "--forward-to",
            webhook_url,
        ],
        cwd=project_root,
        env=cli_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )

    payment_service = None
    initial_output = queue.Queue()
    reader = threading.Thread(
        target=stream_output,
        args=(listener, "[stripe] ", initial_output),
        daemon=True,
    )
    reader.start()

    try:
        deadline = time.monotonic() + 30
        webhook_secret = ""
        while time.monotonic() < deadline:
            if listener.poll() is not None and initial_output.empty():
                raise SystemExit("Stripe webhook listener exited before it was ready.")
            try:
                line = initial_output.get(timeout=0.25)
            except queue.Empty:
                continue

            match = WEBHOOK_SECRET_PATTERN.search(line)
            if match:
                webhook_secret = match.group(0)
                break
            print(f"[stripe] {redact_secrets(line.rstrip())}", flush=True)

        if not webhook_secret:
            raise SystemExit("Timed out waiting for the Stripe webhook signing secret.")

        print(f"Stripe test webhook forwarding is ready at {webhook_url}", flush=True)
        os.environ["STRIPE_WEBHOOK_SECRET"] = webhook_secret

        drain_thread = threading.Thread(
            target=lambda: stream_queued_output(initial_output, "[stripe] "),
            daemon=True,
        )
        drain_thread.start()

        payment_service = subprocess.Popen(
            [sys.executable, "-u", "src/payments/stripe_coin_service.py"],
            cwd=project_root,
            env=os.environ.copy(),
        )
        return payment_service.wait()
    except KeyboardInterrupt:
        return 130
    finally:
        stop_process(payment_service)
        stop_process(listener)


def stream_queued_output(output_queue, prefix):
    while True:
        line = output_queue.get()
        print(f"{prefix}{redact_secrets(line.rstrip())}", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
