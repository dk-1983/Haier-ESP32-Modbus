[English](FLASHING.md) | [Русский](FLASHING_RU.md)

# Install the firmware, step by step

## Install the ready-made v1.1.0 binary (Windows)

No compiler, Git or ESPHome installation is needed for this path. Use **ESP32-S3-WROOM-1 N16R8 only**, with 16 MB Flash and 8 MB octal PSRAM.

1. Open [release v1.1.0](https://github.com/dk-1983/Haier-ESP32-Modbus/releases/tag/v1.1.0) → **Assets**. For first installation download **`haier-s3-n16r8-v1.1.0-factory.bin`** and `SHA256SUMS.txt`. `Source code` is optional. The **`-ota.bin`** file is only for network updates of an already installed controller.
2. Install Python 3.11 with its launcher. Open PowerShell in the download folder and run `py -3.11 -m pip install "esptool>=5,<6"`. Check the download with `Get-FileHash .\haier-s3-n16r8-v1.1.0-factory.bin -Algorithm SHA256`; compare the hash with that filename's entry in `SHA256SUMS.txt`.
3. Disconnect the controller from the air conditioner and wire on the bench with power off: **adapter TX (3.3 V logic) → GPIO44, adapter RX ← GPIO43, GND ↔ GND**. Supply the board correctly; a bare module takes regulated **3.3 V**, never 5 V. GPIO17/18 are for Haier, not flashing. See [schematic](SCHEMATIC.md).
4. Find the adapter COM port in Device Manager and close serial monitors. Remove board power → hold **PGM/BOOT** (GPIO0 to GND) → apply power → release PGM. On a development board, hold BOOT while pressing/releasing RESET.
5. Replace **COM6** with your port and write the merged image at **0x0**:

```powershell
py -3.11 -m esptool --chip esp32s3 --port COM6 --baud 460800 --before no-reset --after no-reset write-flash 0x0 haier-s3-n16r8-v1.1.0-factory.bin
```

6. Wait for successful writing and hash verification. If high-speed writing fails, retry at `--baud 115200`. **Restart power without holding PGM**: the command leaves the module in download mode. Full flash erasure is not required and would delete saved settings.
7. On a new device, join **`haier-s3-<suffix>-setup`**, password **`Haier-Setup`**. Open **http://192.168.4.1** and set all three personal passwords: web, OTA and setup AP. After the restart, reconnect using your new AP password, open `/wifi`, choose your 2.4 GHz Wi-Fi and save. Return to your normal network and find the ESP address in the router's DHCP list.
8. Open `http://DEVICE_IP/control`: username **`admin`**, your personal **web password**. Power off before connecting the Haier UART with RX level conversion; then check telemetry and configure [MQTT](MQTT.md) or [Modbus](SYSTEM.md).

Passwords are saved in NVS and survive public OTA and Wi-Fi reset. Use `/settings` to change web, OTA or setup AP passwords independently. If you forget the web password, serial recovery is required; ordinary OTA does not override saved credentials. Upgrading an old v1.0.0 personal installation requires a one-time private v1.1.0 build first to migrate its compiled credentials; then the public OTA image can be used. [Details](MANAGEMENT.md).

### Updating an installed controller with the release OTA file

Download `haier-s3-n16r8-v1.1.0-ota.bin`. Use Arduino's [espota.py from Arduino-ESP32 3.3.9](https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.9/tools/espota.py) (save it as `espota.py` in the same folder), or the copy installed by a previous source build:

```powershell
py -3.11 espota.py -i DEVICE_IP -p 8266 -a "CURRENT_OTA_PASSWORD" -f haier-s3-n16r8-v1.1.0-ota.bin
```

Use the password currently on the device for uploading. The saved personal passwords remain unchanged after restart. Allow the uploader through the local firewall. Never upload the factory file through OTA. This is **ArduinoOTA**, not ESPHome native OTA or browser upload.

## Alternative: build with personal passwords

The following steps are for a custom build. v1.0.0 remains source-only; v1.1.0 offers both source and ready-made binaries.

## 1. Download the release and prepare Windows

Use an **ESP32-S3-WROOM-1 N16R8** (16 MB Flash, 8 MB octal PSRAM) and a USB-UART adapter with **3.3 V logic**. This firmware is not for ESP8266 or the original Haier Wi-Fi module.

1. Install **Python 3.11** with the Python launcher and **Git**; reopen PowerShell after installation.
2. Open [release v1.1.0](https://github.com/dk-1983/Haier-ESP32-Modbus/releases/tag/v1.1.0), expand **Assets**, download **Source code (zip)** and extract it.
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

Replace both placeholders. Authenticate with the password **currently installed**; saved NVS passwords take precedence over compiled values after the update. Use the same local network and allow the uploader through the computer's firewall. Wait for completion and reboot, then check `/control`. Keep passwords out of shared screenshots and command logs. **Do not send `firmware.factory.bin` through OTA.**

References: [Espressif esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/) and [ESPHome command-line build tools](https://esphome.io/guides/cli/). Project-specific pins, paths and OTA port above follow this repository's configuration.
