# ESP-NOW on the Pi 5

## How it works

**Receive-only.**

```
air ──▶ Nexmon brcmfmac + firmware ──▶ mon0 ──espnow.ko──▶ espnow0 ──AF_PACKET──▶ your program
```

1. **Nexmon firmware and driver** put the onboard CYW43455 into monitor mode, so it receives every frame on the channel with a radiotap header (`mon0`).
2. **`espnow.ko`** (`packages/espnow-nexmon/espnow.c`) attaches to `mon0` and creates `espnow0`. It drops everything that isn't an ESP-NOW frame addressed to this Pi or to broadcast, and hands the rest to you. Filtering in the kernel means your program only wakes up for real ESP-NOW traffic.

**Why no sending:** injecting frames crashes this firmware (Nexmon 7.45.206) after somewhere between 40 and 230 frames, whatever the frame format, pacing or queue depth; measured 2026-10-27. It's a known upstream bug ([#335](https://github.com/seemoo-lab/nexmon/issues/335), [discussion #605](https://github.com/seemoo-lab/nexmon/discussions/605), [#668](https://github.com/seemoo-lab/nexmon/issues/668)), so `espnow0` refuses to send (writes are dropped and counted in `tx_dropped`). Receive-only ran with no crashes.

## Using `espnow0`

`espnow0` is an Ethernet-like interface. Each packet looks like this:

| Bytes | Field |
|---|---|
| 0–5 | Destination MAC (this Pi, or `ff:ff:ff:ff:ff:ff` for broadcast) |
| 6–11 | Source MAC: the sending ESP |
| 12–13 | Ethertype `0x88B5` |
| 14– | ESP-NOW payload |

- **Payload:** up to 1470 bytes. Chained ESP-NOW v2.0 elements are put back together; this was verified with 1000-byte messages from an ESP32-C3.
- **Unencrypted only.** Encrypted (CCMP) frames are ignored, and ESP peers must not set an LMK.

Open a raw packet socket bound to `espnow0` with protocol `0x88B5` and `recv()` from it:

- **Python:** `socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x88B5))`, then `.bind(("espnow0", 0x88B5))`. See `test/espnow_rx.py`.
- **C:** `socket(AF_PACKET, SOCK_RAW, htons(0x88B5))`, then `bind()` with a `struct sockaddr_ll` (`sll_family=AF_PACKET`, `sll_protocol=htons(0x88B5)`, `sll_ifindex=if_nametoindex("espnow0")`).
- **Rust:** use the `libc` crate with the same calls as C, or a crate such as `pnet_datalink`, on interface `espnow0`.

Raw sockets need root or `CAP_NET_RAW`.

## Channel

ESP-NOW has no channel negotiation, so the Pi and every ESP node must sit on the same 2.4 GHz channel.
- **Pi:** set `ESPNOW_CHANNEL` in `/etc/conf.d/espnow`, then run `rc-service espnow restart`.
- **ESP32:** call `esp_wifi_set_channel(N, WIFI_SECOND_CHAN_NONE)` after `WiFi.mode(WIFI_STA)`.

## Limitations

