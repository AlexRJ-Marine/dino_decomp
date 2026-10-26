# Research hand-off checklist

The project's memory is spread across `notes/`, `docs/formats/`,
`docs/decomp/`, `TASKS.md` and `notes/STATE.md`. The recurring failure mode is
a correct note plus a stale canonical doc, so use this checklist for every
research batch.

## 1. Update the canonical doc in the same change

| Finding | Must also update |
| --- | --- |
| file layout/field meaning | `docs/formats/<name>.md` |
| function/address/status | `docs/decomp/function-status.md` |
| reversed premise | the old claim in the canonical doc **and** the note |
| gameplay constant/flow | `docs/decomp/<system>.md` |
| task status | `TASKS.md` (checkbox + evidence) |
| next action/blocker | `notes/STATE.md` |

A note alone is not a completed result. If a claim is corrected, remove or
rewrite the stale sentence; do not leave both versions.

## 2. Label confidence

Use one of:

- **verified** — a parser/test/runtime trace against retail data.
- **decompiled** — pseudo-C/disassembly, behaviour understood.
- **inferred** — consistent pattern, no direct consumer yet.
- **open** — explicitly unknown; do not build gameplay on it.

Do not promote inferred to solved in `TASKS.md`.

## 3. Runtime vs static

- Static (Ghidra) proves the code path; runtime proves the values.
- When a field is only initialised in a ctor, a save sample is weak evidence —
  trace a fresh instance.
- Record the exact breakpoint/address/memory offset in the note.

## 4. Evidence files

- Do not overwrite tracked reference screenshots (`notes/park-after2.png`,
  `notes/menu-main.png`, etc.). Add a new file with a descriptive name.
- If a file is accidentally overwritten, restore it from git and note it.
- Keep only the final PNG(s) referenced by docs; delete intermediate BMPs.

## 5. Before handing off

- [ ] `scripts/run-tests.ps1` passes (or the failing test is named with its
      cause, e.g. an external file overwritten by another agent).
- [ ] `docs/decomp/function-status.md` has the new/changed addresses.
- [ ] `TASKS.md` and `notes/STATE.md` agree.
- [ ] Stale-term search is clean:
      `Select-String -Path TASKS.md,notes\*.md,docs\*.md,docs\**\*.md -Pattern "<old claim>"`
- [ ] The next action is a concrete experiment, not "continue research".
