[English](THIRD_PARTY_NOTICES.md) | [Русский](THIRD_PARTY_NOTICES_RU.md)

# Licenses and provenance

This is an independent interoperability project. Haier, ESPHome, Espressif and product names identify compatible hardware/software; no affiliation or endorsement is claimed.

| Scope | Origin | License |
|---|---|---|
| New Modbus core, register planner, transport adapter, setup integration and documentation | This project | MIT, see LICENSE |
| Wi-Fi portal and setup UI | Adapted from4VRS rack_bootstrap | MIT, see LICENSE-4VRS |
| `components/haier/*.cpp`, `*.h` and subdirectories (except independently authored MIT `bridge_control.h`) | ESPHome2026.6.5 Haier component, author Pavlo Dudnytskyi; includes local changes | GPLv3, see LICENSE-ESPHome |
| Python files in `components/haier` | ESPHome2026.6.5 | MIT portion of LICENSE-ESPHome |
| ESPHome runtime linked at build time | ESPHome2026.6.5 | GPLv3 C++ runtime; Python/tooling MIT |
| HaierProtocol | pavlodn/HaierProtocol0.9.31 | MIT, see LICENSE-HaierProtocol |
| Arduino-ESP32 / ESP-IDF | Downloaded dependencies | Their own LGPL/Apache and component-specific notices; retain upstream notices |

MIT applies to independently authored files and is not a relicensing of ESPHome. Distribution of a combined firmware must comply with GPLv3, including provision of corresponding source, modifications and build information. Dependency licenses remain in force. See upstream https://github.com/esphome/esphome and https://github.com/paveldn/HaierProtocol.

Local Haier changes: MONITOR_ONLY action guard retained from the PoC; validated field overrides added at the end of the existing group-command encoder for atomic Modbus writes, independent power/mode and remote lock. These modifications to GPL files remain under GPLv3.

YCJ-A002 numeric register definitions are used for interoperability. Original manuals remain available from the linked publishers; this repository does not redistribute their PDFs or Haier firmware. A register-compatible implementation is not a claim of identical behavior in every undocumented corner case.

ESP-MQTT: official Espressif MQTT client bundled with ESP-IDF 5.5.4, Apache-2.0, see LICENSE-ESP-MQTT and https://github.com/espressif/esp-mqtt. MQTT adapters and tests written for this project are MIT.
