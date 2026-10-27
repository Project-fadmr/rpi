# Alpine Pi 5 image with ESP-NOW

A diskless (RAM-root) Alpine Linux 3.23 image for the Raspberry Pi 5, tuned for low power:
- ESP-NOW **receive** on the onboard Wi-Fi, through Nexmon monitor mode and a custom kernel driver that provides `espnow0`
- Python, C and Rust toolchains
- GPIO, I2C, SPI and UART enabled

## Build (no sudo; only Docker)

    ./build.sh binfmt   # once per host boot: registers arm64 emulation
    ./build.sh          # → out/alpine-espnow-pi5.img

- **SSH key:** if `~/.ssh/id_ed25519.pub` exists, it's added for root. Set `SSH_PUBKEY_FILE=…` to use another key.
- **Build stages:**
  1. `scripts/build-packages.sh` runs in an arm64 container. It builds `packages/*` into a signed repo, generating `keys/espnow.rsa` the first time; keep that key.
  2. `scripts/make-image.sh` assembles the FAT32 image from the official Alpine tarball, our repo, a pre-filled apk cache and the apkovl.

## Flash and boot

    dd if=out/alpine-espnow-pi5.img of=/dev/sdX bs=4M conv=fsync   # SD card or USB drive; or Raspberry Pi Imager → custom image

- **Console:** the debug UART (3-pin JST between the HDMI ports, 115200 baud). Log in as root; there's no password on the serial console. Set one with `passwd` followed by `lbu commit`.
- **Network:** `eth0` gets DHCP. SSH is key-only.
- **HDMI is off on purpose,** for power.
- **Keep changes:** `lbu commit`. Without it, everything resets on reboot.
- **Once:** apply the EEPROM power setting from [docs/config.md](docs/config.md#one-time-eeprom-step-manual).

## ESP-NOW quick test

ESP-NOW on the Pi is **receive-only** (why: [docs/espnow.md](docs/espnow.md)). Flash the test sender onto a USB-connected ESP32 (Docker, no sudo; the board defaults to `esp32c3`):

    scripts/flash-esp32.sh [/dev/ttyACM0] [esp32c3|esp32|esp32s3|esp32c6]

Its `CHANNEL` must match `/etc/conf.d/espnow` (default 1). Then on the Pi:

    ./espnow_rx.py 30     # script is in /root; prints frames and the delivery rate

## Docs

- [docs/espnow.md](docs/espnow.md): frame format, the `espnow0` API for Python/C/Rust, channel, limitations
- [docs/config.md](docs/config.md): every boot, overlay and service setting, plus power tuning
- [docs/software.md](docs/software.md): installed packages and versions
- [docs/hardware.md](docs/hardware.md): pin map and device nodes
- [docs/pins.md](docs/pins.md): pinned versions, hashes and commits, and how to upgrade
