# Software on the image

Everything listed in `overlay/etc/apk/world`, plus its dependencies, is installed into RAM at every boot from the signed local repo on the card (`custom/`, marked `.boot_repository`). No network is needed to boot. Versions are from Alpine 3.23 aarch64 as of 2026-10-27.

| Package | Version | Why |
|---|---|---|
| alpine-base, openrc | 3.23.7, 0.63 | Base system and init |
| dropbear | 2025.88 | SSH server, key-only logins |
| espnow-nexmon | 0.1.0 (ours) | Nexmon brcmfmac, Nexmon firmware, the `espnow.ko` driver and the `espnow` service. See [espnow.md](espnow.md) |
| iw | 6.17 | Creates `mon0` and sets the channel |
| python3, py3-pip | 3.12.16, 25.1.1 | Python |
| gcc, musl-dev, make, linux-headers | 15.2.0, 1.2.5, 4.4.1, 6.16.12 | C toolchain (`linux-headers` are the userspace UAPI headers) |
| rust, cargo | 1.91.1 | Rust toolchain |
| libgpiod, libgpiod-dev, py3-libgpiod | 2.2.2 | GPIO through the character device (`gpioinfo`, `gpioset`, the C API, Python `gpiod`) |
| i2c-tools, i2c-tools-dev, py3-smbus | 4.4 | `i2cdetect`/`i2cget`/…, the C `libi2c`, Python `smbus` |
| py3-spidev | 3.8 (ours) | Python SPI. Alpine 3.23 doesn't package it, so it's built from PyPI in `packages/py3-spidev` |
| picocom | 2024.07 | Serial terminal for the UARTs |

## Per language

- **Python:** `import gpiod`, `import smbus`, `import spidev`. For a UART, use the `py3-pyserial` package or plain `open('/dev/ttyAMA0')`.
- **C:** `#include <gpiod.h>` (link with `-lgpiod`), `<linux/i2c-dev.h>` with `<i2c/smbus.h>` (`-li2c`), `<linux/spi/spidev.h>`, and `termios` for UARTs.
- **Rust:** add the [`rppal`](https://crates.io/crates/rppal) crate (GPIO, I2C, SPI, PWM and UART, with Pi 5 support) or `gpiocdev`, `i2cdev` and `spidev` to your project. Crates are downloaded by cargo per project, so the device needs network the first time.
- **ESP-NOW:** every language uses a raw `AF_PACKET` socket on `espnow0`, so no library is needed. See [espnow.md](espnow.md).

## RAM use

The root filesystem is a tmpfs. The toolchains (rust and gcc especially) take roughly 1–1.5 GB of RAM once installed, and unpacking them adds to boot time. A 4 GB or larger Pi 5 is recommended. On a 2 GB board, remove `rust`, `cargo` and `gcc` from `world`, or install them on demand with `apk add`.

Anything you install or change at runtime is lost on reboot unless you run `lbu commit` (saves `/etc` changes to the apkovl) and, for packages, rebuild the image with the package added to `overlay/etc/apk/world`.
