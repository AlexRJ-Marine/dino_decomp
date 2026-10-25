# `.tga` format

Status: **solved** — 649/649 retail files parse; decoded pixels match PIL.

## Inventory

| Kind | Count |
| --- | ---: |
| True-color 32-bit (BGR(A)) | 405 |
| True-color 24-bit (BGR) | 240 |
| Grayscale 8-bit | 4 |
| Total | 649 |
| Pixels | 37,486,449 |

All retail files are uncompressed (`image_type` 2 or 3), no color map, and
bottom-left origin. The reader also supports 16-bit true-color and the RLE
variants (`image_type` 10/11) for completeness.

## Header

18 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | image ID length |
| 1 | 1 | color-map type (`0` in retail) |
| 2 | 1 | image type (`2` true-color, `3` grayscale, `10`/`11` RLE) |
| 3 | 2 | color-map origin |
| 5 | 2 | color-map length |
| 7 | 1 | color-map depth |
| 8 | 2 | X origin |
| 10 | 2 | Y origin |
| 12 | 2 | width |
| 14 | 2 | height |
| 16 | 1 | pixel depth (8/16/24/32) |
| 17 | 1 | descriptor: bits 0–3 alpha, bit 4 right origin, bit 5 top origin |

Pixel data starts at `18 + image_id_length`.

## Pixel encoding

- **24-bit**: B, G, R per pixel; alpha = 255.
- **32-bit**: B, G, R, A per pixel.
- **16-bit**: little-endian A1R5G5B5; expand 5-bit channels to 8-bit.
- **8-bit**: grayscale, R=G=B=value, alpha = 255.

Rows are bottom-to-top unless descriptor bit 5 is set. The reader returns
top-left RGBA8, flipping vertically and horizontally as needed.

## Validation

`tools/formats/tga.py`:

- `read_tga(path, decode=False)` reads only the header (fast scan).
- `read_tga(path)` returns `Tga.pixels` as top-left RGBA8 bytes.

Tests compare RGBA bytes against PIL for:

| File | Size | Depth | Result |
| --- | --- | ---: | --- |
| `Data/Graphics/Cursors/CursorMain.tga` | 32×32 | 32 | exact match |
| `Data/Graphics/Textures/actors/animals/cow_lod1.tga` | 128×128 | 24 | exact match |
| `Data/Graphics/Textures/helper/case.tga` | 32×32 | 8 | exact match |

Full scan: 649 files, 0 errors, 37,486,449 pixels
(`tests/test_tga.py`).

## Viewer

`tools/viewer/asset_viewer.py --texture <file.tga> --out <png>` writes a
preview on a neutral background; `notes/view-cursor.png` is an example.
