[English](FLASHING.md) | [Русский](FLASHING_RU.md)

# Install the firmware, step by step

**Release v1.0.0 contains source code only. There is no ready-to-flash `.bin` in its Assets.** “Source code (zip)” is an archive to extract and build, not a file to upload to the ESP. Setup and web/OTA passwords are compiled into this version, so build with your own values. The procedure below starts from the release and ends with a working controller.

## 1. Download the release and prepare Windows

Use an **ESP32-S3-WROOM-1 N16R8** (16 MB Flash, 8 MB octal PSRAM) and a USB-UART adapter with **3.3 V logic**. This firmware is not for ESP8266 or the original Haier Wi-Fi module.

1. Install **Python 3.11** with the Python launcher and **Git**; reopen PowerShell after installation.
2. Open [release v1.0.0](https://github.com/dk-1983/Haier-ESP32-Modbus/releases/tag/v1.0.0), expand **Assets**, download **Source code (zip)** and extract it.
3. Open PowerShell in the extracted folder containing `haier-s3.yaml` and `Build.ps1`. Check `py -3.11 --version` and `git --version`.

## 2. Set your passwords

```powershell
Copy-Item secrets.example.yaml secrets.yaml
notepad secrets.yaml
```

Replace both example values, keeping the YAML quotes:

- `setup_password`: password for the controller's setup Wi-Fi network; use 8–63 ASCII characters.
- `ota_password`: password for the web user `admin` and ArduinoOTA; use a strong personal password.

Save the file. Home Wi-Fi credentials are entered after flashing, through the setup page. Keep `secrets.yaml` and your compiled images private.

## 3. Build the firmware

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build.ps1
```

The script creates its Python environment and downloads the pinned compiler dependencies; the first build needs Internet access and can take a while. Wait for successful compilation. A build error must be resolved before continuing; building alone does not flash the board.

With the current configuration, the output directory is `.esphome/work/build/.pioenvs/haier-s3/`:

| File | Use |
|---|---|
| `firmware.factory.bin` | First USB-UART installation: merged bootloader, partition table and application; write at **0x0** |
| `firmware.bin` | Application image for **ArduinoOTA** on an already installed controller |

Do not write the application-only `firmware.bin` at 0x0. If the build path is changed, use the paths printed by the build.

## 4. Connect the programmer

For first installation, work on the bench with the controller disconnected from the air conditioner. Wire with power off; use a suitable board power supply. A bare module needs regulated 3.3 V, not 5 V, and a small adapter's 3.3 V output may not supply enough current.

| USB-UART adapter | ESP32-S3 |
|---|---|
| TX, 3.3 V logic | GPIO44 / RXD0 |
| RX | GPIO43 / TXD0 |
| GND | GND |

GPIO17/18 are the air conditioner UART, **not** the programming port. Follow the [power and BOOT schematic](SCHEMATIC.md). Find the adapter's COM number under **Device Manager → Ports (COM & LPT)**; install its manufacturer's driver if necessary. Close serial monitors that hold the port open.

## 5. Enter download mode

For the manually controlled prototype: remove power → hold **PGM/BOOT** (GPIO0 to GND) → apply power → release PGM. On a development board, holding BOOT while pressing/releasing RESET usually serves the same purpose. GPIO0 must be low at reset; EN must return high.

## 6. Write the factory image

Install esptool in the build environment, then replace **COM6** below with your port:

```powershell
.\work\tools\Scripts\python.exe -m pip install "esptool>=5,<6"
.\work\tools\Scripts\python.exe -m esptool --chip esp32s3 --port COM6 --baud 460800 --before no-reset --after no-reset write-flash 0x0 .esphome/work/build/.pioenvs/haier-s3/firmware.factory.bin
```

Wait for writing and hash verification to complete successfully. If connection fails, check the port, crossed TX/RX, common GND and download mode. If writing fails at high speed, repeat at `--baud 115200`. A full erase is not part of this procedure and would remove saved settings.

The command deliberately leaves the module in download mode: **restart power without holding PGM/BOOT** to launch the firmware.

## 7. Configure Wi-Fi and open the controls

1. Join **`haier-s3-<suffix>-setup`** using your `setup_password`.
2. Open **http://192.168.4.1**, select your **2.4 GHz** Wi-Fi and save the credentials.
3. Return your computer/phone to your normal network. Find the controller's assigned IP in the router's DHCP list.
4. Open `http://DEVICE_IP/control`; log in as **admin** with your `ota_password`.
5. Power off before attaching the Haier UART according to the [schematic](SCHEMATIC.md), including RX level conversion. After restart, check fresh air conditioner telemetry.
6. Configure [MQTT / Home Assistant](MQTT.md) or [Modbus](SYSTEM.md). MQTT and Modbus transports start disabled.

## 8. Later updates over Wi-Fi

This project uses **ArduinoOTA on port 8266**, not ESPHome's native OTA protocol. After building the new version with your chosen passwords, use the `espota.py` supplied by the Arduino framework downloaded by `Build.ps1`:

```powershell
.\work\tools\Scripts\python.exe work/platformio/packages/framework-arduinoespressif32/tools/espota.py -i DEVICE_IP -p 8266 -a "CURRENT_OTA_PASSWORD" -f .esphome/work/build/.pioenvs/haier-s3/firmware.bin
```

Replace both placeholders. Authenticate with the password **currently installed**; after the update, the password compiled into the new image applies. Use the same local network and allow the uploader through the computer's firewall. Wait for completion and reboot, then check `/control`. Keep passwords out of shared screenshots and command logs. **Do not send `firmware.factory.bin` through OTA.**

References: [Espressif esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/) and [ESPHome command-line build tools](https://esphome.io/guides/cli/). Project-specific pins, paths and OTA port above follow this repository's configuration.
