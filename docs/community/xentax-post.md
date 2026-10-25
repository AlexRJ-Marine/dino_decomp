# XeNTaX / ZenHax post draft

**Title:** Dino Island (Monte Cristo, 2002) — mcLib/mcWorld asset formats

Hi all,

I'm reverse-engineering **Dino Island** (Monte Cristo, 2002, PC) for a
preservation/port project. The executable is MSVC 6.0, D3D8 + DirectDraw +
DirectInput8 + DirectSound8, no DRM, no PDB/RTTI. The engine is Monte Cristo's
`mcLib` / `mcWorld`; assert strings expose the module layout (`mg*` graph,
`mi*` input, `mo*` overlay, `ms*` sound, `rs*`/`wd*` world).

I've decoded the main 3D formats far enough to parse the full retail set:

- **`.mesh`** — 536 files. Shared 5-byte magic `01 03 00 00 00`, u32 hash,
  sentinel `0x12345678`, chunk count, name, vertices (3×f32), u32 indices, UVs,
  normals, and an optional per-vertex RGBA table (65 dinosaur meshes) followed
  by a material block with the TGA name.
- **`.skel`** — 64 files. Bone matrices are 64-byte row-major 4×4 floats
  (3ds Max row-vector convention: translation in row 3), followed by child
  lists, an explicit parent table, names, and flags. 20 dinosaurs × 80 bones;
  `Scene_Root` carries the 0.01 scale.
- **`.skin`** — 104 files. 35 per-bone proxy blocks plus one full-mesh block.
  The full-mesh tail is a rigid skin table: every vertex belongs to exactly one
  bone, weight 1.0, with a 4×4 inverse-bind matrix per bone. 60 dinosaur skins
  (20 × 3 LOD) all partition their mesh completely.
- **`.tga`** — ordinary TGA; dinosaur meshes use a small atlas plus vertex
  colors, with bottom-left-origin UVs (`v_tex = 1 + v` when sampling a
  top-left image).

I have parsers and tests that scan all 536 meshes, 64 skeletons and 104 skins
with zero errors, and a software renderer that reproduces the allosaurus bind
pose.

**Questions:**

1. Has anyone here looked at Dino Island or other Monte Cristo `mcLib` games
   (e.g. *Dino Island*, *Paris 1313*, *The Messenger*)? I'd rather compare notes
   than duplicate work.
2. Any prior work on the `.anim` format? That's my next target: 299 files, same
   5-byte magic, presumably per-bone keyframes with times/transforms.
3. Any info on `.lnd`/`.raw` landscape, `.wld` saves, `.fnt` bitmap fonts, or
   the `Data/Design/...` scenario files?
4. Is there a known tool for the 2002 Monte Cristo overlay/UI `.ini` + TGA
   system?

I'm not redistributing game assets; the goal is interoperability and a native
port. I can share format notes, hex dumps of headers, or the parsers.

Thanks!
