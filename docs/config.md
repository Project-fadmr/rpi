# Configuration reference

## Boot partition

Alpine's `config.txt` is left untouched. It loads `boot/vmlinuz-rpi` and the initramfs, then runs `include usercfg.txt`.

### `boot/usercfg.txt`

| Line | Purpose |
|---|---|
| `dtparam=i2c_arm=on` | I2C1 on GPIO2/3 |
| `dtparam=spi=on` | SPI0 with two chip selects |
| `enable_uart=1` | Debug UART (`ttyAMA10`) for the console |
| `dtparam=uart0=on` | UART0 on GPIO14/15 (`ttyAMA0`) |
| `dtoverlay=disable-bt` | Bluetooth powered off (power) |
| `dtparam=audio=off` | No audio device (power) |
| `camera_auto_detect=0`, `display_auto_detect=0` | Firmware doesn't probe for or load camera/DSI overlays (power) |
| `max_framebuffers=0` | No firmware framebuffer. Together with no `vc4-kms` overlay, HDMI isn't driven (power). **HDMI shows nothing; use the UART or SSH** |

### `boot/cmdline.txt`

| Parameter | Purpose |
|---|---|
| `modules=loop,squashfs,sd-mod,usb-storage` | Alpine default: what the initramfs needs to find the boot media and modloop |
| `quiet` | Less console output |
| `console=ttyAMA10,115200` | Kernel console on the debug UART |
| `firmware_class.path=/usr/lib/espnow/firmware` | Searched before `/lib/firmware`, so brcmfmac loads the Nexmon firmware |
| `cpufreq.default_governor=powersave` | CPU stays at its lowest frequency (1.5 GHz on the Pi 5) |
| `usbcore.autosuspend=1` | Idle USB devices suspend after 1 s |

## apkovl (`overlay/` → `espnow-pi.apkovl.tar.gz` on the card)

| File | Purpose |
|---|---|
| `etc/apk/world` | Packages installed at every boot, see [software.md](software.md) |
| `etc/apk/repositories` | Upstream v3.23 main and community repos, for `apk add` at runtime. At boot, Alpine's initramfs installs from every card folder marked `.boot_repository`: the official `apks/` and our `custom/`, which holds all 107 packages signed with our key. The device name doesn't matter, and no network is needed |
| `etc/apk/keys/espnow.rsa.pub` | Public key for our repo (added by `make-image.sh`; the private key stays in `keys/`, which git ignores) |
| `etc/modules` | Loads `i2c-dev` and `spidev` at boot |
| `etc/inittab` | Only a getty on `ttyAMA10`; no HDMI ttys |
| `etc/network/interfaces` | `eth0` via DHCP |
| `etc/conf.d/dropbear` | `-s`: no password logins |
| `etc/conf.d/power` | `ETH=on`/`off`, see below |
| `etc/local.d/power.start` | Runtime power tuning, see below |
| `etc/sysctl.d/power.conf` | `vm.dirty_writeback_centisecs=6000`: fewer writeback wakeups |
| `etc/rc.conf` | `rc_logger="YES"`: service output goes to `/var/log/rc.log` |
| `root/.ssh/authorized_keys` | Your key, if `build.sh` found one (`SSH_PUBKEY_FILE`, default `~/.ssh/id_ed25519.pub`) |

### Services (runlevels, created by `make-image.sh`)

| Runlevel | Services |
|---|---|
| sysinit | devfs, dmesg, mdev, hwdrivers, modloop |
| boot | modules, sysctl, hostname, bootmisc, syslog |
| default | networking, dropbear, espnow, local |
| shutdown | killprocs, mount-ro |

Deliberately left out:
- **ntp/chrony:** the clock starts at the RTC value or at 1970. Add `chrony` if you need real time.
- **cron, acpid, Bluetooth and Wi-Fi client services.**

### `espnow` service (from the `espnow-nexmon` package)

- **Config:** `/etc/conf.d/espnow`, with `ESPNOW_CHANNEL` (default 1), plus optional `ESPNOW_WLAN`, `ESPNOW_MON` and `BRCMFMAC_OPTS`.
- **Blacklist:** `/etc/modprobe.d/espnow-nexmon.conf` blacklists the stock brcmfmac so it can't grab the chip first.
- **What it does:** see [espnow.md](espnow.md#boot-sequence).

## Power tuning

| Measure | Where | Effect |
|---|---|---|
| Bluetooth off | `disable-bt` | Bluetooth radio unpowered |
| Audio, camera and display probing off; no framebuffer or KMS | `usercfg.txt` | Display pipeline idle |
| `powersave` governor | cmdline | Lowest CPU clock |
| USB autosuspend; runtime PM on USB and PCIe | cmdline, `power.start` | Idle devices suspend |
| LEDs off | `power.start` | A few mW |
| `ETH=off` (opt-in) | `/etc/conf.d/power` | Takes eth0 down after boot. **Only the UART console is left for access** |
| RAM root (diskless) | Alpine mode | Boot media (SD or USB) is idle after boot |
| Minimal services, one getty | apkovl | Fewer wakeups |

Not tuned, by design:
- **The Wi-Fi radio stays awake.** A monitor-mode receiver can't use 802.11 power save, and that's the cost of listening with no gaps.
- **Wi-Fi TX power** isn't lowered, because that would reduce range.

### One-time EEPROM step (manual)

The bootloader EEPROM isn't part of the boot image. From Raspberry Pi OS on the same board, run once:

    sudo rpi-eeprom-config --edit
    # set:
    POWER_OFF_ON_HALT=1
    WAKE_ON_GPIO=0

After `poweroff`, the board then draws about 0.01 W instead of about 1.4 W.

### Measured draw

To be filled in from a USB-C power meter, as in verification step 6 of the plan.

| State | Stock Alpine 3.23 | This image |
|---|---|---|
| Idle, eth on | TBD | TBD |
| Idle, eth off | – | TBD |
| ESP-NOW 10 msg/s | – | TBD |
| Halted (with EEPROM step) | – | TBD |
