[English](HARDWARE.md) | [Русский](HARDWARE_RU.md)

<a id="стенд-esp32-s3-wroom-1-n16r8"></a>

# ESP32-S3-WROOM-1 N16R8 bench hardware

This configuration targets a new ESP32-S3 module, not the factory single-core ESP32-for-Haier. It needs 3.3 V power, common ground, EN/BOOT circuitry and a UART programmer with 3.3 V logic. The module has no 5 V input; consult [Espressif's hardware guide](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html).

| Purpose | ESP32-S3 pin | Connection |
|---|---|---|
| UART0 TX | GPIO43 / TXD0 | ESP Link programmer RX |
| UART0 RX | GPIO44 / RXD0 | ESP Link programmer TX |
| hOn TX | GPIO17 | Air conditioner RX |
| hOn RX | GPIO18 | Air conditioner TX |
| RS485 TX | GPIO15 | Transceiver DI |
| RS485 RX | GPIO16 | Transceiver RO |
| RS485 direction | GPIO21 | DE and /RE together, 10 kΩ pull-down to GND |
| GND | GND | Common signal ground according to the power/isolation design |

Pins are substitutions in haier-s3.yaml. UART0 is reserved for flashing/logs; ESPHome assigns the other two hardware UARTs to the two UART components. GPIO35..37 on N16R8 are occupied by PSRAM and unused here. GPIO0/45/46 are not used for the buses.

The transceiver must have compatible 3.3 V levels (for example, a 3.3 V RS-485 transceiver); do not wire a bare 5 V MAX485 RO output directly to ESP32. A MAX485ESA USB adapter can be used on the RS-485 client side without modification. Termination and biasing depend on the actual line; this is not a finished industrial protection/isolation design.

Provide a hardware DE pull-down so the transmitter stays disabled during reset/boot. ESPHome flow_control_pin uses the standard UART RS485 half-duplex driver. Disabled RTU does not reply. Verify module power and isolation from the power board before connecting USB/computer equipment.

<a id="электрическая-схема"></a>

## Electrical schematic

[Schematic, values and pin verification](SCHEMATIC.md) · [SVG](assets/haier-s3-schematic-en.svg) · [PNG](assets/haier-s3-schematic-en.png).

<a id="проверенное-подключение"></a>

## Validated connections

Haier UART: 9600 8N1, GPIO17 TX → Haier RX; Haier TX → level conversion → GPIO18. No inversion or internal pull-ups. A 5 V output must not connect directly to an ESP32 GPIO. The resistor divider applies only in the 5 V → 3.3 V direction:

```text
Source TX ── Rupper ──┬── ESP32 RX
                     Rlower
                       │
                      GND
```

Vout = Vin × Rlower / (Rupper + Rlower). Select values based on the actual maximum source level, tolerances and waveform. Measure relative to common ground before connecting the GPIO. MAX485 RO was tested with Rupper=10 kΩ and Rlower=20 kΩ; this is a recorded bench result, not a universal value for any supply.

MAX485: DI (4) ← GPIO15; RO (1) → divider → GPIO16; DE (3) and /RE (2) together ← GPIO21; GPIO21 has a 10 kΩ pull-down to GND. MAX485 supply is 5 V. Direct DI/DE control requires compliance with the exact transceiver's thresholds; RO is converted separately. The physical channel was validated at 19200 8N1 through [4VRS Gateway for Moxa](https://github.com/dk-1983/moxa-4vrs-gateway) on UC-7420-LX Plus in Modbus TCP → RTU mode and [Modbus Devices for Home Assistant](https://github.com/dk-1983/Modbus_Devices) using the YCJ-A002 profile. [Gateway configuration guide](https://github.com/dk-1983/moxa-4vrs-gateway/blob/main/docs/user-guide.md).

Two ordinary transistor optocouplers were tested at UART 9600. Communication worked but edges were slow. At MAX485 transmit speed 19200, short pulses were distorted; direct DI drive restored communication. An isolated version needs optocouplers/digital isolators with suitable timing. Shared ground or a shared non-isolated supply defeats full galvanic isolation.

<a id="стенд-до-подключения-кондиционера"></a>

## Bench before connecting the AC

`bench/` contains a Haier simulator for Arduino Nano ATmega328P (5 V), SoftwareSerial RX D10 / TX D11. First verify GPIO and TX/RX loopback at 3.3 V, then level conversion and simulator communication. Check waveforms at the receiving pin, including short pulses, not only at the source TX.

Confirmed: 16 MB Flash, 8 MB PSRAM, UART without pull-ups/inversion, local controls and OTA. `gpio17-test.yaml` and `uart-loopback-test.yaml` are diagnostics, not operating AC firmware. Results: [VALIDATION.md](VALIDATION.md).

This is a tested interface prototype, not a finished PCB. Regulator, supply protection, enclosure, connectors, isolation and RS-485 protection need separate design work. GPIO numbers are not air conditioner connector pin numbers.

## Connector ordering reference

The [upstream haier-esphome README](https://github.com/paveldn/haier-esphome#haier-climate) identifies **JST SM04B-GHS-TB** in the ESP32-for-Haier Wi-Fi module context. This is a board-mounted connector; it must not be assumed to identify the indoor-unit motherboard socket shown in our photos.

For that GH connector, the wire-side housing is **GHR-04V-S**, with four **SSHL-002T-P0.2** crimp contacts. GH pitch is **1.25 mm**; see the [JST series catalog](https://www.jst-mfg.com/product/index.php?lang=2&series=105) and [manufacturer datasheet](https://www.jst-mfg.com/product/pdf/eng/eGH.pdf). Before ordering for our motherboard socket, confirm contact pitch and the mating/keying geometry on the original hardware. Its exact mating part number is still unconfirmed; the rendered illustration is not a measurement reference.
