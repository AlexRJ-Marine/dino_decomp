# `.loc` format

Status: **solved** — 2/2 retail files parse, 758 records.

## Layout

UTF-16LE with a `FF FE` BOM and CRLF line endings. Each line is
`KEY="value"`. The file is a sequence of records:

```text
FILE_VERSION="2"          (first record only)
FIELD_ID="&NoText"
TEXT="NO TEXT"
LAST_MODIFIED="16/7/2001 12:27"
FIELD_ID="&FreeGame"
TEXT="FREE GAME"
LAST_MODIFIED=""
...
```

- `FIELD_ID` starts a record; `TEXT` and `LAST_MODIFIED` fill it.
- Text values may contain literal `\n` escapes; they are preserved verbatim.
- Values may contain quotes only at the outer delimiters (retail files do not
  escape inner quotes).

## Inventory

| File | Records |
| --- | ---: |
| `Data/Overlay/MenuEng.loc` | 56 |
| `Data/Overlay/ParkEng.loc` | 702 |
| Total | 758 |

Examples:

| Field | Text |
| --- | --- |
| `&NoText` | `NO TEXT` |
| `&Isl10_Name` | `ANTARTIKA` |

## Reader/API

`tools/formats/loc.py`:

- `read_loc(path)` → `Loc(file_version, records)`.
- `Loc.by_id(field_id)`.
- `scan(root)`.

Tests: `tests/test_loc.py` (record counts, first/last values, full scan,
malformed UTF-16 rejection).