- **Receive-only:** see above.
- **Have the ESPs broadcast.** In monitor mode the chip doesn't acknowledge frames, so an ESP sending *unicast* to the Pi gets `ESP_NOW_SEND_FAIL` and retries, even though the Pi received the frame. Broadcast has no ACKs and no retries.
- **Range:** measured 2026-10-27, an ESP32-C3 SuperMini (no external antenna) at 3–4 m delivered 100 % at 11–19.5 dBm TX power and 91 % at 5 dBm.
- **No RSSI yet.** The radiotap header from Nexmon is dropped and not exposed.
- **The radio is always on** while listening, see [config.md](config.md#power-tuning).
- **Nothing else can use `mon0`.** `espnow.ko` consumes every frame on it. `tcpdump -i mon0` still works, because taps run first.

## Boot sequence (the `espnow` OpenRC service)

1. It loads `cfg80211` and `brcmutil` from the modloop, then `insmod`s the Nexmon `brcmfmac.ko` from `/usr/lib/espnow/modules` with `roamoff=1 feature_disable=0x282000`. The stock brcmfmac is blacklisted.
2. It runs `ip link set wlan0 up` once, which initialises the chip. `wlan0` never associates.
3. It runs `iw dev wlan0 interface add mon0 type monitor`, then `ip link set wlan0 down` (cfg80211 only allows a monitor channel change when monitor interfaces are the only ones running; in Nexmon this doesn't power off the radio), then `ip link set mon0 up`. Nexmon sets radiotap monitor mode when the interface goes up.
4. It runs `iw dev mon0 set channel $ESPNOW_CHANNEL`.
5. It runs `insmod espnow.ko parent=mon0`, then `ip link set espnow0 up`.

The module also attaches by itself whenever an interface named `parent` comes up, and detaches cleanly if that interface goes away.

Two driver details found on hardware:
- **MAC address:** Nexmon's `mon0` has an all-zero MAC, so `espnow0` takes the radio's address (`wiphy->perm_addr`) and filters unicast on that.
- **Frame start:** brcmfmac runs `eth_type_trans()` on monitor frames, so `skb->data` is 14 bytes past the radiotap header by the time the handler runs. The handler parses from the MAC header, as AF_PACKET does.

## Frame format (researched 2026-10-27)

Sources:
- [Espressif ESP-NOW docs (stable)](https://docs.espressif.com/projects/esp-idf/en/stable/api-reference/network/esp_now.html)
- [Espressif ESP-NOW docs (v5.5.5)](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/api-reference/network/esp_now.html)

An ESP-NOW frame is an 802.11 vendor-specific action frame:

| Field | Bytes | Value |
|---|---|---|
| Frame control | 2 | `d0 00` (management, action; ToDS = FromDS = 0; not protected) |
| Duration | 2 | 0 |
| Addr1 | 6 | Destination |
| Addr2 | 6 | Source |
| Addr3 | 6 | `ff:ff:ff:ff:ff:ff` |
| Sequence control | 2 | Sequence number << 4 |
| Category | 1 | 127 (vendor specific) |
| OUI | 3 | `18 fe 34` (Espressif) |
| Random | 4 | Random bytes (anti-replay) |
| Vendor element(s) | 7+n | see below |
| FCS | 4 | Added by hardware |

Vendor element:

| Field | Bytes | Value |
|---|---|---|
| Element ID | 1 | 221 |
| Length | 1 | 5 + n (OUI + type + version + body). Espressif's table words this ambiguously; 5 + n matches real ESP traffic |
| OUI | 3 | `18 fe 34` |
| Type | 1 | 4 (ESP-NOW) |
| Version | 1 | bits 0–3: version (1 = v1.0, 2 = v2.0); bit 4: "more data" (v2.0) |
| Body | n | 0–250 bytes |

Versions:
- **v1.0:** one element, up to 250 bytes.
- **v2.0:** up to 1470 bytes, carried across several elements. A v1.0 receiver gets at most 250 bytes of it.

A real frame from the ESP32-C3, as captured on `mon0` (radiotap stripped):

    d000 0000 ffffffffffff e072a1727bcc ffffffffffff e001   header: action, broadcast, from the ESP
    7f 18fe34 c613756b                                      category, OUI, random
    dd 0d 18fe34 04 02 68656c6c6f203330                     element: len 13 = 5+8, type 4, v2.0, "hello 30"
    0eb6ad55                                                FCS

`packages/espnow-nexmon/frame_test.c` covers the parser in `espnow_frame.h` (it has its own test-only frame builder): round trip, filtering, every truncation, v2.0 chaining and overflow, and 200k random frames. It runs in the APKBUILD `check()`. On the host you can run it with ASan:

    gcc -fsanitize=address,undefined -Ipackages/espnow-nexmon packages/espnow-nexmon/frame_test.c && ./a.out

## Testing with an ESP32

1. Flash `test/esp32_sender` with `scripts/flash-esp32.sh`. It broadcasts `hello N` every second, and every 5th message as 1000 bytes.
2. On the Pi, run `./espnow_rx.py [seconds]` (in `/root`). It prints each frame, the per-sender delivery rate, and whether the 1000-byte v2.0 frames arrived whole.
