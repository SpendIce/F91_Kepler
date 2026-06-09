# F91 Kepler - Codex Session Guide

This repo was initially prepared with Claude Code artifacts. For Codex, the active repo contract is `AGENTS.md`.

## What Was Found

- `CLAUDE.md` contains useful project context and embedded firmware constraints.
- `CLAUDE_CODE_GUIDE.md` is Claude-specific and includes commands that conflict with the current repo rules, such as `ls`, `grep`, `git checkout --`, and build-oriented workflows.
- `.claude/settings.local.json` allows Claude to run `gcc` and two prebuilt host test binaries. Codex must not copy that into its policy because this repo says never build after changes.
- `.claude/skills/` contains Claude-format skills for embedded systems and PCB hardware. They are not Codex-native auto-loaded skills.
- `skills-lock.json` locks a Claude skill source and should not be treated as Codex configuration.
- `.codex/` existed but was empty.
- No `RTK.md` file was found in the repository.

## Starting a Codex Session

From the repo root:

```bash
codex
```

Then use a scoped prompt:

```text
Read AGENTS.md first, then read docs/phase0/00_phase0_overview.md and the task-specific spec.
Verify the relevant board definitions before editing.
Do not build after changes.
Do not touch unrelated dirty worktree changes.
```

## Recommended Task Prompt Shape

```text
Work only on Task N: <task name>.

Read:
- AGENTS.md
- docs/phase0/00_phase0_overview.md
- docs/phase0/<task spec>.md
- any existing implementation files for this task

Before implementation:
- verify the board/pin assumptions from the source files
- show the header/API contract if new interfaces are needed
- stop for approval before writing the .c implementation

After implementation:
- verify without building
- commit only the files changed for this task
```

## Tooling Rules

Use:

```bash
fd -HI <pattern>
rg -n <pattern>
bat --paging=never --style=numbers <file>
eza -la --group-directories-first
git status --short
git diff --check
```

Do not use:

```bash
cat
grep
find
sed
ls
```

If `sd` or another required repo tool is missing and the task actually needs it, install via `brew`.

## Verification Without Builds

Use the smallest proof that matches the change:

- Documentation-only changes: `git diff --check` plus targeted `rg` checks.
- Firmware contract changes: `git diff --check`, targeted `rg`, and existing prebuilt tests only if relevant.
- Existing prebuilt task tests:
  - `./kepler/test/test_task1`
  - `./kepler/test/test_task2`

Do not compile new test binaries as part of verification.

## Commit Discipline

Before committing:

```bash
git status --short
git diff --check
```

Stage only files changed by the completed task. Do not stage `.claude/settings.local.json`, dependency directories, generated outputs, or unrelated untracked files.

Use conventional commits, for example:

```bash
git commit -m "docs: add codex operating guide"
```

Never include AI attribution or `Co-Authored-By`.

## Claude Artifact Handling

Keep Claude artifacts as legacy material unless the user asks for a migration:

- Keep `CLAUDE.md` as historical project context.
- Do not rely on `.claude/settings.local.json` for Codex.
- Do not treat `skills-lock.json` as Codex policy.
- If a Claude skill has useful domain guidance, summarize and move the rule into `AGENTS.md` instead of asking Codex to load the Claude skill directly.
