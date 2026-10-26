---
description: Record the session handoff in NOW/history and the evidence index
---

Create a short handoff for the next agent:

1. Run `.\scripts\state.ps1 -Done "<one-line result>"`.
2. If the next action changed, run `.\scripts\state.ps1 -Next "<action>"`.
3. Run `.\scripts\tasks.ps1 -Update`.
4. Add any new load-bearing claim to `notes/evidence-index.md`.
5. Append the detailed session card to `notes/history/<yyyy-mm>.md`:
   task, files touched, commands run, evidence paths, open questions.

Keep it to one screen. Do not restate the whole `STATE.md`.
