# Bayou Card Server Deployment

The card server listens on TCP port `55004` and stores `cards.db` in its working directory.

## Build on Linux

```sh
bash deploy/build-cardserver-linux.sh
```

The default output is `dist/cardserver/bin/cardserver`.

## Install as a Linux Service

Run this from a checkout of the repository on the Oracle Cloud VM:

```sh
sudo bash deploy/install-cardserver-linux.sh
```

Defaults:

- Binary prefix: `/opt/bayou/cardserver`
- Data directory: `/var/lib/bayou/cardserver`
- Service: `bayou-cardserver`
- Port: `55004/tcp`

Check the service:

```sh
systemctl status bayou-cardserver
journalctl -u bayou-cardserver -f
```

Oracle Cloud also needs an ingress rule allowing TCP `55004` to the VM from the client IP range you choose.

## Connect the Card Editor

On Windows PowerShell:

```powershell
$env:BAYOU_CARD_SERVER_HOST = "<oracle-vm-public-ip>"
.\build\Debug\cardeditor.exe
```
