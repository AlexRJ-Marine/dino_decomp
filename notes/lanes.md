# Lanes — ownership and build isolation

Updated: 2026-10-27

Two or more agents can work in this tree at once. Claim a lane before editing
and use its build directory. Do not run a shared executable while another lane
is building it; build into your own directory instead.

## Contributors

| Contributor | Agent lane | Branch | Build dir |
| --- | --- | --- | --- |
| AlexRJ-Marine | hybrid/TOOL-4 + P3 runtime (`src/hook`, harness probes) | `main` | `build/port` |
| marcusrj | port/viewer UI, world and formats (`src/viewer`, `src/ui`, `src/world`, `tools/formats`) | `portable-setup` -> `main` | `build/marcus` |

The queued jobs for marcusrj are in `notes/jobs-marcusrj.md`. Do not start a
row there without claiming it here, and do not touch another lane's files.

### Branch and merge flow

1. Work on your own branch (`portable-setup`, or `<name>/<topic>`), commit small.
2. Before hand-off: `git fetch origin` and rebase your branch on `main`.
3. The `main` owner fast-forwards (`git merge --ff-only origin/<branch>`) only
   after `scripts/verify.ps1 -Fast` passes, then pushes `main`.
4. Never force-push `main` and never rewrite another contributor's commits.

## Lanes

| Lane | Owner | Owns | Build dir | Runtime |
| --- | --- | --- | --- | --- |
| hybrid | hybrid agent | `src/hook/**`, `docs/decomp/**`, `notes/hybrid-*`, `run-test/DINPUT8.dll` | n/a (32-bit DLL) | `run-test/` via `scripts/x32dbg/*` |
| port | port/viewer agent | `src/viewer/**`, `src/world/**`, `src/ui/**`, `src/formats/**`, `CMakeLists.txt`, `tests/test_viewer_cpp.py` | `build/port` | `run-window/` |
| formats | formats agent | `tools/formats/**`, `docs/formats/**`, `tests/test_*.py` (except viewer) | n/a | none |
| recon | recon agent | `notes/**`, `docs/decomp/**` (read-only code) | n/a | `run-test/` |
| harness | harness agent | `scripts/**`, `run-test/**`, `run-window/**`, `docs/platform/windowed-testing.md` | n/a | both |
| docs | archivist | `notes/NOW.md`, `notes/STATE.md`, `notes/OPEN-TASKS.md`, `notes/history/**`, `notes/evidence-index.md` | n/a | none |

## Rules

- **Build dirs:** `scripts/build.ps1 -BuildDir build/<lane>`; tests
  `scripts/test.ps1 -Exe build/<lane>/dino_viewer.exe ...`. Never build into
  another lane's directory.
- **Viewer processes:** each lane may run `dino_viewer.exe` from its own build
  dir. Killing by image name can hit another lane; target the exact PID.
- **Retail copies:** `run-window/` is for normal play, `run-test/` for the
  x32dbg/virtual-input harness, `run/` is the legacy fullscreen copy. Do not
  modify `run/`, `run-test/` or `run-window/` data from a lane that does not
  own the harness.
- **Shared files:** `notes/NOW.md`, `notes/STATE.md`, `TASKS.md`,
  `opencode.jsonc`, `AGENTS.md` are merged by targeted edits. Announce a
  completion with `scripts/state.ps1 -Done "..."` so the other lane sees it.
- **Commits:** stage only your lane's files. If another lane's hunk is in the
  same file, commit it too and say so in the message (merge, do not revert).
- **Read-only:** `original/**` and `game/**` are never writable; `scripts/verify.ps1`
  fails if `git status` reports changes there.

## When to use a worktree

Use `tools.opencode.worktree_create` for a lane that will touch many files for
more than a day (e.g. a large rendering refactor). Junction `game/` into the
worktree (it is read-only) and use a lane-local `run-*` directory. Merge back
normally; do not rewrite history.
