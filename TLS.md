# TLS configuration

All Bayou C++ services now use Asio with Mbed TLS 3.6, certificate verification,
and length-framed connections. TLS 1.2 is the minimum protocol version. Debug
launchers generate a localhost-only development CA and a server certificate it
signs under `tls/`; those private development files are ignored by Git.

Release servers fail closed unless their certificate and key are configured.
Release clients use `BAYOU_TLS_CA_FILE` when set, otherwise they load
`bayou-ca.pem` beside the executable. The release packaging script requires
that public CA file and places it there automatically.

Configuration variables:

- `BAYOU_TLS_CERT_FILE`: PEM certificate chain used by every C++ server.
- `BAYOU_TLS_KEY_FILE`: PEM private key corresponding to the server certificate.
- `BAYOU_TLS_CA_FILE`: PEM CA bundle used by clients and service-to-service calls.
- `BAYOU_TLS_SERVER_NAME`: certificate DNS identity for service-to-service calls
  that connect to localhost. Public clients verify the configured endpoint host.

Use a certificate issued for the public service hostname in production. Keep the
private key outside the repository and restrict its filesystem permissions.
The current production identity is `game.gloomthorn.com`; release packaging
bundles the official ISRG Root X1 trust anchor from `deploy/ca/` by default.
