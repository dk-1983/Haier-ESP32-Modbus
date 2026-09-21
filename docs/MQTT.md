[English](MQTT.md) | [Русский](MQTT_RU.md)

<a id="mqtt-клиент-через-wi-fi"></a>

# MQTT client over Wi-Fi

The controller connects to an external broker using MQTT 3.1.1 TCP (default port 1883). There is no built-in broker. This initial implementation targets trusted local networks; TLS is not enabled. Home Assistant Discovery is available since 0.6.0.

<a id="настройка"></a>

## Setup

Open `/mqtt` and authenticate with `admin` and the control password. Enter the broker host (IPv4 or DNS, without `mqtt://`), port, username/password and a unique topic prefix. An empty username selects a connection without a username. The default prefix matches the device hostname.

MQTT defaults to disabled and is independent of RTU/TCP. Settings are stored in NVS. An empty new-password field keeps the current password; a separate checkbox clears it. GET `/mqtt/config` returns only `password_set`, never the password. Configuration changes apply in the background; another write during application returns HTTP409.

<a id="топики"></a>

## Topics

`BASE` is the selected prefix.

| Topic | Direction | Content |
|---|---|---|
| `BASE/state` | Controller → broker | JSON matching `/haier/status`, excluding `last_status_hex`, every 5 seconds while connected |
| `BASE/availability` | Controller → broker | `online` / `offline`, QoS1 retained; offline is also the Last Will |
| `BASE/result` | Controller → broker | JSON: accepted / confirmed / timeout_unconfirmed / rejected |
| `BASE/set/<field>` | Broker → controller | One exact value from the following table, without JSON |

`availability` describes controller-to-broker connectivity. For AC connectivity, check `state.available` and `age_ms`. State/result use QoS0 without retain, so a new subscriber is not given old state as fresh. Normal state interval is 5 seconds. State publication pauses while confirmation is pending and resumes after confirmation or timeout.

| field | Values (case-sensitive) |
|---|---|
| power | OFF, ON |
| target | Integer 16..30 |
| mode | COOL, HEAT, DRY, FAN_ONLY, AUTO |
| hvac_mode | OFF, COOL, HEAT, DRY, FAN_ONLY, AUTO |
| fan | LOW, MEDIUM, HIGH, AUTO |
| lock | UNLOCK, LOCK |
| swing | OFF, VERTICAL, HORIZONTAL, BOTH |
| preset | NONE, BOOST, SLEEP |
| quiet | OFF, ON |
| display | OFF, ON |
| vertical_position | HEALTH_UP, MAX_UP, HEALTH_DOWN, UP, CENTER, DOWN |
| horizontal_position | CENTER, MAX_LEFT, LEFT, RIGHT, MAX_RIGHT |

Commands share the Modbus validator and arbiter. Mode is independent of power: writing mode does not turn on a powered-off AC. Combination constraints are in [REGISTERS.md](REGISTERS.md). Until a command completes, further writes from any interface are rejected as busy.

`accepted` means accepted, not executed. An accepted MQTT command receives a `request_id` such as `mqtt-N`; two subsequent matching status packets produce `confirmed`. Timeout is 30 seconds. Rejections include field, error (Modbus-compatible codes) and reason. Network loss may lose a result message; check actual state, request_id and command_state.

<a id="пример"></a>

## Example

```sh
mosquitto_sub -h BROKER -t 'BASE/state' -t 'BASE/result' -t 'BASE/availability'
mosquitto_pub -h BROKER -t 'BASE/set/target' -m '22' -q 0
```

Add broker authentication as needed. Do not use `-r` for commands. The controller subscribes at QoS0 with clean session; offline commands do not accumulate. Retained packets delivered on subscription and DUP packets are rejected. MQTT 3.1.1 does not let the subscriber detect the publisher's original retain flag during ordinary live forwarding: every sender must follow the no-retained-commands rule.

<a id="работа-при-сбоях"></a>

## Failure handling

ESP-MQTT from the pinned ESP-IDF performs network operations in background tasks. The main loop does not call connect/subscribe/publish/stop and continues UART, web and Modbus processing if the broker is unavailable. Reconnect is approximately every 5 seconds; keepalive is 30 seconds. Subscriptions and state publication resume on reconnect.

Queues are bounded: 8 incoming commands and 4 publications; command payload up to 95 bytes and topic up to 127 bytes. Queued commands older than 1 second are rejected. Overflow can lose messages; `/mqtt/config` exposes a dropped counter. This controls device state; it is not a guaranteed job queue.

Orderly shutdown attempts to publish offline and await acknowledgement. On connection loss, the broker publishes the Last Will after detecting failure/keepalive expiry. An already accepted hOn command completes independently of MQTT.

<a id="проверка"></a>

## Validation

A real Haier with ESP32-S3 confirmed 41 commands: setpoint, every fan speed, lock, display, Quiet, presets, swing/fixed positions, modes while off, OFF/ON and HVAC COOL. Each received confirmed after two matching hOn packets. A retained command delivered on connection was rejected; five Discovery configurations received PUBACK.

The 1.0.0 fix uses the same ESPHome clock for reception timestamps and command age. Mixing Arduino millis and ESPHome millis caused false expired_command errors. The entire series passed after the fix.

## Home Assistant (0.6.0)

`/mqtt` has a separate HA discovery checkbox. On connection, the controller publishes five retained QoS1 configurations, sequentially awaiting PUBACK: climate, quiet, display, vertical_position, horizontal_position. Reconnect and a live `homeassistant/status=online` repeat the cycle. `/mqtt/config.discovery_sent` counts acknowledged configurations (0..5).

Discovery defaults to enabled; MQTT still defaults to disabled. Existing NVS settings are preserved using a previously reserved zero byte. Disabling Discovery stops publication but does not remove broker-retained configurations or HA entities.

HA climate uses `hvac_mode`: OFF turns power off; other values atomically select mode and turn power on. FAN_ONLY also selects LOW because AUTO fan is invalid in that mode. The older `mode` topic preserves power independence. `power=ON` retains the previously observed mode.

Discovery presets list only boost/sleep; HA adds none. Missing mode/fan/swing fields yield `None` in templates. Quiet/display use optimistic HA indication; actual state arrives after command completion. Other entities are not optimistic. Old state in the worker's own queue is discarded by command generation; messages already sent over the network cannot be recalled.

JSON, Jinja templates, atomic command conversion and delivery of all five configurations with PUBACK from a separate broker were tested. Home Assistant's UI with this S3 was not separately tested; broker validation does not replace HA UI validation.

On 1.0.0, MQTT recovery after a diagnostic Wi-Fi disconnect without restarting ESP and redelivery of five Discovery configurations were also verified. Broker password authentication and forced broker restart were not separately tested in this series.
