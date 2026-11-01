# stage-package.ps1 - assemble the Windows package folder for Re-Jeweled.
#
# Puts the launcher (windeployqt'd), the matched game program set, xctd.exe and
# the attribution files into one folder, then asserts the set is complete. The
# layout is what the launcher expects: the program set sits beside the
# launcher, so resolveExecutableSet finds it in its own folder.
#
#   pwsh -File windows/stage-package.ps1 `
#       -RepoRoot . -SdkRoot ../rexglue-sdk `
#       -GameBuildDir out/build/win-amd64-release `
#       -XctdExe tools/xctd/build-win/xctd.exe -OutDir dist-windows
#
# Never mix program sets: the exe and the runtime DLLs in the package MUST come
# from one SDK build (a mixed set froze the Linux build at boot on 2026-10-12).

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $RepoRoot,
    [string] $SdkRoot = "",
    [string] $GameBuildDir = "out/build/win-amd64-release",
    [string] $XctdExe = "tools/xctd/build-win/xctd.exe",
    [string] $LauncherExe = "tp-launcher/out/build/win/ReJeweledLauncher.exe",
    [Parameter(Mandatory = $true)] [string] $OutDir
)

$ErrorActionPreference = 'Stop'

function Resolve-Under([string] $base, [string] $path) {
    if ([System.IO.Path]::IsPathRooted($path)) { return $path }
    return [System.IO.Path]::GetFullPath((Join-Path $base $path))
}

$repo        = (Resolve-Path $RepoRoot).Path
$out         = Resolve-Under (Get-Location).Path $OutDir
$launcher    = Resolve-Under $repo $LauncherExe
$gameBuild   = Resolve-Under $repo $GameBuildDir
$xctd        = Resolve-Under $repo $XctdExe
$gameExe     = Join-Path $gameBuild 'bejeweled.exe'

function Need-File([string] $path, [string] $what) {
    if (-not (Test-Path $path -PathType Leaf)) {
        throw "$what not found: $path"
    }
    return $path
}

$null = Need-File $launcher  'Launcher exe'
$null = Need-File $gameExe   'Game exe'
$null = Need-File $xctd      'xctd decoder'

# ---------------------------------------------------------------- clean start
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $out | Out-Null

# ------------------------------------------------------------------- launcher
Copy-Item $launcher (Join-Path $out 'ReJeweledLauncher.exe')

$windeployqt = (Get-Command windeployqt.exe -ErrorAction SilentlyContinue)
if (-not $windeployqt) {
    $candidate = Join-Path $env:QT_ROOT_DIR 'bin/windeployqt.exe'
    if (Test-Path $candidate) { $windeployqt = $candidate }
}
if (-not $windeployqt) {
    throw ("windeployqt.exe not found. Install the Qt 6 MSVC kit and put its bin " +
           "folder on PATH (GitHub Actions: jurplel/install-qt-action).")
}
$windeployqtExe = if ($windeployqt -is [string]) { $windeployqt } else { $windeployqt.Source }
Write-Host "windeployqt: $windeployqtExe"

# --compiler-runtime: ship vcruntime140/msvcp140 next to the exe, so the folder
# runs on a machine without the Visual C++ redistributable. The OpenGL software
# fallback, D3D compiler and translations are not used by this Widgets app.
& $windeployqtExe --release --compiler-runtime --no-translations --no-opengl-sw `
    --no-system-d3d-compiler --dir $out (Join-Path $out 'ReJeweledLauncher.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit $LASTEXITCODE)" }

# --------------------------------------------------------- game program set
# exe + every DLL it was built against, taken whole from one build folder.
Copy-Item $gameExe $out
Get-ChildItem $gameBuild -Filter *.dll | ForEach-Object {
    Copy-Item $_.FullName $out
}

# Fallback: a build that did not stage the GPU plugin next to the exe leaves it
# in the SDK output folder (rexglue-sdk/out/win-amd64). Take it from there, from
# the same SDK build, rather than silently shipping an incomplete set.
$plugin = Join-Path $out 'rexgpu-xenos.dll'
if (-not (Test-Path $plugin)) {
    if (-not $SdkRoot) { throw "rexgpu-xenos.dll is missing and -SdkRoot was not given" }
    $sdkPlugin = Get-ChildItem -Path (Resolve-Under $repo $SdkRoot) -Recurse `
        -Filter 'rexgpu-xenos.dll' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $sdkPlugin) { throw "rexgpu-xenos.dll not found in the game build or the SDK output" }
    Copy-Item $sdkPlugin.FullName $out
    Write-Host "rexgpu-xenos.dll copied from $($sdkPlugin.FullName)"
}

# ----------------------------------------------------------------------- xctd
Copy-Item $xctd (Join-Path $out 'xctd.exe')

# ---------------------------------------------------------------- attribution
# Same pack the AppImage ships. The xctd sources ride along because the decoder
# links a modified libmspack lzxd.c (LGPL-2.1).
$licenses = Join-Path $out 'licenses'
New-Item -ItemType Directory -Force -Path $licenses | Out-Null
Copy-Item (Join-Path $repo 'LICENSE') $licenses
Copy-Item (Join-Path $repo 'CREDITS.md') $licenses
Copy-Item (Join-Path $repo 'licenses/*') $licenses

$xctdSrc = Join-Path $out 'xctd-src'
New-Item -ItemType Directory -Force -Path $xctdSrc | Out-Null
foreach ($f in @('README.md', 'xctd.cpp', 'lzxd_xmem.c')) {
    Copy-Item (Join-Path $repo "tools/xctd/$f") $xctdSrc
}

Copy-Item (Join-Path $repo 'windows/README.txt') (Join-Path $out 'README.txt')

# ------------------------------------------------------------------- checks
$required = @(
    'ReJeweledLauncher.exe',
    'bejeweled.exe',
    'rexruntime.dll',
    'rexgpu-xenos.dll',
    'xctd.exe',
    'Qt6Core.dll',
    'platforms/qwindows.dll',
    'vcruntime140.dll',
    'msvcp140.dll',
    'README.txt',
    'licenses/CREDITS.md',
    'xctd-src/xctd.cpp'
)
$missing = $required | Where-Object { -not (Test-Path (Join-Path $out $_)) }
if ($missing) {
    throw ("package is incomplete - missing: " + ($missing -join ', ') +
           "`n(a missing Qt or VC runtime file means windeployqt did not deploy)")
}

# FSR silently degrades to bilinear without the FidelityFX DLL, so say out loud
# when the package has no present_effect=fsr backing.
$fx = Get-ChildItem $out -Filter 'amd_fidelityfx*.dll'
if (-not $fx) {
    Write-Warning ("No amd_fidelityfx*.dll in the package: the launcher's FSR " +
                   "setting will fall back to bilinear. Rebuild the SDK with " +
                   "-DREXGLUE_ENABLE_FIDELITYFX=ON.")
}

$total = (Get-ChildItem $out -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host ("Package ready: {0} ({1:N1} MB)" -f $out, ($total / 1MB))
