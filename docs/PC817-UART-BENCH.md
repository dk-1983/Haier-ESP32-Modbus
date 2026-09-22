[English](PC817-UART-BENCH.md) | [Русский](PC817-UART-BENCH_RU.md)

Updated optical schematic with **1 kΩ / 2 kΩ** pull-ups, pin verification and 9600/19200 results: [Optical UART](OPTICAL-UART.md). Any 2.2/4.7 kΩ values below describe the initial bench.

# Two PC817 channels for Haier UART — bench option

Initial status: proposed, not yet assembled/validated (later bench update below). 9600 8N1. Powering ESP from Haier through an ordinary regulator shares ground: this is optical signal transfer and level conversion, NOT full galvanic isolation. Overload of the older ESP8266 GPIO was not proven; successful communication does not make 5 V acceptable on its GPIO.

PC817: 1 anode, 2 cathode, 3 emitter, 4 collector. Verify the exact manufacturer's package numbering.

## U1: Haier TX → ESP RX

```text
+5V Haier -- R1 680 ohm -- U1 pin1 (A)
TX Haier --------------- U1 pin2 (K)

+3.3V ESP -- R2 2.2 kohm --+-- RX ESP
                          |
                        U1 pin4 (C)
GND ESP ---------------- U1 pin3 (E)
```

## U2: ESP TX → Haier RX

```text
+3.3V ESP -- R3 390 ohm -- U2 pin1 (A)
TX ESP ----------------- U2 pin2 (K)

+5V Haier -- R4 4.7 kohm --+-- RX Haier
                          |
                        U2 pin4 (C)
GND Haier -------------- U2 pin3 (E)
```

At LOW, the transmitter sinks LED current and the optocoupler pulls RX low. At HIGH, the LED is off and RX rises through its pull-up. Both channels are non-inverting; firmware inversion is disabled. ESP GPIO must never connect to U2's 5 V output node.

Starting bench values give about 5 mA LED current at LOW (dependent on VF/VOL). U1 collector load is about 1.5 mA, U2 about 1 mA, excluding any internal Haier pull-up. Haier TX must sink this current; its load rating/internal circuit was not established. Verify LOW/HIGH and waveforms, particularly with high CTR and PC817 saturation. Do not leave the old divider/Zener in parallel with the optocoupler output.

## Bench before Haier

Disconnect both Haier lines. Test each PC817 separately with the UART loopback test: GPIO17 drives the LED from 3.3 V through 390 Ω; collector to GPIO18 with 2.2 kΩ pull-up to 3.3 V, emitter to GND. Remove the direct 17–18 jumper. This tests both optocouplers under identical 3.3 V conditions, not Haier TX or the final 5 V U1/U2 conditions.

Do not loop the adapter output into its input without a load calculation: U2's collector would sink U1 LED current plus its own pull-up current, more than a normal RX load.

After individual PASS results, test at operating voltages using an appropriate 5 V driver and measurements, then Haier. Prefer a scope/analyzer for edges. PC817 is not guaranteed for an arbitrary UART9600 circuit: typical 4/3 µs, maximum 18 µs timing is specified at VCE 2 V, IC 2 mA, RL 100 Ω, not these pull-ups. A local PASS does not guarantee Haier communication.

S3: TX GPIO17, RX GPIO18. Historical ESP8266 project: TX GPIO5(D1), RX GPIO4(D2).

Source: [Sharp PC817XxNSZ1B pinout/CTR/timing](https://global.sharp/products/device/lineup/data/pdf/datasheet/PC817XxNSZ1B_e.pdf).

## Arduino bench instead of the AC

The [Nano ATmega328P hOn simulator](../bench/README.md) tests both channels at 5/3.3 V with the main ESP firmware. Nano D11 replaces Haier TX, D10 replaces Haier RX. Each LED circuit has its own MCU driver; this is not an optocoupler loop. NEC PS2561-1 is an alternative after pinout verification. Communication on this bench was confirmed. Slow edges limit applicability; see [HARDWARE.md](HARDWARE.md) for results and tested alternatives.
