# CCS map gates without Codex builds

The project needs CCS linker `.map` files to manage the CC2640R2F flash/RAM risk, but Codex is forbidden from building by repo policy. The user or CCS workflow generates map files, Codex analyzes them, and generated artifacts stay uncommitted.
