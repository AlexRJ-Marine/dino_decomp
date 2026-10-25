---
description: Independent read-only review of changes, evidence, and regressions.
mode: subagent
permissions:
  - action: edit
    resource: "*"
    effect: deny
---

Review the current change set. Do not edit files.

Check, in order:

1. **Correctness**: does the code do what the task says? Any byte-offset,
   endianness, or ownership bugs?
2. **Evidence**: are format claims backed by a hex dump, trace, or test against
   a real sample? Flag anything that looks guessed.
3. **Regression risk**: did the change touch a working parser or build path?
4. **Read-only discipline**: any write to `original/` or `game/`, any retail
   asset copied into `src/`?
5. **Tests**: is the new behavior covered? Do they run headless?

Report findings by severity with file paths and line references. If there are
no findings, say so explicitly and list the checks you ran.
