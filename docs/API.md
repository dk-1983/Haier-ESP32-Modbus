[English](API.md) | [Русский](API_RU.md)

<a id="http-и-управление"></a>

# HTTP and control

- GET `/`: current state and links.
- GET `/health`: version/memory/Wi-Fi/OTA/uptime.
- GET `/haier/status`: fresh state, status count, age, command_state and request_id.
- GET `/control`: main panel; HTTP Basic `admin` / personal web password.
- GET `/haier/test-token`: current-boot token, authenticated.
- POST `/haier/control`: token, request_id and target/mode/fan/swing/preset; original validated controls.
- POST `/haier/extended`: token, request_id and exactly one quiet/display=ON|OFF or vertical_position/horizontal_position using a panel enum name.
- GET `/modbus`: independent RTU/TCP switches, address and baud rate.
- GET `/modbus/config`: configuration; authentication required.
- POST `/modbus/config`: token, rtu=0|1, tcp=0|1, unit=1..247, baud=9600|19200|38400|57600|115200. Saved in NVS. Returns 409 while a command is pending.
- `/wifi`, `/scan`, `/network`: setup via the setup AP (see 0.6.0 additions below for `/wifi` on LAN).

Both Modbus transports are disabled after a clean first flash. Changes do not disable Wi-Fi, web control or OTA. Disabling TCP closes the listener and clients; disabling RTU stops replies and clears its buffer when applied.

HTTP202 means accepted; `command_state=confirmed` means two matching Haier replies. The common arbiter prevents simultaneous web/RTU/TCP commands. Reusing the last HTTP request_id does not send the command again; reboot clears this protection. Modbus has no HTTP request_id or persistent deduplication.

OTA: ArduinoOTA UDP8266, personal OTA password stored in NVS. For ESP32 use espota.py from the pinned Arduino-ESP32 framework: 3.3.9 uses its supported authentication mechanism. ESP8266 images are incompatible. Automatic rollback is not promised without separate setup and validation.

MQTT: GET `/mqtt` settings; GET `/mqtt/config` configuration/diagnostics without password; POST `/mqtt/config` fields token, enabled, host, port, username, password, clear_password and prefix. All routes require Basic authentication. See [MQTT.md](MQTT.md).

<a id="сброс-wi-fi"></a>

## Wi-Fi reset

GET `/wifi/reset` serves an authenticated confirmation page on the main LAN and setup AP. POST requires the current-boot `token` and `confirm=RESET_WIFI`. It deletes both Wi-Fi records (active and candidate), disconnects STA and opens `<hostname>-setup` at `192.168.4.1`. The personal AP password stored in NVS is preserved. MQTT/Modbus settings and control passwords are preserved; this is a network reset, not a full configuration wipe.

Success: HTTP202. Pending command, scan, network application or an existing reset: HTTP409. Save failure: HTTP503. Disconnect is delayed 750 ms so the browser can receive the response. No reboot is needed. If the normal network is unavailable, existing recovery opens the setup AP after about 60 seconds.

Saved-network reset was not part of acceptance testing on the operating AC. Saved Wi-Fi recovery after OTA was verified; that is a different operation.

<a id="дополнения-060"></a>

## 0.6.0 additions

`POST /haier/control`: standalone `power=ON|OFF`, with token and request_id and no other settings. ON restores the mode from fresh hOn status; OFF turns power off. Shared authentication, arbiter and two-packet confirmation apply.

`GET /about` is an authenticated diagnostic page. `/wifi` shows network status on LAN and configuration on the setup AP. `/health` adds min_heap, max_block, fragmentation (an approximate largest-block/free-heap relationship), psram_size, free_psram and flash_size; memory values are bytes.

`/mqtt/config` adds discovery (POST 0/1, GET boolean) and discovery_sent (GET PUBACK count 0..5). Omitting discovery from older POST requests preserves its value.

## 1.1.0 management

All routes below require the personal web password and POST requests require the current-boot `token`. Initial password provisioning is available only from the setup AP. See [management](MANAGEMENT.md).

- GET `/settings`, `/settings/status`.
- POST `/settings/passwords`: `web_password`, `ota_password`, `setup_password`. HTTP 202.
- GET `/updates`, `/updates/status`.
- POST `/updates/config`: `enabled=0|1`.
- POST `/updates/check`. HTTP 202.
- POST `/updates/install`. HTTP 202; HTTP 409 when updates are disabled.
