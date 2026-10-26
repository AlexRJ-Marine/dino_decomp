

## Tail read order (disassembly, 2026-10-26)

`0x53e060` reads the header, then a u8 at record `+0x2d` (file 0x56), then the
tail. The reader vtable calls are `call [edx+8]` with args pushed
`(dest, size, count)`. The observed order of the first tail reads:

```
+0x2d  u8
       4-byte scalar
       4-byte scalar
       count -> 12-byte records (3 u32)
       count -> 12-byte records (3 u32)
       count -> 8-byte records (2 u32)
       count -> 8-byte records (2 u32)
       ...
```

For `Eat.bai` this gives:

```
0x56 u8 1
0x57 count 1 -> 0x5b (4, 1, 200)
0x67 count 0
0x6b count 2 -> 0x6f (0,30), 0x77 (1,10)
0x7f count 0
0x83 ... more scalars/sections (not fully parsed)
```

A simple [12,12,8,8] parse consumes only part of most files (only 1/64 exact),
so there are further sections after the four arrays: scalars, then a vector of
0x58-byte records whose file form includes a 16-byte sub-vector and a string
(reader at 0x53e9a1..0x53ec06 reads `+0x28`, `+0x30` string, `+0x2c`, 16-byte
records, `+0x54`, then `+0x14/+0x18/+0x1c/+0x20/+0x24/+0x3c/+0x40`).

The vectors live in the 0xac-byte runtime record at `+0x58`, `+0x6c`
(12-byte), `+0x7c`, `+0x94` (8-byte) and `+0xa0` (0x58-byte). Finishing this
needs the loop bounds from the decompiler (currently ambiguous) or a runtime
dump of one action record.
