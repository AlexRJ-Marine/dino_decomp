---
description: Build and test the current lane (fast by default)
---

Run the verification loop for the current lane:

```powershell
.\scripts\verify.ps1 -Fast
```

If `$ARGUMENTS` contains `full`, run `.\scripts\verify.ps1` instead. Report the
build directory, the test counts, and any failures with the minimal repro.
Do not fix unrelated failures from another lane; note them and keep going.
