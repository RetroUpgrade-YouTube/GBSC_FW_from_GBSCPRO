# AGENTS.md

> **Before running any build/flash/debug commands, read [`MEMORY.md`](MEMORY.md)** — it contains toolchain paths, PowerShell gotchas, and environment-specific facts not derivable from source.

## 1. Overview

This repository contains the complete software for a **GBSC / GBSC-Pro retro video processor**: firmware for a TV5725-based analog video scaler (RGB / YPbPr / VGA / S-Video / composite in → SD/HD progressive out) plus a cross-platform Python flashing tool.

| Project | What it is | Target |
|---|---|---|
| `gbs-control/` | Arduino/PlatformIO firmware: scaler control, presets, sync automation, OLED menu, IR remote, TV OSD, WiFi web UI, audio, S-Video/composite via a second MCU | **ESP8266** (Wemos D1 Mini class, 160 MHz, 4 MB flash, LittleFS) |
| `gbsc-pro-flasher/` | GUI/CLI flasher that auto-detects and flashes both the ESP8266 controller and the HC32F460 "ADV" controller | Python 3.8+, Windows/macOS/Linux |

Key facts:

- **The MCU is an ESP8266, not an ESP32.** `gbs-control/platformio.ini` pins `platform = espressif8266@4.2.1`, `board = d1_mini`. The "ESP32" wording that appears in some comments comes from the `esp32async/ESPAsyncTCP` + `esp32async/ESPAsyncWebServer` library forks (which support both chips). Trust `platformio.ini`.
- **Two processors on the hardware.** The ESP8266 owns boot, input detection, presets, sync, UI, WiFi. On GBSC-Pro boards a second MCU (HC32F460, "ADV Controller") decodes S-Video/composite through an ADV7280/ADV7391 decoder; the ESP8266 talks to it over a UART packet protocol (`gbs-control/pro/drivers/adv_controller.h`).
- **Single application, two layers.** `gbs-control/gbs-control.ino` (≈11,700 lines) is the base system (`setup()`/`loop()`); `gbs-control/pro/` layers the GBSC-Pro features (IR, TV OSD, audio, ADV link) on top. There is no separate "PRO firmware" binary — one build, one env: `gbsc-pro`.
- **Provenance:** based on `ramapcsx2/gbs-control`; the Pro fork is `brisma/gbsc-pro`. The root `README.md` describes the consumer device (inputs/outputs/switches) and notes this tree is a backport of the Pro firmware.
- **Firmware version constant:** `GBS_FW_VERSION "2.4.1"` and `ADV_FW_VERSION "2.4.1"` in `gbs-control/pro/gbs-control-pro.h:34-35`. The web build inlines `GBS_FW_VERSION` into the webapp (`public/scripts/build.js`), so keep the two in sync automatically — don't hardcode a version in the web UI.

## 2. Repository layout (file map)

```
GBSC_FW_from_GBSCPRO/
├── README.md                        # Consumer-facing device manual (inputs, outputs, switches)
├── AGENTS.md                        # This file
├── gbs-control/                     # ── ESP8266 firmware (PlatformIO project) ──
│   ├── gbs-control.ino              # MAIN PROGRAM (~11.7k lines): setup/loop, presets, sync, web, slots
│   ├── options.h                    # PresetPreference enum + userOptions/runTimeOptions/adcOptions structs (guard: _USER_H_)
│   ├── slot.h                       # SlotMeta (exactly 128 bytes, static_assert), slot file format
│   ├── tv5725.h                     # TV5725 register template (typedef TV5725<GBS_ADDR> GBS; GBS_ADDR 0x17)
│   ├── tw.h                         # Low-level I2C wire helper used by tv5725.h
│   ├── framesync.h                  # FrameSyncManager: frame-time lock (FTL)
│   ├── fastpin.h                    # Fast GPIO helpers
│   ├── rgbhv.h                      # RGBHV mode definitions
│   ├── ntsc_*.h pal_*.h             # 12 generated preset LUT arrays (PROGMEM) — see §9.3
│   ├── presetMdSection.h            # Mode-detect section patches for writeProgramArrayNew()
│   ├── presetDeinterlacerSection.h  # Deinterlacer section patches
│   ├── presetHdBypassSection.h      # HD bypass section (loadHdBypassSection)
│   ├── ofw_RGBS.h / ofw_ypbpr.h     # Output filter word tables
│   ├── OLEDMenu*.h/.cpp             # Legacy/rotary menu system (OLEDMenuManager, OLEDMenuItem, fonts, config)
│   ├── OSDManager.h/.cpp            # Legacy OSD manager
│   ├── osd.h / fonts.h / images.h   # OSD primitives, fonts, logo bitmaps
│   ├── PersWiFiManager.h/.cpp       # MODIFIED copy of PersWiFiManager (original: 3rdparty/)
│   ├── generate_translations.py     # i18n generator → OLEDMenuTranslations.h (needs Pillow)
│   ├── OLEDMenuTranslations.h       # Generated: pixel-rendered menu strings
│   ├── platformio.ini               # Single env: gbsc-pro (ESP8266 d1_mini, 160 MHz, LittleFS, qio)
│   ├── src/                         # MODIFIED vendored libs (built into firmware)
│   │   ├── WebSockets*.h/.cpp       # Markus Sattler WebSockets (original: 3rdparty/WebSockets)
│   │   └── si5351mcu.*              # Si5351mcu clock-gen driver (global instance `Si`)
│   ├── 3rdparty/                    # UNMODIFIED reference copies: PersWiFiManager, WebSockets, Si5351mcu
│   ├── public/                      # ── Web UI source + build tooling ──
│   │   ├── package.json             # scripts: start / build / dev
│   │   ├── dev-server.js            # Local mock server (HTTP :8080 + WS :81) for UI development
│   │   ├── scripts/build.js         # Inlines JS/CSS/assets → ../webui.html (injects GBS_FW_VERSION)
│   │   ├── scripts/html2h.sh        # gzip -c9 + xxd -i + sed → ../webui_html.h (PROGMEM C array)
│   │   ├── src/index.ts             # Webapp source (TypeScript)
│   │   ├── src/index.js             # tsc output (consumed by build.js)
│   │   ├── src/index.html.tpl       # HTML template with ${styles}/${js}/${favicon}/${manifest}/${icon1024}
│   │   ├── src/style.css            # CSS with ${oswald}/${material} base64 font placeholders
│   │   ├── src/manifest.json        # PWA manifest (with ${icon1024} placeholder)
│   │   └── assets/                  # fonts (oswald, material), icons (gbsc-logo, icon-1024*)
│   ├── pro/                         # ── GBSC-Pro layer ──
│   │   ├── gbs-control-pro.h        # PRO declarations: enums (InputSource/InputType), ADV_* prototypes, GBS_FW_VERSION
│   │   ├── gbs-control-pro.cpp      # PRO implementations: ADV wrappers, switchInput(), broadcastProStatus()…
│   │   ├── options-pro.h            # PRO option enums + USER_OPTIONS_PRO_FIELDS macro (guard: OPTIONS_PRO_H_)
│   │   ├── drivers/
│   │   │   ├── adv_controller.h     # ADV UART protocol: frames, commands 'S'/'T'/'N'/'C', I2P/ACE/filter/comb defaults
│   │   │   ├── ir_remote.h          # NEC key codes (kRecvPin = 2)
│   │   │   ├── pt2257.h             # PT2257 audio attenuator (PT2257_ADDR 0x44)
│   │   │   └── stv9426.h            # STV9426 TV OSD (ADDR_STV 0x5D)
│   │   ├── menu/                    # OLED menu state machine
│   │   │   ├── menu-registry.h      # OLED_MenuState enum + MENU_ITEMS_* X-macros
│   │   │   ├── menu-core.h/.cpp     # Dispatch (IR_handleMenuSelection etc.)
│   │   │   ├── menu-presets.h/.cpp  # Menu presets/helpers
│   │   │   ├── handlers/menu-*.cpp  # Per-section handlers: main, output, input, adv, color, screen,
│   │   │   │                        #   profile, preferences, misc, system, developer
│   │   │   └── helpers/menu-helpers.cpp
│   │   └── osd/                     # TV (STV9426) OSD system
│   │       ├── osd-registry.h       # OsdCommand enum, OSD_MAX_MENU_ROWS 3, OSD_CLOSE_TIME 16000, X-macros
│   │       ├── osd-core.h/.cpp      # OSD_handleCommand dispatch, oledToOsdMap[]
│   │       ├── handlers/osd-*.cpp   # Per-section renderers (adv, color, developer, firmware, input, main,
│   │       │                        #   misc, output, preferences, profile, screen, system)
│   │       └── helpers/osd-helpers.cpp
│   ├── webui.html                   # GENERATED: single-file webapp (do not hand-edit)
│   └── webui_html.h                 # GENERATED: gzipped webui.html as const uint8_t webui_html[] PROGMEM
└── gbsc-pro-flasher/                # ── Python flashing tool ──
    ├── gbsc_flasher.py              # Single-file tool: GUI (PySide6) + CLI, auto-detect, YMODEM + esptool
    ├── requirements.txt             # pyserial, esptool, PySide6 (optional)
    └── README.md                    # Flasher usage docs
```

## 3. Architecture and data flow

### 3.1 System block diagram

