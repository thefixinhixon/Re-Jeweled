# Re-Jeweled

A native port of **Bejeweled Blitz Live** (Xbox Live Arcade, 2011,
Torpex Games / PopCap Games) for Linux and Windows, built with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) static recompiler.
It belongs to the same family as
['Splosion Man](https://github.com/thefixinhixon/SplosionManRecomp) and
[Re-Cherry](https://github.com/thefixinhixon/Re-Cherry).

**Status (Test build 1b): playable on Linux.** The title screen, menus,
Classic one-player rounds and results all work, with full graphics,
PipeWire audio and controller input. It runs at a locked 60 fps using
about 0.3 to 0.9 of a CPU core.

**Windows (new, build 1): builds in CI, not yet play-tested.**
`.github/workflows/windows.yml` produces a Windows 10/11 x64 folder
package (launcher + matched program set + `xctd`, no game data); Windows
runs the runtime's D3D12 backend. See `docs/windows.md` for how it is
built and what is still unproven.

> **No game data is included in this repository or its releases.**
> Supply your own legally obtained copy of the game, title ID **58410A57**.
> That can be the XBLA package, or an extracted folder containing
> `Default.xex`, `blitz.pak`, `blitzgfx.pak`, `blitztitle.pak`,
> `blitz.xpr`, `loc/`, `properties/` and `video/`.

## Play (release AppImage)

1. Download `ReJeweled-x86_64.AppImage` from the
   [Releases](../../releases) page.
2. Run `chmod +x ReJeweled-x86_64.AppImage`, then
   `./ReJeweled-x86_64.AppImage`.
3. Click **Import game data**. Choose either:
   - **Extracted folder:** the folder is used where it is. Four of the
     game's content files ship XCTD-compressed. The Xbox 360
     decompresses these on the fly, but this port needs them
     decompressed once. The launcher offers to do that in place or to
     make a prepared copy.
   - **XBLA package:** the launcher extracts the package into a folder
     you choose and decompresses the files there.
4. Click **PLAY**.

The game binary needs **glibc 2.43 or newer**, a Vulkan GPU and a
controller. See `appimage/README.txt` for the full release notes.

## Play (Windows package)

1. Download the `ReJeweled-windows-x64` artifact from the latest
   **Windows build** workflow run and unzip it into a writable folder.
2. Run `ReJeweledLauncher.exe`, then **Import game data** exactly as on
   Linux, and click **PLAY**.

Windows 10/11 x64, a D3D12-capable GPU and a controller. `README.txt`
inside the package is the user-facing version of this section.

## Build with GitHub Actions

`.github/workflows/windows.yml` runs on `workflow_dispatch`, pushes to
`main` and pull requests, and needs no Windows machine. It pins the SDK
commit, applies the house patches, builds the game/launcher/`xctd` and
uploads the packaged folder.

`generated/` is not committed, so the job needs the recompiled sources
from somewhere: a committed `generated/` tree, the Actions cache from an
earlier run, or `assets/default.xex` in the checkout (then it runs
codegen itself and caches the result). With none of the three it stops
with an explanation. `docs/windows.md` has the details.

## What this port fixed

- **Missing art.** All GFx art and text rendered near-black, leaving
  only the circuit-board frame. The SDK's SPIR-V translator read each
  texture's `exp_adjust` from fetch-constant dword 4 bits 13..18, which
  overlap the LOD bias. The correct source is dword 3. This game's
  −0.5 LOD bias decoded as an exponent of −8, which scaled every texel
  by 2⁻⁸.
- **CPU use** (about 2.3 cores, now 0.3 to 0.9):
  - The game's XDK D3D fence wait (`sub_82422E60`) busy-spun. It is
    overridden in `src/perf_hooks.cpp` to sleep 100 µs between polls.
  - The runtime's audio worker spun in `WaitMultiple`. That is fixed in
    the SDK patch.
- **XCTD content files.** These are decoded offline by `tools/xctd`,
  which is LZX via a lightly modified libmspack `lzxd.c`. The launcher
  runs the decoder on import (`tp-launcher/src/xctdprep.cpp`).

`docs/porting-notes.md` is the full investigation log.

## Repository layout

| Path | What |
|---|---|
| `bejeweled_manifest.toml`, `bejeweled_config.toml` | ReXGlue codegen project and per-function config |
| `src/` | App entry, import stubs, perf hook |
| `scripts/` | Post-codegen fixes, including the VFS archive-root fallback and the movie-preload skip (`fix-handedits.py`). Run `scripts/refix.sh` after every codegen. |
| `patches/` | ReXGlue SDK patches this port needs (see `patches/README.md`) |
| `tools/xctd/` | XCTD (XCompress LZX) decoder used on import; standalone CMake project, builds on Linux and Windows |
| `tools/framegrab/` | Debug helper: dump a guest texture from the memory arena |
| `tp-launcher/` | Re-Jeweled Launcher (Qt 6), the Bejeweled profile of the house launcher |
| `appimage/` | AppImage packaging script, release README, licenses |
| `windows/` | Windows package staging script and the README that ships in the package |
| `.github/workflows/` | Windows CI build |
| `docs/windows.md` | Windows port notes: what CI does, what is verified, what is not |
| `assets/`, `generated/` | Your `default.xex` and codegen output. Both are gitignored and never committed. |

## Build from source (Linux)

You need clang-20, cmake 3.25 or newer, ninja, Qt 6 (6.4 or newer) and
Python 3.

1. **SDK.** Clone `rexglue-sdk` at commit `f5337cd`, then apply
   `patches/rexglue-sdk-f5337cd-to-test1.patch`:
   ```sh
   git apply /path/to/Re-Jeweled/patches/rexglue-sdk-f5337cd-to-test1.patch
   ```
   Build the `rexglue` CLI following the SDK wiki.
2. **Game executable.** Copy your `default.xex` into `assets/`.
3. **Codegen.** This takes about 9 minutes, and it is a manual step on
   purpose:
   ```sh
   rexglue codegen bejeweled_manifest.toml
   bash scripts/refix.sh        # REQUIRED after every codegen
   ```
4. **Build the game:**
   ```sh
   cmake --preset linux-amd64-release -DREXSDK_DIR=/path/to/rexglue-sdk
   cmake --build out/build/linux-amd64-release --target bejeweled
   ```
   This produces `out/build/linux-amd64-release/bejeweled`. The runtime
   libraries `librexruntime.so` and `librexgpu-xenos.so` land in the
   SDK's `out/linux-amd64/`. The exe and the libraries are a **matched
   set**, so never mix builds.
5. **Launcher:**
   ```sh
   cmake -S tp-launcher -B tp-launcher/build -DCMAKE_BUILD_TYPE=Release
   cmake --build tp-launcher/build
   ```
6. **xctd:** standalone CMake project (see `tools/xctd/README.md`):
   ```sh
   cmake -S tools/xctd -B tools/xctd/build -DREXSDK_DIR=/path/to/rexglue-sdk
   cmake --build tools/xctd/build
   ```
7. **Run without the launcher.** Put `bejeweled` and the two libraries
   in one folder, then run:
   ```sh
   SDL_AUDIO_DRIVER=pulseaudio ./bejeweled --gpu_plugin=xenos --license_mask=1 \
       --game_data_root=/path/to/prepared/gamedata
   ```
   A prepared data folder is one whose XCTD files are decoded, either
   with `xctd <in> <out>` per file or through the launcher's import.
8. **AppImage.** Copy the matched set into `out/run/`, then run
   `env -u LD_PRELOAD appimage/build-appimage.sh`. The script needs
   `linuxdeploy`, `linuxdeploy-plugin-qt` and `appimagetool` in
   `$TOOLS`, which defaults to `~/tools`.

## Credits and license

See `CREDITS.md`. This project's own code is BSD 3-Clause (`LICENSE`).
Third-party licenses are in `licenses/`. Bejeweled is © PopCap Games /
Electronic Arts. This is an unofficial fan project, not affiliated with
or endorsed by PopCap, EA, Torpex or Microsoft.
