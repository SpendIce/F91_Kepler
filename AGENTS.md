# F91 Kepler - Codex Operating Guide

## Repository Contract

- Never add `Co-Authored-By` or AI attribution to commits. Use conventional commits only.
- Never build after changes. Do not run CCS builds, Gradle builds, generated make builds, or ad-hoc `gcc` rebuilds unless this rule is explicitly changed by the user.
- When a code, config, or documentation change is complete and verified, commit it before handing back unless the user explicitly asked not to commit or the task is review-only/planning-only.
- Before committing, inspect `git status` and stage only the files or hunks changed for the completed task. Preserve unrelated dirty worktree changes.
- Do not commit generated artifacts, secrets, dependency directories, local settings, or known-bad work.
- If verification fails or a review blocker remains, do not commit. Report the blocker and the exact verification state.
- Never use `cat`, `grep`, `find`, `sed`, or `ls`. Use `bat`, `rg`, `fd`, `sd`, and `eza`. If a required tool is missing, install it via `brew` only when the task actually needs it.
- When asking the user a question, stop and wait for the response. Do not continue by assuming the answer.
- Never agree with technical claims without verification. Say "dejame verificar" and check code, docs, or tool output first.
- If the user is wrong, explain why with evidence. If you were wrong, acknowledge it with proof.
- Always propose alternatives with tradeoffs when relevant.

## Communication

- Spanish input: use Rioplatense Spanish with voseo.
- English input: respond in English.
- Be direct and technical, but explain the concepts behind the change. The user is expected to lead; Codex executes and verifies.
- Push back on requests that skip essential context, especially embedded firmware or hardware changes where a wrong assumption can cost a board respin.

## Project Summary

F91 Kepler is a firmware and hardware project for a Casio F91W smartwatch rebuild.

- MCU: Texas Instruments CC2640R2F, ARM Cortex-M3, BLE 4.2.
- Firmware language: C only, no C++.
- Main firmware baseline: `Firmware/f91_kepler_app/`.
- New firmware modules: `kepler/`.
- Hardware files: `Hardware/`.
- Android placeholder app: `Software/`.
- Phase 0 specs: `docs/phase0/`.
- Build system: TI Code Composer Studio / TI toolchain project metadata, not Makefile or CMake.

## Ground Truth Order

For firmware work, read sources in this order before editing:

1. `AGENTS.md`
2. `docs/phase0/00_phase0_overview.md`
3. The task-specific file in `docs/phase0/`
4. Existing implementation under `kepler/`
5. Original firmware under `Firmware/f91_kepler_app/Application/`
6. Board and pin definitions, especially `Firmware/f91_kepler_app/Application/CC2640R2_KEPLER.h`

When spec and implementation disagree, follow the spec only after verifying the mismatch and reporting it.

## Embedded Firmware Rules

- No dynamic memory in new `kepler/` firmware: no `malloc`, `calloc`, `realloc`, or `free`.
- Keep code pure C.
- ISRs must only record minimal state and post/defer work to task context. Do not perform peripheral I/O inside interrupt handlers.
- All hardware-dependent code must be guarded by `KEPLER_HAS_*` feature flags from `kepler/kepler_config.h`.
- Code must remain useful with hardware absent by using stubs or disabled feature flags where the spec allows it.
- Prefer static or caller-owned buffers. Be explicit about buffer length and ownership.
- Keep drivers small, testable, and separated by peripheral responsibility.
- Sharp Memory LCD CS is active HIGH; do not rely on normal SPI chip-select polarity.
- Verify GPIO assignments from the board file or schematic before changing pin config.

## Session Workflow

- Work on one task at a time.
- For a new firmware task, read the task spec in full before writing code.
- For new interfaces, show the `.h` contract first and wait for user approval before writing the `.c` implementation.
- If approval is needed, ask once and stop. Do not continue until the user answers.
- Keep changes narrow. Do not opportunistically refactor unrelated firmware, hardware, app, or documentation files.
- If a Claude-specific instruction conflicts with this file, use this file for Codex work and mention the conflict.

## Verification

- Do not build after changes.
- Allowed verification examples:
  - `git diff --check`
  - `rg`/`fd`/`bat` probes that confirm documentation or code contracts
  - existing prebuilt host test binaries such as `./kepler/test/test_task1` and `./kepler/test/test_task2`, when relevant
- Do not rebuild host test binaries with `gcc` as part of verification.
- If a test binary is stale, missing, or irrelevant, say so instead of manufacturing a build step.

## Legacy Claude Artifacts

The repo may contain `.claude/`, `CLAUDE.md`, `CLAUDE_CODE_GUIDE.md`, and `skills-lock.json`.

- Treat `.claude/settings.local.json` as local Claude state, not Codex policy.
- Treat `.claude/skills/` as legacy reference material only. Do not assume Codex auto-loads those skills.
- Use `AGENTS.md` as the active Codex instruction source.
- Do not delete or stage Claude artifacts unless the user explicitly asks to migrate, remove, or commit them.
