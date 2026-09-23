[English](SCHEMATIC.md) | [Русский](SCHEMATIC_RU.md)

## Updated electrical schematic — rev1.0

![Electrical schematic rev1.0](assets/haier-schematic-rev1.0.png)

[Open / download SVG](assets/haier-schematic-rev1.0.svg) · [PNG](assets/haier-schematic-rev1.0.png)

This is the current electrical design for the planned v1.0 board. It includes both 10 kΩ / 20 kΩ RX dividers, the GPIO21 10 kΩ pull-down, R7 (120 Ω) with the normally open JP1 termination jumper, J2 (RS-485 A/B), parallel solder pads TP7/TP8, UART0 programming pads and EN/BOOT buttons. C1 is 220 µF / 10 V and C3 is 100 µF / 10 V; both use tantalum case C (6032-28). C2/C4/C5/C6 are 100 nF / 50 V. J1 is JST SM04B-GHS-TB: pins 1–4 are +5 V, GND, Haier TX, Haier RX.

Identical net labels indicate electrical connections. H_TX = GPIO17 → Haier RX; H_RX = divided Haier TX → GPIO18. MB_TX = GPIO15 → MAX485 DI; MB_RX = divided MAX485 RO → GPIO16; DIR = GPIO21 → DE and /RE. TP6 (+3V3) is a voltage measurement point, not a separate power input.

For an MQTT-only build, the optional RS-485 section comprises U3, R4–R7, C6, JP1, J2 and TP7/TP8. Keep the Haier UART dividers and power circuit.

The schematic passed KiCad ERC with zero errors and warnings. The v1.0 PCB has **not yet been assembled and tested**. PCB layout, manufacturing files and Gerbers will remain unpublished until the author's hardware validation. J2's physical part selection is still pending; its circuit connections are defined here. The earlier bench schematic and its notes are retained below for reference.

Updated optical schematic with **1 kΩ / 2 kΩ** pull-ups, pin verification and 9600/19200 results: [Optical UART](OPTICAL-UART.md). Any 2.2/4.7 kΩ values below describe the initial bench.

<a id="электрическая-схема-макета"></a>

# Prototype electrical schematic

![Haier ESP32-S3 and MAX485 schematic](assets/haier-s3-schematic-en.png)

[Vector SVG](assets/haier-s3-schematic-en.svg) · [PNG](assets/haier-s3-schematic-en.png)

Redrawn from the author's sketch with clarifications dated 2026-09-21. This documents the tested prototype and identified additions, not a finished PCB. Pin labels were checked against `haier-s3.yaml` and manufacturer documentation. Actual installed part markings and capacitor ESR cannot be established from the drawing.

<a id="сборка-для-mqtt-без-rs-485"></a>

## MQTT build without RS-485

**For Home Assistant over MQTT, omit the “03 RS-485” section if desired.** Leave out **U3 (MAX485), R4, R5, R6**, the local MAX485 supply capacitor and A/B connector. Connections to GPIO15, GPIO16 and GPIO21 are unnecessary.

Keep ESP32-S3, power, EN/BOOT and Haier UART: **GPIO17 → Haier RX**, **Haier TX → R2/R3 divider → GPIO18**, common ground. **Keep R2 and R3**: these convert the AC signal, not RS-485.

Use the same `haier-s3.yaml` firmware. Leave **RTU disabled** at `/modbus` and enable MQTT/Discovery at `/mqtt`. Web control and OTA remain available. **Modbus TCP over Wi-Fi works without MAX485** as well.

<a id="проверка-выводов"></a>

## Pin verification

| Circuit | ESP32-S3 GPIO | WROOM-1 pad | Other end |
|---|---|---|---|
| Controller Haier TX | 17 | 10 | Haier RX, direct |
| Controller Haier RX | 18 | 11 | Haier TX through R2/R3 |
| Controller RS-485 TX | 15 | 8 | MAX485 DI, pin 4 |
| Controller RS-485 RX | 16 | 9 | MAX485 RO, pin 1, through R4/R5 |
| RS-485 direction | 21 | 23 | MAX485 DE (3) and /RE (2) together |
| UART0 TX | 43 | 37 | Programmer RX, 3.3 V logic |
| UART0 RX | 44 | 36 | Programmer TX, 3.3 V logic |
| BOOT | 0 | 27 | SB1 to GND at power-up |
| EN | — | 3 | R1 10 kΩ to 3.3 V |
| Supply | — | 2 | 3.3 V |
| Ground | — | 1, 40, 41 | Common GND, including center pad |

