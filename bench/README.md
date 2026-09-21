[English](README.md) | [Русский](README_RU.md)

# Haier hOn: Arduino Nano bench simulator 0.1.1

Simulates AC responses to test the **normal ESP32-S3 firmware** and two optical channels on a bench, with the real AC completely disconnected. An independent test tool, not factory firmware or a complete Haier behavioral model.

## Connections

Confirmed board: classic **Nano ATmega328P, 5 V**. ESP runs the main firmware, GPIO17 TX / GPIO18 RX, 9600 8N1, no inversion.

Nano uses SoftwareSerial: **D10 RX, D11 TX, 9600 8N1**. Hardware UART D0/D1 remains for USB diagnostics at 115200, avoiding an output conflict between the USB-UART adapter and optocoupler on RX0. ESP exercises the main firmware's hardware UART, not a separate loopback program.

Use two independent channels from the [schematic](../docs/PC817-UART-BENCH.md), PC817 or NEC PS2561-1 after pinout verification:

| Connection | Bench wiring |
|---|---|
| U1 anode, pin 1 | Nano +5 V through 680 Ω |
| U1 cathode, pin 2 | Nano **D11 TX** |
| U1 collector, pin 4 | ESP **GPIO18 RX**, 2.2 kΩ pull-up to **ESP 3.3 V** |
| U1 emitter, pin 3 | ESP GND |
| U2 anode, pin 1 | ESP +3.3 V through 390 Ω |
| U2 cathode, pin 2 | ESP **GPIO17 TX** |
| U2 collector, pin 4 | Nano **D10 RX**, 4.7 kΩ pull-up to **Nano 5 V** |
| U2 emitter, pin 3 | Nano GND |

Both channels are non-inverting. Do not bypass optocouplers with direct TX-RX jumpers or parallel the old divider/Zener with U1's output. 5 V, 3.3 V and TX are separate nets.

Power Nano from USB and ESP from its suitable supply/programmer. **Do not power ESP from Nano's 3.3 V output.** Do not connect separate supply outputs together. Shared ground is acceptable, but shared ground/USB means this is not a galvanic-isolation test. Rewire with power removed.

## Build and upload

Open `haier_simulator/haier_simulator.ino` in Arduino IDE. Select Arduino Nano, ATmega328P, or ATmega328P (Old Bootloader) for older bootloaders; the specific board's bootloader was not yet identified in this record. Identify Nano's current port separately; **do not assume COM6 from old messages**. Only the built-in SoftwareSerial library is required.

CLI from repository root:

```powershell
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old --build-path work/nano-simulator bench/haier_simulator
```

For a newer bootloader use `cpu=atmega328`. This selects Nano bootloader/speed; previous 2 Mbaud uploads concerned S3, not Nano. Build checked with Arduino AVR core 1.8.6: 6928 flash bytes, 721 static RAM bytes. On 2026-09-20, v0.1.1 was uploaded to Nano ATmega328P and flash readback verified. At that stage optical communication had not passed: incoming frames were distorted. Later bench results are in [HARDWARE.md](../docs/HARDWARE.md).

## Simulated behavior

- Version 0x61/0x62 (SIMULATR identifier, CRC capability), ID 0x70/0x71.
- Poll 0x4D01 and big-data 0x4DFE, STATUS response 0x02.
- Group write 0x6001 from the current main firmware; all 10 control bytes stored in RAM.
- Subset of 0x5Dxx single writes: power, temperature, mode, fan, swing, display, quiet, boost, sleep, etc.; unknown commands return INVALID 0x03.
- Empty alarms 0x73/0x74, link control 0xFC/0xFD, acknowledgement 0xF7.
- Checksum, CRC-16/ARC, FF55 stuffing, corrupt/truncated packet recovery. Maximum payload 64 bytes; inter-byte gap >100 ms discards an incomplete frame.

STATUS matches the local configuration: 2 subtype bytes + 10 control + 18 sensor + 4 extra sensor bytes. Big-data adds 14 bytes. Status header size is 0. Changing ESP settings requires matching simulator changes.

Initial state: power off, COOL selected, 24 °C setpoint, AUTO fan, display on. Synthetic readings: room **23.5 °C**, humidity 45%, outdoor 20 °C. Commands change state immediately until Nano reboots. Thermal behavior, compressor, cycle delays, faults and autonomous mode changes are not simulated. Zero big-data power/drive values are placeholders.

**ESP web/MQTT presents these values as normal device data.** Initially use the web panel and disable MQTT/Modbus if home automations are bound to this controller. Use separate MQTT names/topics if later connecting the bench to HA. ESP firmware does not label the values as synthetic.

## Bench procedure

1. Open Nano USB logs at 115200. Opening a monitor may restart Nano and reset its state.
2. Start normal ESP firmware and wait for hOn initialization. Nano should log 0x61, 0x70, 0x01, 0x73; ESP should report First HVAC status received / available, increasing status_frames and 23.5 °C.
3. Test ON/OFF, setpoint, mode, fan, quiet/display and louvres from the ESP panel. Nano `commands` increases; polls return the latest state. GPIO13 LED blinks on a valid received packet.
4. Leave for at least 10 minutes. Expect increasing `good`/`sent`, no growth in `bad`, `partial_timeout`, `overflow`, and no ESP timeout in ordinary exchange. This validates the bench, not real Haier behavior.
5. Send `m` to Nano USB: replies stop and ESP should time out. `r` resumes replies; verify recovery (ESP may repeat initialization after a long pause).
6. `c` corrupts one byte of the next reply without recomputing checksum/CRC. ESP should reject it and recover on the next request. `s` prints statistics immediately.

`bytes=0`: Nano receives no ESP bytes (check U2/TX17/D10). Increasing bytes with good=0 suggests levels, baud, inversion or framing. Increasing good/sent without ESP replies suggests U1/D11/RX18. SoftwareSerial may also lose data; check overflow. These are diagnostic directions, not unique diagnoses.

Success demonstrates main ESP firmware and adapter operation with 5 V Nano pins at 9600 baud. It **does not prove** that real Haier TX can sink the same LED current or has identical thresholds/edges.

Version 0.1.1 prints `WIRE RX` after 30 ms silence: up to 96 raw received SoftwareSerial bytes before frame parsing. This is not an analog waveform measurement.

## Automated checks

`python tools/test_hon_simulator.py` uses HaierProtocol 0.9.31 from the local ESP build as an independent codec reference. It checks 2000 payload/CRC combinations, FF55, checksum/CRC corruption, truncated/sequential frames, recovery, handshake and command persistence. These are host tests, not electrical measurements.

Simulator code is independently written, MIT. Format references: `components/haier/hon_packet.h`, `hon_climate.cpp` and HaierProtocol 0.9.31; third-party source is not copied into this sketch. Arduino AVR core and SoftwareSerial retain their own licenses.
