# Third-party code

## `pl_mpeg.h`

- **Project:** PL_MPEG — MPEG1 video/MP2 audio decoder and MPEG-PS demuxer
- **Author:** Dominic Szablewski
- **License:** MIT (SPDX header in the file)
- **Source:** https://github.com/phoboslab/pl_mpeg (master, fetched 2026-10-27)
- **Used by:** `src/formats/mpg.cpp` (`--video-info`, `--video-frame`)

The file is vendored unmodified. Only the video decoder is used; audio is
disabled. The retail `.mpg` ending videos are read from `game/` at runtime and
are not copied into the repository.
