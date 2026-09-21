[English](IMPLEMENTATION.md) | [Русский](IMPLEMENTATION_RU.md)

# Software architecture

`haier-s3.yaml` defines two hardware UARTs, PSRAM and local components. UART0 remains reserved for the bootloader/logs.

`fourvrs_portal` owns Wi-Fi, network setup, WebServer and ArduinoOTA. The ESPHome wifi component is not enabled: two radio owners would conflict. Python configuration explicitly enables Arduino libraries, SoftAP/DHCP and OTA dependencies.

`modbus_core.h` parses bounded PDU/ADU buffers without dynamic allocation. `register_model.h` validates the complete change set before hardware calls. `modbus_bridge.cpp` handles RTU, two TCP clients, settings and telemetry/register mapping.

All interfaces share a pending-command flag. A positive Modbus response acknowledges acceptance; two subsequent matching status packets confirm state. Unconfirmed commands time out after 30 seconds. Reads use received status fields, not optimistic ClimateCall properties.

The Haier component is retained from ESPHome. Local changes prevent automatic actions in MONITOR_ONLY and apply validated Modbus fields after the existing SET_GROUP_PARAMETERS encoder. `bridge_control.h` preserves independent mode/power and enables multi-field writes regardless of the previous mode. hOn was not reimplemented.

Configured status format: 2 subcommand bytes, 10 control bytes, 18 sensor bytes and 4 extra bytes; copying is bounded by the sensor structure size; status_message_header_size=0. Verify the format before using another model.
