# Firmware baseline

Imported from `arm_preparation/esp32`. This is the existing LeNet inference
demo used to establish the build and QEMU baseline; it is not CoffeeNet or the
Coffee Pot Alarm implementation. The original `Ryk` names and demo behaviour
are retained. Host training code and sample datasets are not imported.

## Build and flash

Use ESP-IDF **6.1.0**, targeting ESP32-S3. Managed component versions are pinned
in `main/idf_component.yml` and resolved in `dependencies.lock`. Application
sources compile as C++20. The configuration selects 16 MB flash and 8 MB octal
PSRAM; confirm these settings against the physical module before flashing.

```bash
source /path/to/esp-idf/export.sh
cd firmware
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

The flash target writes `models/lenet_quantized.tflite` into `model_a` as well
as flashing the application. The existing partition table retains `model_a`
and `model_b` for the imported demo.

On physical hardware, the demo uses BLE Wi-Fi provisioning via the managed
`espressif/network_provisioning` component, replacing the SDK 5.x
`wifi_provisioning` API. It retains the original `ARM_PREP` service name and
demonstration provisioning parameters in `main/ryk_network.cpp`. Read the
assigned DHCP address from the serial log. Reconnection, onboarding changes,
and the new device API remain separate milestone tasks.

## Run in QEMU

With ESP-IDF active and Espressif's Xtensa QEMU already installed:

```bash
cd firmware
idf.py qemu monitor
```

ESP-IDF automatically loads the project-local `idf_ext.py`. It selects 8 MB
emulated PSRAM and forwards host `127.0.0.1:8081` to guest HTTP port 80,
checking for occupied HTTP and monitor ports before launching. ESP-IDF reads
the flash size and Octal PSRAM mode from `sdkconfig`; no SDK changes are needed.

Override defaults with `QEMU_RAM_SIZE=16M` or `QEMU_HTTP_PORT=8082` before
the command. Explicit `-m` or `-nic` options in `--qemu-extra-args` take
precedence over their corresponding defaults and environment variables.
The board uses 8 MB Octal PSRAM; other RAM sizes require a compatible model
and firmware configuration. The default SDK size of 32 MB can exhaust flash
mapping space and cause NVS initialisation to fail.

For debugging, run `idf.py qemu --gdb monitor`, then `idf.py gdb` from
`firmware` in another ESP-IDF terminal. The optional `run_qemu.sh` remains
a convenience wrapper that changes directory and invokes the same command.

```bash
curl --max-time 5 http://127.0.0.1:8081/
```

The imported HTTP root returns `Hello`. The existing demo also accepts
inference inputs at `POST /` and model uploads at `POST /model`. These are
prototype demo endpoints, not the planned authenticated device-control API.
The optional `ui/` files are the original standalone HTML/JavaScript client;
the firmware does not serve these files, and they are not a React application.

## QEMU startup compatibility

The same image chooses QEMU Ethernet or physical-board Wi-Fi at runtime:

- QEMU uses the OpenCores MAC and SDK 6.1's generic IEEE 802.3 PHY driver
  for the emulated PHY; the old SDK 5.x DP83848 factory is no longer built in.
- `main/Qemu.h` checks Espressif's `QEMU` signature at
  `SYSCON_DATE_REG - 4`. Both startup code and networking use this helper.
- `bootloader_components/qemu_startup` supplies `bootloader_after_init`.
  In QEMU only, it disables SPI1 CS1 before bootloader flash writes, avoiding
  access to the uninitialised octal PSRAM model.
- `main/PatchQemu.cmake` builds a local copy of the SDK ADC source with a
  runtime guard around startup calibration, which QEMU does not support.
  Physical hardware keeps normal calibration. The SDK checkout is unchanged.
  The patch fails configuration if its expected constructor no longer matches;
  review it when changing SDK versions.

QEMU verifies emulated boot/network/application behaviour. Physical Wi-Fi,
board memory, and peripheral operation still require tests on the real board.

## Import validation

Validated with ESP-IDF 6.1 and Espressif QEMU 9.2.2
(`esp_develop_9.2.2_20260417`): full build, retained bootloader hook, 8 MB
emulated PSRAM, automatic Ethernet/DHCP, `GET /` returning `Hello`, and
`POST /` with 784 zero-valued pixels returning ten finite outputs. The HTTP
root also responds after inference. The launcher passes `bash -n`.

The project-local defaults were also validated with plain
`idf.py qemu monitor`: a 16 MB flash image, 8 MB Octal PSRAM, successful
NVS initialisation, Ethernet DHCP, and HTTP `200 Hello`. Checks cover
environment/explicit argument precedence, invalid configuration, and occupied
or duplicate host ports. No shell launcher is required.

QEMU logs RTC/MSPI calibration and unsupported multicast-filter messages;
the IPv4 HTTP smoke checks above pass despite these emulator limitations.
Physical-board boot and BLE/Wi-Fi provisioning have not been validated here.
