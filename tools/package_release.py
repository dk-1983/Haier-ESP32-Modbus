"""Package only clean CI builds made with the declared public credentials."""
from pathlib import Path
import hashlib
import json
import os
import re
import posixpath
import subprocess

root = Path(__file__).resolve().parents[1]
if os.environ.get("GITHUB_ACTIONS") != "true":
    raise SystemExit("Public binaries must be produced in the clean release CI job.")
if (root/"secrets.yaml").read_bytes() != (root/"release.defaults.yaml").read_bytes():
    raise SystemExit("Refusing to package: credentials differ from public defaults.")
if subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root).strip():
    raise SystemExit("Refusing to package a modified tracked source tree.")
version = re.search(r'HAIER_FIRMWARE_VERSION "([0-9.]+)"', (root/"components/fourvrs_portal/version.h").read_text())[1]
build = root/".esphome/work/build/.pioenvs/haier-s3"
factory = (build/"firmware.factory.bin").read_bytes()
ota = (build/"firmware.bin").read_bytes()
assert factory[0] == 0xE9 and ota[0] == 0xE9, "Invalid ESP image header"
assert len(ota) > 100000 and len(factory) > len(ota), "Unexpected image size"
# ESP32-S3 chip ID in the extended image header.
assert int.from_bytes(ota[12:14], "little") == 9, "Not an ESP32-S3 image"
assert (ota[3] >> 4) == 4, "Image header must specify 16 MB Flash"
app_offset = factory.find(ota)
assert app_offset >= 0x10000 and app_offset % 0x10000 == 0, "Merged image does not contain this application"
assert version.encode() in ota, "Version string missing"
generated = (root/".esphome/work/build/src/main.cpp").read_text()
assert "set_public_release(true)" in generated, "Not a public provisioning build"
assert "#define FOURVRS_TEST_FAIL_BOOT" not in (root/".esphome/work/build/src/esphome/core/defines.h").read_text(), "Boot failure injection enabled"
assert "#define HAIER_TEST_FEED" not in (root/".esphome/work/build/src/esphome/core/defines.h").read_text(), "Testing feed enabled"
for forbidden in (b"Haier-Admin-2026!", b"Haier-Setup-2026"):
    assert forbidden not in ota, "Obsolete shared passwords in public binary"
out = root/"work/public-release"
out.mkdir(parents=True, exist_ok=False)
prefix = f"haier-s3-n16r8-v{version}"
for suffix, data in (("factory.bin", factory), ("ota.bin", ota)):
    (out/f"{prefix}-{suffix}").write_bytes(data)
for source, target in (("FLASHING.md", "INSTALL-EN.md"), ("FLASHING_RU.md", "INSTALL-RU.md")):
    text = (root/"docs"/source).read_text(encoding="utf-8")
    def absolute_link(match):
        url = match[1]
        if ":" in url or url.startswith("#"):
            return match[0]
        path = posixpath.normpath("docs/" + url)
        return "](https://github.com/dk-1983/Haier-ESP32-Modbus/blob/v" + version + "/" + path + ")"
    text = re.sub(r"\]\(([^)]+)\)", absolute_link, text)
    (out/target).write_text(text, encoding="utf-8")
metadata = {
    "version": version,
    "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
    "target": "ESP32-S3-WROOM-1 N16R8", "flash_bytes": 16*1024*1024,
    "psram": "8 MB octal", "factory_write_offset": "0x0",
    "application_offset_in_factory": hex(app_offset),
    "ota_protocol": "ArduinoOTA", "ota_port": 8266,
    "credentials": "Personal passwords provisioned on first boot; retained in NVS",
    "public_provisioning": True,
    "personal_network_configuration": False,
    "esphome": "2026.6.5", "haier_protocol": "0.9.31",
    "ci_run": os.environ.get("GITHUB_RUN_ID"),
}
(out/"build-info.json").write_text(json.dumps(metadata, indent=2)+"\n", encoding="utf-8")
lines = [hashlib.sha256(p.read_bytes()).hexdigest()+"  "+p.name for p in sorted(out.iterdir())]
(out/"SHA256SUMS.txt").write_text("\n".join(lines)+"\n", encoding="ascii")
print(json.dumps(metadata, indent=2))
print("Factory/application consistency, target header, public credentials and SHA256: PASS")
