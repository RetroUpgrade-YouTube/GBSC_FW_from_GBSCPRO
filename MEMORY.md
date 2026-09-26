# MEMORY.md — Operational Knowledge for Agents

Read this file before running any build/flash/debug commands. Contains environment-specific facts that are NOT derivable from the source code.

## Toolchain paths (Windows)

| Tool | Path | Notes |
|---|---|---|
| **PlatformIO (pio)** | `C:\Users\PC\AppData\Roaming\Python\Python314\Scripts\pio.exe` | **NOT on PATH.** Invoke with full path or `$env:PATH += ";C:\Users\PC\AppData\Roaming\Python\Python314\Scripts"` |
| **Python** | `python` (on PATH, Python 3.14) | |
| **Node.js / npm** | `npm` (on PATH) | |
| **Git Bash** | `C:\Program Files\Git\bin\bash.exe` | Needed for `html2h.sh` (web build) |
| **esptool** | via `pip` (in the same Python Scripts dir as pio) | For flash dump/restore |

## Build commands (PowerShell)

```powershell
# Build firmware
$pio = "C:\Users\PC\AppData\Roaming\Python\Python314\Scripts\pio.exe"
& $pio run 2>$null
# Check: $LASTEXITCODE == 0 and output contains [SUCCESS]
# Output: gbs-control/.pio/build/gbsc-pro/firmware.bin (~785 KB)

# Build web UI (needs Git Bash for html2h.sh)
cd gbs-control/public
npm run build   # runs tsc + node build.js + html2h.sh

# Flash
& $pio run --target upload   # from gbs-control/
# OR:
python gbsc-pro-flasher/gbsc_flasher.py --esp gbs-control/.pio/build/gbsc-pro/firmware.bin
```

## PowerShell gotchas

- **`&&` does NOT work** — use `;` as statement separator.
- `2>$null` suppresses stderr (pio prints progress there).
- Long builds: set `timeoutMs: 300000` (5 min) on the `pwsh` tool call.
- Use `workdir` parameter instead of `cd` (each pwsh call is a fresh process).

## Flash dump / restore (esptool)

```bash
# DUMP full 4MB flash (backup before overwriting!)
esptool.py --chip esp8266 --port COMx --baud 460800 read_flash 0x0 0x400000 backup.bin

# RESTORE from dump
esptool.py --chip esp8266 --port COMx --baud 460800 write_flash 0x0 backup.bin

# List ports
esptool.py --chip esp8266 --baud 460800
```

## Web UI build loop (full cycle)

```
edit public/src/index.ts (or other src files)
  → cd gbs-control/public && npm run build
    → regenerates gbs-control/webui.html AND gbs-control/webui_html.h
  → cd .. && pio run
    → compiles firmware with new webui_html.h
  → flash
```

**Never hand-edit** `webui.html` or `webui_html.h` — they are generated artifacts.

## Invariants (do NOT break)

- `sizeof(SlotMeta) == 128` (static_assert in `slot.h:114`)
- `SLOTS_FORMAT_VERSION 0x01` + magic `"GBSPS"` (mismatch = boot auto-wipe)
- `/preferencesv2.txt` positional field order (loader/saver/web-UI all independent)
- `USE_NEW_OLED_MENU 1` (gates large #if blocks)
- `GBS_FW_VERSION "2.4.1"` (in `pro/gbs-control-pro.h`, auto-injected into web UI)
- Web WS protocol: base `#` status + PRO `$` 26-byte status (positional)

## GPIO / hardware constraints

- **GPIO0 must be HIGH at power-on** (rotary encoder push = pin 0)
- ESP8266: 160 MHz, 4 MB flash, LittleFS, qio mode (build) / dio (flasher)
- Flash layout: app ~3 MB + LittleFS 1 MB + WiFi cal 16 KB

## Repo conventions

- `AGENTS.md` — architecture guide (676 lines)
- `MEMORY.md` — this file (operational knowledge)
- `PRO_REMOVAL.md`, `AGENT_HANDOFF.md` — gitignored session docs, do NOT commit
- Commit style: imperative subject, no AI co-author trailer
- Author: `Caleco81 <carlosgms81@gmail.com>`
- Branch: `main`
