# Dino Island — agent workspace

Reverse-engineering and native-port workspace for **Dino Island** (Monte Cristo,
2002). See `notes/FINDINGS.md` for the evidence and `AGENTS.md` for the rules.

## What is here

| Path | Purpose |
|---|---|
| `original/` | Read-only ISO-derived files (installer, docs). Never edit. |
| `game/%MAINDIR%/` | Extracted retail game tree. Read-only reference. |
| `notes/` | Findings, logs, `STATE.md`. |
| `tools/` | Extraction/triage scripts and format parsers. |
| `src/` | The portable C++ port. |
| `tests/` | Python and C++ tests. |
| `analysis/` | Ghidra project and MCP artifacts. Do not edit. |
| `.opencode/agents/` | Project-specific agents. |

## Quick start

```powershell
# 1. Verify the toolchain
.\scripts\check-env.ps1

# 2. Start the headless Ghidra MCP server (leave running)
.\scripts\start-ghidra.ps1

# 3. Build the C++ skeleton
.\scripts\build.ps1

# 4. Run the tests
.\scripts\run-tests.ps1

# 5. Open the project in OpenCode; the orchestrator agent is the default
opencode
```

## Ghidra MCP

The server is headless (`pyghidra-mcp`) and listens at
`http://127.0.0.1:8000/mcp`. Ghidra 12.2 requires JDK 25
(`C:\Tools\jdk25\jdk-25.0.4.1+1`); `start-ghidra.ps1` sets `JAVA_HOME` for you.
OpenCode connects through `mcp-proxy` (stdio → Streamable HTTP) because the
direct remote-HTTP client refused the loopback connection in this environment.
`opencode mcp list` should print `✓ ghidra connected`.
The Ghidra project is `analysis/ghidra/dino.gpr`; the binary is
`game/%MAINDIR%/DinoIsland.exe`.

If the server is not running, the `ghidra` tools will be unavailable; start it
with `scripts/start-ghidra.ps1` and check with `scripts/ghidra-smoke.ps1`.

## Runtime patches

The retail executable checks for a CD-ROM at startup. A documented, reversible
no-CD patch is in [`docs/patches/no-cd.md`](docs/patches/no-cd.md), with an IPS
at `patches/dinoisland-nocd.ips` and a patcher at `tools/patch_nocd.py`.
Apply it only to the `run/` copy, never to `game/` or `original/`.

## Working method

Read `AGENTS.md`, `notes/STATE.md`, and `TASKS.md` before starting. Pick one
task. Keep `notes/STATE.md` current. Evidence over guesses.

See `notes/ROADMAP.md` for throughput estimates and the critical path.