```
                 ┌────────────────────────── ESP8266 (gbs-control firmware) ─────────────────────────┐
                 │  setup()/loop()  ·  OLED menu (rotary+IR)  ·  TV OSD  ·  web UI  ·  serial        │
                 │        │                        │                    │                             │
                 │   uopt (persisted)        rto (runtime state)   adco (ADC cal)                     │
                 └────┬──────────────────┬──────────────────┬───────────┬──────────────────┬──────────┘
             I2C 0x17 │           I2C    │             UART  │    I2C    │   I2C            │
            ┌─────────▼───┐     ┌────────▼────┐     ┌────────▼────┐  ┌──▼──────┐  ┌────────▼──────┐
            │   TV5725    │     │ SSD1306 OLED│     │  ADV MCU    │  │ STV9426 │  │   Si5351      │
            │ scaler/core │     │  (0x3C,     │     │ HC32F460    │  │  TV OSD │  │ clock gen     │
            │ (GBS:: regs)│     │   D2/D1)    │     │  (ADV_FW)   │  │  (0x5D) │  │  (I2C)        │
            └──────▲───────┘     └─────────────┘     └──▲─────────┘  └─────────┘  └───────────────┘
                   │ RGB/YpBPr/VGA/RGBHV                        │ ADV7280/ADV7391 S-Video/composite
   analog input ───┘                                            │
                   └────────────────────  output: RGBS/VGA/Component/HDMI ─────────────┘
```

### 3.2 Boot sequence (`setup()`, `gbs-control/gbs-control.ino:7857`)

1. `system_update_cpu_freq(160)` — run at 160 MHz.
2. OLED init: `SSD1306Wire display(0x3c, D2, D1)` (`gbs-control.ino:42`).
3. IR receiver: `irrecv.enableIRIn()` (pin 2, `pro/drivers/ir_remote.h`).
4. `OSD_clearAll()` / `OSD_init()` (STV9426 TV OSD).
5. PT2257 audio: `PT2257_mute()` / `PT2257_setVolume()` from `uopt` (pin-independent I2C 0x44).
6. Rotary encoder ISRs: `isrRotaryEncoderRotateForNewMenu` / `isrRotaryEncoderPushForNewMenu`; pins `pin_clk=14`, `pin_data=13`, `pin_switch=0` (`gbs-control.ino:43-45`) — **GPIO0 must be pulled HIGH at power-on or the chip won't boot**.
7. `initOLEDMenu()` / `initOSD()`; Serial @ 115200.
8. `startWire()`; GBS dummy I2C reads to wake the TV5725.
9. WiFi: `WiFi.persistent(false)`, `WIFI_PHY_MODE_11G`, `WiFi.setOutputPower(18.0f)`, `startWebserver()` (async server on :80, WS on :81, MDNS `gbscontrol.local`, AP fallback `gbscontrol`/`qqqqqqqq` — `THIS_DEVICE_MASTER`, `gbs-control.ino:120-128`).
10. `loadDefaultUserOptions()` (`gbs-control.ino:7681`), then `/preferencesv2.txt` load with per-field clamping (`gbs-control.ino:8045-8175`).
11. Runtime defaults: `rto->autoBestHtotalEnabled=true`, `rto->syncWatcherEnabled=true`, `rto->syncLockFailIgnore=16`, `rto->currentLevelSOG=5` (`gbs-control.ino:7939-7980`).
12. PRO: `applySavedInputSource()` restores `uopt->activeInputType` (`gbs-control.ino:8345`, `pro/gbs-control-pro.cpp`).
13. ~1.5 s OLED logo; GBS soft reset (`RESET_CONTROL_0x46/0x47`, `PLLAD_VCORST`).
14. LittleFS mount + `slots.bin` validation: magic `"GBSPS"` + `SLOTS_FORMAT_VERSION 0x01`; **mismatch auto-wipes all slots and preset files**.
15. Optional ADC calibration (`uopt->enableCalibrationADC`, `gbs-control.ino:8336`).

### 3.3 Main loop (`loop()`, `gbs-control/gbs-control.ino:8596`)

Each iteration (no fixed-rate scheduler; cadence via `millis()`):

| Work | Cadence | Where |
|---|---|---|
| `IR_handleMenuSelection()` + `IR_handleInput()` | every pass | `pro/menu/menu-core.cpp:174` |
| `refreshMenusOnSignalChange()` | every pass | `pro/gbs-control-pro.cpp` |
| `oledMenu.tick(oledNav)` — rotary navigation | every pass | `OLEDMenuManager` |
| PT2257 volume poll | ~400 ms | `pro/drivers/pt2257.h` |
| `handleWiFi(0)` — MDNS, PersWiFiManager, DNSServer, WS | every pass | `gbs-control.ino` |
| `ADV_applyPendingOptions()` — drain deferred UART queue | ~100 ms | `pro/gbs-control-pro.cpp:320` |
| Serial command dispatcher (stateful, 300 ms) | ~300 ms | `gbs-control.ino:9846` (`handleType2Command`) |
| `runSyncWatcher()` — SOG/phase/HS auto-tuning, input toggle, low-power | every pass | `gbs-control.ino:6415` |

### 3.4 Control paths (how a setting reaches the silicon)

All UIs converge on the same globals, then the same writers:

- **State:** `uopt` (`userOptions`, persistent), `rto` (`runTimeOptions`, volatile), `adco` (`adcOptions`, ADC gains) — declared `gbs-control.ino:205-210`, structs in `options.h` + `pro/options-pro.h`.
- **Writers to TV5725:** `writeProgramArrayNew()` (`gbs-control.ino:563`) programs 6 banks of 16-byte register segments from a PROGMEM LUT; `doPostPresetLoadSteps()` (`gbs-control.ino:3388`) applies gain, sync-processor prep, SP/coast/clamp position, PLL/SDRAM resets, DAC enable, YUV/RGB patches, HDMI limited range.
- **Paths:**
  - OLED menu (rotary) → `OLEDMenuManager` → menu handlers (`pro/menu/handlers/menu-*.cpp`) → `uopt`/`rto` → TV5725 registers / ADV UART.
  - IR remote → NEC decode (`irrecv`) → same menu state machine (`IR_handleMenuSelection`, `pro/menu/menu-core.cpp:174`).
  - TV OSD → `OsdCommand` → `OSD_handleCommand()` (`pro/osd/osd-core.cpp`) — mirrors the OLED menu via `oledToOsdMap[]`.
  - Web UI → HTTP routes (`startWebserver()`, `gbs-control.ino:10521`) + WebSocket status (`updateWebSocketData()`, `gbs-control.ino:8457`; `broadcastProStatus()`, `pro/gbs-control-pro.cpp:578`).
  - Serial → single-char command state machine (`serialCommand`, `handleType2Command`, `gbs-control.ino:9846`); web-injected commands share the same buffer.

### 3.5 Persistence map (LittleFS)

| File | Content | Writer / reader |
|---|---|---|
| `/preferencesv2.txt` | `userOptions` as a **positional stream of ASCII decimal digits** (each field in struct order; multi-digit fields: TVMODE 2 digits, BCSH 3 digits, etc.) | `saveUserPrefs()` `gbs-control.ino:11576`; loader `gbs-control.ino:8045-8175`; defaults `loadDefaultUserOptions()` `gbs-control.ino:7681` |
| `/slots.bin` | 36 slots, `SlotMeta` **exactly 128 bytes** (`slot.h:114` static_assert), header magic `"GBSPS"`, `SLOTS_FORMAT_VERSION 0x01` | `saveSlotSettingsAt()` `gbs-control.ino:4350`; `loadSlotSettings()` `gbs-control.ino:4468` |
| `/preset_<mode>.<slot>` | Custom TV5725 register program array per (video mode, slot) | `loadPresetFromLittleFS()` (decl. `gbs-control.ino:39`); deleted by `deleteAllSlotsAndPresets()` `gbs-control.ino:7658` |

### 3.6 Web UI pipeline (source → device)

```
public/src/index.ts ──tsc──► public/src/index.js
        │                            │
        │            public/scripts/build.js (node)
        │            reads index.html.tpl + index.js + style.css + manifest.json
        │            base64-embeds fonts/icons; injects GBS_FW_VERSION (parsed from pro/gbs-control-pro.h)
        ▼                            ▼
   gbs-control/webui.html  (single-file webapp)
        │
        │  public/scripts/html2h.sh : gzip -c9 → xxd -i → sed to "const uint8_t webui_html[] PROGMEM"
        ▼
   gbs-control/webui_html.h  ──#include at gbs-control.ino:9843──►  firmware
        ▼
   served at HTTP "/" via beginResponse_P(webui_html, webui_html_len)  (gbs-control.ino:10547)
```

So a web-UI change is: edit `public/src/*` → `npm run build` (regenerates `webui.html` **and** `webui_html.h`) → `pio run` → flash. `webui.html` / `webui_html.h` are generated artifacts — never hand-edit.

### 3.7 Flash pipeline (repo → hardware)

```
pio run  ──►  gbs-control/.pio/build/gbsc-pro/firmware.bin
                │
                │  gbsc-pro-flasher/gbsc_flasher.py
                │  - ADV target:  YMODEM 128-byte packets, CRC-16-CCITT, 115200, HCMGBoot bootloader
                │  - ESP target:  esptool (chip esp8266, 460800, dio/40m/4MB), write_flash 0x0
                ▼
             flashed device (boot → setup() → serves UI on gbscontrol.local)
```

## 4. Feature inventory

