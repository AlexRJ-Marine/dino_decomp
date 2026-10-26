---
name: Dino format loop
description: Add or extend a Dino Island asset-format parser, validate it against every retail file, and document it. Use for tools/formats, docs/formats, or a new .mesh/.skin/.bbi-style format task.
---

# Dino format loop

## 1. Find the evidence

- Read `notes/FINDINGS.md`, the relevant `docs/formats/*.md`, and
  `notes/open-issues.md`.
- Confirm loader evidence first: hex dump, Ghidra function, or an existing
  parser. If the layout is unknown, use the `ghidra-query` skill or `recon`
  before writing code.

## 2. Write the parser

- Python parsers live in `tools/formats/<name>.py`, one module per format.
- Use explicit little-endian reads and named fields; label unknowns
  `unknown_XX`, never invent semantics.
- A malformed file must raise with the byte offset, not silently truncate.
- Expose a dataclass-like result and a `main()` with `--dump`/`--scan` where
  useful. Keep dependencies to the standard library + `construct`/`pillow`.

## 3. Validate against retail

```powershell
.venv\Scripts\python.exe tools\formats\<name>.py --dump "game\%MAINDIR%\Data\..."
.venv\Scripts\python.exe -m pytest tests\test_<name>.py -q
```

- Parse the **full set** of that asset type; assert 0 errors and the exact
  counts (files, records, keys, resolved paths).
- Check paths against the real tree; missing assets are evidence, not a reason
  to hide the error.
- For geometry/textures, render or export a sample and inspect it.

## 4. Document

`docs/formats/<name>.md` must contain:

- magic/version, full field table with offsets and sizes;
- record layouts and counts;
- which runtime function/loader proves each non-obvious field;
- open questions, explicitly marked;
- the exact scan/test command and result.

Add one row to `notes/evidence-index.md` when the claim is load-bearing.

## 5. Finish

```powershell
.venv\Scripts\python.exe -m pytest tests -q          # full suite
.\scripts\state.ps1 -Done "<format> solved"
.\scripts\tasks.ps1 -Update
```

Mark the task done in `TASKS.md`, write the evidence note in `notes/`, and
commit parser + doc + test together. Hand a new parser to `reviewer` if it
changes a shared format or a documented layout.
