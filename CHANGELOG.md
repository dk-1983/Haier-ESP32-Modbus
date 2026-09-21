[English](CHANGELOG.md) | [Русский](CHANGELOG_RU.md)

# 1.0.0 — 2026-09-21

- Unified MQTT clock: receive timestamp and age check use ESPHome millis, preventing false expired_command.
- First stable ESP32-S3 N16R8 release: local web controls, MQTT/Discovery, independent Modbus RTU/TCP and ArduinoOTA.
- Commands validated against real Haier replies; RS-485 bench validated with a simulator.
- Includes bounded hOn copy fix (0.6.7) and OTA watchdog servicing (0.6.8).
- Updated connections, test results and limitations. Source only, without individual credentials or device images.

# 0.6.8 — 2026-09-21

- Services the ESPHome watchdog during synchronous ArduinoOTA start/progress events; watchdog remains enabled.
- Fixes the confirmed loopTask reset during full-image Wi-Fi upload.

# 0.6.7 — 2026-09-21

- Fixes an out-of-bounds copy into the 18-byte hOn sensor structure: four extra protocol bytes no longer overwrite adjacent web-server fields.
- Adds guard-byte and short-packet copy tests.
- Versions 0.6.1–0.6.6 were used for separate hardware diagnostic builds.

# 0.6.0 — 2026-09-20

- Web-panel parity with ESP8266 v0.5.1: power, control sections, shared navigation, overview, Wi-Fi and memory/PSRAM information.
- Home Assistant Discovery for five entities with QoS1/PUBACK and an independent switch.
- Atomic HVAC mode+power, suppression of intermediate telemetry, stale queued-state rejection.
- hOn message table stored without dynamic strings. Modbus and existing NVS layout preserved.
- Software checks/build completed; S3 hardware tests awaited hardware readiness at this stage.

# Changes

## 0.5.1 — 2026-09-20

- Protected `/wifi/reset` page clears active/candidate networks and restores the setup AP.
- Preserves MQTT/Modbus settings and control passwords during network reset.
- Reset follows the HTTP response; pending AC commands or network configuration return busy.

## 0.5.0 — 2026-09-20

- External-broker Wi-Fi MQTT client using ESP-MQTT.
- Independent switch and broker settings at `/mqtt`, NVS persistence without API password disclosure.
- State, availability/Last Will, command and result topics; shared HTTP/Modbus arbiter.
- Background networking, bounded queues, retained-command rejection on subscription.
- Command conversion/message fragmentation tests and MQTT documentation.

## 0.4.0 — 2026-09-20

- Port of the tested ESP8266 PoC to ESP32-S3-WROOM-1 N16R8.
- YCJ-A002 map/extensions, independent Modbus RTU and TCP.
- Web controls, Wi-Fi setup, OTA, source references and native tests.
- Local commit 0e3f371; health version was historically `0.4.0-s3-modbus`.

Firmware version is defined in components/fourvrs_portal/version.h and shown at `/health`. Git tags vX.Y.Z identify version source. Host/build checks and hardware validation are recorded separately; a tag is not hardware certification.

## 0.6.1 — cancelled diagnostic build

- RX pull-up experiment built but not installed: OTA was interrupted. The user cancelled the experiment; UART source/version returned to v0.6.0 without pull-ups. Do not use the experimental 0.6.1 image.

## 0.6.2-tx-invert — diagnostic build

- At the user's request, only Haier TX GPIO17 was inverted. RX GPIO18 remained non-inverted; both pull-ups disabled. Intended for output measurements, not validated as a working Haier configuration. Build passed, OTA interrupted; installation unconfirmed.