### 4.1 Video processing (TV5725)

| Feature | Key code |
|---|---|
| SD→HD upscaling: 240p/288p/480i/576i → 480p/576p/720p/960p("x1024")/1024p/1080p | `applyPresets()` `gbs-control.ino:4590`; LUT headers `ntsc_*.h`/`pal_*.h` |
| HD passthrough (720p/1080i/1080p/24 kHz sources) | `setOutModeHdBypass()` `gbs-control.ino:5536`; `PresetHdBypass = 0x21` (`gbs-control.ino:202`) |
| RGBHV scaling / bypass | `applyPresets(14/15)`; `bypassModeSwitch_RGBHV()` `gbs-control.ino:5833`; `PresetBypassRGBHV = 0x22` (`gbs-control.ino:203`) |
| SOG/CSync detection & auto-switching input | `detectAndSwitchToActiveInput()` `gbs-control.ino:1534` |
| Sync automation: SOG level, phase-SP scan, auto-best Htotal | `runSyncWatcher()` `:6415`, `optimizeSogLevel()` `:1432`, `optimizePhaseSP()` `:1313`, `runAutoBestHTotal()` |
| Frame-time lock (FTL) | `FrameSyncManager` `framesync.h`; toggled by serial `W`/`5` |
| Deinterlacing (adaptive/bob) + scanlines (MADPT mix) | `uopt->deintMode`, `uopt->wantScanlines`, `uopt->scanlineStrength`; write sites `gbs-control.ino:6070, 6099, 6111` |
| VDS line filter, peaking, step response, full-height | `wantVdsLineFilter` `:3889`, `wantPeaking` `:3895`, `wantStepResponse` `:3911`, `wantFullHeight` `:3578` |
| YPbPr/component output | `OutputComponentOrVGA()` `gbs-control.ino:913` |
| ADC auto-gain + offset calibration | `runAutoGain()` `:9634`, `calibrateAdcOffset()`, `adco` (invariant `options.h:97-104`) |
| External Si5351 clock generator detect/init | `externalClockGenDetectAndInitialize()` `gbs-control.ino:416`; global `Si` (`src/si5351mcu.h:118`) |
| PAL-60 forcing | `uopt->PalForce60`; remap in `applyPresets()` `gbs-control.ino:4675-4688` |
| HDMI limited-range pre-compression (MS9288) | `applyHdmiLimitedRange()` `gbs-control.ino:1030`; `uopt->hdmiLimitedRange` |
| Low-power idle with input detection | `goLowPowerWithInputDetection()` |

### 4.2 PRO features (S-Video / composite / audio / OSD)

| Feature | Key code |
|---|---|
| Input routing table (RGBs/RGsB/VGA/YPbPr/SV/AV → `GBS::ADC_INPUT_SEL`, SOG/SP sync sel) | `switchInput()` + `inputConfigs[]` `pro/gbs-control-pro.cpp:352-360` |
| ADV7280/ADV7391 decoder control over UART (I2P, smooth, ACE, BCSH, filters, comb) | `pro/drivers/adv_controller.h`; wrappers `pro/gbs-control-pro.cpp:123-250` |
| LM1881 sync stripper on/off | `uopt->advSyncStripper`; `ADV_SyncStripper_On/Off` |
| Audio (PT2257 attenuator, volume 0–50, mute) | `pro/drivers/pt2257.h`; poll in `loop()` |
| TV OSD (STV9426, 3 rows, themes, auto-close) | `pro/osd/*`, `ADDR_STV 0x5D` (`pro/drivers/stv9426.h:12`) |
| IR remote (NEC) | `pro/drivers/ir_remote.h`, `kRecvPin=2` |
| OLED menu w/ rotary encoder | `OLEDMenu*.*`, `pro/menu/*` |
| 36 profile slots (A–Z, 0–9) with per-slot PRO fields | `slot.h`; `slotIndexMap` `gbs-control.ino:212` |
| PRO WebSocket status (`$` message, 26 bytes) | `broadcastProStatus()` `pro/gbs-control-pro.cpp:578` |
| Developer overrides (NTSC/PAL groups: Htotal, PLL div, SDRAM clock, ADC filter, OSR, SOG level, sync invert, screen move/scale) | `dev*_ntsc/pal`, `screen*_ntsc/pal` in `pro/options-pro.h:115-141` |

### 4.3 Connectivity & UX

| Feature | Key code |
|---|---|
| Web UI (PWA, slots, WiFi mgmt, pro settings, dev options) | `public/src/index.ts`; served from `webui_html.h` |
| HTTP API routes | `startWebserver()` `gbs-control.ino:10521-11346` |
| WebSocket status (base `#` + PRO `$`) | `updateWebSocketData()` `gbs-control.ino:8457`; `broadcastProStatus()` |
| PersWiFiManager captive portal + mDNS `gbscontrol.local` | `PersWiFiManager.*`; AP `gbscontrol`/`qqqqqqqq` |
| ArduinoOTA support (`rto->allowUpdatesOTA`) | `ESP8266mDNS.h`/`ArduinoOTA` includes `gbs-control.ino:82-84` |
| Serial command protocol (ASCII, stateful) | `handleType2Command()` `gbs-control.ino:9846` |
| OLED i18n (pixel-rendered translations) | `generate_translations.py` → `OLEDMenuTranslations.h` |

### 4.4 Flasher features (`gbsc-pro-flasher`)

| Feature | Key code |
|---|---|
| Auto-detect device (VID/PID heuristics) + firmware (size + `0xE9` magic) | `detect_port_type()`/`detect_firmware_type()` `gbsc_flasher.py:108-177` |
| ADV (HC32F460) YMODEM flasher (128 B packets, CRC-16-CCITT, 115200) | `ADVFlasher` `gbsc_flasher.py:262` |
| ESP8266 esptool flasher (460800, dio, 40 MHz, 4 MB) | `ESPFlasher` `gbsc_flasher.py:474` |
| Selective erase: user area `0x000000–0x3FC000`, WiFi/RF_CAL `0x3FC000–0x400000`, or full chip | `ESPFlasher.erase()` `gbsc_flasher.py:500-556`; constants `gbsc_flasher.py:66-69` |
| GUI (PySide6): port hot-plug, drag & drop, erase checkboxes, progress | `run_gui()` `gbsc_flasher.py:812` |
| CLI: positional `port firmware` / `firmware`, `--adv/--esp/--list/--version/--erase-user/--erase-wifi` | `main()` `gbsc_flasher.py:1309` |

## 5. Configuration reference

### 5.1 Compile-time switches

| Switch | Value | Defined at | Effect |
|---|---|---|---|
| `HAVE_BUTTONS` | 0 | `gbs-control.ino:34` | Legacy button polling (`handleButtons`/`readButtons`) — off |
| `USE_NEW_OLED_MENU` | 1 | `gbs-control.ino:35` | Selects `OLEDMenuManager` + PRO OSD menu vs legacy string menu (gates large `#if` blocks at `:48, 7769, 7796, 8424`) — **must stay 1** |
| `THIS_DEVICE_MASTER` | defined | `gbs-control.ino:120` | Naming: `gbscontrol.local`, AP `gbscontrol`/`qqqqqqqq` (else `gbsslave*`) |
| `HAVE_PINGER_LIBRARY` | undef | `gbs-control.ino:101` | Optional `Pinger` WiFi debug lib |
| `GBS_ADDR` | 0x17 | `tv5725.h:6` | TV5725 I2C address |
| `PT2257_ADDR` | 0x44 | `pro/drivers/pt2257.h:15` | Audio attenuator I2C address |
| `ADDR_STV` | 0x5D | `pro/drivers/stv9426.h:12` | TV OSD I2C address |
| `SLOTS_TOTAL` / `SLOTS_FORMAT_VERSION` | 36 / 0x01 | `slot.h:4, 12` | Slot count; bump version on `SlotMeta` layout change (mismatch ⇒ boot auto-wipe) |
| `OSD_MAX_MENU_ROWS` / `OSD_CLOSE_TIME` / `OSD_MUTE_CLOSE_TIME` | 3 / 16000 ms / 3000 ms | `pro/osd/osd-registry.h:36, 26-27` | OSD menu rows; auto-close timeouts |
| `kRecvPin` | 2 | `pro/gbs-control-pro.cpp:63` | IR receiver pin |
| `pin_clk` / `pin_data` / `pin_switch` | 14 / 13 / 0 | `gbs-control.ino:43-45` | Rotary encoder A/B/push (GPIO0 boot-HIGH constraint) |
| `DEBUG_IN_PIN` | D6 | `gbs-control.ino:152` | Debug input pin |
| `ap_ssid` / `ap_password` | `"gbscontrol"` / `"qqqqqqqq"` | `gbs-control.ino:122-123` | AP-mode fallback credentials |
| WiFi tuning | 11g, 18.0 dBm, NONE_SLEEP | `gbs-control.ino:7921-7923` | PHY mode / output power / sleep in `setup()` |

### 5.2 Base options — `struct userOptions` (`options.h:19-44`, defaults at `gbs-control.ino:7681-7758`)

