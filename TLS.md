# TLS configuration

All Bayou C++ services now use Asio with Mbed TLS 3.6, certificate verification,
and length-framed connections. TLS 1.2 is the minimum protocol version. Debug
launchers generate and trust a localhost-only self-signed certificate under
`tls/`; those private development files are ignored by Git.

Release builds fail closed unless these environment variables are configured:

- `BAYOU_TLS_CERT_FILE`: PEM certificate chain used by every C++ server.
- `BAYOU_TLS_KEY_FILE`: PEM private key corresponding to the server certificate.
- `BAYOU_TLS_CA_FILE`: PEM CA bundle used by clients and service-to-service calls.
- `BAYOU_TLS_SERVER_NAME`: certificate DNS identity for service-to-service calls
  that connect to localhost. Public clients verify the configured endpoint host.

Use a certificate issued for the public service hostname in production. Keep the
private key outside the repository and restrict its filesystem permissions.
