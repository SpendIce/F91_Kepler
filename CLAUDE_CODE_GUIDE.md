# F91 Kepler — Claude Code Session Guide

## Before your first session

### Step 1 — Fork and clone

1. Go to https://github.com/PegorK/F91_Kepler
2. Click **Fork** (top right) — fork to your own GitHub account
3. Clone your fork locally:

```bash
git clone https://github.com/YOUR_USERNAME/F91_Kepler.git
cd F91_Kepler
```

### Step 2 — Copy the doc files into the repo

Copy all files from the downloaded `f91_kepler_phase0/` folder into the repo:

```bash
mkdir -p docs/phase0
cp /path/to/downloads/f91_kepler_phase0/*.md docs/phase0/
```

Copy `CLAUDE.md` to the repo root:

```bash
cp /path/to/downloads/CLAUDE.md .
```

Verify the structure:

```bash
ls docs/phase0/
# Should show: 00_phase0_overview.md  01_sharp_lcd_driver.md  02_buttons_time_setting.md
#              03_haptic_driver.md  04_accelerometer.md  05_power_ble_storage_buzzer.md
#              06_screens_weather_ui.md  SESSION_PROMPTS.md
ls CLAUDE.md
```

### Step 3 — Create the kepler/ module directory skeleton

```bash
mkdir -p kepler/display kepler/input kepler/haptic kepler/accel \
         kepler/power kepler/ble kepler/screens kepler/storage kepler/audio
touch kepler/kepler_main.c kepler/kepler_config.h
```

### Step 4 — Commit the docs and skeleton to your fork

```bash
git add docs/ CLAUDE.md kepler/ CLAUDE_CODE_GUIDE.md
git commit -m "Add project docs and kepler/ module skeleton"
git push
```

### Step 5 — Install Claude Code

```bash
npm install -g @anthropic/claude-code
```

Then authenticate:

```bash
claude
```

Follow the login prompt. Claude Code will open in your browser for OAuth.

---

## Starting Session 1

Navigate into the repo root, then launch Claude Code:

```bash
cd F91_Kepler
claude
```

When the Claude Code prompt appears, paste this exactly:

---

```
Read CLAUDE.md first, then read docs/phase0/00_phase0_overview.md and
docs/phase0/01_sharp_lcd_driver.md in full.

Then read Firmware/Application/CC2640R2_KEPLER.h and extract all IOID_x
pin assignments relevant to our project: SPI (CLK, MOSI, CS), I2C (SDA, SCL),
buttons (1, 2, 3), buzzer PWM, and any display control pins. Show me a
completed pin verification table before writing any code.

Once I confirm the pins, proceed to implement Task 1: the Sharp Memory LCD
driver. Start with sharp_lcd.h in kepler/display/ — show me the header and
wait for my approval before writing sharp_lcd.c.

Do not implement any other task in this session.
```

---

## Session workflow — same pattern every time

Each session follows this exact rhythm:

1. **Navigate to repo root** → `cd F91_Kepler`
2. **Launch Claude Code** → `claude`
3. **Paste the session prompt** from `docs/phase0/SESSION_PROMPTS.md`
4. **Approve headers** before implementations get written
5. **Test** using the acceptance checklist at the end of each spec file
6. **Commit** when the task passes all acceptance criteria:
   ```bash
   git add kepler/
   git commit -m "Task N: <description>"
   git push
   ```
7. **Start next session** only after the commit

---

## Session prompts (quick reference)

Full prompts with exact attachment instructions are in `docs/phase0/SESSION_PROMPTS.md`.

| Session | Task | Key files to mention |
|---------|------|----------------------|
| 1 | Sharp LCD driver + UI renderer | `01_sharp_lcd_driver.md` |
| 2 | Button handlers + time setting | `02_buttons_time_setting.md` |
| 3 | DRV2605L haptic driver | `03_haptic_driver.md` |
| 4 | LIS2DW12 accelerometer | `04_accelerometer.md` |
| 5 | Power, BLE, storage, buzzer | `05_power_ble_storage_buzzer.md` |
| 6 | All 6 screens, weather, stopwatch, alarms | `06_screens_weather_ui.md` |

In each session, tell Claude Code to:
- Read `CLAUDE.md` and `docs/phase0/00_phase0_overview.md` first
- Read the task-specific spec file
- Confirm each header before the .c implementation
- Not work on any other task

---

## Useful Claude Code commands during sessions

```bash
# Ask Claude Code to explain what it is about to do before doing it
# (type in the Claude Code prompt):
"Before writing any file, explain your plan and wait for my go-ahead."

# If Claude Code drifts to a different task:
"Stop. We are only working on Task N today. Return to that."

# To see what files have changed:
git diff --stat

# To review a specific file Claude Code just wrote:
git diff kepler/display/sharp_lcd.c

# If something breaks and you want to revert a file:
git checkout -- kepler/display/sharp_lcd.c
```

---

## CCS project integration — adding kepler/ source files

After Session 1, you need to add the new source files to the CCS project so they compile.

In CCS:
1. Right-click the project → **Add Files** → navigate to `kepler/display/`
2. Add `sharp_lcd.c` and `ui_renderer.c`
3. Repeat for each session's new files

Or edit `.cproject` directly to add source file entries — Claude Code can do this
for you at the end of each session if you ask:
```
"Update the CCS .cproject file to include all new .c files from kepler/."
```

---

## GPIO verification — what Claude Code will do in Session 1

Claude Code will read `Firmware/Application/CC2640R2_KEPLER.h` directly from the
filesystem and extract the real IOID numbers. It will then fill in the pin checklist
from `docs/phase0/00_phase0_overview.md` with the real values and update
`kepler/kepler_config.h` with confirmed defines.

The file to look for is likely named one of:
- `Firmware/Application/CC2640R2_KEPLER.h`
- `Firmware/Application/board.h`
- `Firmware/Application/kepler_board.h`

If pin definitions are spread across multiple files, Claude Code will grep for
`IOID_` across the Application/ directory to find them all:

```bash
grep -r "IOID_" Firmware/Application/ | grep -v "\.o:"
```

---

## After Phase 0 is complete

Phase 0 finishes after Session 6 passes all acceptance criteria. At that point:

- All firmware modules are written and compile cleanly
- All 6 screens render correctly on the Sharp LCD (tested on launchpad)
- BLE connects and receives test notifications via nRF Connect
- All buttons respond correctly

Phase 2 (PCB revision) can then begin — order components using the BOM in
`docs/f91_kepler_build_plan.md`, spin the PCB through JLCPCB, and enable
hardware feature flags one by one as components arrive.

---

## If something goes wrong

**Build errors in CCS:** Claude Code cannot run CCS directly, but it can read
compiler error output if you paste it in. Start your message with:
```
"I got this CCS build error: [paste error]"
```

**GPIO conflicts:** If two peripherals want the same pin, open the KiCad schematic
in `Hardware/` and check what is actually routed. The schematic is the ground truth.

**SDK API not found:** The TI SimpleLink CC2640R2 SDK must be installed and the
CCS project must point to it. Default install path:
`C:/ti/simplelink_cc2640r2_sdk_4_xx_xx_xx` (Windows) or
`~/ti/simplelink_cc2640r2_sdk_4_xx_xx_xx` (Linux/Mac).

**Lost context in a long session:** If Claude Code starts contradicting the spec,
say: `"Re-read CLAUDE.md and docs/phase0/[current task spec].md before continuing."`