| Field | Type / values | Default | Effect & key sites |
|---|---|---|---|
| `presetPreference` | `PresetPreference`: 0=Output960P, 1=Output480P, 2=OutputCustomized, 3=Output720P, 4=Output1024P, 5=Output1080P, 6=OutputDownscale, 10=OutputBypass (`options.h:4-13`) | 0 | Boot output resolution for `applyPresets()` `:4590`. Mapping (verified `:4762-4819`): 0→`ntsc_240p`/`pal_1280x1024`¹, 1→`ntsc_720x480`/`pal_768x576`, 2→custom from LittleFS, 3→`ntsc_1280x720`/`pal_1280x720`, 4→`ntsc_1280x1024`²/`pal_1280x1024`, 5→`ntsc_1920x1080`/`pal_1920x1080`, 6→`ntsc_downscale`/`pal_downscale`. ¹ with `matchPresetSource` 1² with `matchPresetSource && result!=8 && RGBHV scaling off`, NTSC falls back to `ntsc_240p` |
| `presetSlot` | `'A'-'Z'/'0'-'9'` | `'A'` | Active slot; per-slot preset files `/preset_<mode>.<slot>` |
| `enableFrameTimeLock` | 0/1 | 0 | FTL via `FrameSync` (`framesync.h`) |
| `frameTimeLockMethod` | 0=move VS (Vtotal+VSST), 1=1 Vtotal only | 0 | `framesync.h:265, 453` |
| `enableAutoGain` | 0/1 | 0 | Apply `adco` gains on stable source (`:9634`); invariant `options.h:97-104` |
| `wantScanlines` | 0/1 | 0 | MADPT scanline effect; 480i/576i only with `deintMode==1` (`:6070`) |
| `wantOutputComponent` | 0/1 | 0 | YUV/component out (`OutputComponentOrVGA()` `:913`); **hidden in PRO web UI** (`public/src/index.ts:733`) |
| `deintMode` | 0=adaptive, 1=bob (load clamps >2) | 0 | Bob required for scanlines |
| `wantVdsLineFilter` | 0/1 | 0 | `VDS_D_RAM_BYPS = !want` (`:3889`) |
| `wantPeaking` | 0/1 | 1 | `VDS_PK_Y_H_BYPS = !want` (`:3895`); required for sharpness (`:4508`) |
| `wantTap6` | 0/1 | 1 | **Stored but ineffective** — `VDS_TAP6_BYPS` code commented out (`:3901-3909`) |
| `preferScalingRgbhv` | 0/1 | 1 | RGBHV-compatible scaling when `rto->isValidForScalingRGBHV` (`:725, 7015`); **hidden in PRO web UI** (`index.ts:682`) |
| `PalForce60` | 0/1 | 0 | PAL-50 → NTSC-60 presets (`:4675-4688`) |
| `disableExternalClockGenerator` | 0/1 | 0 | Skip Si5351 detect/init (`:416-427`) |
| `matchPresetSource` | 0/1 | 1 | Force preset to match source family (`:4777, 4793`) |
| `wantStepResponse` | 0/1 | 1 | `VDS_UV_STEP_BYPS=0` except 1080p presets (`:3911-3920`) |
| `wantFullHeight` | 0/1 | 1 | Full-height vertical scaling for NTSC (`:3578+`) |
| `enableCalibrationADC` | 0/1 | 1 | Boot-time ADC calibration (`:8336`) |
| `scanlineStrength` | 0x10–0x50 step 0x10 (0x50 = off) | 0x30 | MADPT mix offsets (`:6099, 6111`) |
| `adcOptions.r/g/b_gain`, `.r/g/b_off` | 0–255 | set by calibration | Mirror of `GBS::ADC_RGCTRL` — must be re-synced whenever auto-gain or that register is written (`options.h:97-104`) |

### 5.3 PRO options — `USER_OPTIONS_PRO_FIELDS` (`pro/options-pro.h:67-143`, defaults `gbs-control.ino:7704-7757`)

| Field | Type / values | Default | Effect & key sites |
|---|---|---|---|
| `INPUT_presetPreference` | `INPUT_PresetPreference`: MT_RGBs, MT_RGsB, MT_VGA, MT_YPBPR, MT_SV, MT_AV (`options-pro.h:19-26`) | MT_RGBs | PRO input-source selection (`pro/menu/menu-core.cpp:440`) |
| `SETTING_presetPreference` | MT_I2P_*/MT_SMOOTH_*/MT_SYNCSTRIPPER_*/MT_ACE_* (`options-pro.h:32-41`) | MT_I2P_OFF | PRO video-processing selection (`menu-core.cpp:482`) |
| `TVMODE_presetPreference` | 0=Auto, 1=PAL, 2=NTSC-M, 3=PAL-60, 4=NTSC-443, 5=NTSC-J, 6=PAL-Nw/p, 7=PAL-Mw/o p, 8=PAL-M, 9=PAL Cmb-N, 10=PAL Cmb-N w/p, 11=SECAM (`options-pro.h:47-60`) | 0 | ADV7280 TV mode (`pro/gbs-control-pro.cpp:599`); **serialized as 2 ASCII digits** |
| `volume` | 0–50, **50 = max, 0 = mute** (inverted scale) | 38 | `PT2257_setVolume()` (`pro/drivers/pt2257.h:35`); applied at load `gbs-control.ino:8149` |
| `audioMuted` | 0/1 | 0 | `PT2257_mute()` (`gbs-control.ino:8148`) |
| `activeInputType` | 1–6: RGBs/RGsB/VGA/YUV/SV/AV (`InputType`, `pro/gbs-control-pro.h:83-90`) | 1 | Routes active input; 5/6 go through ADV7280; validated 1–6 at load (`:8151-8153`) |
| `svVideoFormat` / `avVideoFormat` | 0=Auto, 1–11 | 0 | ADV7280 S-Video / composite format (`pro/osd/handlers/osd-adv.cpp:245`) |
| `bcshAdjustMode` | 0–2 (clamped >2→0) | 0 | BCSH UI mode |
| `advSyncStripper` | 0=off, 1=on | 1 | LM1881 stripper (`ADV_SyncStripper_On/Off`, `adv_controller.h:61-62`); hidden for SV/AV |
| `osdTheme` | 0–3 | 0 | TV OSD theme |
| `gbsColorR/G/B` | 0–255 | 128 each | TV5725 color balance; 3 ASCII digits each |
| `advI2P` | 0/1 | **0** (header comment claims 1 — trust code, `gbs-control.ino:7720`) | ADV7280 interlace-to-progressive (`adv_controller.h:58-59`) |
| `advSmooth` | 0/1 | 0 | ADV7280 smooth interpolation |
| `advACE` | 0/1 | 0 | ADV7280 ACE on/off |
| `advBrightness/Contrast/Saturation/Hue` | 0–254 (Hue 128 = 0°) | 128 each | BCSH command; 3 ASCII digits each |
| `advACELumaGain/ChromaGain/ChromaMax/GammaGain/ResponseSpeed` | 0–31 / 0–15 / 0–15 / 0–15 / 0–15 | 13/8/8/8/15 (`ADV_ACE_*_DEFAULT`, `adv_controller.h:81-85`) | ACE subcommands 0x82–0x86, reset 0x87 |
| `advFilterYShaping/CShaping/WYShaping/WYOverride` | 0–30 / 0–7 / 2–19 / 0–1 | 1/0/19/1 (`ADV_FILTER_*_DEFAULT`, `adv_controller.h:113-118`) | Shaping 0xB0–0xB3, reset 0xB7 |
| `advFilterCombNTSC/PAL` | 0–3 | 0 (Narrow) / 1 (Medium) | Comb bandwidth 0xB4/0xB5 |
| `advCombLumaModeNTSC/ChromaModeNTSC/ChromaTapsNTSC` | 0,4,5–7 / 0,4,5–7 / 0–3 | 0 (not set in `loadDefaultUserOptions`; intended 0/0/2, `adv_controller.h:123-125`) | NTSC comb 0xB8–0xBA |
| `advCombLumaModePAL/ChromaModePAL/ChromaTapsPAL` | same | 0 (intended 0/0/3, `adv_controller.h:126-128`) | PAL comb 0xBB–0xBD |
| `hdmiLimitedRange` | 0=Off, 1=HD, 2=SD, 3=All | 1 | `applyHdmiLimitedRange()` `gbs-control.ino:1030`; skipped in HD bypass |
| `devHTotal_ntsc / devPllDiv_ntsc / devSdramClock_ntsc / devAdcFilter_ntsc / devOsr_ntsc / devSogLevel_ntsc / devSyncInvert_ntsc` | 16-bit (0 = no override) / 8-bit (0xFF = no override) | 0 / 0xFF | Developer overrides: `VDS_HSYNC_RST`, `PLLA_D_MD`, `PLL_MS`, `ADC_FLTR`, `OSR`, `ADC_SOGCTRL`, sync-invert bits |
| `screenHMove_ntsc / screenVMoveSt_ntsc / screenVMoveSp_ntsc / screenHScale_ntsc / screenVScale_ntsc` | 16-bit; 0 (VMove: 0xFFFF) = no override | 0 / 0xFFFF | `IF_HBIN_SP`, `IF_VB_ST/SP`, `VDS_HSCALE`, `VDS_VSCALE` |
| `dev*_pal` (7) / `screen*_pal` (5) | same types/semantics | same | PAL-group (videoStandardInput 2/4) variants |
| `slotSyncwatcherMode` | 0=inherit, 1=force ON, 2=force OFF | 0 | Per-slot SyncWatcher override |

### 5.4 Runtime state — `struct runTimeOptions` (`options.h:48-93`, pointer `rto`)

