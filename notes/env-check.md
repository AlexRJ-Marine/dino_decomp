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
