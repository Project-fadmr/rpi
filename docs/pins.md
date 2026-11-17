# Pins

Every external input the build depends on. When you change one, change it in the file listed and record the date here.

| What | Value | Where it's set | Pinned |
|---|---|---|---|
| Alpine release | 3.23.8 (branch `v3.23`) | `build.sh` | 2026-11-11 |
| Alpine RPi tarball | `alpine-rpi-3.23.8-aarch64.tar.gz`, sha256 `b8e6298889deac0382df24e5d94362def5558825c55c7c0f4e538244aa9daa56` | `build.sh` | 2026-11-11 |
| Kernel package | `linux-rpi` / `linux-rpi-dev` **6.12.115-r0** (kernel release `6.12.115-0-rpi`) | `packages/espnow-nexmon/APKBUILD` and `packages/rpi-ws281x-pwm/APKBUILD` (`_kpkgver`, `_krel`; keep both in sync) | 2026-11-11 |
| Nexmon brcmfmac driver | Kali `brcmfmac-nexmon-dkms` 6.12.2, commit `fff319040f6a8034fce36f846cce2dc19de1b396` | APKBUILD `_drvcommit` | 2026-10-27 |
| Nexmon firmware | Kali `firmware-nexmon` 0.2, commit `42e244621979ba96d82220a3324771d0b6e212eb`; Pi 5 uses `cypress/cyfmac43455-sdio-standard.bin` (bcm43455c0, Nexmon 7_45_189 lineage), installed sha256 `37b46819372d624e9b8153d164857dc983b696919b0899bf8034a1bad65bf832` | APKBUILD `_fwcommit` | 2026-10-27 |
| Nexmon brcmfmac local patches | `packages/espnow-nexmon/0001-set_monitor_channel-net_device.patch` (cfg80211 `set_monitor_channel` gets the `net_device` argument of 6.12.115) and `0002-cypress-43752-sdio-id.patch` (alias for the renamed `SDIO_DEVICE_ID_BROADCOM_43752`, value `0xaae8`). Both are compile fixes for the 6.12.115 kernel, local to this project, not upstream | `packages/espnow-nexmon/APKBUILD` (`source`, `sha512sums`) | 2026-11-11 |
| spidev (Python) | PyPI `spidev` 3.8, sdist sha256 `2bc02fb8c6312d519ebf1f4331067427c0921d3f77b8bcaf05189a2e8b8382c0` | `packages/py3-spidev/APKBUILD` | 2026-10-27 |
| WS281x Python bindings | rpi-ws281x-python tag `pi5-beta2` (version 6.0.0), commit `df3197bd213c6d74db5755711d615ce20c7e0d1e` (same as the `pi5` branch head) | `packages/py3-rpi-ws281x/APKBUILD` (`_pycommit`) | 2026-11-11 |
| WS281x C library (userspace) | jgarff/rpi_ws281x commit `35a942c452e216e38fb91a5a522435e8aae56d36`: the `library/lib` submodule that `pi5-beta2` points to. The board is a Pi 5 8GB (revision code `d04170`), which this commit detects | `packages/py3-rpi-ws281x/APKBUILD` (`_libcommit`) | 2026-11-11 |
| WS281x kernel module source | jgarff/rpi_ws281x commit `1396df4f35a86de0f7e5eda91d94d4539eef1727` (`pi5` branch head, `rp1_ws281x_pwm/`) | `packages/rpi-ws281x-pwm/APKBUILD` (`_pi5commit`) | 2026-11-11 |
| Alpine build container | `alpine@sha256:ff7f0c727dd796aa36787bfcd9a64bf35a6ce72a73451b3359fde849b6703d63` (tag 3.23) | `build.sh` | 2026-10-27 |
| ESP32 test toolchain | arduino-cli 1.1.1, arduino-esp32 core 3.2.0 (ESP-IDF 5.4, ESP-NOW v2.0), `python:3.12-slim@sha256:12316debbfb86890f6b2d825820612741ba527ad5a44e3902759141d572bfe8f` | `scripts/flash-esp32.sh` | 2026-10-27 |
| binfmt helper | `tonistiigi/binfmt@sha256:400a4873b838d1b89194d982c45e5fb3cda4593fbfd7e08a02e76b03b21166f0` | `build.sh` | 2026-10-27 |

Source tarballs are also checked by the `sha512sums` in each APKBUILD. Regenerate them after editing any local package file:

    docker run --rm -v "$PWD:/work" -w /work <alpine image> sh -c 'apk add -q abuild && cd packages/espnow-nexmon && abuild -F checksum'

## Why these versions

- **Alpine 3.23, not 3.24:** 3.24 ships `linux-rpi` 6.18. The newest Nexmon brcmfmac tree (Kali's) targets 6.12, and 3.23 is the newest Alpine release on 6.12. It's supported until about November 2027.
- **Kali's tree, not seemoo-lab/nexmon:** upstream Nexmon's driver patches stop at 6.6.y. Kali ported them to 6.12 and ships them on the Pi 5.
- **Why 3.23.8 (kernel 6.12.115):** as of 2026-11-11, `linux-rpi` / `linux-rpi-dev` 6.12.85-r0 is no longer on the v3.23 mirror (dl-cdn): its APKINDEX lists only 6.12.115-r0. The 3.23.7 build could no longer install its pinned kernel headers, so the image moved to Alpine 3.23.8, whose tarball ships `boot/config-6.12.115-0-rpi` (checked).
- **Kernel guard:** the modules only load on the exact kernel they were built for. `espnow-nexmon` depends on `linux-rpi=6.12.115-r0`, and `check()` verifies the vermagic. `make-image.sh` refuses a tarball whose kernel doesn't match.

## Upgrading the kernel

1. Pick a new Alpine 3.23.x tarball and update `ALPINE_VER` and `ALPINE_SHA256`.
2. Read the `linux-rpi` version shipped in that tarball (`boot/config-*`), then set `_kpkgver` and `_krel` in both `espnow-nexmon` and `rpi-ws281x-pwm`.
3. Bump `pkgrel`, regenerate the checksums, and run `./build.sh`.