Not persisted; notable fields: `videoStandardInput` (video-mode code, see §9.1), `noSyncCounter`, `syncLockFailIgnore`, `continousStableCounter`, `presetID` (== `GBS_PRESET_ID` register), `isCustomPreset`, `HPLLState`, `osr`, `clampPositionIsSet`/`coastPositionIsSet`/`phaseIsSet`, `inputIsYpBpR`, `syncWatcherEnabled`, `outModeHdBypass`, `allowUpdatesOTA`, `autoBestHtotalEnabled`, `videoIsFrozen`, `forceRetime`, `deinterlaceAutoEnabled`, `scanlinesEnabled`, **`boardHasPower`** (gates all video work), `presetIsPalForce60`, `syncTypeCsync`, `isValidForScalingRGBHV`, `useHdmiSyncFix`, `extClockGenDetected`, `phaseSP`/`phaseADC`, `currentLevelSOG`/`thisSourceMaxLevelSOG`, `notRecognizedCounter`, `isInLowPowerMode`, `applyPresetDoneStage`, `presetVlineShift`, `freqExtClockGen` (default 81 MHz), `webServerEnabled`/`webServerStarted`, `printInfos`, `sourceDisconnected`, `motionAdaptiveDeinterlaceActive`, `wantPeaking`-derived `isPeakingLocked` (`VDS_PK_LB_GAIN != 0x16`).

### 5.5 Video-mode codes — `rto->videoStandardInput` (decoded by `getVideoMode()`, `gbs-control.ino:4868`)

| Code | Meaning |
|---|---|
| 0 | unknown (fallback preset path in `applyPresets()` `:4635`) |
| 1 | NTSC interlaced |
| 2 | PAL interlaced |
| 3 | 480p (EDTV 60) |
| 4 | 576p (EDTV 50) |
| 5 | 720p |
| 6 | 1080i |
| 7 | 1080p |
| 8 | 24 kHz (2376×1250) |
| 9 | unrecognized (stable after `notRecognizedCounter == 255`) |
| 13 | graphics RGB-over-YUV (SOG) |
| 14 | RGBHV scaling |
| 15 | RGBHV bypass |

## 6. Build and tooling (exact commands)

> All commands assume the repository root is `GBSC_FW_from_GBSCPRO`.
> Toolchain: **PlatformIO Core (CLI or VS Code extension)**, **Node.js + npm**, **bash + gzip + xxd + sed** (Linux/macOS, or Git Bash / WSL on Windows), **Python ≥ 3.8** with Pillow (for i18n).

### 6.1 Firmware (PlatformIO)

```bash
cd gbs-control
pio run                          # build env "gbsc-pro"
# output:  gbs-control/.pio/build/gbsc-pro/firmware.bin

pio run --target upload          # flash over serial (upload_speed 460800 in platformio.ini)
pio device monitor               # serial monitor @ 115200
pio run --target uploadfs        # (if needed) upload LittleFS contents — slots/presets are written by the device itself
```

Relevant settings (`gbs-control/platformio.ini:15-34`): `espressif8266@4.2.1`, `board = d1_mini`, `board_build.f_cpu = 160000000L`, `board_build.ldscript = eagle.flash.4m1m.ld`, `board_build.filesystem = littlefs`, `board_build.flash_mode = qio`, source filter `+<**/*.c> +<**/*.cpp> +<**/*.ino> -<./3rdparty/*>`. Libs come from `lib_dir = ./src/` plus `lib_deps` (esp32async forks, SSD1306Wire).

> ⚠️ The flasher's `gbsc-pro-flasher/README.md` says the output is `.pio/build/esp8266/firmware.bin` — that is **wrong for this tree**; the env is named `gbsc-pro`, so the real path is `.pio/build/gbsc-pro/firmware.bin`.

### 6.2 Web UI (npm + html2h)

```bash
cd gbs-control/public
npm install
npm run build
#   = tsc ./src/index.ts --target ES6            # index.ts → src/index.js
#   && cd scripts && node ./build.js             # → ../webui.html  (inlines JS/CSS/fonts/icons, injects GBS_FW_VERSION)
#   && ./html2h.sh                               # → ../webui_html.h (gzip -c9 + xxd -i + sed → PROGMEM C array)
```

Verify after a build that both artifacts changed: `gbs-control/webui.html` and `gbs-control/webui_html.h`.

Development loop (no hardware):

```bash
cd gbs-control/public
npm start          # tsc --watch ./src/index.ts   (terminal 1)
npm run dev        # dev-server.js: HTTP :8080 + WebSocket mock :81 (terminal 2)
# open http://localhost:8080
```

### 6.3 i18n translations (optional)

```bash
cd gbs-control
python generate_translations.py            # needs Pillow; default output OLEDMenuTranslations.h
python generate_translations.py --help     # supports --fonts / --output
```

### 6.4 Flasher (Python)

```bash
cd gbsc-pro-flasher
pip install -r requirements.txt            # pyserial, esptool, PySide6 (optional GUI)

python gbsc_flasher.py                     # GUI mode
python gbsc_flasher.py --list              # list detected devices + all serial ports
python gbsc_flasher.py firmware.bin        # auto-detect device & port, flash
python gbsc_flasher.py COM3 firmware.bin   # explicit port
python gbsc_flasher.py --adv firmware.bin  # force ADV (HC32F460, YMODEM)
python gbsc_flasher.py --esp firmware.bin  # force ESP8266 (esptool)
python gbsc_flasher.py --erase-wifi                    # ESP only: wipe WiFi/RF_CAL (0x3FC000–0x400000)
python gbsc_flasher.py --erase-user firmware.bin       # ESP only: wipe user area 0x000000–0x3FC000, then flash
python gbsc_flasher.py --erase-user --erase-wifi       # ESP only: full chip wipe
```

Flasher internals: ADV = YMODEM (128-byte packets, CRC-16-CCITT, 115200, chip probe `0x55`, download-mode `'1'`, `'C'` handshake; `ADVFlasher` `gbsc_flasher.py:262`). ESP = `esptool.main(["--chip","esp8266","--port",…,"--baud","460800","--before","default_reset","--after","hard_reset","write_flash","--flash_mode","dio","--flash_freq","40m","--flash_size","4MB","0x0",fw])` (`ESPFlasher.flash()` `gbsc_flasher.py:558`). Auto-detection: ADV = VID 0x2E88/PID 0x4603 (XHSC) or "usbmodem" nodes; ESP = CH340 (0x1A86) or CP210x (0x10C4); firmware type from size (<256 KB ⇒ ADV, ≥256 KB ⇒ ESP) or leading `0xE9` byte.

> Note: the `gbsc_flasher.py` docstring mentions `--auto`, but that flag does **not** exist in the argparse implementation (`main()`, `gbsc_flasher.py:1338-1354`) — single-argument `firmware.bin` invocation is the auto-detect mode.

### 6.5 End-to-end: change → flashed device

1. Make code/setting changes in `gbs-control/` (see §7).
2. If the web UI changed: `cd gbs-control/public && npm run build` (regenerates `webui.html` + `webui_html.h`).
3. `cd gbs-control && pio run` → `.pio/build/gbsc-pro/firmware.bin`.
4. Flash: `pio run --target upload` (direct) **or** `cd ../gbsc-pro-flasher && python gbsc_flasher.py --esp .pio/build/gbsc-pro/firmware.bin` from repo root.
5. Verify: OLED menu / TV OSD / `http://gbscontrol.local` / serial @ 115200.
6. ADV controller firmware (HC32F460) is **not built in this repo** — flash a prebuilt ADV `.bin` with `python gbsc_flasher.py --adv <file>` (device must be in HCMGBoot mode: hold button while connecting USB).

## 7. How-to runbook (step-by-step for common changes)

### 7.1 Add a new persisted user setting

