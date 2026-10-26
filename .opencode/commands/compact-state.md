---
description: Compact NOW, OPEN-TASKS, and the history archive
agent: archivist
---

Compact the project state without losing evidence:

1. Read `notes/NOW.md`, `TASKS.md`, `notes/OPEN-TASKS.md`, and the tail of
   `notes/STATE.md`.
2. Move completed `TASKS.md` sections to `notes/tasks-done.md` (create it if
   missing); keep only open work in `TASKS.md`, with acceptance/evidence lines.
3. Keep `notes/NOW.md` under ~80 lines: environment, lanes, focus, next five
   actions, recent five completions, blockers, pointers.
4. Append older completions from `NOW.md`/`STATE.md` to
   `notes/history/<yyyy-mm>.md`; do not rewrite history.
5. Regenerate `notes/OPEN-TASKS.md` with `.\scripts\tasks.ps1 -Update`.
6. Report the line counts before/after and any dangling links.

Do not change code, docs/formats, or evidence claims.
