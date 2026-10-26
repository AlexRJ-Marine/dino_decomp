# Vector reader hooks (`0x47d460`, `0x47cc90`)

Status: **ABI solved; both detours implemented in `src/hook/dinput8_proxy.cpp`
behind `DINO_HOOK_VECTOR=1`.** Runtime call verification is pending a
dinosaur/campaign load (the free-game park does not call them).

## Signatures

Both are `__thiscall`, one stack argument, `RET 4`, return in AL:

```text
bool f(void* this /*ECX*/, Stream* stream /*[esp+4]*/)
```

Prologue (both):

```text
51            push ecx
53 56 57      push ebx / esi / edi
8B 7C 24 14   mov edi,[esp+0x14]     ; stream
8B F1         mov esi,ecx            ; this
```

The 5-byte detour would split `mov edi,[esp+0x14]`; patch 8 bytes
(`E9 rel32` + 3 NOP).

## Stream protocol

`stream->vtable[2]` (`+0x8`), `__thiscall`:

```text
read(void* dst, u32 elem_size, u32 count) -> u32 elems_read
```

Order: **one u32 count first**, then `count` separate one-element reads.
`0x47d460` uses `elem_size = 2` (u16); `0x47cc90` uses `elem_size = 4`
(u32). There is no sanity limit on the count.

## Destination vector

16-byte polymorphic wrapper (`ctor 0x479460`, vtable `0x6256cc`):

```text
+0x00 void** vptr
+0x04 T*     first
+0x08 T*     last
+0x0C T*     end_of_storage
```

The readers work on `&first` (`obj+4`). They **resize to the count**, never
append:

```text
n < size   -> last = first + n
n >= size  -> _Insert_n(&first, last, n - size, &zero)
              capacity = size + max(n - size, size)
```

Insert helpers: `0x47d500` (u16), `0x47cac0` (u32); allocation goes through
mcLib `mcMemSTL` (`0x458f40` -> `0x4052c0`), not `operator new`.

## Call sites

- `0x47cc90` is the `.skel` **child index list** reader: the `.skel` object
  `+0x28` is an outer vector of 16-byte holders; per bone it reads a u32
  child count then that many u32 indices into the holder's inner vector.
  This matches the port's `.skel` parser exactly (child indices are u32).
- `0x47d460` is **not** `.skel`: its only xref is `0x4724de` inside
  `FUN_00472240`, reading a u16 vector at `+0x78` of a 0xb8-byte mesh record
  (subobject `+0x14`). Which member that is remains unknown.

## Replacement

```cpp
bool __thiscall hook_47cc90(Vec<uint32_t>* vec, Stream* stream) {
    uint32_t n;
    if (read(stream, &n, 4, 1) != 1) return false;
    resize_to(vec, n, insert_47cac0);
    for (uint32_t i = 0; i < n; ++i)
        if (read(stream, &vec->first[i], 4, 1) != 1) return false;
    return true;
}
```

`hook_47d460` is identical with u16 and `insert_47d500`.

Failure leaves the vector resized but partly filled and returns 0; the `.skel`
caller then skips that loop and continues, so byte-exact stream consumption
matters.

## Verification state

- Both hooks install cleanly (`hook: FUN_0047d460 patched`, `FUN_0047cc90
  patched`) and the free-game park remains playable with them active.
- **Runtime-verified on a Cpg04 campaign load:** `hook_47d460` and
  `hook_47cc90` were each called **2000+ times**, and the park rendered its
  terrain, buildings and dinosaurs normally (`notes/hyb3-campaign.png`).
  `notes/campaign-park.png` shows the mission briefing over the loaded park.
- The free-game park does not call either reader (0 calls).
- Both are now installed by default; delete `run/DINPUT8.dll` to revert the
  whole proxy.

## Evidence

- `0x47d460`, `0x47cc90` prologues/disassembly; `0x47d500`, `0x47cac0`
  insert helpers; `0x479460` vector ctor; `0x458f40`/`0x4052c0` allocator.
- Xrefs: `0x4724de` (mesh record), `0x477951` (`.skel` reader).
- `src/hook/dinput8_proxy.cpp`, `docs/decomp/hybrid.md`.
