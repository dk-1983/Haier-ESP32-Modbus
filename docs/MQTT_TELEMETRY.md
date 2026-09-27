[English](MQTT_TELEMETRY.md) | [Русский](MQTT_TELEMETRY_RU.md)

# MQTT observation contract, schema 1

Added in v1.3.0. Hardware/HA validation status is recorded in the release notes. Existing climate IDs, state/command topics, registers and GPIO assignments are unchanged. The server-room controller has NOT been modified by this change.

## Purpose

Home Assistant MQTT may suppress unchanged climate updates. `climate.last_reported` is not proof of the last AC observation. A publication heartbeat must not rejuvenate an old UART sample or acknowledge a command. This contract separates actual observations from command confirmation.

## Transport and timing

`<prefix>/telemetry`: JSON, QoS 0, not retained. Sent on connection, after a new accepted hOn status (rate limited to one publication per 250 ms), or every 5 seconds without a new frame. Continues while a command is pending. No new UART requests are introduced. Worker-queue observations older than 250 ms are discarded, and telemetry is not stored in the MQTT outbox as QoS-0 offline backlog. Network/broker delivery latency is not bounded by this firmware.

`<prefix>/state` keeps existing behavior, including suppression during command confirmation. `~/availability` keeps its existing LWT behavior. Discovery now contains eight entities instead of five.

## Fields

The telemetry object contains the existing state fields (`available`, mode, target_temperature, current_temperature, fan, power, swing, preset, command_state, request_id, etc.) plus:

| Field | Meaning |
|---|---|
| schema | Integer 1. Reject unsupported versions. |
| boot_id | Random 32-character hex session identifier, regenerated at ESP startup; not a board serial or secret. |
| status_frames | Accepted UART status counter. A heartbeat does not advance it. |
| age_ms | UART observation age at serialization, or null before any observation. |
| sample_uptime_ms | 64-bit ESP monotonic milliseconds when the status was accepted; zero before any status. |
| published_uptime_ms | Same monotonic clock at serialization. Not Unix time; does not require NTP. |
| fault_valid | Fresh connected status AND complete sensor block. |
| fault | Boolean error_status != 0 when valid, otherwise null. |
| fault_code | Native hOn error_status byte, 0..255 when valid, otherwise null. |

`available` becomes false at 30 seconds without accepted status, or when protocol connection is invalid. Fault validity additionally requires the sensor block. A truncated status cannot clear an old fault. Nonzero code descriptions/YCJ equivalence are not claimed. An AC-reported fault is different from UART framing errors or missing communication.

## Automatically discovered entities

- Telemetry sensor: unique ID `<device_id>_telemetry`, state = status_frames, full payload in attributes, force_update=true, expires after 15 seconds without telemetry, diagnostic category.
- AC fault binary sensor: `<device_id>_fault`, device_class=problem, ON means fault. Attributes include boot_id, status_frames, age_ms and fault_code. Availability requires broker online and fault_valid. Expires after 15 seconds without messages.
- AC fault code sensor: `<device_id>_fault_code`, native numeric code, same fault validity and 15-second expiry.

Actual entity IDs are assigned by HA and can be renamed. Find them by device/unique ID. Do not hardcode `sensor.haier_s3_telemetry`. Fault OFF with unchanged attributes does not guarantee last_reported advances: use the shared telemetry observation contract, not the binary sensor timestamp.

## Handoff to server-room-climate-controller

Add an optional telemetry entity role to the integration configuration. Keep legacy behavior for unrelated climate integrations. Use one atomic telemetry snapshot for AC mode, target and fault; continue issuing commands through the existing climate entity. Telemetry temperatures are Celsius (unlike HA climate attributes, which may use system units).

On each genuinely received diagnostic update, validate schema, types, finite nonnegative times, sample_uptime_ms <= published_uptime_ms, non-restored state, available and matching device. Record HA monotonic receipt time and session/sample identity. Cache on actual update events, not on every one-second watchdog read. On startup require a live update; do not accept restored diagnostic attributes as a new reception. HA diagnostic force_update supports identical-value update delivery.

Compute source age from published_uptime_ms - sample_uptime_ms, then add time elapsed since receipt. This is a LOWER BOUND on real age because network latency is unknown; do not describe it as precise synchronized time. A conservative configured transport allowance can be added, but is not a guaranteed bound. Never refresh the sample timestamp merely because the same sample was published again. Ignore backwards publication times within a session; handle a new boot_id as a new session and discard previous confirmation context. Track receipt age separately; invalidate after 15 seconds without observations. Use 30 seconds as the maximum source age, in addition to available/fault_valid checks.

For stronger command confirmation, do not substitute a heartbeat or climate.last_reported for proof. Preserve post-command sample checks. The existing result topic reports accepted with a firmware-generated request_id and later confirmed/timeout_unconfirmed with that ID; telemetry carries request_id/command_state. Confirmed means two matching subsequent UART statuses. Subscribe before submitting a command, serialize writes and correlate accepted/result IDs; another interface may also write. This is NOT a new client-supplied correlation protocol. A sequence increase alone does not prove that a command caused the observation. If strict caller correlation is needed with concurrent external writers, extend the command protocol in a separate coordinated change.

The controller's default ac_readback_timeout=3 seconds is shorter than the firmware's 30-second confirmation window. Review it separately; 35 seconds is a starting proposal, not a measured worst-case guarantee. Do not confuse this timeout with ac_max_age=180 or minimum_ac_off=180. Do not disable compressor or ventilation interlocks.

Read fault from the SAME valid snapshot: false permits operation, true indicates fault, invalid/missing means unknown. Selecting the new binary sensor alone without updating the controller's freshness handling can reproduce the original stale-OFF problem.

## Acceptance checks before deployment

Software checks cover discovery JSON/templates, fault values 0/1/255, invalid fault availability, heartbeat/new-frame rate limits, 30-second cutoff and timer wrap. Full ESP build is required. Hardware/HA checks still required: stable readings >180 seconds, pending command, lost UART, lost MQTT, ESP/HA restart, partial sensor packet, and restored messages. Simulate nonzero fault offline; do not intentionally fault the operating server-room AC. Verify no cached heartbeat confirms a command. New fault semantics are not hardware-validated by a zero-code-only run.

## Validation performed

Local ESPHome/ESP32 build passed. `tools/test_host.py`, `tests/test_discovery.py`, `tools/test_hon_simulator.py` and `tools/check_docs.py` passed. Upstream compiler/linker warnings remain. No OTA, live HA configuration change or release publication was performed. The adapter described above is work for the server-room controller project.
