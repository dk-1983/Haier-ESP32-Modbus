[English](SYSTEM.md) | [Русский](SYSTEM_RU.md)

<a id="вся-система-haier--esp32-s3--home-assistant"></a>

# Complete system: Haier → ESP32-S3 → Home Assistant

Three projects form the control path: **Haier-ESP32-Modbus** communicates with the AC, **Moxa / 4VRS Gateway** bridges RS-485 and the network, and **Modbus Devices** adds the equipment to Home Assistant. Source, setup instructions and validation results are available for each part.

<a id="что-подготовить"></a>

## What to prepare

| Component | Purpose | Documentation |
|---|---|---|
| Compatible Haier with hOn UART | Controlled AC; AS25HSL1HRA-W tested | [Validation](VALIDATION.md) |
| ESP32-S3-WROOM-1 N16R8, power and UART level conversion | Controller running this firmware | [GPIO and levels](HARDWARE.md), [firmware build](../README.md#build) |
| Home Assistant and Modbus Devices | Power, mode, temperature and fan controls | [Modbus Devices](https://github.com/dk-1983/Modbus_Devices) |
| RS-485 transceiver and Moxa with 4VRS Gateway | Serial-line connection option | [MAX485 connections](HARDWARE.md), [Gateway project](https://github.com/dk-1983/moxa-4vrs-gateway) |

Moxa and the transceiver are used for RS-485. Direct Modbus TCP over Wi-Fi needs only ESP and Home Assistant with Modbus Devices. The project provides software and tested bench connections; a finished PCB, enclosure and industrial protection circuit are not yet available.

<a id="1-запустить-контроллер-haier"></a>

## 1. Start the Haier controller

Build `haier-s3.yaml` with your credentials following the [README](../README.md#build). Connect UART and power as described in [HARDWARE.md](HARDWARE.md). Configure Wi-Fi through the setup access point, then open `/control` at the ESP address.

Before configuring other components, verify fresh temperature and AC state. One test command should receive Haier confirmation; restore the original setting afterward. [API and confirmation rules](API.md).

<a id="2-выбрать-путь-связи"></a>

## 2. Choose the connection path

| Path | ESP settings | Moxa settings | Modbus Devices transport |
|---|---|---|---|
| ESP → Wi-Fi → Home Assistant | TCP enabled, port 502 | Not used | Modbus TCP/IP |
| ESP → RS-485 → Moxa → Home Assistant | RTU enabled; documented bench: 19200 8N1, address 1 | Modbus TCP → RTU, matching serial settings | Modbus TCP/IP; Moxa host and network port |
| ESP → RS-485 → Moxa RAW → Home Assistant | RTU enabled, matching Moxa parameters | RAW TCP | Modbus RTU over TCP; Moxa host and network port |

ESP settings are at `/modbus`; RTU and TCP are independent. For Moxa, follow the [4VRS Gateway guide](https://github.com/dk-1983/moxa-4vrs-gateway/blob/main/docs/user-guide.md). Modbus TCP uses MBAP; RAW TCP carries RTU frames with CRC. Match the client transport to the gateway mode.

The physical-line test used Moxa UC-7420-LX Plus with 4VRS Gateway in Modbus TCP → RTU mode. [Register map and transport options](REGISTERS.md#home-assistant-and-moxa).

<a id="3-подключить-modbus-devices"></a>

## 3. Connect Modbus Devices

Install or update [Modbus Devices](https://github.com/dk-1983/Modbus_Devices) to **1.3.0 or later** through HACS and restart Home Assistant. Add a new hub and select manufacturer **4VRS**, model **Haier-ESP32**. Enter the chosen transport, ESP or Moxa host, corresponding port and Unit ID.

This ready-made profile provides climate, swing/fixed louvre positions, presets, quiet, display and link/command diagnostics. No manual register definitions are required. Commands are confirmed through the controller and readback. The separate **Haier YCJ-A002** profile remains for the factory adapter; select **4VRS Haier-ESP32** for ESP extensions.

You can use the Modbus Devices dashboard card generator. [Quick setup](../README.md#ready-made-home-assistant-integration). According to Modbus Devices documentation, full extended-profile hardware validation is planned with the production PCB. Completed firmware results are in [VALIDATION.md](VALIDATION.md).

<a id="4-проверить-всю-цепочку"></a>

## 4. Verify the complete path

1. Compare temperature, setpoint and power in Home Assistant with the ESP web panel.
2. Change one setting and wait for actual readback. A successful Modbus response means accepted; `Input 7 = 2` means two matching Haier packets confirmed it.
3. Power uses **Coil 0: 0 = off, 1 = on**. Choose a suitable time; avoid frequent compressor cycling.
4. Restore the original settings and verify fresh state.

If there is no response, check in order: fresh Haier data on ESP → enabled transport → serial parameters/Moxa mode → client transport/address. [Systems and test results](VALIDATION.md#systems-used-in-validation).

<a id="дополнительный-путь-mqtt"></a>

## Alternative MQTT path

The same firmware supports MQTT and Home Assistant Discovery through an external broker. This is a separate HA connection option; see [MQTT.md](MQTT.md). The validation report distinguishes MQTT command tests from Home Assistant UI coverage.
