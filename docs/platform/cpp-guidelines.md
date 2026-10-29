# C++ guidelines for the port

Status: **maintained policy, 2026-10-29.** The port follows the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
where they improve safety or readability. This page records the rules we act
on, the deliberate exceptions, and how they are enforced. It is not a summary
of the guide; read the linked rule when changing the pattern.

Scope: first-party code under `src/` except `src/hook/**` (32-bit retail proxy)
and `src/third_party/**` (vendored decoders). Those two keep their own style.

## Rules we apply

### Functions and naming (P.1, P.3, F.2, F.3, NL.5)

- One logical operation per function; the phase names in `game.h` say what a
  frame step does. When a body outgrows a screen or two, split it by domain
  (MAINT-2/MAINT-5 in `TASKS.md`) instead of adding comments to a monolith.
- Names say intent, not type: `update_menu_loading`, not `do_menu_bool`.
- New helpers are declared in `game.h` next to their phase with a one-line
  banner comment, and the phase keeps the call order.

### Resources and ownership (P.8, R.1, R.11, C.21, C.35, C.128)

- Own resources with RAII, never with a naked `new`/`delete`. A C handle is
  wrapped in a local `std::unique_ptr` with a deleter at the boundary
  (`PlmVideo` in `formats/mpg.cpp`).
- Pimpls use `std::unique_ptr<Impl>`; the destructor is out of line where the
  `Impl` is incomplete (`formats/mpg.h`).
- A type that owns a resource defines or deletes all of copy, move and
  destructor (`Game`, `MpgStream`). `Game` is non-copyable and non-movable.
- Base classes have a public virtual destructor or a protected non-virtual
  one; overriders say exactly one of `virtual`, `override`, `final`.
- The raw `SDL_*`, `GLuint`, `plm_t*` handles are C API values. They are owned
  by `Game` (released in `Game::shutdown()`) or by an RAII wrapper; they never
  appear as transferable parameters.
- Writes go through `formats/atomic_file.h` (temp file, flushed and renamed),
  not a bare `std::ofstream` on the target; a crash cannot leave a truncated
  save.

### Initialization and constants (ES.20, ES.23, ES.45, Per.11)

- Initialize objects at declaration; prefer `{}`. `Game` is the documented
  exception: its members are former `main()` locals initialized by the boot
  phases in retail load order, so they deliberately have no default
  initializer. Do not "fix" that without preserving the order.
- No magic maths constants: `dino::kPi` (`formats/math.h`) is the one value
  for pi, and expressions keep the retail order `x * kPi / 180.0f` so the
  float rounding does not change. Numeric retail offsets and file field values
  stay numeric; their comment names the binary field.
- New fixed-size containers use `std::array`/`std::vector`. `Mat4::m[16]`
  stays a flat C array because it mirrors the file layout and is uploaded to
  GL as a block; `Vec3` stays a plain aggregate.

### Types and casts (P.4, ES.48, ES.49, Enum.3)

- Prefer typed values and `enum class`; a plain `enum` is allowed only where a
  serialized file or C API fixes the underlying type, and then it is named and
  commented.
- Use only named casts. `static_cast` for arithmetic and upcasts,
  `reinterpret_cast` only at the OS/GL/ABI boundary (buffer pointers, hook
  trampolines). No `const_cast` in first-party code.
- Narrowing conversions from file data are made explicit with
  `static_cast<T>` after a range check or with a clamp.

### const and interfaces (Con.1, Con.2, Con.3)

- Locals and member functions are `const` by default. `Game` phase methods
  mutate the session by design; pure queries are `const` (see `park_camera`,
  format readers).
- Pass large values by `const&`; pass sink values by value and `std::move`
  them. Do not return `const` values.
- No `using namespace` at global scope in headers; headers include what they
  use (SF.7, SF.10, SF.11).

### Errors (E.2, E.14)

- Format readers throw `std::runtime_error` with the path and field; callers
  catch at a command or boot boundary and print one diagnostic.
- C callbacks do not throw. Preallocate or return a status instead.
- Do not use `catch (...)` in first-party code: it would turn allocation
  failures into "missing value" fallbacks. Catch `std::exception` (or a more
  specific type) at the parse/boot boundary; an unknown exception is a bug
  and should terminate the run visibly.

## Deliberate deviations

| Area | Deviation | Why |
| --- | --- | --- |
| I/O | `std::printf`/`std::fprintf` for reports and diagnostics | The `--*-info`/`--validate` output is a stable test contract; iostream would add no value and change the bytes. |
| Numbers | Numeric retail offsets/constants in expressions | They mirror the binary and the file data; naming them would hide the address the evidence refers to. |
| `Mat4` | `float m[16]` | Matches the row-major 4x4 file/GL layout and is uploaded as a block. |
| GL names | `GpuMesh`/`GLuint` are non-owning names; the owner calls `destroy(glfns)` / `glDeleteTextures` | The deleter needs the loaded `GlFns` table; `Game::shutdown` and the per-scene clear paths own the lifetime. Do not destroy a copied name twice. |
| `Game` members | No default member initializers | They are former `main()` locals initialized by boot phases in retail load order (P.8/ES.20 exception, documented in `app-shell.md`). |
| Build | No exceptions disabled, no RTTI use | Compiler defaults; the code never uses `typeid`/`dynamic_cast`. |

## Enforcement

- `tests/test_cpp_style.py` bans naked `new`/`delete`, `using namespace` in
  headers, `NULL`, `std::endl` and new `3.14159` literals in first-party code.
- `tests/test_naming.py` pins the layer/executable names; `test_app_shell.py`
  pins the phase order, the member-shadow rule, the SDL-free lower layers and
  the fast-gate split.
- `-Wall -Wextra` is on for every target. `scripts/verify.ps1 -Fast` runs the
  static guards and the headless app tests; the full suite runs the window
  tests.
- Review: a change that owns a new resource, changes a cast, or grows a phase
  past ~200 lines should cite the rule it follows or the reason for the
  exception in the commit message.
