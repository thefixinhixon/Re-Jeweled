cmake_minimum_required(VERSION 3.16)
project(TPHouseLauncher LANGUAGES CXX)

# House launcher for ReXGlue recomp games, Bejeweled Blitz Live build.
# The shared launcher source compiles one profile per game; this repo
# ships only the Bejeweled profile's art and theme:
#   bejeweled -> ReJeweledLauncher "Re-Jeweled Launcher"
# Qt Widgets, C++20. Stick to Qt 6.4-era APIs.

set(TP_GAME "bejeweled" CACHE STRING "Game profile to build (this repo: bejeweled)")
if(TP_GAME STREQUAL "bejeweled")
    set(TP_TARGET ReJeweledLauncher)
    set(TP_QRC resources-bejeweled.qrc)
    set(TP_GAME_DEFINE TP_GAME_BEJEWELED)
else()
    message(FATAL_ERROR "This repo ships only the bejeweled launcher profile (got: ${TP_GAME})")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(TP_SOURCES
    ${TP_QRC}
    src/main.cpp
    src/mainwindow.cpp
    src/gamesettings.cpp
    src/gamedetect.cpp
    src/launcher.cpp
    src/platform.cpp
    src/stfs/stfspackage.cpp
    src/xctdprep.cpp
)

# Windows: GUI subsystem, so launching the launcher does not open a console
# window behind it. src/winentry.cpp supplies the WinMain that calls main():
# Qt 6.8 exports its entry-point library only as the internal
# Qt6::EntryPointPrivate, so we do not depend on it.
if(WIN32)
    add_executable(${TP_TARGET} WIN32 ${TP_SOURCES} src/winentry.cpp)
else()
    add_executable(${TP_TARGET} ${TP_SOURCES})
endif()
target_compile_definitions(${TP_TARGET} PRIVATE ${TP_GAME_DEFINE})
# Games on the current house runtime (Oct 2026 SDK): its audio has gain and
# force-stereo but no highpass / front-only / XMA-worker cvars.
if(TP_GAME STREQUAL "bejeweled")
    target_compile_definitions(${TP_TARGET} PRIVATE TP_HOUSE_AUDIO)
endif()

target_include_directories(${TP_TARGET} PRIVATE src)
target_link_libraries(${TP_TARGET} PRIVATE Qt6::Widgets)

# Windows needs no extra runtime files: windeployqt collects the Qt DLLs and
# the platform plugin next to the exe (see .github/workflows/windows.yml and
# WINDOWS.md). The game program set itself is resolved by platform.cpp.

# src/stfs/ (XBLA/STFS package import) is built into the launcher -
# see the sources above and the stfs_test harness below.
#
# Theming is built in: theme/*.qss + per-game art are embedded via
# the per-game resources*.qrc (AUTORCC); the stylesheet is applied
# in main.cpp from the active profile.

# Headless STFS reader test harness: lists + extracts a package.
option(TPMAW_BUILD_STFS_TEST "Build the stfs_test CLI harness" OFF)
if(TPMAW_BUILD_STFS_TEST)
    add_executable(stfs_test
        src/stfs/stfs_test_main.cpp
        src/stfs/stfspackage.cpp
    )
    target_include_directories(stfs_test PRIVATE src)
    target_link_libraries(stfs_test PRIVATE Qt6::Core)
endif()
