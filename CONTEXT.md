# F91 Kepler

F91 Kepler is a constrained smartwatch rebuild of the Casio F91W. This context keeps the project's language precise so future planning does not drift between legacy Claude setup, Codex workflow, firmware phases, and hardware goals.

## Language

**Plan Maestro**:
The single source of truth for project scope, phase order, architecture, and resource gates.
_Avoid_: CLAUDE.md as project plan, phase specs as final authority

**Agent Session**:
A bounded development slice that implements one planned task or cleanup step and verifies it with the smallest allowed evidence.
_Avoid_: Claude session, broad sprint

**Fase 0**:
Firmware-first work that proves the watch behavior on existing development hardware before PCB v2 is started.
_Avoid_: prototype phase, app phase

**Resource Gate**:
A decision checkpoint based on measured flash and RAM usage from the CCS linker map.
_Avoid_: rough size guess, host-test proof

**PCB v2**:
The hardware revision that adds the new display, sensing, haptic, NFC, charging, and optional external flash while preserving the F91W case constraints.
_Avoid_: MVP board, final enclosure

**Companion App**:
The Android app responsible for notification relay, weather, locator, alarm sync, time sync, and user settings.
_Avoid_: diagnostic app, nRF Connect workflow
