# Pins

Every external input the build depends on. When you change one, change it in the file listed and record the date here.

| What | Value | Where it's set | Pinned |
|---|---|---|---|
| Alpine release | 3.23.7 (branch `v3.23`) | `build.sh` | 2026-10-27 |
| Alpine RPi tarball | `alpine-rpi-3.23.7-aarch64.tar.gz`, sha256 `0529b72802ee37fa480bb57e7843ce0ef065438df697e45a9f957f0538a6817e` | `build.sh` | 2026-10-27 |
| Kernel package | `linux-rpi` / `linux-rpi-dev` **6.12.85-r0** (kernel release `6.12.85-0-rpi`) | `packages/espnow-nexmon/APKBUILD` (`_kpkgver`, `_krel`) | 2026-10-27 |
| Nexmon brcmfmac driver | Kali `brcmfmac-nexmon-dkms` 6.12.2, commit `fff319040f6a8034fce36f846cce2dc19de1b396` | APKBUILD `_drvcommit` | 2026-10-27 |
| Nexmon firmware | Kali `firmware-nexmon` 0.2, commit `42e244621979ba96d82220a3324771d0b6e212eb`; Pi 5 uses `cypress/cyfmac43455-sdio-standard.bin` (bcm43455c0, Nexmon 7_45_189 lineage), installed sha256 `37b46819372d624e9b8153d164857dc983b696919b0899bf8034a1bad65bf832` | APKBUILD `_fwcommit` | 2026-10-27 |
| spidev (Python) | PyPI `spidev` 3.8, sdist sha256 `2bc02fb8c6312d519ebf1f4331067427c0921d3f77b8bcaf05189a2e8b8382c0` | `packages/py3-spidev/APKBUILD` | 2026-10-27 |
| Alpine build container | `alpine@sha256:ff7f0c727dd796aa36787bfcd9a64bf35a6ce72a73451b3359fde849b6703d63` (tag 3.23) | `build.sh` | 2026-10-27 |
| ESP32 test toolchain | arduino-cli 1.1.1, arduino-esp32 core 3.2.0 (ESP-IDF 5.4, ESP-NOW v2.0), `python:3.12-slim@sha256:12316debbfb86890f6b2d825820612741ba527ad5a44e3902759141d572bfe8f` | `scripts/flash-esp32.sh` | 2026-10-27 |
| binfmt helper | `tonistiigi/binfmt@sha256:400a4873b838d1b89194d982c45e5fb3cda4593fbfd7e08a02e76b03b21166f0` | `build.sh` | 2026-10-27 |

Source tarballs are also checked by the `sha512sums` in each APKBUILD. Regenerate them after editing any local package file:

    docker run --rm -v "$PWD:/work" -w /work <alpine image> sh -c 'apk add -q abuild && cd packages/espnow-nexmon && abuild -F checksum'

## Why these versions

- **Alpine 3.23, not 3.24:** 3.24 ships `linux-rpi` 6.18. The newest Nexmon brcmfmac tree (Kali's) targets 6.12, and 3.23 is the newest Alpine release on 6.12. It's supported until about November 2027.
- **Kali's tree, not seemoo-lab/nexmon:** upstream Nexmon's driver patches stop at 6.6.y. Kali ported them to 6.12 and ships them on the Pi 5.
- **Kernel guard:** the modules only load on the exact kernel they were built for. `espnow-nexmon` depends on `linux-rpi=6.12.85-r0`, and `check()` verifies the vermagic. `make-image.sh` refuses a tarball whose kernel doesn't match.

## Upgrading the kernel

1. Pick a new Alpine 3.23.x tarball and update `ALPINE_VER` and `ALPINE_SHA256`.
2. Read the `linux-rpi` version shipped in that tarball (`boot/config-*`), then set `_kpkgver` and `_krel`.
3. Bump `pkgrel`, regenerate the checksums, and run `./build.sh`.
