# Bayou Server Deployment

For Stripe-backed coin purchases, see [stripe-payments.md](stripe-payments.md).

The release services use these TCP ports:

- Accounts: `55000`
- Matchmaking: `55001`
- Game coordinator: `55002`
- Card server: `55004`
- Game sessions: `56000+`

The accounts and card services use `accounts.db` and `cards.db` in their
shared working directory.

## Build on Linux

```sh
bash deploy/build-servers-linux.sh
```

The default output is `dist/servers/bin`.

## Install as a Linux Service

Run this from a checkout of the repository on the Oracle Cloud VM:

```sh
sudo bash deploy/install-servers-linux.sh
```

Defaults:

- Versioned releases: `/opt/bayou/releases/<UTC timestamp>`
- Current release: `/opt/bayou/current`
- Shared data: `/var/lib/bayou/shared`
- Services: `bayou-accounts`, `bayou-cardserver`, `bayou-gameserver`,
  and `bayou-matchmaking`

Check the services:

```sh
systemctl status bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking
journalctl -u 'bayou-*' -f
```

Oracle Cloud VCN ingress rules must also allow TCP `55000-55002`, `55004`,
and the game-session range `56000-65535` from the intended client IP range.

The payment unit is installed but is not enabled automatically. Configure
`/etc/bayou/stripe.env` with the production Stripe key, persistent HTTPS
webhook signing secret, database path, listener address, and public URL before
enabling `bayou-payments`.

## Connect the Card Editor

On Windows PowerShell:

```powershell
$env:BAYOU_CARD_SERVER_HOST = "<oracle-vm-public-ip>"
.\build\Debug\cardeditor.exe
```
