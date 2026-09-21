[English](MANAGEMENT.md) | [Русский](MANAGEMENT_RU.md)

# Passwords and GitHub updates

## Passwords

Open `/settings` after signing in as `admin`. Set web, ArduinoOTA and setup Wi-Fi passwords independently; leave unchanged fields blank. Web/OTA require 16–64 printable ASCII characters without spaces; AP requires 8–63. Saving restarts the controller after the response, so reconnect using the new password. Saved values are never returned by the API. Wi-Fi reset does not remove passwords.

A new public installation exposes only initial provisioning through the setup AP (`Haier-Setup`). Set all three personal passwords before joining your network. The initial AP is a provisioning access mechanism, not an administrator password. Public firmware has no shared web/OTA login. Existing NVS credentials always take precedence. A private migration build on v1.0.0 is required to retain that version's compiled credentials before installing public binaries. Never publish a private migration image. Lost credentials require serial recovery; full flash erasure also loses Wi-Fi/MQTT/Modbus configuration.

## Updates

Open `/updates`. The persisted switch enables/disables installation, including the Install button. Check fetches metadata without installation; enable the switch and press Install for an explicit update. Automatic checks start after approximately one minute, then every six hours with jitter. Disabling installation still allows metadata checks. An in-progress download is cancelled on disable before boot-slot selection; once restart is committed, disabling cannot undo it.

Only `releases/stable.json` from this project's GitHub main branch is eligible. The manifest's exact payload is signed with a dedicated ECDSA P-256 key; the firmware embeds only its public key. HTTPS verifies certificates and hostnames, including redirected GitHub CDN URLs. A valid manifest must match profile `haier-s3-n16r8-v1`, stable numeric version, size and SHA-256. The app header must identify ESP32-S3. Factory images, incompatible profiles, older/equal versions and failed-image retries are rejected. NTP time and Internet access are required.

The download runs on a worker while local services continue. It waits for active AC commands to finish and blocks new writes during maintenance. ArduinoOTA and GitHub updates share a flash lock. Only the inactive app partition is written; a hash/image failure leaves the running slot selected. The previous slot and pending trial are persisted before selecting the candidate. A candidate confirms after 45 seconds of healthy local service and Wi-Fi. An unconfirmed restart or a 120-second unhealthy trial attempts to select the previous slot.

**Rollback limitation:** the bootloader installed by v1.0.0 does not enable ESP-IDF automatic rollback. The application fallback requires the new application to reach its updater initialization; a crash earlier than that can still require serial recovery. Two slots alone do not guarantee recovery from every bad image. No eFuse or bootloader rewrite is performed over OTA. Boot confirmation is not a full AC-function test.

## Release maintenance

Build public images through the clean `Release binaries` CI workflow with `public_release=true`. Run host tests, compile and verify the images, then sign with `tools/sign_release.py --build ... --key private/release-signing.pem --out .../stable.json`. The private key must be backed up outside Git and never uploaded to a release. Keep the corresponding public key stable for future updates. Publish binaries and their matching source under the version tag; promote the signed manifest to `releases/stable.json` only after testing. A private testing feed is distinct from stable and must never be enabled in a public build.

Useful patterns reused from [4VRS-Display](https://github.com/dk-1983/4VRS-Display): NVS credentials, CSRF-protected settings, signed manifests, bounded JSON, full redirect URLs, sequential flash writes, persisted update policy and trial boot handling. Screen rendering, HA permission leases and display-specific profile/partition constants are not part of this controller.
