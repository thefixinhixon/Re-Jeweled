# Windows build

The Windows package is the same game as the Linux build: the launcher, the
matched program set and `xctd`, with no game data. This file is the
maintainer view - how CI assembles it, what is verified, and what is still
unproven. The user-facing instructions ship inside the package
(`windows/README.txt`).

## What Windows runs

| Piece | Windows form |
|---|---|
| Game exe | `bejeweled.exe` (same recompiled sources as Linux) |
| Runtime / GPU plugin | `rexruntime.dll`, `rexgpu-xenos.dll` (no `lib` prefix on Windows) |
| Graphics backend | D3D12 - the SDK defaults `REXGLUE_USE_D3D12=ON` and `REXGLUE_USE_VULKAN=OFF` on Windows, so the Xenos plugin in this build is the D3D12 one |
| Upscaling | FSR through `amd_fidelityfx_dx12.dll` (SDK configured with `REXGLUE_ENABLE_FIDELITYFX=ON`) |
| Audio | SDL's WASAPI backend; the Linux `SDL_AUDIO_DRIVER=pulseaudio` override is not set (see `gameprofile.h`) |
| Import decoder | `xctd.exe` beside the launcher |

## CI

`.github/workflows/windows.yml` builds on `windows-latest` and uploads
`ReJeweled-windows-x64` (the whole folder, as one ZIP):

1. **SDK** - `rexglue-sdk` at `f5337cd`, cloned with `core.autocrlf=false`
   (the patch is LF-only), submodules shallow, then
   `patches/rexglue-sdk-f5337cd-to-test1.patch` applied with the two D3D12
   presenter files excluded. The patch's guest-output-shared-texture feature
   is half-ported - its base-class half is not in this SDK tree - which
   never shows on Linux (D3D12 sources do not compile there) but breaks the
   Windows build. Windows therefore uses the pristine upstream presenter.
2. **Recompiled sources** - `generated/` is committed (about 340 MB of C++
   derived from the game's `default.xex`; it holds no game data), so the
   probe step finds `generated/default/sources.cmake` and the build goes
   straight to configuring. The cache and codegen paths remain as fallbacks
   for checkouts without the tree: the Actions cache first, else the codegen
   CLI from a standalone SDK configure (the game project cannot configure
   before `generated/rexglue.cmake` exists) plus `bash scripts/refix.sh`.
   Refresh the committed tree with `bash scripts/refix.sh` after a fresh
   codegen.
3. **Game** - `cmake --preset win-amd64-release` (clang/clang++ + Ninja, not
   MSVC: the SDK passes GCC-style flags), `-DREXGLUE_ENABLE_FIDELITYFX=ON`,
   `-march=x86-64-v2`. `rexglue_setup_target(bejeweled GPU_PLUGINS xenos)`
   stages the plugin next to the exe, and the SDK's POST_BUILD step stages
   `rexruntime.dll` and the FidelityFX DLL beside it.
4. **xctd** - its own CMake project against the SDK's libmspack headers; no
   SDK build needed.
5. **Launcher** - Qt 6.8 MSVC kit, `-DTP_GAME=bejeweled`, GUI subsystem
   (`WIN32` + `Qt6::EntryPoint`, so no console window), then `windeployqt`
   with `--compiler-runtime` for the Qt and VC runtime DLLs.
6. **Package** - `windows/stage-package.ps1` assembles the folder, asserts
   the required set is present (launcher, Qt platform plugin, VC runtime,
   game exe, both runtime DLLs, `xctd.exe`), warns when no FidelityFX DLL
   was produced, and fails on anything missing.

Locally the same steps run with a Windows SDK build tree:
`pwsh -File windows/stage-package.ps1 -RepoRoot . -SdkRoot ../rexglue-sdk
-GameBuildDir out/build/win-amd64-release -XctdExe tools/xctd/build-win/xctd.exe
-OutDir dist-windows`.

## Verified

- `tools/xctd` builds for Windows (MinGW cross-build) and runs under Wine;
  its output on synthetic zoned/stored-segment and raw XCTD fixtures is
  byte-identical to the SDK-built binary shipped in the 1b AppImage, and the
  reject path for non-XCTD input matches too.
- The launcher still configures and builds on Linux with the platform
  changes (WIN32 branch is conditional; the Linux program set is unchanged).
- The game build graph change only adds a documented
  `rexglue_setup_target(... GPU_PLUGINS xenos)` argument.

## Not proven yet

Nothing in this list has been run on real Windows hardware from this
repository - the first green CI run is the start of the tester loop:

- The D3D12 path end to end. The exp_adjust (missing art) fix is present in
  the D3D12 translator (`dxbc_translator_fetch.cpp`) as well as the Vulkan
  one, but only the Vulkan path has been seen at runtime.
- FSR engaging on D3D12 (A/B at 720p: FSR vs bilinear).
- Audio and controller behaviour (WASAPI, XInput).
- Where the runtime puts saves by default, and whether the launcher's data
  root defaults match (check `logs/last-command.txt` and the Settings tab).
- Whether the runtime's guest-memory backing is really process-owned on
  Windows - `platform::vacuumGuestMemory()` is a no-op there on that
  assumption (see `tp-launcher/src/platform.cpp`).

## Reporting a problem

Ask for files, not impressions: `logs/last-command.txt`, `logs/game.log`,
and a screenshot for anything visual. The launcher records the exact command
it launched, so a settings change is one diff away.
