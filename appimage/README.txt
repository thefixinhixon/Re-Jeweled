Re-Jeweled — Bejeweled Blitz Live, native Linux port (test build 1)
===================================================================

A static recompilation of Bejeweled Blitz Live (Xbox Live Arcade, 2011,
Torpex Games / PopCap) for Linux, built on the ReXGlue SDK.
Ships as a single AppImage containing the Re-Jeweled Launcher and
the matched game program set. NO game data is included.

What's in test build 1
----------------------
* Full graphics: title screen, menus, gameplay, results. (Earlier
  builds - and upstream Xenia - showed only the circuit-board frame:
  the GPU shader translator read each texture's brightness exponent
  from the wrong word of its fetch constant, so all GFx art and text
  rendered near-black.)
* Light on the CPU: the game's Xbox 360 GPU-wait spin loop now sleeps
  instead of burning a core, and the runtime's audio thread no longer
  spins. About 0.3-0.9 of a CPU core at a locked 60 fps (was ~2.3).
* Import step for the game's compressed content files (see below).

You need
--------
* Your own copy of Bejeweled Blitz Live (title id 58410A57), either:
  - the XBLA package file (the hash-named file from the console's
    Content/.../58410A57/000D0000/ folder), or
  - an already-extracted copy: a folder holding Default.xex,
    blitz.pak, blitzgfx.pak, blitztitle.pak, blitz.xpr, loc/,
    properties/ and video/.
* About 300 MB free for the prepared copy.
* A reasonably current Linux distro (see the glibc note below), a
  Vulkan-capable GPU, and a controller.

Run it
------
1. chmod +x ReJeweled-x86_64.AppImage
2. ./ReJeweled-x86_64.AppImage
3. First run: "Import game data (XBLA package / folder)...".
   - Extracted folder: the folder is used where it is. Four of the
     game's content files ship XCTD-compressed (the Xbox 360
     decompresses them on the fly; the Linux runtime needs them
     decompressed once). If they still are, the launcher asks once
     whether to decompress them in that folder or make a prepared copy
     elsewhere. An already-prepared folder is just registered.
   - XBLA package: pick the package, then a location; the launcher
     writes <location>/<package name>/gamedata/ and decompresses the
     files there.
   The choice is remembered.
4. PLAY. The title screen accepts START after the logo movies.

Settings worth knowing
----------------------
* Vsync is ON by default and paces the game at 60 fps. With vsync
  off, set a frame limit (e.g. 60).
* AMD FSR upscaling and resolution scale are available; the game is
  2D, so FSR at 1x is plenty.
* Saves and logs go to the folders shown on the Settings tab
  (game log: <game data folder>/logs/game.log).

Known notes (test build)
------------------------
* The game binary in this build requires glibc 2.43+ (built on the
  development machine), same as the other house test builds; the
  launcher itself needs less.
* First launch compiles shaders - a brief hitch on the first title
  screen is normal and caches away.
* Audio uses PipeWire's PulseAudio layer (set automatically).
* Xbox Live features (leaderboards, Battle LIVE, Party LIVE) are not
  available offline; One Player modes work.

Credits: see CREDITS.md (also inside the AppImage under
usr/share/licenses/rejeweled/).
