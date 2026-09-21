[English](REGISTERS.md) | [Русский](REGISTERS_RU.md)

<a id="карта-регистров"></a>

# Register map

The map is identical for RTU and TCP. All addresses are **zero-based**. Coil0 and Holding0 are different tables. Clients may display Holding0 as 40001 and Input0 as 30001. Factory fields retain their addresses; extensions follow them.

| Table | Address | Purpose | Access | Values | Origin |
|---|---:|---|---|---|---|
| coil | 0 | power | RW | 0=off;1=on | YCJ-A002 |
| coil | 1 | quiet | RW | 0=off;1=on | extension |
| coil | 2 | display | RW | 0=off;1=on | extension |
| holding | 0 | setpoint | RW | 16..30 Celsius integer | YCJ-A002 |
| holding | 1 | mode | RW | 1=cool;2=heat;3=dry;4=fan;5=auto | YCJ-A002 |
| holding | 2 | fan | RW | 1=low;2=medium;3=high;4=auto | YCJ-A002 |
| holding | 3 | control_lock | RW | 1/2/3=unlock (read1);4=lock | YCJ-A002 |
| holding | 4 | swing | RW | 0=off/center;1=vertical;2=horizontal;3=both | extension |
| holding | 5 | preset | RW | 0=none;1=boost;2=sleep | extension |
| holding | 6 | vertical_position | RW | write1=health_up;2=max_up;3=health_down;4=up;6=center;8=down;read12/14=auto;10=max_down | extension |
| holding | 7 | horizontal_position | RW | write0=center;3=max_left;4=left;5=right;6=max_right;read7=auto | extension |
| input | 0 | room_temperature | R | Celsius integer;floor half-degree | YCJ-A002 |
| input | 1 | fault_code | R | 0=none;native hOn error_status for nonzero;YCJ equivalence unverified | YCJ-A002 |
| input | 2 | machine_number | R | always0 | YCJ-A002 |
| input | 3 | room_temperature_x10 | R | Celsius x10 | extension |
| input | 4 | status_age_seconds | R | 0..65535;saturates;65535=never received | extension |
| input | 5 | status_count_low | R | low16bits | extension |
| input | 6 | status_count_high | R | high16bits | extension |
| input | 7 | command_status | R | 0=idle;1=pending;2=confirmed;3=timeout | extension |
| input | 8 | link_flags | R | bit0=fresh_hOn;bit1=RTU_enabled;bit2=TCP_enabled | extension |

<a id="функции"></a>

## Functions

- FC01: read coils; FC05/FC15(0x0F): write one/multiple coils.
- FC03: read holding registers; FC06/FC16(0x10): write one/multiple holding registers.
- FC04: read input registers.
- RTU CRC16 low byte first; register data big-endian. TCP uses MBAP, not RTU-over-TCP, port 502. Unit ID 1..247, default 1. RTU broadcast 0 accepts writes without replying; TCP Unit ID must match.

<a id="подтверждение-и-ошибки"></a>

## Confirmation and errors

A positive write response means **accepted** for hOn transmission. Execution requires two subsequent matching status packets: Input7=2. While Input7=1, further HTTP/RTU/TCP writes are rejected. FC16/FC15 validate the entire batch before changing anything. Reads always reflect received telemetry, without optimistic substitution. After 30 seconds without fresh status, main reads return exception 0x0B; diagnostic Input4..8 remain readable. Writes require status no older than 10 seconds; confirmation timeout is 30 seconds.

Exceptions: 01 unsupported function; 02 illegal address/span; 03 invalid value/quantity/combination; 04 unrepresentable state; 06 busy; 0B no fresh Haier response. Bad CRC, another RTU address and incomplete frames do not issue commands. Disabling a transport discards its incomplete incoming frames; an accepted hOn command still completes.

<a id="совместимость-и-ограничения"></a>

## Compatibility and limitations

- Power and mode are independent: Holding1 does not power on an off unit. PowerON uses the observed Haier mode.
- FAN_ONLY does not allow AUTO fan; explicit writes of that combination are rejected. Entering FAN_ONLY with an existing AUTO fan selects LOW.
- Holding3=2/3 normalizes to unlock (read1), matching the factory table. It uses hOn `lock_remote`; equivalence of all factory-lock effects needs bench verification.
- Input1 uses the native `error_status` byte. Nonzero codes are not claimed to match YCJ-A002 without a separate mapping. Input2 returns 0 as in the factory map.
- Input0 floors half-degrees to 1 °C resolution; Input3 preserves 0.5 °C steps at scale x10. The factory manual does not specify rounding.
- Holding4=OFF centers both axes. Holding6/7 sets a fixed position and stops swing on that axis. Writing Holding4 together with 6/7 is rejected as ambiguous.
- QUIET ON is invalid in OFF/FAN_ONLY/BOOST. BOOST/SLEEP cannot be enabled in OFF/FAN_ONLY; BOOST is rejected while QUIET is ON.
- Extensions are not part of factory YCJ-A002. Cleaning, sterilization and other undocumented features are not claimed.

<a id="пример-клиента"></a>

## Client example

```python
from pymodbus.client import ModbusTcpClient
with ModbusTcpClient("DEVICE_IP", port=502) as client:
    # pymodbus3.11 API; unit1
    print(client.read_holding_registers(0, count=4, device_id=1))
    print(client.write_register(0, 22, device_id=1))
    # Poll input7 for confirmation, then read holding0 for actual setpoint.
```

RTU uses the same addresses; select 19200, 8N1, address 1 or the settings saved at `/modbus`.

<a id="home-assistant-и-moxa"></a>

## Home Assistant and Moxa

A ready-made **4VRS Haier-ESP32** profile is available in [Modbus Devices 1.3.0 and later](https://github.com/dk-1983/Modbus_Devices#4vrs). It uses base and extension registers for climate, louvres, presets, quiet, display and diagnostics. The separate **Haier YCJ-A002** profile remains for the factory adapter. [Connect this controller](../README.md#ready-made-home-assistant-integration).

| Connection | Client transport | Gateway settings |
|---|---|---|
| Home Assistant → ESP over Wi-Fi | Modbus TCP/IP | Enable ESP TCP; port 502 and configured Unit ID |
| Home Assistant → Moxa → ESP RS-485 | Modbus TCP/IP | Select Modbus TCP in [4VRS Gateway](https://github.com/dk-1983/moxa-4vrs-gateway); enable ESP RTU |
| Home Assistant → Moxa RAW → ESP RS-485 | Modbus RTU over TCP | Select RAW TCP; client sends RTU frames with CRC and no MBAP |

The physical bench used Moxa Modbus TCP → RTU. RAW TCP is a distinct transport supported by the related projects, with different framing. Moxa and ESP serial settings must match: documented bench **19200, 8N1, Unit ID 1**. Moxa's network port is configurable and may differ from ESP's fixed port 502.

Links: [Modbus Devices documentation](https://github.com/dk-1983/Modbus_Devices#readme), [Moxa / 4VRS Gateway guide](https://github.com/dk-1983/moxa-4vrs-gateway/blob/main/docs/user-guide.md).
