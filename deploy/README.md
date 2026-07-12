# Bayou Server Deployment

For Stripe-backed coin purchases, see [stripe-payments.md](stripe-payments.md).

The release services use these TCP ports:

- Accounts: `55000`
- Matchmaking: `55001`
- Game coordinator: `55002`
- Card server: `55004`
- Game sessions: `56000+`

The authoritative card database must already exist in the shared data
directory. The accounts and game services read their card source from
`/etc/bayou/gameserver.cfg`; by default they fetch the catalog from the
configured card server, which is backed by `/var/lib/bayou/shared/cards.db`.
The installer never copies a workspace-local card database into the deployment.

## Build on Linux

```sh
bash deploy/build-servers-linux.sh
```

The default output is `dist/servers/bin`.

## Install as a Linux Service

### Production TLS prerequisites

Provision a certificate before deploying. The certificate must cover the DNS
name (recommended) or IPv4 address that clients use for all four services. Keep
the private key outside the repository. Prepare:

- a PEM server certificate chain;
- its PEM private key;
- a PEM CA bundle that validates that chain; and
- the exact certificate identity, such as `game.example.com`.

The installer verifies expiry, chain trust, key correspondence, and hostname or
IP coverage before changing `/opt/bayou/current`. It installs the material under
`/etc/bayou/tls`, readable only as needed by the `bayou` service account, and
creates `/etc/bayou/tls.env` for all four systemd units.

Run this from a checkout of the repository on the Oracle Cloud VM:

```sh
export TLS_CERT_FILE=/etc/letsencrypt/live/game.gloomthorn.com/fullchain.pem
export TLS_KEY_FILE=/etc/letsencrypt/live/game.gloomthorn.com/privkey.pem
export TLS_CA_FILE="$(pwd)/deploy/ca/isrg-root-x1.pem"
export TLS_SERVER_NAME=game.gloomthorn.com
sudo --preserve-env=TLS_CERT_FILE,TLS_KEY_FILE,TLS_CA_FILE,TLS_SERVER_NAME \
  bash deploy/install-servers-linux.sh
```

Set those four variables to the source files and certificate identity before
running the command. The installer performs TLS handshakes against ports
`55000`, `55001`, `55002`, and `55004` after restart. If a service or handshake
fails, it restores the prior release symlink and restarts the previous binaries.

After certificate renewal, rerun the installer with the renewed source files to
copy them into `/etc/bayou/tls` and validate the deployment.

Defaults:

- Versioned releases: `/opt/bayou/releases/<UTC timestamp>`
- Current release: `/opt/bayou/current`
- Shared data: `/var/lib/bayou/shared`
- Game-server config: `/etc/bayou/gameserver.cfg`
- Services: `bayou-accounts`, `bayou-cardserver`, `bayou-gameserver`,
  and `bayou-matchmaking`

Check the services:

```sh
systemctl status bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking
journalctl -u 'bayou-*' -f
```

## Package the Windows client

The release client requires the public CA certificate and the same DNS name or
IP covered by the server certificate. The packaging script copies the CA as
`bayou-ca.pem` beside `SteamTactics.exe` and rewrites the packaged
`client_release.cfg` endpoints to the verified identity:

```powershell
.\deploy\package-client-release.ps1
```

The CA certificate is public and belongs in the client artifact; never provide
or package the server private key. Packaging fails before building if the
selected CA file is absent or the server identity is invalid.

The repository defaults already use `game.gloomthorn.com` and the official
ISRG Root X1 certificate, so the two environment variables are only needed when
changing certificate authorities or service identity.

Oracle Cloud VCN ingress rules must also allow TCP `55000-55002`, `55004`,
and the game-session range `56000-65535` from the intended client IP range.

The payment unit is installed but is not enabled automatically. Configure
`/etc/bayou/stripe.env` with the production Stripe key, persistent HTTPS
webhook signing secret, database path, listener address, and public URL before
enabling `bayou-payments`.

## Connect the Main Client Card Editor

The card editor is available from the main client after signing in with an admin account. Point the client card server setting at the deployed card server.

On Windows PowerShell:

```powershell
$env:BAYOU_CARD_SERVER_HOST = "<oracle-vm-public-ip>"
.\build\Debug\SteamTactics.exe
```
