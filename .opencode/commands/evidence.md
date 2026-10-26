---
description: Capture reproducible evidence for a claim
---

Capture evidence for: $ARGUMENTS

- Choose the cheapest proof: parser/test, hex dump, decompiled function, or a
  runtime log/screenshot.
- For a runtime capture, write outputs under `notes/harness/` and record the
  command, git hash, timestamp, and output path in
  `notes/harness/manifest.md`.
- Add one row to `notes/evidence-index.md` (claim -> evidence -> test).
- Update the relevant `notes/*-recon.md` or `docs/formats/*.md`; do not paste
  the same numbers into `NOW.md` or `STATE.md`.
