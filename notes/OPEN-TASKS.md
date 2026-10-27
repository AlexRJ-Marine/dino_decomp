# Open tasks

Generated from `TASKS.md` on 2026-10-27 23:25. Do not edit; regenerate with `scripts/tasks.ps1 -Update`.

33 open items.

## FMT-10: `.bvi` visitor profiles
- [ ] Decode the five preference weights and the 91–113-byte tail.

## FMT-11: `.bbi` building definitions
- [ ] Numeric tail (attribute profile values) and the `Security` UI section.

## FMT-12: `.bsi` food sources
- [ ] Exact `quantity`/tail numeric semantics.

## FMT-13: `.bni` animal profiles
- [ ] Full numeric field semantics.

## FMT-15: `.seq` animation events
- [ ] Type codes, Sound flags/value and the family `lead` float.

## FMT-16: Campaign and mission data
- [ ] Human-readable names for the trait/show fields, objective weighting and

## FMT-17: `anim_trans.dic` / `graphic_trans.dic` / `proportion.dat`
- [ ] Meaning of merge codes 1/50/99 and the consumer in the hybrid lab.

## FMT-19: `TranslatorConstants.ini` and the genotype translator
- [ ] Name the remaining phenotype fields (`+0x18`, `+0x20`, `+0x24`, `+0x64`,

## FMT-20: `simu_trans.dic` parts 2/3/10
- [ ] Influence-value semantics, the Part 7 `582`, and the gene→Part 3

## FMT-21: Island `.raw` heightmap and occupation mask
- [ ] The 255 occupation sub-classes (water vs cliff vs cover) and `isl00`.

## FMT-23: `.bfi` show-feature definitions
- [ ] Semantics only: `field_3c`/`field_40`, `anim_flag`/`anim_speed`,

## FMT-24: skin/carapace textures + adornment models
- [ ] Pinpoint the translator tables (`+0x84`/`+0x94`) and the claw index.

## FMT-26: `GeneralParams.bgd` global parameters
- [ ] Numeric meanings (times/rates/distances/ramp).

## FMT-27: `Data/States` render-state profiles
- [ ] Profile selection in the engine and the Dino_* pairings.

## FMT-31: `Actions.bad` behaviour data
- [ ] The internal meaning of the first 450 floats per block (editor-only;

## SIM-2b-W: visitor attraction scoring weights
- [ ] Consumer of the five variable weights (no read in an empty or

## OUTREACH-1: XeNTaX / ZenHax post
- [ ] Publish and record any replies.

## CAM-1: Retail camera controls
- [ ] Port the active-camera rotation/transition object

## DEC-6: Function status list
- [ ] Keep it current as functions are replaced.

## TOOL-4: Background-safe virtual input (focus/cooperative-level shim)
- [ ] The load loop still never finishes: after `ParkSoundDB.ini` x32dbg sees
- [ ] Starting a park brings the retail window to the foreground (dgVoodoo/D3D

## DEC-2: Simulation scheduler and AI update order
- [ ] Runtime-confirm the task order, the `0x1f` idle sentinel, and name the

## WORLD-2: Object placement and frustum culling
- [ ] Read scenario/landscape placement data beyond buildings; instance

## SIM-3: Dinosaurs
- [ ] Employee feeding (nutritionist schedule) and the exact `.bsi`
- [ ] Visitor/obstacle reaction: **inert in this build.** The

## CAMPAIGN-3: Campaign menu start + live traits
- [ ] MP2 audio and the intro movie.
- [ ] Fill scarcity (`+0x1e8`); the retail objectives do not use it.

## SIM-4: Economy and attractions
- [ ] Wire employee wages (`0x585d80`) + EFFECTIFS_SECU (`0x585d50`) once

## UI-1: Menus and HUD
- [ ] New game/load/save flows and mouse-driven info panels.

## UI-2: Original menu parity
- [ ] Phase C rest: sliders and minimap water colour/partition polish; live

## MEDIA-1: Audio and video
- [ ] Port music streaming/looping and SFX playback into the SDL3 viewer;

## SAVE-1: Save/load
- [ ] Campaign autosave class: a live Cpg04 autosave (~53 KB,
- [ ] `--sav-manual` writer option (autosave -> manual); blocked while
