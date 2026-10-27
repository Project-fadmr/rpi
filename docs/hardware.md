# Hardware interfaces (Raspberry Pi 5)

All of these are enabled by `boot/usercfg.txt` and `overlay/etc/modules`. Pin numbers are physical header pins.

| Bus | Signals (GPIO → pin) | Device node | Enabled by |
|---|---|---|---|
| I2C1 | SDA GPIO2 → 3, SCL GPIO3 → 5 | `/dev/i2c-1` | `dtparam=i2c_arm=on` + `i2c-dev` |
| SPI0 | MOSI GPIO10 → 19, MISO GPIO9 → 21, SCLK GPIO11 → 23, CE0 GPIO8 → 24, CE1 GPIO7 → 26 | `/dev/spidev0.0`, `/dev/spidev0.1` | `dtparam=spi=on` + `spidev` |
| UART0 | TX GPIO14 → 8, RX GPIO15 → 10 | `/dev/ttyAMA0` | `dtparam=uart0=on` |
| Debug UART | 3-pin JST connector between the HDMI ports | `/dev/ttyAMA10` | `enable_uart=1`. It's the login console at 115200 8N1 |
| GPIO | All header GPIOs, through RP1 | `/dev/gpiochip*`. The header chip is labelled `pinctrl-rp1`; select it by label, because the number changed between kernels | built into the kernel (`CONFIG_PINCTRL_RP1=y`) |

- **Power pins:** 3.3 V on pins 1 and 17; 5 V on pins 2 and 4; GND on pins 6, 9, 14, 20, 25, 30, 34 and 39.
- **Logic levels:** the header is 3.3 V only. Don't connect 5 V signals.

## Quick checks

    gpioinfo | head          # lists pinctrl-rp1 lines
    gpioset -c pinctrl-rp1 17=1
    i2cdetect -y 1
    ls /dev/spidev0.*
    picocom -b 115200 /dev/ttyAMA0

## Wi-Fi and Bluetooth

- **Wi-Fi:** the onboard CYW43455 is used only for ESP-NOW (monitor mode, see [espnow.md](espnow.md)). Normal Wi-Fi client or AP use isn't configured.
- **Bluetooth:** disabled with `dtoverlay=disable-bt`.
