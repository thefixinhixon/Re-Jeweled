Re-Jeweled - Bejeweled Blitz Live, native Windows port (build 1)
================================================================

A static recompilation of Bejeweled Blitz Live (Xbox Live Arcade, 2011,
Torpex Games / PopCap) for Windows 10/11 x64, built on the ReXGlue SDK.
This folder holds the Re-Jeweled Launcher and the matched game program
set. NO game data is included.

What's in this build
--------------------
* Full graphics: title screen, menus, gameplay, results.
* Launcher: game detection, import of your own copy, and settings
  (vsync, frame limit, resolution scale, FSR, audio), same as the Linux
  launcher.
* Windows runs the runtime's D3D12 backend (the GPU plugin in this
  build is the D3D12 one).

You need
--------
* Windows 10 or 11, x64.
* Your own copy of Bejeweled Blitz Live (title id 58410A57), either:
  - the XBLA package file (the hash-named file from the console's
    Content/.../58410A57/000D0000/ folder), or
  - an already-extracted copy: a folder holding Default.xex,
    blitz.pak, blitzgfx.pak, blitztitle.pak, blitz.xpr, loc/,
    properties/ and video/.
* About 300 MB free for the prepared copy.
* A GPU with working D3D12 drivers, and a controller (XInput).

Run it
------
1. Unzip this folder anywhere writable (not Program Files - the game
   writes saves and logs next to its data).
2. Run ReJeweledLauncher.exe.
3. First run: "Import game data (XBLA package / folder)...".
   - Extracted folder: the folder is used where it is. Four of the
     game's content files ship XCTD-compressed. The Xbox 360
     decompresses them on the fly; this port needs them decompressed
     once, which the launcher does with xctd.exe from this folder.
   - XBLA package: pick the package, then a location; the launcher
     writes <location>/<package name>/gamedata/ and decompresses the
     files there.
   The choice is remembered.
4. PLAY. The title screen accepts START after the logo movies.

Settings worth knowing
----------------------
* Vsync is ON by default and paces the game at 60 fps. With vsync
  off, set a frame limit (e.g. 60).
* Resolution scale and FSR upscaling are available; the game is 2D, so
  FSR at 1x is plenty.
* Saves and logs go to the folders shown on the Settings tab
  (game log: <game data folder>/logs/game.log). The exact command the
  launcher ran is written to logs/last-command.txt - include it when
  reporting a problem.

Known notes
-----------
* First launch compiles shaders - a brief hitch on the first title
  screen is normal and caches away.
* Xbox Live features (leaderboards, Battle LIVE, Party LIVE) are not
  available offline; One Player modes work.
* If the game starts but the window stays black, check that the D3D12
  DLLs in this folder were not removed, and see logs/game.log.
* Do not replace individual files in this folder with ones from
  another build: the exe, rexruntime.dll and rexgpu-xenos.dll are a
  matched set and must stay together.

Files in this folder
--------------------
  ReJeweledLauncher.exe   launcher (Qt 6)
  bejeweled.exe           the recompiled game
  rexruntime.dll          matched ReXGlue runtime
  rexgpu-xenos.dll        matched Xenos GPU plugin (D3D12)
  amd_fidelityfx_dx12.dll FSR support (when present)
  xctd.exe                XCTD decoder used by the import step
  licenses/               attribution and third-party licenses
  xctd-src/               xctd sources (modified libmspack lzxd.c)

Credits: see licenses/CREDITS.md.
Bejeweled is (c) PopCap Games / Electronic Arts. This is an unofficial
fan project, not affiliated with or endorsed by PopCap, EA, Torpex or
Microsoft.
