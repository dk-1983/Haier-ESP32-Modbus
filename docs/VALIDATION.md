[English](VALIDATION.md) | [Русский](VALIDATION_RU.md)

<a id="проверки-100"></a>

# Version 1.0.0 validation

Target: ESP32-S3-WROOM-1 N16R8. AC: Haier AS25HSL1HRA-W. Test date: 2026-09-21. Pre-release commands were tested on 0.6.8; 1.0.0 also fixes the MQTT clock source. The full MQTT series was repeated successfully after installation.

<a id="программные-проверки"></a>

## Software checks

- `python tools/test_host.py`: MQTT/Modbus-to-hOn mapping, atomic writes, FC01/03/04/05/06/15/16, exceptions, RTU CRC/broadcast, MBAP/fragmentation, 50,000 malformed PDUs and buffer boundaries.
- `status_sensors.h`: guard bytes, null, short and extended packets. Four extra response bytes no longer overwrite adjacent objects.
- `python tests/test_discovery.py`: five Discovery JSON definitions, Jinja templates, no `none` in published preset lists, missing values and packet size limits.
- `python tools/test_hon_simulator.py`: 2000 exchanges using the installed HaierProtocol codec, CRC, corrupt frames, recovery and timeouts.
- ESPHome 2026.6.5 / Arduino-ESP32 3.3.9 build for 16 MB Flash and 8 MB PSRAM: SUCCESS.

<a id="реальный-haier-http-и-modbus-tcp"></a>

## Real Haier: HTTP and Modbus TCP

Every accepted command was confirmed by two new matching status packets. HTTP202 or Modbus echo alone did not count as success. Original settings were saved and restored after the series.

| Function | Coverage |
|---|---|
| Setpoint | Change/restore; 16/30 boundaries with power off |
| Fan | LOW / MEDIUM / HIGH / AUTO |
| Swing | OFF / VERTICAL / HORIZONTAL / BOTH |
| Vertical positions | HEALTH_UP / MAX_UP / HEALTH_DOWN / UP / CENTER / DOWN |
| Horizontal positions | MAX_LEFT / LEFT / CENTER / RIGHT / MAX_RIGHT |
| Presets | BOOST / SLEEP / NONE |
| Quiet, display | ON / OFF |
| Power | OFF, at least 180-second pause, ON; restore COOL |
| Modes | Write COOL / HEAT / DRY / FAN_ONLY / AUTO while off |
| Lock | LOCK / UNLOCK, codes 2/3 normalization; hOn bit confirmed |
| Modbus reads | FC01, FC03, FC04 |
| Modbus writes | FC05, FC06, FC15, FC16 with hOn confirmation |
| Errors | Invalid setpoint/address/unsupported function rejected |

Modes were tested without heating the server room. This validates mode writes/readback, not thermal performance in each mode. A confirmed lock bit does not replace testing the physical IR remote's behavior.

<a id="стенд-электрических-интерфейсов"></a>

## Electrical interface bench

- GPIO17/18 UART: loopback PASS, jumper removed FAIL, contact restored PASS, 9600 8N1.
- Arduino Nano ATmega328P Haier simulator: communication and power/setpoint/fan commands.
- RS-485: MAX485, GPIO15 DI, GPIO16 RO through level conversion, GPIO21 DE+/RE. Reads through [4VRS Gateway for Moxa](https://github.com/dk-1983/moxa-4vrs-gateway) (UC-7420-LX Plus, Modbus TCP → RTU) and [Modbus Devices for Home Assistant](https://github.com/dk-1983/Modbus_Devices), YCJ-A002 profile, confirmed at 19200 8N1.
- General-purpose optocouplers caused slow edges; direct MAX485 DI drive removed distortion. RO resistor level conversion was checked with an oscilloscope.
- After dismantling the RS-485 bench, physical RTU writes were not repeated on the real AC. RTU shares the TCP software core, but this is not independent hardware proof of RTU writes.

<a id="границы-проверки"></a>

## Validation boundaries

No claims of zero errors, compatibility with all Haier units, YCJ-A002 equivalence of nonzero fault codes, industrial EMC, finished galvanic isolation or long-duration endurance. Forced AC wire disconnection and Wi-Fi reset in the operating server room were not included. Earlier ESP8266 results do not replace S3 tests.

Raw logs, bench addresses, passwords and images with individual credentials are excluded from the public release.

<a id="повторяемая-проверка-http"></a>

## Repeatable HTTP test

`python tools/hil_controls.py --url http://DEVICE_IP --mac EXPECTED_MAC --log work/hil.jsonl --execute`

The password is entered interactively. The script requires active cooling, saves the original state, tests sequentially and restores settings even after an error. Failed restoration returns an error. This controls real equipment, not a simulator; run only under suitable operating conditions. Keep logs local.

<a id="mqtt-и-ota-на-100"></a>

## MQTT and OTA on 1.0.0

- 41 MQTT commands confirmed by real Haier, including OFF/ON and all available setting groups. Five Discovery configurations received PUBACK; a retained command delivered on subscription was rejected without changing the setpoint.
- The initial 0.6.8 run exposed false expired_command: receiver used Arduino millis, main loop used ESPHome millis. Both use ESPHome millis in 1.0.0; the complete repeat passed.
- Full 1.0.0 image uploaded through ArduinoOTA. After restart: version, MD5 match to local image, saved Wi-Fi, 8 MB PSRAM and fresh hOn recovery checked.
- MQTT used a separate test broker. Actual Home Assistant UI for this S3 was not included.

<a id="восстановление-и-защита-api-на-100"></a>

## Recovery and API protection on 1.0.0

Device checks passed for HTTP401 authentication, 403 bad token, 400 invalid value, repeated request_id without resending, and 409 conflicting write. Main pages returned 200. After diagnostic Wi-Fi disconnection, network/MQTT recovered and five Discovery configurations received PUBACK; uptime continued and fresh hOn packets arrived. MQTT/Modbus settings were restored to originally disabled transports. AC left ON, COOL, 22 °C, AUTO, NONE, Quiet OFF, display ON, swing OFF/CENTER.

<a id="системы-использованные-в-проверках"></a>

## Systems used in validation

- **[Modbus Devices for Home Assistant](https://github.com/dk-1983/Modbus_Devices)** — HA client integration using the YCJ-A002 profile to poll base bridge parameters.
- **[Moxa / 4VRS Gateway](https://github.com/dk-1983/moxa-4vrs-gateway)** — UC-7420-LX Plus gateway connecting the network client to ESP's physical RS-485 line.

Tested path: **Home Assistant / Modbus Devices → Moxa / 4VRS Gateway → RS-485 / MAX485 → ESP32-S3**. Moxa used Modbus TCP → RTU, serial 19200 8N1. Reads were confirmed; write coverage limits are above. [Transport selection](REGISTERS.md#home-assistant-and-moxa).
