# TP House Launcher — Windows port (Phase 2)

Phase 1 (done) moved every OS-specific behavior of the launcher behind
`src/platform.h` / `src/platform.cpp`:

- `platform::gameRootSearchBases()` — where Games libraries live
- `platform::programSetFiles(profile)` — the matched program-file set
- `platform::vacuumGuestMemory()` — pre-launch arena cleanup (no-op on
  Windows by design: the guest-memory backing there is a named
  pagefile mapping owned by the game process, destroyed by the kernel
  at exit; it cannot leak like a `/dev/shm` tmpfs file)

The Windows branches are written in pure Qt behind `#ifdef Q_OS_WIN`.
Phase 2 (2026-10-31) made them build: `.github/workflows/windows.yml`
compiles this launcher with the MSVC Qt kit, as a GUI-subsystem binary
(`WIN32` + `Qt6::EntryPoint`, no console window), and packages it with
`windeployqt`. Runtime behavior on Windows has not been play-tested yet —
see `../docs/windows.md` for the list. Linux behavior is unchanged
(the Windows code paths are all compile-time, and the Linux program set
is untouched).

## Prerequisites

- Windows 10/11 x86_64
- Visual Studio 2022 (MSVC, C++20) — or clang-cl with the MSVC STL
- Qt 6 kit for MSVC (the launcher sticks to Qt 6.4-era APIs; 6.4+ fine).
  Qt Widgets only.
- CMake 3.16+

## Launcher build

Same CMake project, nothing special:

```
cmake -S . -B build-win-maw -DTP_GAME=maw
cmake --build build-win-maw --config Release
cmake -S . -B build-win-splosion -DTP_GAME=splosionman
cmake --build build-win-splosion --config Release
```

Produces `TPMawLauncher.exe` / `TPSplosionmanLauncher.exe` (this repo:
`-DTP_GAME=bejeweled` → `ReJeweledLauncher.exe`). The QSettings ini scheme
carries over unchanged (IniFormat, org "HouseRecomp", per-game app name
→ `%APPDATA%`-side user scope). Run `windeployqt --compiler-runtime` on
the exe before moving it anywhere — Windows/stage-package.ps1 does that.

## Game-side build

The games are ReXGlue recomp projects; the SDK and each game project
configure for Windows with MSVC the same way they do for Linux:

- 'Splosion Man: the SplosionManRecomp repo (config + manifest +
  committed `generated/` tree; the XEX stays out of git — the builder
  supplies their own `assets/default.xex`).
- The Maw: the maw project tree (same shape).
- **Set `REXGLUE_ENABLE_FIDELITYFX=ON` in the SDK configure** — with
  it off, `--present_effect=fsr` silently falls back to bilinear.
  This exact trap shipped once on Linux; audit the produced runtime
  for the `present_fsr` symbols (`FLAGS_present_fsr_quality_mode`)
  before packaging anything, the same strings audit used on Linux
  (plus the PipeWire audit's Windows equivalent: check the runtime's
  audio backend imports are the expected Windows ones).## platform.cpp status

1. DLL names: **confirmed** against the house Windows SDK build
   (SplosionManRecomp run #8, 2026-10-03) — `rexruntime.dll` and
   `rexgpu-xenos.dll`, no `lib` prefix. `programSetFiles()` carries them.
2. Exe-set layout: the SDK's POST_BUILD step stages the runtime DLLs, the
   GPU plugin (via `GPU_PLUGINS`) and the FidelityFX DLLs beside the exe,
   so `resolveExecutableSet`'s "one folder" rule holds unchanged.
3. `vacuumGuestMemory()` stays a no-op on Windows: the guest-memory
   backing is assumed to be a process-owned mapping. If a real Windows
   run shows leaked files anywhere, that is the one spot to revisit.

Two Windows-only differences are handled outside platform.cpp:
`gameprofile.h` drops the Linux `SDL_AUDIO_DRIVER=pulseaudio` override
(empty environment on Windows), and `xctdprep.cpp` looks for `xctd.exe`.

## GitHub Actions (implemented)

`../.github/workflows/windows.yml` builds the whole Windows package in
one job: pinned SDK + house patches, game (clang`, FidelityFX ON),
`xctd`, launcher (Qt 6.8 MSVC, `-DTP_GAME=bejeweled`), `windeployqt`,
and `../windows/stage-package.ps1` assembling and asserting the folder.
The artifact is the complete folder — launcher plus the program set
beside it, the layout `resolveExecutableSet` rule (2) expects. **No game
data**: users import their own XBLA package on first run, same as Linux.
`../docs/windows.md` has the step-by-step and the open questions.

## Tester-loop verification model

There is no Windows machine in the dev loop, so verification rides on
a human tester with a real Windows box (the model that worked for the
Linux ports, with the tester as the remote pilot):

1. Tester unzips the folder build, runs the launcher.
2. First run: detection finds their Games folder (or not) → Import
   XBLA package → PLAY.
3. Evidence comes back as files, not impressions: `last-command.txt`
   and the game log (both written into the data root's logs folder by
   the launcher/runtime), plus screenshots for anything visual.
4. Settings spot-checks are observable the same way as on Linux:
   change one knob, relaunch, diff `last-command.txt`.
5. Audio/input verdicts are the tester's (their session owns the
   devices — the Linux uid-session lesson applies in spirit).

Known Windows unknowns to burn down in the loop, in order: DLL names
(above), FidelityFX actually engaging (A/B at 720p + FSR vs bilinear),
audio backend behavior, controller behavior, and where the runtime
puts saves by default (so the launcher's data-root defaults match).