MAX485 DIP/SO: **1 RO, 2 /RE, 3 DE, 4 DI, 5 GND, 6 A, 7 B, 8 VCC**. The circled 23 in the sketch is the GPIO21 module pad, not GPIO23. RO connects to GPIO16; GPIO18 belongs to the separate Haier UART. Symbol numbers identify component pins, not the external Haier connector order.

<a id="номиналы-и-питание"></a>

## Values and power

| Part | Value | Basis |
|---|---|---|
| U1 | ESP32-S3-WROOM-1 N16R8 | Project target |
| U2 | LM1117-3.3, fixed 3.3 V | Sketch marking; verify package/marking before PCB design |
| U3 | MAX485, 5 V supply | Tested interface |
| R1 | 10 kΩ | EN pull-up |
| R2, R4 | 10 kΩ | Signal source to RX |
| R3, R5 | 20 kΩ | RX to GND |
| R6 | 10 kΩ | DIR pull-down from project documentation; absent in photo |
| C1 | 220 µF / 10 V | Sketch |
| C2, C4 | 0.1 µF | C4 clarified by author |
| C3 | 100 µF / 10 V | Sketch; type and ESR require verification |
| SB1 | Normally-open momentary button | BOOT / PGM |

LM1117-3.3 SOT-223: **1 GND, 2 OUT, 3 IN; TAB is also OUT**. Do not ground the tab. Other packages require their own pinout. TI specifies at least 10 µF (tantalum) on the output and ESR 0.3–22 Ω. Thus 100 µF is sufficient in capacitance, but stability cannot be confirmed without the capacitor type/series. C4 100 nF does not replace C3. Other 1117 manufacturers may have different requirements.

MAX485 supply specification is 4.75–5.25 V. DI, DE and /RE accept HIGH from 2 V, so no extra 3.3 → 5 V drive stage is needed for these inputs. RO is on the 5 V side, so its direct ESP connection is replaced with a divider.

Both dividers use `V_RX = V_source × 20 / (10 + 20)`: 5.0 V gives 3.33 V. This is a nominal calculation, not regulation. Verify maximum voltage/tolerances and minimum HIGH at the receiving GPIO. MAX485's guaranteed VOH at its specified load is 3.5 V, or 2.33 V after the divider. The lightly loaded bench worked, but this does not guarantee margin over all parts/temperatures. Consider a 3.3 V transceiver or a buffer with a 5 V-tolerant input for production.

<a id="что-дополнено-и-что-остаётся-для-pcb"></a>

## Additions and remaining PCB work

- Restored the missing MAX485 RO divider R4/R5, confirmed by the author and bench.
- R6 is shown as the intended hardware direction pull-down; the photo does not establish its installation.
- Add a local 100 nF capacitor between MAX485 pins 8 and 5, close to the package. It is noted rather than claimed to be installed.
- Provide EN startup delay according to Espressif (typical RC: 10 kΩ / 1 µF); the sketch contains only R1. Check power routing and LM1117 heating during Wi-Fi peaks. At 0.5 A with 5 V input, calculated dissipation is 0.85 W.
- RS-485 termination/biasing depends on the line and existing gateway resistors; it is not specified here.
- Haier supply return is GND, not “−5 V”. Grounds are shared; this circuit does not provide galvanic isolation.

<a id="первичные-источники"></a>

## Primary sources

- [Espressif ESP32-S3-WROOM-1/1U — module pins](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf).
- [Espressif — power and EN](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html).
- [Analog Devices / Maxim — MAX485 pins and electrical specifications](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX1487-MAX491.pdf).
- [Texas Instruments — LM1117 pins and output capacitors](https://www.ti.com/lit/ds/symlink/lm1117.pdf).

The schematic is SVG, with PNG previews. Generator: `tools/draw_schematic.py` (PNG rendering requires `resvg-py`).
