# Stripe Coin Purchases

Bayou coin purchases use Stripe Checkout. The C++ client opens the payment service, and coins are credited only when the service receives a verified Stripe webhook.

Checkout Sessions enable Stripe Managed Payments and use the
`txcd_10201001` video-game tax code. Stripe handles the applicable indirect
tax, fraud, disputes, and transaction-level customer support for those
sessions.

## Install

```powershell
python -m pip install -r requirements-stripe.txt
```

## Configure

Required environment variables:

```powershell
$env:STRIPE_SECRET_KEY = "sk_test_..."
```

For local development, copy `.env.stripe.example` to the ignored `.env.stripe`
file and add your Stripe test secret key:

```powershell
Copy-Item .env.stripe.example .env.stripe
```

`run_stripe.bat` starts the Stripe CLI webhook forwarder and supplies its
temporary `STRIPE_WEBHOOK_SECRET` to the payment service automatically.

For a deployed service, configure both `STRIPE_SECRET_KEY` and the
`STRIPE_WEBHOOK_SECRET` from its persistent HTTPS webhook endpoint.

Optional environment variables:

```powershell
$env:BAYOU_ACCOUNTS_DB = "accounts.db"
$env:BAYOU_STRIPE_HOST = "127.0.0.1"
$env:BAYOU_STRIPE_PORT = "55005"
$env:BAYOU_STRIPE_PUBLIC_BASE_URL = "http://127.0.0.1:55005"
```

The client reads `payment_server_url` from `client.cfg`; the local default is `http://127.0.0.1:55005`.

Use Stripe test-mode keys while developing. Test-mode purchases do not require
connecting a real bank account.

## Run Locally

Start the game services:

```powershell
.\run_services.bat
```

When `.env.stripe` exists, `run_services.bat` starts Stripe in its own window.
Keep that window open while testing purchases. Closing it stops both the
payment service and local webhook forwarding. You can still run
`run_stripe.bat` directly when testing the payment service by itself.

In production, set `BAYOU_STRIPE_PUBLIC_BASE_URL` to the public HTTPS origin that forwards to the payment service, then configure the Stripe webhook endpoint as:

```text
https://your-domain.example/stripe/webhook
```

Listen for `checkout.session.completed` and `checkout.session.async_payment_succeeded`. The service also records expired and failed Checkout Sessions.
