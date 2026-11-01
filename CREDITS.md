# Credits & Attribution

Re-Jeweled (Linux and Windows port) stands on a tall stack of other people's work.
This file credits every project, library and tool the port uses, and
the license each one is under. License texts live in `licenses/`.
This project's own code is BSD 3-Clause (see `LICENSE`).

## The game

- **Bejeweled Blitz Live** (Xbox Live Arcade, 2011) — developed by
  **Torpex Games**, based on **PopCap Games**' Bejeweled Blitz;
  © PopCap Games / Electronic Arts.
  No game data is bundled with this port; supply your own copy.

## The recompilation

- **Re-Jeweled** — the ReXGlue recompilation of Bejeweled Blitz Live:
  codegen configuration, file-layer fixes and the XCTD content decoder
  by **Moosh**; graphics fix (texture exponent bias), CPU spin-wait fixes
  and packaging by **thefixinhixon** and the house team.
- **ReXGlue SDK** — static recompilation runtime and codegen
  (BSD 3-Clause; `licenses/ReXGlue-SDK-BSD-3-Clause.txt`), with
  house patches.
- **Re-Jeweled Launcher** — the house TP-style Qt launcher (themed
  variant), BSD 3-Clause. Launcher art is original and procedurally
  generated; it contains no game assets.

## Tools & libraries

- **Qt 6** (LGPL-3.0) — launcher UI.
- **FFmpeg** (LGPL-2.1) — media decoding in the runtime.
- **libmspack** (LGPL-2.1; `licenses/libmspack-LGPL-2.1.txt`) — LZX
  decoding in the bundled `xctd` tool, which prepares the game's
  compressed content files on import. `xctd` links a modified copy of
  libmspack's `lzxd.c` (XMemCompress alignment variant); its complete
  source ships in the AppImage (`usr/share/rejeweled/xctd-src/`) and in
  the Windows package (`xctd-src/`).
- **linuxdeploy / appimagetool** — AppImage packaging.
