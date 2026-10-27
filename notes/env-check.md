# Environment check — 2026-10-26

## `scripts/check-env.ps1`

```text
[ OK ] python venv            .venv\Scripts\python.exe
[ OK ] python packages        pyghidra_mcp,pefile,capstone,construct,pytest,PIL
[ OK ] pyghidra-mcp.exe       .venv\Scripts\pyghidra-mcp.exe
[ OK ] mcp-proxy.exe          .venv\Scripts\mcp-proxy.exe
[ OK ] pytest.exe             .venv\Scripts\pytest.exe
[ OK ] cmake.exe              .venv\Scripts\cmake.exe
[ OK ] ninja.exe              .venv\Scripts\ninja.exe
[ OK ] ghidra                 C:\Tools\ghidra_12.2_PUBLIC
[ OK ] g++ (w64devkit)        C:\Tools\w64devkit\w64devkit\bin\g++.exe
[ OK ] SDL3 (MinGW)           C:\Tools\SDL3\SDL3-3.4.18\x86_64-w64-mingw32
[ OK ] java 25 (JDK)          openjdk version "25.0.4.1" 2026-08-18 LTS
[ OK ] game exe               game\%MAINDIR%\DinoIsland.exe
[ OK ] ghidra mcp :8000       listening
```

Python tests: **317 passed**.

## `scripts/build.ps1`

```text
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: build/mingw
ninja: no work to do.
```

## Ghidra MCP

The `ghidra` tools list `/DinoIsland.exe-ebea17` with metadata:

```text
Program Name:  DinoIsland.exe-ebea17
Language:      x86:LE:32:default (4.9)
Compiler:      windows / visualstudio:unknown
Bytes:         2,637,824
Functions:     7,273
Analyzed:      true
MD5:           5c8352429b0ede4a0d3ff4c8743f59cb
SHA256:        7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c
Executable:    game/%MAINDIR%/DinoIsland.exe
```

## Notes

- The MCP server was already running on `http://127.0.0.1:8000/mcp`;
  `scripts/start-ghidra.ps1` is the script used to start it.
- The C++ build is up to date (no work to do).

## 2026-10-27 — Ghidra fails when launched from a packaged (MSIX) app

Symptom: `start-ghidra.ps1` (and plain `analyzeHeadless ... -postScript
HelloWorldScript.java`) fail with
`ERROR: Bundle org.apache.felix.framework [0] The data file must be inside the data dir.`
followed by `NullPointerException ... "dataFile" is null` in
`Felix.handleJavaVersionChange`. pyghidra-mcp hits it at index time
(chromadb's `import google.colab` -> pyghidra import hook -> OSGi bundle host).

Cause: the process was started from the Claude desktop app, an MSIX package.
Windows redirects new files under `%APPDATA%` into
`%LOCALAPPDATA%\Packages\<package>\LocalCache\Roaming`. After Felix creates
`osgi\felixcache\bundle0\data`, Java canonicalises that dir to the redirected
path but the data file to the real one, so Felix's containment check fails.
jshell evidence (same process tree):

```text
dir  C:\Users\<user>\AppData\Local\Packages\Claude_pzs8sxrjxfjjc\LocalCache\Roaming\ghidra\...\bundle0\data
file C:\Users\<user>\AppData\Roaming\ghidra\...\bundle0\data\last.java.version
inside=false
```

Ruled out: JDK 25.0.4.1 vs 25.0.5, jpype1 1.5.2 vs 1.7.1, the copied
project's owner, stale processes.

Fix: `start-ghidra.ps1` sets `APPDATA=analysis\ghidra-settings` for the server
process only, so Ghidra's settings and Felix cache live in the (gitignored)
checkout. With that, `HelloWorldScript.java` runs headless and the MCP server
starts; `FUN_004663a0` decompiles to `read(param_1, 4, 0x10) == 0x10`.
