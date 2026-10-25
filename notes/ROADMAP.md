# Agent throughput estimate

This is a planning estimate, not a promise. It assumes:

- 2–4 concurrent agents (orchestrator + recon + formats + port), same repo,
  strict task queue, tests, and screenshots.
- A frontier coding model. A small/fast model increases time by ~1.5–2×.
- 30–60 minutes/day of human oversight for priorities, visual validation, and
  unblocking. Truly zero-human operation is slower and riskier.
- The original binary and assets stay available as the oracle.

## Estimate by milestone

| Milestone | Agent wall-clock | Confidence | Main risk |
|---|---:|---|---|
| Environment + harness | 1–3 days | high | installs |
| `.mesh` header + simple formats | 1–2 weeks | high | done for header |
| `.mesh` geometry + OBJ/glTF export | 2–6 weeks | medium | vertex/index layout |
| `.skin` + `.skel` + `.anim` | 4–10 weeks | medium | bone weights, tracks |
| First lit, animated dinosaur in a window | 2–4 weeks after formats | medium-high | rendering setup |
| One island, terrain, object placement | 6–12 weeks | medium | `.lnd`/`.raw` semantics |
| **Playable vertical slice** (camera, path/fence, one dino, 10 min) | **4–8 months** | low-medium | simulation semantics |
| Broad playable port (most systems, several islands) | 9–18 months | low | content/UI breadth |
| Full parity (all scenarios, UI, audio, save, edge cases) | 2–4 years | low | long-tail behavior |
| Full decompilation to compilable C++ | 3–6+ years | very low | 100k–200k LOC, no symbols |

## Why the long tail is long

- The bottleneck is not typing. It is **deciding what an unknown byte means** and
  **proving it** against the original game.
- 2.24 MB of `.text` is roughly 100k–200k lines of C++ equivalent. Ghidra output
  is pseudocode; turning it into a coherent, buildable engine is manual.
- Dependencies serialize: formats → viewer → world → simulation → UI. Only the
  format and tooling work parallelizes well early.
- Visual correctness needs screenshots and human (or multimodal) inspection.
  An agent that cannot see the result can write plausible but wrong code.
- The game is a 2002 business sim: the value is in the simulation rules and
  content, not in the renderer. Those rules are the least documented part.

## Fastest path to something playable

1. Mesh/skin/skeleton/animation exporters (the current critical path).
2. A minimal SDL3 + OpenGL viewer for one animated dinosaur.
3. One island terrain + object placement.
4. Camera + path/fence placement.
5. Visitors + one attraction + money.
6. Stop and playtest; only then expand content.

## What would change the estimate

- A working matching decompiler / recovered source tree: months saved.
- A clean format spec from the community: weeks saved.
- Human completes the hard 3D format RE: 30–50% faster overall.
- More parallel agents: helps formats/tools, not the serial integration path.
- A weaker model or no visual feedback: 1.5–2× slower.
