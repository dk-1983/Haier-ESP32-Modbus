[English](VALIDATION-1.1.0.md) | [Русский](VALIDATION-1.1.0_RU.md)

# v1.1.0 acceptance checks

Date: 2026-09-22. Target: ESP32-S3-WROOM-1 N16R8, connected to the operating Haier air conditioner.

- Clean CI build from `dc116e95495efe7cf3f51ecd1423d5d11973282a`: native MQTT/Modbus/credential/version tests, 50,000 malformed Modbus PDUs, Discovery validation, 2,000 upstream hOn wire/CRC cases, firmware build and binary packaging passed.
- Personal web password changed and old password rejected; independent OTA password changed and successfully used for an actual ArduinoOTA upload. Passwords and the disabled update policy persisted across restarts/uploads. Original web access was restored.
- Invalid CSRF token and invalid password/policy values were rejected.
- GitHub manifest with an invalid signature was received over verified HTTPS (HTTP 200) and rejected without changing the running image. Installation with the policy disabled returned HTTP 409.
- The signed public v1.1.0 image was downloaded from GitHub release assets, verified, written to the inactive slot and booted. Partition changed from `app1` to `app0`; the application's 45-second boot confirmation completed. Running image MD5: `7a5f89f5bfc5a46a7161e83c678b25c8`; release SHA-256: `fd8c58f50ce3ce56cff01b8c6de235e2cb5f37d1c29d87e6643fa8dcc16e812b`.
- Cooling remained ON, COOL 22 °C, fan AUTO, swing OFF, preset NONE, quiet OFF, display ON; both direction settings CENTER. Fresh hOn responses remained available.
- Factory/application consistency and every packaged SHA-256 were checked. The public image contains none of the local migration password values.

Limits: clean first-boot provisioning on an erased physical board and forced rollback after a bad candidate were not exercised on this operating controller. Password validation has native tests, but those do not replace a full first-boot hardware test. The existing bootloader's early-crash rollback limitation remains; see [management](MANAGEMENT.md). These checks do not imply zero possible faults.
