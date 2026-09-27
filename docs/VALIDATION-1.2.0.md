[English](VALIDATION-1.2.0.md) | [Русский](VALIDATION-1.2.0_RU.md)

# v1.2.0 acceptance — 2026-09-27

Target: ESP32-S3-WROOM-1 N16R8 and Haier AS25HSL1HRA-W with the factory Wi-Fi UART connected through the ESP. The server-room air conditioner remained powered in COOL mode.

## Public binary

Clean GitHub Actions [run 36295257669](https://github.com/dk-1983/Haier-ESP32-Modbus/actions/runs/36295257669), firmware source commit `9974607026cc72d878e1b9bd8234711c74f0a7ae`. Later release commits change documentation and the signed manifest only.

- OTA MD5: `3b533cb0e3097f35e4ca229ef9ce0e7c`.
- OTA SHA-256: `35b04fdbd933ee448b77a45f034acfdde858377d06cf7da5eb15b3a31418650a`.
- Factory/OTA images and bundled installation files matched CI checksums. The update manifest signature was verified against the public key compiled into the firmware; profile is `haier-s3-n16r8-v2`.
- This exact public OTA binary was installed via ArduinoOTA. Existing network, passwords, MQTT/Modbus settings and cooling settings were retained; factory communication and fresh appliance state returned after boot.

## Commands and restoration

Production MQTT automation replaced the request identifier during the first HTTP attempt. That attempt was stopped and settings restored. The subsequent command sequence used a temporary isolated MQTT broker to prevent competing production commands.

- MQTT discovery, state, rejection of retained commands and invalid target temperatures passed. Seventeen valid commands covered target 21/22 °C, HIGH/AUTO fan, vertical swing/OFF, BOOST/NONE, quiet and display ON/OFF, vertical/horizontal positions and power ON.
- Sixteen HTTP commands covered the same controls except power; each was confirmed by two appliance state messages and checked against the requested value.
- Modbus TCP reads FC01/03/04 and writes FC05/06/15/16 received valid responses and appliance command confirmations. Invalid value/address/function requests returned the expected exceptions. This exercises the TCP server and UART command path, not the physical RS-485 transceiver.
- At final restoration: 549 main-board frames, 464 factory frames, 86 local requests/86 replies and 4 recovered preambles since boot. Unhandled parser errors, partial/transaction timeouts and overflows were zero.
- Production MQTT was restored and connected; no dropped messages or MQTT error. Original RTU/TCP enable flags restored. Final appliance settings: **power ON, COOL, 22 °C, LOW, swing OFF, preset NONE, quiet OFF, display ON, both positions CENTER**.

## Earlier hardware checks and limits

The development image with the same bridge implementation completed the [30-minute run and physical factory-module removal/return checks](UART_BRIDGE.md#current-hardware-results--2026-09-27). These are separate from public-binary command acceptance; a second 30-minute run was not claimed for the public binary.

Physical RS-485 on GPIO8/9 remains deferred until the MAX485 board arrives. Power OFF and heating/drying modes were not exercised during this public-binary check because the unit cools server equipment. Successful tests on this installation do not certify every Haier model, factory feature or factory OTA procedure.

The first upgrade from v1.1.0 is manual after [checking the changed pinout](FLASHING.md). Ordered PCB rev1.0 uses the old RS-485 pins; its layout and Gerbers are unchanged and remain private.