1. **Declare** the field:
   - base setting → `struct userOptions` in `gbs-control/options.h:19-44`;
   - PRO setting → `USER_OPTIONS_PRO_FIELDS` macro in `gbs-control/pro/options-pro.h:67-143` (add an enum to one of `INPUT_/SETTING_/TVMODE_PresetPreference` if it's a choice list).
2. **Default**: `loadDefaultUserOptions()` — `gbs-control/gbs-control.ino:7681-7758`.
3. **Persist**: add a load line (with range clamping) in the `/preferencesv2.txt` loader `gbs-control.ino:8045-8175` **and** a save line in `saveUserPrefs()` `gbs-control.ino:11576-11676`. Position must match the fixed serialization order; multi-digit values are written as decimal ASCII digits.
4. **Per-slot?** add the field to `SlotMeta` in `gbs-control/slot.h` (keep `sizeof(SlotMeta) == 128` — `static_assert` at `slot.h:114`) and **bump `SLOTS_FORMAT_VERSION` (`slot.h:12`)** if layout changes; then wire `saveSlotSettingsAt()` (`gbs-control.ino:4350`) / `loadSlotSettings()` (`gbs-control.ino:4468`).
5. **UI**: menu handler `gbs-control/pro/menu/handlers/menu-*.cpp`; OSD handler `gbs-control/pro/osd/handlers/osd-*.cpp`; web field table `gbs-control/public/src/index.ts` (and `webui.html` is regenerated by `npm run build`).
6. **Effect**: implement at the consumption site — `applyPresets()` `:4590`, `applyRGBPatches()`/`applyYuvPatches()` `:970-1024`, `OutputComponentOrVGA()` `:913`, `externalClockGenDetectAndInitialize()` `:416`, `framesync.h` (timing), or `pro/gbs-control-pro.cpp` (ADV7280/PT2257).
7. **Build & verify**: `pio run` (and `npm run build` in `public/` if the web UI changed); test via OLED menu, TV OSD, and web UI.

### 7.2 Add a menu item (OLED + TV OSD)

1. Add enum value to `OLED_MenuState` in `gbs-control/pro/menu/menu-registry.h`.
2. If mirrored on TV: add `OsdCommand` value in `gbs-control/pro/osd/osd-registry.h`.
3. Register in the `MENU_ITEMS_*` / `OSD_DISPATCH_ENTRIES` X-macros (same headers) so `oledToOsdMap[]` and the dispatch tables pick it up.
4. Implement IR dispatch handler in `gbs-control/pro/menu/handlers/menu-*.cpp` (dispatch: `IR_handleMenuSelection()`, `pro/menu/menu-core.cpp:174`).
5. Implement OSD render in `gbs-control/pro/osd/handlers/osd-*.cpp` (dispatch: `OSD_handleCommand()`, `pro/osd/osd-core.cpp`).
6. Add i18n string tag to the `menu_items` list in `gbs-control/generate_translations.py` and regenerate `OLEDMenuTranslations.h`.
7. `pio run` → flash → verify with rotary encoder + IR.

### 7.3 Add a new output resolution / preset LUT

1. Create a PROGMEM array header following the pattern of `gbs-control/ntsc_1920x1080.h` (6 banks of 16-byte segments; MD/deinterlacer/bypass sections per `presetMdSection.h` etc.).
2. `#include` it at the top of `gbs-control/gbs-control.ino` (lines 1-12).
3. Map it in `applyPresets()` — either a new `uopt->presetPreference` value (add to `enum PresetPreference`, `options.h:4-13`, plus the dispatch block `gbs-control.ino:4762-4819`) or a new video-mode code in `getVideoMode()` (`gbs-control.ino:4868`) if it needs source detection.
4. Update `updateWebSocketData()` preset-id switch (`gbs-control.ino:8457`) and the web UI button mapping so the UI shows it.
5. Rebuild + flash; verify with a matching source signal.

### 7.4 Change sync / auto-detect behavior

Edit in `gbs-control/gbs-control.ino`:

- `runSyncWatcher()` `:6415` (SOG-bad windows `STATUS_INT_SOG_BAD/SOG_SW`, no-sync `ADC_INPUT_SEL` toggle every 413 ticks, low-power entry).
- `detectAndSwitchToActiveInput()` `:1534` (SOG test, `STATUS_SYNC_PROC_VSACT/HSACT` waits, CSync decode via `getSourceFieldRate(1)` — ≥2 of 3 > 40 Hz ⇒ `rto->syncTypeCsync`).
- `optimizeSogLevel()` `:1432`, `optimizePhaseSP()` `:1313` (34-step `phaseSP` scan vs `STATUS_SYNC_PROC_HTOTAL`).
- `runAutoBestHTotal()` / `applyBestHTotal()` + `FrameSync::init()` (`framesync.h`).

Thresholds are inline literals (e.g., SOG window ~3000 ms near `:6478`) — there is no config table for them.

### 7.5 Add/modify a PRO menu or setting that talks to the ADV MCU

1. Packet constants: `gbs-control/pro/drivers/adv_controller.h` (frame `[0x41 0x44][cmd][data][random][0xFE][checksum]`, commands `'S'` source, `'T'` TV mode, `'N'` BCSH, `'C'` custom I2C; defaults `ADV_ACE_*_DEFAULT`, `ADV_FILTER_*_DEFAULT`, `ADV_COMB_*_DEFAULT`).
2. Wrappers: `gbs-control/pro/gbs-control-pro.cpp:123-250`.
3. Routing flags for inputs: `inputConfigs[]` table, `pro/gbs-control-pro.cpp:352-360` (sets `GBS::ADC_SOGEN`, `SP_EXT_SYNC_SEL`, `ADC_INPUT_SEL`; color defaults RGB 128/128/128 vs YUV 129/123/132; `applyRGBtoYUVConversion()`).
4. Remember ADV writes are **queued** — the only flush point is `ADV_applyPendingOptions()` (`pro/gbs-control-pro.cpp:320`, called ~100 ms in `loop()`).

### 7.6 Change the web API / WebSocket protocol

1. HTTP routes: lambdas in `startWebserver()` — `gbs-control/gbs-control.ino:10521-11346` (routes include `/`, `/sc`, `/uc`, `/wifi/connect`, `/bin/slots.bin`, `/slot/set|save|remove`, `/filesystem/upload|download|dir|format`, `/wifi/status`, `/gbs/restore-filters`, `/pro`).
2. WebSocket payloads: `updateWebSocketData()` (`gbs-control.ino:8457`) — 6-byte base status `#` + preset digit + slot + 3 bit-flag bytes; then `broadcastProStatus()` (`pro/gbs-control-pro.cpp:578`) — 26-byte `$` message (input 1–6, TV format, I2P/Smooth/Sharpness/ACE, ACE gains, Y/C/WY filters, comb NTSC/PAL, `hdmiLimitedRange`, `advSyncStripper`, hue).
3. **Both must stay byte-position compatible with the webapp** — update the parser in `gbs-control/public/src/index.ts` in the same change.
4. Keep the dev-server mock in sync: `gbs-control/public/dev-server.js`.
5. `npm run build` in `public/`, then `pio run`.

### 7.7 Change the serial command protocol

- Parser is the stateful single-char `serialCommand` dispatcher in `gbs-control/gbs-control.ino` (multi-stage `s`/`t`/`g` register writes use `inputStage`/`segmentCurrent`/`registerCurrent`; `handleType2Command()` at `:9846`).
- Current commands: `d` dump registers (6 segments), `+`/`-` `shiftHorizontalLeft/Right`, `0`/`1` `moveHS`, `4`/`5` `scaleVertical`, `6`/`7` canvas moves, `l` `resetSyncProcessor()`, `W` FTL toggle, `E`/`R` load NTSC/PAL 1280×1024 presets, `Z` `matchPresetSource` toggle, `D` debug view, `s`/`t`/`g` segment/byte register writes; type-2 set: `0` PAL-force-60, `1` factory reset (`deleteAllSlotsAndPresets()` + `InputRGBs()` + `ESP.reset()`), `3`/`4` load/save custom preset, `5` FTL toggle, `7` scanlines, `^` deferred slot load, `e` list LittleFS, `a` restart.
- `@` (0x40) is the "no command" sentinel; the same buffer accepts web-injected commands — keep both paths working.

### 7.8 Update the web UI bundle after any `public/src` change

```bash
cd gbs-control/public && npm run build
# then: cd .. && pio run && pio run --target upload
```

### 7.9 Flash the device

- Firmware: `cd gbs-control && pio run --target upload`, or via the flasher: `python gbsc-pro-flasher/gbsc_flasher.py --esp gbs-control/.pio/build/gbsc-pro/firmware.bin`.
- ADV (prebuilt `.bin`, device in bootloader mode): `python gbsc-pro-flasher/gbsc_flasher.py --adv path/to/adv.bin`.
- Wipes: `--erase-user` (keep WiFi), `--erase-wifi`, or both (full chip) — ESP8266 only.

### 7.10 Factory reset / restore

- Firmware-side: serial command `1` (type-2 set) runs `deleteAllSlotsAndPresets()` (`gbs-control.ino:7658`) + `InputRGBs()` + `ESP.reset()`; the web UI has `/gbs/restore-filters`; LittleFS wipe via flasher erase options.
- Boot-time auto-wipe: `slots.bin` magic `"GBSPS"` / version mismatch wipes slots + presets (`gbs-control.ino` setup path, format in `slot.h:10-22`).

### 7.11 Regenerate i18n

```bash
cd gbs-control
python generate_translations.py --output OLEDMenuTranslations.h
# then pio run
```

## 8. Gotchas and invariants

**Storage & format**

- `SlotMeta` must stay **exactly 128 bytes** — enforced by `static_assert` at `gbs-control/slot.h:114`; the webapp depends on the same layout. `SlotsFileHeader` is 16 bytes (`slot.h:22`).
- `slots.bin` magic `"GBSPS"` + `SLOTS_FORMAT_VERSION 0x01` (`slot.h:10-12`); **any mismatch auto-wipes all slots and preset files on boot** — bump the version deliberately.
- `/preferencesv2.txt` is a **positional ASCII digit stream**: loader (`gbs-control.ino:8045-8175`), saver (`:11576-11676`), and the web-UI field tables all encode the same field order independently. Adding a field in only one place silently desyncs every later field.
- **Known bug**: at `gbs-control.ino:8119-8120`, a loaded `scanlineStrength > 0x60` writes `0x30` into `enableCalibrationADC` (wrong variable) instead of clamping `scanlineStrength`.
- Per-slot options diverge from globals on slot switch (copy `uopt`→slot and slot→`uopt`, `gbs-control.ino:4281-4497`); editing a global does not rewrite existing slots.
- `webui.html` / `webui_html.h` are generated — regenerate via `npm run build`, never edit by hand.

**Execution invariants**

- `rto->boardHasPower` gates `applyPresets()` and `runSyncWatcher()` — a dead TV5725 I2C link silently skips all video work ("GBS board not responding!", `gbs-control.ino:4592`).
- `ESP.reset()` (not `restart()`) is used deliberately to avoid WebSocket reconnect breakage.
- `handleWiFi(0)` must be pumped inside long blocking loops (drives PersWiFiManager, DNSServer, MDNS, OTA).
- `ADV_applyPendingOptions()` is the **only** UART flush point (loop, ~100 ms) — ADV writes are queued, not immediate; `switchInput()` also calls `resetSyncProcessor()` and sets `rto->sourceDisconnected=true` to force re-detection.
- `applyPresets(14)` (RGBHV scaling) requires `rto->syncTypeCsync` to be known up front (`gbs-control.ino:4599-4608`); mode 15 bypass (`bypassModeSwitch_RGBHV()` `:5833`, `setOutModeHdBypass()` `:5536`) reconfigures SP/coast/PLL differently and is order-sensitive; `rto->outModeHdBypass` toggling flips `autoBestHtotalEnabled` in `doPostPresetLoadSteps()` (`:3491-3496`).
- `uopt->presetPreference == 2` (custom) changes multiple code paths (debug-view carry-over `:4627`, gain handling in `doPostPresetLoadSteps()`).
- `presetPreference == 10` (bypass) is silently rewritten to `Output960P` when a preset actually gets applied (`gbs-control.ino:7079, 7192`).
- `adco` invariant (`options.h:97-104`): whenever `enableAutoGain` is set or `GBS::ADC_RGCTRL` is written, the `adco->r_gain` mirror must be re-synced.
- `isPeakingLocked()` is **derived** state (`VDS_PK_LB_GAIN != 0x16`), not a stored flag.
- Scanlines on 480i/576i hard-require `deintMode == 1` (`gbs-control.ino:6070-6071`); `deintMode` load accepts 0–2 (clamps >2) but UIs expose only 0/1.
- Serial parser is shared state between real serial and web-injected commands; `@` (0x40) is the sentinel.

**Hardware / platform**

- **GPIO0 (`pin_switch`) must be held HIGH at power-on** or the ESP8266 fails to boot (`gbs-control.ino:45`).
- `digitalRead` is macro-redefined to a raw GPIO register read (`gbs-control.ino:164`) — don't use it inside ISR-sensitive code without knowing this.
- `uopt->volume` scale is **inverted** in the PT2257 path: 50 = max, 0 = mute.
- IR receiver (GPIO2) shares a pin with I2C SDA on some pinouts — pin map is hardware-specific (D1 Mini vs NodeMCU differ; see `DEBUG_IN_PIN` comment at `gbs-control.ino:152-154`).
- `broadcastProStatus()` is only sent when `ESP.getFreeHeap() > 6000`; base `#` status always precedes it in the same broadcast — the webapp parses positionally.
- `wantOutputComponent` and `preferScalingRgbhv` are deliberately **hidden in the PRO web UI** (`public/src/index.ts:733, 682`) — changing their semantics affects non-PRO builds.
- `wantTap6` is persisted and UI-toggleable but has **no hardware effect** (write code commented out, `gbs-control.ino:3901-3909`).
- `USE_NEW_OLED_MENU` must stay 1 for the PRO menu/OSD paths to compile in (it gates large `#if` blocks, not just small ones).
- Include guards: `options.h` uses `_USER_H_` (grep for `OPTIONS_H` will miss it); `pro/options-pro.h` uses `OPTIONS_PRO_H_`.
- **Naming**: the chip is ESP8266; "ESP32" appears only via the esp32async library fork names and stale docs. The "960P" preset (`Output960P`, enum value 0) maps to the `ntsc_240p` / `pal_1280x1024` LUTs — there is **no** `ntsc_1280x960.h` in this tree (a module summary referenced one; the source does not have it).
- `advI2P` header comment says "default 1" (`pro/options-pro.h:84`) but the code default is 0 (`gbs-control.ino:7720`) — trust the code.
- `advComb*Mode/Taps` fields are never assigned in `loadDefaultUserOptions()` (stay 0 on fresh flash); intended defaults are `ADV_COMB_*_DEFAULT` in `pro/drivers/adv_controller.h:123-128`.

**Flasher**

- Flash mode mismatch: `platformio.ini` builds with `qio`, while the flasher writes with `--flash_mode dio` (`gbsc_flasher.py:56`) — esptool rewrites the iROM flash-mode header to `dio` on write; most D1-Mini clones boot fine with either, but know the discrepancy exists before debugging boot issues.
- Flasher README build-output path (`.pio/build/esp8266/`) is stale — real path is `.pio/build/gbsc-pro/`.
- Flasher `--auto` flag (docstring) does not exist in the implementation.
- ADV firmware size threshold: <256 KB ⇒ treated as ADV; ESP firmware is ~850 KB+ (constants `gbsc_flasher.py:72-73`).
- ESP flash regions in the flasher: user area `0x000000` + `0x3FC000`, WiFi/RF_CAL `0x3FC000` + `0x004000` (`gbsc_flasher.py:66-69`) — matches the Arduino core's `cfgSize` from end of flash.
- ADV flashing requires the device in HCMGBoot bootloader mode (hold button while plugging USB); NAK storms ⇒ re-enter bootloader.

## 9. Module deep dives

### 9.1 Core firmware — `gbs-control/gbs-control.ino`

The whole base system in one 11,678-line Arduino sketch. Notable layout (verified line anchors):

| Area | Symbol | Line |
|---|---|---|
| LUT includes | `ntsc_*.h`/`pal_*.h`/`preset*Section.h` | 1-17 |
| Pins, display, menu mode | `pin_clk/data/switch`, `display`, `USE_NEW_OLED_MENU` | 34-45 |
| Network globals | `server(80)`, `webSocket(81)`, `dnsServer`, `persWM`, AP/hostname | 120-150 |
| `digitalRead` macro | — | 164 |
| `enum PresetID` (`PresetHdBypass 0x21`, `PresetBypassRGBHV 0x22`) | — | 201-204 |
| `uopt`/`rto`/`adco` globals | — | 205-210 |
| `slotIndexMap` (36 slots) | — | 212 |
| LUT writer | `writeProgramArrayNew()` | 563 |
| Si5351 detect/init | `externalClockGenDetectAndInitialize()` | 416 |
| Preset engine | `applyPresets(uint8_t result)` | 4590 |
| Post-preset steps | `doPostPresetLoadSteps()` | 3388 |
| SOG / phase optimizers | `optimizeSogLevel()` / `optimizePhaseSP()` | 1432 / 1313 |
| Input auto-detect | `detectAndSwitchToActiveInput()` | 1534 |
| Sync automation | `runSyncWatcher()` | 6415 |
| Factory reset | `deleteAllSlotsAndPresets()` | 7658 |
| Defaults / prefs | `loadDefaultUserOptions()` / prefs loader | 7681 / 8045-8175 |
| Boot | `setup()` | 7857 |
| Web status | `updateWebSocketData()` | 8457 |
| Main loop | `loop()` | 8596 |
| Serial protocol | `handleType2Command()` | 9846 |
| `webui_html.h` include | — | 9843 |
| Web server routes | `startWebserver()` | 10521-11346 |
| Prefs save | `saveUserPrefs()` | 11576 |

Behavior notes:

- `applyPresets(result)` flow: board-power check → RGBHV sync-type probe (mode 14) → digital-reset pre-init for bypass/unknown → debug-view carry-over (skipped for custom) → unknown-mode fallback (tries RGB then YPbPr via `GBS::ADC_INPUT_SEL`) → `PalForce60` remap → custom-preset path with `applySavedBypassPreset()` (checks `GBS_PRESET_ID == PresetHdBypass` after `writeProgramArrayNew()`; `PresetBypassRGBHV` is a TODO) → LUT dispatch by (`result`, `presetPreference`) → `doPostPresetLoadSteps()`.
- `getVideoMode()` (`:4868`) decodes `GBS::STATUS_00/03/04/05/16` + `MD_*_CNTRL` into the §5.5 codes; 1080i vs 576p disambiguation uses `GBS::VPERIOD_IF < 1160`.
- Clocking: `Si` (Si5351mcu) via `externalClockGenResetClock()` / `externalClockGenSyncInOutRate()`; `uopt->disableExternalClockGenerator`, `rto->extClockGenDetected`.
- Rate helpers: `snapToIntegralFrameRate()`, `getOutputFrameRate()`, `getSourceFieldRate()`, `getPllRate()`; power path: `checkBoardPower()`, `calibrateAdcOffset()`, `runAutoGain()`.

### 9.2 PRO layer — `gbs-control/pro/`

- `gbs-control-pro.h` — PRO declarations: `InputSource`/`InputType` enums, `getInputSourceFromType()`, all `ADV_send*` prototypes, OLED/IR/status prototypes, `GBS_FW_VERSION`/`ADV_FW_VERSION` (`:34-35`); includes `osd-registry.h` + `menu-registry.h`.
- `gbs-control-pro.cpp` — `ADVController` packet wrappers (`:123-250`), `ADV_applyPendingOptions()` (`:320`), `switchInput()` + `inputConfigs[]` routing table (`:352-360`), color conversion (`applyRGBtoYUVConversion()`), TV-mode application (`:599`), `broadcastProStatus()` (`:578`), `refreshMenusOnSignalChange()`.
- `options-pro.h` — PRO enums + `USER_OPTIONS_PRO_FIELDS` (see §5.3).
- `drivers/` — `adv_controller.h` (UART protocol + register map + defaults), `ir_remote.h` (NEC keys: UP/DOWN/LEFT/RIGHT/OK/MENU/EXIT/INFO/SAVE/MUTE/VOL_UP/VOL_DN, `kRecvPin=2`), `pt2257.h` (audio attenuator), `stv9426.h` (TV OSD).
- `menu/` — state machine `OLED_MenuState` (`menu-registry.h`) with X-macro item tables; dispatch `IR_handleMenuSelection()` (`menu-core.cpp:174`); per-section handlers `handlers/menu-{main,output,input,adv,color,screen,profile,preferences,misc,system,developer}.cpp`.
- `osd/` — `OsdCommand` enum + `OSD_MAX_MENU_ROWS 3`, `OSD_CLOSE_TIME 16000` (`osd-registry.h`); dispatch `OSD_handleCommand()` (`osd-core.cpp`); `oledToOsdMap[]` (PROGMEM, built from `ALL_MAPPED_MENU_ITEMS` X-macro) maps OLED pages to OSD pages.

### 9.3 TV5725 driver + preset engine

- `tv5725.h` — register-level template `TV5725<T_ADDR>` over `tw.h`; segment register 0xF0; used as `typedef TV5725<GBS_ADDR> GBS;` (`gbs-control.ino:109`). All scaler state lives in these registers; "presets" are 6 banks × 16-byte segment arrays in PROGMEM.
- LUT headers (12, included at `gbs-control.ino:1-12`): `ntsc_240p.h`, `pal_240p.h`, `ntsc_720x480.h`, `pal_768x576.h`, `ntsc_1280x720.h`, `ntsc_1280x1024.h`, `ntsc_1920x1080.h`, `ntsc_downscale.h`, `pal_1280x720.h`, `pal_1280x1024.h`, `pal_1920x1080.h`, `pal_downscale.h`; plus `presetMdSection.h` (mode-detect patches), `presetDeinterlacerSection.h`, `presetHdBypassSection.h` (`loadHdBypassSection()`), `ofw_RGBS.h`/`ofw_ypbpr.h` (output filter words).
- `writeProgramArrayNew(programArray, skipMDSection)` (`:563`) writes the banks with per-bank patches; `doPostPresetLoadSteps()` (`:3388`) then applies: gain (`adco`), `prepareSyncProcessor()`, RGBHV sync path, `updateSpDynamic()`, `updateCoastPosition()`, `updateClampPosition()`, `resetPLLAD()`, `ResetSDRAM()`, DAC enable, `applyYuvPatches()`/`applyRGBPatches()` (`:970-1024`), `applyHdmiLimitedRange()`, and caches `rto->presetID` / `rto->isCustomPreset` from `GBS_PRESET_ID`/`GBS_PRESET_CUSTOM`.
- `framesync.h` — `FrameSyncManager`: FTL methods 0 (move VS: Vtotal+VSST) / 1 (1 Vtotal); driven by `uopt->enableFrameTimeLock` / `frameTimeLockMethod`.

### 9.4 Menu & OSD subsystem

- Legacy menu code (`OLEDMenuManager`, `OLEDMenuItem`, `OLEDMenuImplementation.cpp`, `OSDManager.*`) is the base; `USE_NEW_OLED_MENU 1` selects the `OLEDMenuManager`+PRO OSD path (`gbs-control.ino:48-54`).
- Navigation: rotary encoder ISRs (rotate/push) → `volatile OLEDMenuNav oledNav` → `oledMenu.tick(oledNav)` in `loop()`; IR keys map onto the same state machine.
- Menu i18n: `generate_translations.py` (Pillow) pixel-renders each `menu_items` tag into `OLEDMenuTranslations.h` (default output name, `--output` flag); preview JPGs are generated and cleaned up.
- OSD: 3-row menu (`OSD_MAX_MENU_ROWS`), theme support, 16 s auto-close (`OSD_CLOSE_TIME`), 3 s for mute (`OSD_MUTE_CLOSE_TIME`).

### 9.5 Networking & web API

- Stack: `ESPAsyncTCP` + `ESPAsyncWebServer` (esp32async forks, `lib_deps`), WebSockets (Markus Sattler, **modified copy** in `src/`, pristine in `3rdparty/WebSockets`), PersWiFiManager (**modified copy** at `gbs-control/` root, pristine in `3rdparty/PersWiFiManager`), DNSServer, mDNS, ArduinoOTA, LittleFS.
- Endpoints: `http://gbscontrol.local` (or `http://gbscontrol`), AP fallback SSID `gbscontrol` pass `qqqqqqqq`; WebSocket on port 81.
- Status protocol (webapp parses positionally):
  - base: `#` + preset digit + slot char + 3 bit-flag bytes (e.g. `#1A\x01\x02\x00`; bit 0 = auto-gain active, bit 1 = FTL active — per `public/README.md`);
  - PRO: `$` + 26 bytes — input type (1–6), TV format (0–11), I2P, Smooth, Sharpness, ACE on/off + ACE gains (hex), Y/C/WY filters, comb NTSC/PAL, `hdmiLimitedRange`, `advSyncStripper`, hue.
- Dev server (`public/dev-server.js`): HTTP :8080 with REST mocks (`/slot/save`, `/slot/set`, `/wifi/status`, `/bin/slots.bin`, `/pro`, …) + WS :81 simulator — the canonical place to see the expected message shapes when changing the protocol.

### 9.6 Storage (LittleFS)

- Partition: `eagle.flash.4m1m.ld` (`platformio.ini:20`), LittleFS (`board_build.filesystem`).
- `slot.h` — `SlotsFileHeader` (16 B: magic `"GBSPS\0"`, version, counts) + `SlotMeta` (exactly 128 B incl. PRO fields: `gbsColorR/G/B`, `adv*` BCSH/ACE/filter/comb, `hdmiLimitedRange`, `dev*`/`screen*` NTSC+PAL overrides, `slotSyncwatcherMode`); `SLOTS_TOTAL 36`.
- Slot switch copies a subset of `uopt` ↔ slot (`wantScanlines`, `scanlineStrength`, `deintMode`, `enableFrameTimeLock`, `frameTimeLockMethod`, `wantVdsLineFilter`, `wantStepResponse`, `wantPeaking`, `wantFullHeight`, `PalForce60`) at `gbs-control.ino:4281-4497`.
- Factory reset path: `deleteAllSlotsAndPresets()` (`:7658`) removes `/slots.bin` + `/preset_*` entries.

### 9.7 Web UI — `gbs-control/public/`

- `src/index.ts` — the entire app (TypeScript); field tables for the same preference layout as `options.h`/`options-pro.h` (must be kept in sync with the firmware serialization order).
- `src/index.html.tpl`, `src/style.css`, `src/manifest.json` — templates with `${...}` placeholders resolved by `scripts/build.js`.
- `scripts/build.js` — single-file bundler: base64-embeds `oswald.woff2`, `material.woff2`, icons, favicon, manifest; injects `GBS_FW_VERSION` parsed from `pro/gbs-control-pro.h` (replaces `"__FW_VERSION__"` in `src/index.js`).
- `scripts/html2h.sh` — `gzip -c9` + `xxd -i` + `sed` → `webui_html.h` with `const uint8_t webui_html[] PROGMEM` and `webui_html_len`. Requires bash, gzip, xxd, sed.
- `dev-server.js` — mock backend for hardware-less development.
- Assets: `assets/fonts/{material,oswald}.woff2`, `assets/icons/{gbsc-logo.png, icon-1024.png, icon-1024-maskable.png}`.
- PWA: manifest + maskable icon → installable on phones.

### 9.8 Flasher — `gbsc-pro-flasher/`

- Single file `gbsc_flasher.py` (v1.0.0, ~1400 lines), no packaging metadata beyond `requirements.txt`.
- `ControllerType` enum (ADV/ESP/UNKNOWN); `auto_detect()` prefers firmware-type detection, then port heuristics; `find_all_devices()` dedupes macOS double device nodes (`_device_fingerprint()`).
- `ADVFlasher` — HCMGBoot protocol: connect @115200 → chip info (send `0x55`, expect ≥48 B) → enter download mode (send `'1'`) → YMODEM `C` handshake → 128-byte packets `SOH seq ~seq data crc16` → EOT/ACK dance; max 10 NAK retries per packet.
- `ESPFlasher` — wraps `esptool.main()`; `erase()` supports user-area / WiFi-area / full-chip wipes (region constants at `gbsc_flasher.py:66-69`); monkey-patches esptool's logger for progress capture (restores it in `finally`).
- GUI (`PySide6`): 1 s USB poll timer, drag & drop of `.bin`, colored device label, erase checkboxes (ESP-only, auto-hidden for ADV), destructive-op confirmation dialog, `FlashWorker` QThread.
- CLI exit codes: 0 success, 1 failure/cancel; `--list` prints detected devices + all serial ports.
- Platform notes: Windows CH340 driver for ESP; macOS ADV = `/dev/cu.usbmodemXXXX`, ESP = `/dev/cu.wchusbserialXXXX`; Linux `dialout` group for `/dev/ttyUSB*`/`/dev/ttyACM*`.

---

*Line numbers refer to the tree as of `GBS_FW_VERSION 2.4.1` (`pro/gbs-control-pro.h:34`). When in doubt, grep for the symbol name — the line anchors in `gbs-control.ino` are stable but the file is actively edited.*
