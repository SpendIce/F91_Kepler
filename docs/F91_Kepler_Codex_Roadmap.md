# F91 Kepler — Codex Continuation Roadmap

This roadmap translates the Plan Maestro into the next executable Codex steps. It is subordinate to `docs/F91_Kepler_Plan_Maestro.md` for project decisions and to `AGENTS.md` for agent behavior.

## Verified Baseline

- `docs/F91_Kepler_Plan_Maestro.md` is the planning source of truth.
- `AGENTS.md` is the Codex operating source of truth.
- Task 1 host test passes from the existing prebuilt binary: `./kepler/test/test_task1` reports 36/36.
- Task 2 host test passes from the existing prebuilt binary: `./kepler/test/test_task2` reports 13/13.
- `kepler_main.c` is still empty.
- `kepler/haptic`, `kepler/accel`, `kepler/power`, `kepler/ble`, `kepler/screens`, `kepler/storage`, and `kepler/audio` are still empty.
- No `.map`, `.out`, `.hex`, `.elf`, or `.bin` artifact is present locally.
- CCS metadata currently targets SimpleLink CC2640R2 SDK 4.40.0.10.

## Grilled Findings

- The immediate blocker is not another driver; it is resource proof. The Plan Maestro says RAM/flash margins must be tracked from day one, and no map file exists yet.
- The old Phase 0 specs are still valuable, but they are no longer the final source of truth. Any conflict is resolved in favor of the Plan Maestro.
- The agent no-build rule and the Plan Maestro map gate are compatible only if CCS builds happen outside Codex and Codex analyzes the generated map.
- `event_queue` remaining in Session 5 is acceptable only if Sessions 3 and 4 stay stub-first and avoid permanent coupling decisions that would fight the future central queue.
- The Android app can start before PCB v2, but only after the BLE characteristic surface is stable enough to avoid churn.

## Continuation Plan

### 0. Documentation And Repo Hygiene

Status: in progress.

- Commit the Plan Maestro and Codex cleanup docs.
- Keep `.claude/`, `CLAUDE_CODE_GUIDE.md`, and `skills-lock.json` untouched unless the user explicitly chooses to remove or preserve them in version control.
- Keep generated build outputs out of git.

### 1. CCS Integration And Map Gate

Goal: prove that Task 1/2 actually fit the firmware project.

- Add or document the CCS linkage for `kepler/` sources.
- Have the user build in CCS.
- Analyze the generated `.map`.
- Record flash/RAM margins against the Plan Maestro gates.
- If the image approaches the 110 KB flash warning threshold, recut fonts before writing more feature code.

### 2. Task 1/2 Firmware Cable-Up

Goal: move from host-tested modules to firmware-integrated modules.

- Wire `sharp_lcd`, `ui_renderer`, `buttons`, and `time_set` into the app lifecycle.
- Decide whether `kepler_main.c` becomes the new top-level event loop immediately or remains a staged integration surface until Session 5.
- Preserve the no-I/O-in-ISR rule.
- Re-run prebuilt host tests where relevant and request CCS map/build evidence from the user.

### 3. Session 3 — Haptic Stub-First

Goal: implement DRV2605L APIs without requiring the part on current hardware.

- Start with `drv2605l.h` and `haptic_patterns.h`.
- Keep `KEPLER_HAS_DRV2605L=0` behavior as no-op/stub.
- Avoid central event queue assumptions until Session 5.
- Defer final vibration pattern tuning to the open Plan Maestro decision.

### 4. Session 4 — LIS2DW12 Stub-First

Goal: implement accelerometer interfaces and pedometer/actigraphy structure.

- Use single INT1 as the Plan Maestro decided.
- Document the day/night register reconfiguration sequence in code comments where it matters.
- Keep `KEPLER_HAS_LIS2DW12=0` behavior clean.
- Do not write software step-count algorithms; the plan requires the hardware pedometer engine.

### 5. Session 5 — Central Runtime

Goal: integrate the system shape.

- Implement `event_queue`, `power_manager`, `ble_manager`, notification service, storage, and buzzer.
- Decide the final `kepler_main.c` loop shape.
- Stub 0xFF06-0xFF0A BLE characteristics as the Plan Maestro requires.
- Load/store the nine NV items through the selected NV backend.
- Run the full integration checklist with CCS build/map evidence supplied outside Codex.

### 6. Session 6 — Feature Completion

Goal: replace UI placeholders with real behavior.

- Complete weather, phone locator, stopwatch, and alarms.
- Use GPT from XOSC for stopwatch timing.
- Replace weather and locator icon stubs with compact bitmaps only after the map gate says flash is safe.
- Complete BLE handlers for 0xFF06-0xFF0A.

### 7. Phase 1 Android

Goal: build the companion app once the GATT contract is stable.

- Start after Session 5 stubs the GATT table.
- Default weather provider is Open-Meteo.
- Keep the watch provider-agnostic by sending compact payloads.

### 8. Phase 2 PCB v2

Goal: move only after firmware risk is reduced.

- Do not start PCB v2 layout until the single-image flash/RAM risk is measured.
- Decide external SPI flash for OAD before board layout.
- Preserve the case, gasket, and no-hole constraints.

## Do Not Do Yet

- Do not start PCB v2 before a CCS map proves the firmware is viable.
- Do not implement software pedometer logic.
- Do not combine screen files to "save flash"; the Plan Maestro says fonts, not screen file count, are the flash risk.
- Do not build from Codex.
- Do not commit generated firmware outputs.
