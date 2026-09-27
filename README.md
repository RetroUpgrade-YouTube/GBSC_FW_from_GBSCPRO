# GBSC-Pro Firmware for Base GBSC

GBSC-Pro firmware backported to run on the **base GBSC** board. All hardware-dependent drivers (ADV/HC32F460, STV9426, PT2257, IR) are no-op'd — APIs preserved, bodies emptied — so the firmware compiles cleanly and the missing chips are never touched.

Based on the incredible work by [brisma/gbsc-pro](https://github.com/brisma/gbsc-pro), originally by [ramapcsx2/gbs-control](https://github.com/ramapcsx2/gbs-control).

---

## Features

| Category | Details |
|---|---|
| **Menu** | OLED + rotary encoder, full navigation, 36 profile slots (A–Z, 0–9) |
| **Web UI** | PWA — slots, WiFi management, all PRO settings, developer overrides |
| **Video** | 240p/288p/480i/576i → 480p/576p/720p/960p/1024p/1080p, HD bypass, RGBHV scaling/bypass |
| **Sync** | Auto SOG/phase tuning, auto-best Htotal, input detection, frame-time lock |
| **Processing** | Deinterlacing (adaptive/bob), scanlines, VDS line filter, peaking, step response |
| **Color** | R/G/B balance, YPbPr/component output, HDMI limited-range, PAL-60 forcing |
| **Developer** | NTSC/PAL overrides: Htotal, PLL div, SDRAM clock, ADC filter, OSR, SOG, sync invert, screen move/scale |
| **Network** | WiFi, WebSocket status, mDNS `gbscontrol.local`, AP fallback, OTA |
| **Clocking** | Si5351 external clock generator, ADC auto-gain + offset calibration |

## Disabled (no hardware on base GBSC)

| Feature | Requires | Status |
|---|---|---|
| S-Video / composite input | ADV7280/ADV7391 + HC32F460 | No-op stub |
| TV OSD | STV9426 | No-op stub |
| Audio volume / mute | PT2257 | No-op stub |
| IR remote | IR receiver diode | No-op (decode → false) |

These are **inert** — no UART/I2C/GPIO traffic. Safe to ignore in menus.

## Notable changes vs stock GBSC-Pro

1. **Rotary encoder rewritten** — quadrature transition accumulator replaces the buggy 100 ms-gate single-cell handoff. Fixes missed rotations and double jumps.
2. **Missing-hardware drivers no-op'd** — `adv_controller.h`, `pt2257.h`, `stv9426.h`, `ir_remote.h` have empty bodies, all symbols preserved.
3. **AGENTS.md** — 676-line canonical repo guide (architecture, invariants, build pipeline).
4. **Flasher: pre-flash firmware backup** — `gbsc_flasher.py` gains a `--backup` CLI flag (and a GUI checkbox) that dumps the full 4 MB ESP8266 flash to a timestamped `.bin` before any erase/write, so the current firmware is always recoverable.

---

## Building

```bash
cd gbs-control
pio run
# → .pio/build/gbsc-pro/firmware.bin  (~785 KB)
```

PlatformIO · `espressif8266@4.2.1` · `d1_mini` (4 MB flash, LittleFS, 160 MHz)

## Flashing

```bash
# Via PlatformIO
cd gbs-control && pio run --target upload

# Via Python flasher
pip install pyserial esptool
python gbsc-pro-flasher/gbsc_flasher.py --esp gbs-control/.pio/build/gbsc-pro/firmware.bin
```

> ⚠️ **GPIO0** (rotary encoder push) must be pulled HIGH at power-on or the ESP8266 will not boot.

---

## Hardware quick reference

| Control | Function |
|---|---|
| **RGBS SYNC button** (pressed) | LM1881 sync-stripping for composite |
| **RGBS SYNC button** (popped) | Bypass composite |
| **SYNC ON GREEN button** (pressed) | Accept VGA / RGBHV / RGsB |
| **SYNC ON GREEN button** (popped) | Accept RGBS / Component (YPbPr) |
| **POWER button** | Power on/off |
| **HDMI slide switch** | HDMI output ON/OFF (must be OFF for RGBS/Component via Saturn A/V) |
| **RGBHV/RGBS slide switch** | RGBS output mode (Saturn A/V connector) |
| **Compatibility mode switch** | Sega 32x / Neo Geo AES (ON) — restart to apply |
| **RGB knob** | R/G/B signal intensity |

**Inputs:** RGBS · RGsB · RGBHV · Component (YPbPr) · VGA · SCART (RGBS only)
**Outputs:** HDMI · VGA · RGBS/Component (Saturn A/V) · Audio (3.5 mm)

> HDMI and VGA output cannot be active simultaneously.

---

## Repository layout

| Path | Purpose |
|---|---|
| `gbs-control/` | ESP8266 firmware (PlatformIO) |
| `gbs-control/pro/` | PRO layer (menu, OSD, drivers) |
| `gbs-control/public/` | Web UI source (TypeScript PWA) |
| `gbsc-pro-flasher/` | Python GUI/CLI flasher |
| `AGENTS.md` | Canonical repo guide |

## Provenance

- Base: [ramapcsx2/gbs-control](https://github.com/ramapcsx2/gbs-control)
- Pro fork: [brisma/gbsc-pro](https://github.com/brisma/gbsc-pro)
- Version: `GBS_FW_VERSION 2.4.1`
