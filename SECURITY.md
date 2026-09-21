[English](SECURITY.md) | [Русский](SECURITY_RU.md)

# Security

This project is intended for a trusted local network. Modbus TCP has no authentication or TLS; do not expose port502 to the Internet. Setup credentials and OTA/web password belong only in untracked secrets.yaml. Web Basic authentication uses plain HTTP; use network isolation as appropriate. RTU/TCP are disabled by default and can be enabled independently. No factory keys or firmware are distributed.

For reports, provide a minimal reproduction with credentials, MAC addresses and personal network details removed. Before making an exploitable vulnerability public, coordinate privately with the repository maintainer through their listed contact method.

Public v1.1.0 has no shared admin/OTA password. On an unprovisioned board, join the setup AP using `Haier-Setup` and immediately set personal web, OTA and AP passwords before connecting Wi-Fi. Access to this initial AP permits first provisioning. Saved credentials take precedence over compiled values and survive OTA. Password endpoints require authentication (or the initial AP before provisioning) and a per-boot CSRF token. GitHub images must pass HTTPS, project-specific ECDSA signature, SHA-256 and board checks. See [management](docs/MANAGEMENT.md).
