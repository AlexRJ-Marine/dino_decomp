---
name: Ghidra query
description: Ask the Ghidra MCP one narrow, evidence-producing question about DinoIsland.exe. Use before decompiling a function, resolving a struct/ABI, or checking a runtime assumption.
---

# Ghidra query

## Before asking

- Is the answer already in `docs/formats/`, `docs/decomp/`, or `notes/*-recon.md`?
  Search first; do not re-derive.
- State the **one** question. If several functions share a contract, ask them
  in one call with numbered sub-questions; a fresh subagent is expensive.
- Give the parent context it cannot infer: binary path, Ghidra project
  (`DinoIsland.exe-ebea17`), MCP URL `http://127.0.0.1:8000/mcp`, known struct
  layouts/offsets, and the exact addresses already found.

## Prompt template

```text
DinoIsland.exe (PE32, MSVC 6, Ghidra project DinoIsland.exe-ebea17,
MCP http://127.0.0.1:8000/mcp).

Narrow question: <one sentence>.

Known: <addresses, struct offsets, prior evidence>.

Return, with disassembly evidence:
1. <sub-question>
2. <sub-question>

Print tables in full (no ellipses). State confidence and ambiguity. Evidence
only; no file edits.
```

## Output contract

- Answer first, then the evidence table.
- Include the exact addresses/bytes for each non-obvious claim.
- Mark uncertain items explicitly; never fill gaps with plausibility.
- If the answer is large (a 20x20 table, a long struct), ask for it as a table
  and write it to a note yourself; do not ask the Ghidra agent to edit files.
- Keep the final note in `notes/<topic>-recon.md`; promote to `docs/` only when
  it is load-bearing, and add an `notes/evidence-index.md` row.

## Follow-ups

- If the answer is truncated or a helper contract is missing, continue the same
  subagent session (`sessionID`) instead of starting over.
- For runtime confirmation (the static answer is ambiguous), use
  `dino-retail-run`: x32dbg breakpoint or a logged field, then record the
  addresses and observed values.
