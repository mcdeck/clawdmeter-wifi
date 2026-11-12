# Clawdmeter — Wi-Fi native fork

> **Fork of [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter).**
> This version removes the host daemon and BLE data channel entirely. The ESP32 polls
> the Anthropic API directly over Wi-Fi — no host PC required once provisioned.
> See [`WIFI_FORK.md`](WIFI_FORK.md) for the full technical delta.

## Table of contents

- [Screens](#screens)
- [Hardware](#hardware)
- [Prerequisites](#prerequisites)
- [Getting started](#getting-started)
  - [1. Flash the firmware](#1-flash-the-firmware)
  - [2. Provision via captive portal](#2-provision-via-captive-portal-recommended)
  - [2a. Provision via serial](#2a-provision-via-serial-alternative)
  - [Moving to a different Wi-Fi network](#moving-to-a-different-wi-fi-network)
- [Physical buttons](#physical-buttons)
- [Claude Code hook integration (live reactions)](#claude-code-hook-integration-live-reactions)
- [How it works](#how-it-works)
- [Serial commands](#serial-commands)
- [Recompiling fonts](#recompiling-fonts)
- [Converting Lucide icons](#converting-lucide-icons)
- [Splash animations](#splash-animations)
- [Credits](#credits)
- [Licensing gray area warning](#licensing-gray-area-warning)

---

A small ESP32 dashboard for your desk that tracks Claude Code usage in real time.
It runs on a [Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm)
(480×480 square) and the splash screen plays pixel-art Clawd animations that get busier as your usage climbs.
The two side buttons send Space and Shift+Tab over BLE HID for Claude Code's voice mode
and mode-toggle shortcuts.

|              Usage meter              |              Clawd animation screen              |
| :-----------------------------------: | :----------------------------------------------: |
| ![Usage meter](assets/demo.jpeg) | ![Clawd animation screen](assets/demo.gif) |

The Clawd animations come from [claudepix](https://claudepix.vercel.app),
[@amaanbuilds](https://x.com/amaanbuilds)'s library of pixel-art Clawd sprites.

## Screens

Press the middle (PWR) button to cycle through the three non-splash screens. Tap anywhere
on screen to toggle the splash on or off; while the splash is up, the PWR button cycles
animations instead of screens.

|              Splash               |              Usage              |                Bluetooth                |              Wi-Fi              |
| :-------------------------------: | :-----------------------------: | :-------------------------------------: | :-----------------------------: |
| ![Splash](screenshots/splash.png) | ![Usage](screenshots/usage.png) | ![Bluetooth](screenshots/bluetooth.png) | ![Wi-Fi](screenshots/wifi.png) |
| Touch-toggle anytime; PWR cycles animations | Session and weekly utilization | BLE connection status and bond reset | Stored credentials and hotspot trigger |

The usage screen shows an overlay while connecting or if there is an error (wrong password,
invalid token, API unreachable). The overlay clears automatically once the first successful
poll arrives.

## Hardware

| Board | Status |
|---|---|
| [Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm) (480×480) | ✅ Tested |
| [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm) (368×448) | 🧪 Builds; awaiting community test |

> For the upstream multi-board version with the host daemon, see
> [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter).

## Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html)
- A Claude Code subscription (any tier) — the Anthropic API token is what the device polls

No daemon, no Python, no Bluetooth pairing required for the usage data.

## Getting started

### 1. Flash the firmware

Replace `waveshare_amoled_216` with `waveshare_amoled_18` for the 1.8" board.
Close any serial monitor (PuTTY, etc.) before flashing — the port must be free.

**macOS:**
```bash
# Find the port: ls /dev/cu.usbmodem*
pio run -d firmware -e waveshare_amoled_216 -t upload --upload-port /dev/cu.usbmodem101
```

**Linux:**
```bash
# Find the port: ls /dev/ttyACM*
pio run -d firmware -e waveshare_amoled_216 -t upload --upload-port /dev/ttyACM0
```

**Windows (native):**
```powershell
# Find the port: Device Manager → Ports (COM & LPT)
$env:PYTHONUTF8=1; pio run -d firmware -e waveshare_amoled_216 -t upload --upload-port COM4
```

**WSL:**

The ESP32-S3 USB-JTAG port must be forwarded into WSL with
[usbipd-win](https://github.com/dorssel/usbipd-win) — WSL cannot access
USB devices directly.

1. Install usbipd-win (once):
   ```powershell
   winget install usbipd
   ```

2. In an **elevated** PowerShell, find the device and attach it:
   ```powershell
   usbipd list
   # Look for "USB JTAG/serial debug unit" or similar
   usbipd attach --wsl --busid <busid>
   ```

3. Inside WSL, the device appears as `/dev/ttyACM0` (run `dmesg | tail` to confirm):
   ```bash
   pio run -d firmware -e waveshare_amoled_216 -t upload --upload-port /dev/ttyACM0
   ```

   After flashing you can detach the device in PowerShell:
   ```powershell
   usbipd detach --busid <busid>
   ```

### 2. Provision via captive portal (recommended)

On first boot the device starts a Wi-Fi access point named **ClawdMeter**.

1. On your phone, connect to **ClawdMeter** (no password).
2. Your phone should show a *"Sign into network"* notification — tap it. If not,
   open a browser and go to **192.168.4.1**.
3. Fill in your Wi-Fi network name, password, and Anthropic token (`sk-ant-…`).
4. Tap **Save & Connect**. The device reboots and connects to your network.

The token field is optional during setup — you can add it later via serial if you
prefer not to enter it on a phone browser.

> **Token:** Run `claude setup-token` on any machine where Claude Code is signed in to
> your subscription. It walks you through a browser sign-in and prints a long-lived
> (one-year) token starting `sk-ant-oat01-…`; copy it then, as it isn't saved anywhere.
> Don't use the `accessToken` from `~/.claude/.credentials.json`: that one expires
> after a few hours and the device will show "Token invalid".

### 2a. Provision via serial (alternative)

Connect a serial terminal to the device at **115200 baud** (`/dev/ttyACM0` on Linux/WSL,
`/dev/cu.usbmodem*` on macOS, `COM<N>` on Windows) and send commands:

```
ssid  MyNetwork
pass  MyPassword
token sk-ant-oat01-…
```

Confirm with `status`. Reboot the device to connect.

### Moving to a different Wi-Fi network

Navigate to the **Wi-Fi screen** (PWR button twice from Usage) and tap **Start Hotspot**.
The device immediately switches to AP mode and locks to the Wi-Fi screen — follow the
captive portal steps above to re-provision. After saving, the device reboots and connects
to the new network.

The device also falls back to hotspot mode automatically after three successive connection
failures.

## Physical buttons

| Button           | GPIO         | Normal function                                    | On splash         |
| ---------------- | ------------ | -------------------------------------------------- | ----------------- |
| **Left**         | GPIO 0       | Hold to send Space (Claude Code voice-mode PTT)    | Same              |
| **Middle** (PWR) | AXP2101 PKEY | Cycle screens: Usage → Bluetooth → Wi-Fi → Usage   | Cycle animations  |
| **Right**        | GPIO 18      | Press to send Shift+Tab (Claude Code mode toggle)  | Same              |

Space and Shift+Tab are standard BLE HID keyboard reports — they trigger in whatever window
has focus on the paired host, not just Claude Code. BLE pairing is separate from Wi-Fi and
optional; the usage display works without it.

## Claude Code hook integration (live reactions)

Clawdmeter can react to Claude Code activity in real time: Clawd waves and
points from his corner when you start a prompt, and calms down when the turn
ends. This uses [Claude Code hooks](https://docs.anthropic.com/en/docs/claude-code/hooks)
to POST a small event to the device over your local network.

### 1. Find the device address

The device registers as **clawdmeter.local** via mDNS once connected to Wi-Fi.
Test it:

```bash
curl -s http://clawdmeter.local/event -X POST -d "t=p"
# → HTTP 204 No Content (success) or 401 Unauthorized
```

If mDNS doesn't work on your network, use the device's IP address instead
(visible on the Wi-Fi screen, or check your router's DHCP table).

### 2. Set an event token (optional but recommended)

The `POST /event` endpoint accepts unauthenticated requests by default (fine on a
home LAN). To require a secret:

1. Open **http://clawdmeter.local** in a browser.
2. Under **Hook event token**, enter any string and save.
3. Add `Authorization: Bearer <token>` to your hook's `curl` call (see below).

### 3. Add hooks to Claude Code

Add the following to your Claude Code settings file. The recommended location is
`~/.claude/settings.json` (applies to all projects) or `.claude/settings.json` in
a specific project.

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -sf http://clawdmeter.local/event -X POST -d 't=p' -o /dev/null"
          }
        ]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "curl -sf http://clawdmeter.local/event -X POST -d 't=s' -o /dev/null"
          }
        ]
      }
    ]
  }
}
```

If you set an event token, add `-H "Authorization: Bearer <token>"` to each `curl` call.

### Event types

| `t=` | Trigger | Clawd's reaction |
|------|---------|-----------------|
| `p`  | Prompt started (PreToolUse) | Waves and points from his corner |
| `s`  | Turn ended (Stop) | Calms down, returns to idle |
| `b`  | Back after a gap (optional) | Subtle notice animation |

The `es` (sequence), `n` (session count), and `a` (activity level 0–3) parameters are
optional and can be omitted for simple integrations.

### Tuning reactivity

On **http://clawdmeter.local**, the **Clawd** section controls:

- **Liveliness** (1–5) — how often Clawd does idle acts and reacts to events;
  level 1 sits still and ignores events entirely.
- **Attention animation** — how long the wave/point runs before Clawd returns to
  his seat (120 s / 30 s / 10 s / Off).

---

## How it works

1. On boot the device reads Wi-Fi credentials and an Anthropic OAuth token from NVS
   (ESP32 non-volatile storage — survives reboots and firmware updates).
2. It connects to your Wi-Fi network and syncs time via NTP.
3. Every 60 seconds it makes a minimal API call to `api.anthropic.com/v1/messages`
   — one token of claude-haiku, essentially free.
4. Usage figures come from **response headers** (`anthropic-ratelimit-unified-*`),
   not the response body.
5. The firmware updates the LVGL dashboard and picks Clawd animations from the
   usage-rate mood group.
6. The BLE HID keyboard runs independently — it works whenever a host is paired,
   regardless of Wi-Fi state.

See [`WIFI_FORK.md`](WIFI_FORK.md) for the full technical details: API headers,
TLS approach, NVS layout, error states.

## Serial commands

The serial interface (115200 baud) is always available for inspection and re-provisioning.
Connect with any terminal at **115200 8N1** — port is `/dev/ttyACM0` on Linux/WSL,
`/dev/cu.usbmodem*` on macOS, or `COM<N>` on Windows. WSL users need usbipd attached first
(see [Flash the firmware](#1-flash-the-firmware)).

| Command | Effect |
| ------- | ------ |
| `ssid <value>` | Store Wi-Fi SSID to NVS |
| `pass <value>` | Store Wi-Fi password to NVS |
| `token <value>` | Store Anthropic OAuth token to NVS |
| `status` | Print stored values (token masked to first 20 chars) |
| `clear` | Wipe all NVS keys (triggers hotspot on next reboot) |
| `screenshot` | Dump LVGL framebuffer as raw RGB565 over serial |

## Recompiling fonts

The `firmware/src/font_*.c` files are pre-compiled LVGL bitmap fonts.

```bash
npm install -g lv_font_conv
```

Generate each one with `--no-compress` (required for LVGL 9):

```bash
# Tiempos Text (titles, 56px)
lv_font_conv --font assets/TiemposText-400-Regular.otf -r 0x20-0x7E \
  --size 56 --format lvgl --bpp 4 --no-compress \
  -o firmware/src/font_tiempos_56.c --lv-include "lvgl.h"

# Styrene B (large numbers 48, panel labels 28, small text 24/20/16/14)
for size in 48 28 24 20 16 14; do
  lv_font_conv --font assets/StyreneB-Regular.otf -r 0x20-0x7E \
    --size $size --format lvgl --bpp 4 --no-compress \
    -o firmware/src/font_styrene_${size}.c --lv-include "lvgl.h"
done

# DejaVu Sans Mono (spinner Unicode chars)
lv_font_conv --font assets/DejaVuSansMono.ttf \
  -r 0x20-0x7E,0xB7,0x2026,0x2722,0x2733,0x2736,0x273B,0x273D \
  --size 32 --format lvgl --bpp 4 --no-compress \
  -o firmware/src/font_mono_32.c --lv-include "lvgl.h"
```

**Important:** `lv_font_conv` v1.5.3 outputs LVGL 8 format. Each generated file must be
patched for LVGL 9 compatibility — remove `#if LVGL_VERSION_MAJOR >= 8` guards, drop
`.cache`, add `.release_glyph`, `.kerning`, `.static_bitmap`, `.fallback`, `.user_data`.
Without these patches, fonts compile but render as invisible.

> **Note:** The Styrene font does not include `•` (U+2022) or `…` (U+2026). Use ASCII
> equivalents (`*`, `...`) in any string rendered with a Styrene variant.

## Converting Lucide icons

```bash
node tools/png_to_lvgl.js assets/icon_bluetooth_48.png icon_bluetooth_data ICON_BLUETOOTH_WIDTH ICON_BLUETOOTH_HEIGHT
```

Default tint is white (`0xFFFFFF`); Lucide PNGs ship as black-on-transparent. Pass
`--no-tint` for pre-coloured artwork. Battery icons use RGB565A8 (with alpha) so they
blend over the splash; the rest are baked RGB565. Paste converter output into
`firmware/src/icons.h`.

## Splash animations

Animations come from [claudepix.vercel.app](https://claudepix.vercel.app). To re-pull:

```bash
node tools/scrape_claudepix.js
node tools/convert_to_c.js
pio run -d firmware -t upload
```

See `tools/README.md` for details.

## Credits

- Pixel-art Clawd animation by [@amaanbuilds](https://x.com/amaanbuilds),
  sourced from [claudepix.vercel.app](https://claudepix.vercel.app).
- Lucide icon set ([lucide.dev](https://lucide.dev), MIT) for bluetooth and battery glyphs.
- Anthropic brand fonts (Tiempos Text, Styrene B) — see licensing warning below.
- Original project by [@hermannbjorgvin](https://github.com/HermannBjorgvin).

## Licensing gray area warning

The software in this repository uses and adheres to the Anthropic brand guidelines and
uses the same proprietary fonts that Anthropic has a license for but this software uses
without permission, as well as using assets from Anthropic such as the copyrighted Clawd
mascot. Even though the code itself is non-proprietary, it will not be licensed under a
copyleft license since this repo includes proprietary fonts and copyrighted assets.
**You have been warned!**
