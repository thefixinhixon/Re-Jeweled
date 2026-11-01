// gameprofile.h - what the launcher knows about one recomp game.
//
// Profiles are compiled in (one per game). A profile is deliberately
// just data: detection, settings and launching are all written against
// it, so adding a game is a new entry here, not new code.
//
// Which profile a build serves is chosen at configure time by the
// TP_GAME CMake option (see CMakeLists.txt); activeProfile() resolves
// it via the TP_GAME_* compile definition the build sets.
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

// Audio environment for the game process on Linux: SDL's native PipeWire
// backend can record silence on 6-channel streams, while PipeWire's
// PulseAudio layer plays correctly (2026-10-11). Windows needs no override -
// SDL picks WASAPI there, and naming a driver that does not exist breaks
// audio init, so the list is empty there.
inline QStringList linuxPulseAudioEnv()
{
#if defined(Q_OS_WIN)
    return {};
#else
    return {QStringLiteral("SDL_AUDIO_DRIVER=pulseaudio")};
#endif
}

struct GameProfile {
    QString name;         // Display name, e.g. "The Maw"
    QString exeName;      // Game binary file name, e.g. "themaw"
    QString titleId;      // Xbox 360 title id, e.g. "584108C2"
    QStringList forcedArgs; // Always passed, in order (see launcher.cpp)
    QString accent;       // Theme accent; substituted into the .qss at startup
    QString launcherName; // Product name: window title + QSettings app name
    QString themeFile;    // Stylesheet resource, e.g. ":/theme/maw.qss"
    QStringList environment; // Extra env vars for the game process,
                             // "KEY=VALUE" (see launcher.cpp); empty
                             // for most profiles.
};

// The Maw (Twisted Pixel, XBLA). Title id 584108C2.
//
// forcedArgs holds only what the user cannot change. The staged launch
// script also proved --vulkan_async_skip_incomplete_frames=false, but
// that flag is now a user setting (Advanced section, default off - the
// same value) so it is emitted from settings in launcher.cpp instead.
// --present_effect is NOT here either: it is derived from the user"s
// upscaling setting at launch time (launcher.cpp).
inline const GameProfile &mawProfile()
{
    static const GameProfile profile{
        QStringLiteral("The Maw"),
        QStringLiteral("themaw"),
        QStringLiteral("584108C2"),
        {
            QStringLiteral("--gpu_plugin=xenos"),
            QStringLiteral("--license_mask=1"),
        },
        QStringLiteral("#7a4fd0"), // theme accent (drives the .qss)
        QStringLiteral("TP Maw Launcher"),
        QStringLiteral(":/theme/maw.qss"),
    };
    return profile;
}

// 'Splosion Man (Twisted Pixel, XBLA). Title id 5841098F.
// Same runtime family and the same proven forced args as The Maw; the
// settings-derived flags (async-skip, sparse memory, ...) likewise come
// from settings in launcher.cpp.
inline const GameProfile &splosionProfile()
{
    static const GameProfile profile{
        QStringLiteral("'Splosion Man"),
        QStringLiteral("splosionman"),
        QStringLiteral("5841098F"),
        {
            QStringLiteral("--gpu_plugin=xenos"),
            QStringLiteral("--license_mask=1"),
        },
        QStringLiteral("#f06414"), // hot orange (drives the .qss)
        QStringLiteral("TP Splosionman Launcher"),
        QStringLiteral(":/theme/splosion.qss"),
    };
    return profile;
}

// Real Steel (Yuke's, XBLA). Title id 584111E0.
//
// --gpu_allow_invalid_fetch_constants=true is a title requirement, not
// a preference: the game issues texture fetches with 'invalid' fetch
// constants thousands of times per session, and without the runtime's
// tolerance flag those draws misrender (flickering/green textures -
// proven 2026-10-04). Vsync is the opposite story: it stays a user
// setting, but this build's DEFAULT is off (gamesettings.h) because
// the runtime's vsync emulation runs this game's fights at half
// speed; the 60 fps frame limiter still paces it.
inline const GameProfile &realsteelProfile()
{
    static const GameProfile profile{
        QStringLiteral("Real Steel"),
        QStringLiteral("realsteel"),
        QStringLiteral("584111E0"),
        {
            QStringLiteral("--gpu_plugin=xenos"),
            QStringLiteral("--license_mask=1"),
            QStringLiteral("--gpu_allow_invalid_fetch_constants=true"),
        },
        QStringLiteral("#1fb6c9"), // cage-light cyan (drives the .qss)
        QStringLiteral("Factual Metal Launcher"),
        QStringLiteral(":/theme/realsteel.qss"),
    };
    return profile;
}

// Lollipop Chainsaw (Grasshopper Manufacture, full disc title).
// Title id 565707D0. Disc game: the data root is an extracted disc
// folder (default.xex + AshGame/ + Engine/ at the top level), which
// gamedetect's top-level-xex rule already accepts.
//
// --target_fps is NOT a forced arg here: it is this build's 60 FPS
// setting (gamesettings.h), emitted from launcher.cpp. The recomp's
// own fps hook translates it into the game's frame divider
// (60 -> every vsync, 30 -> every other vsync, the original cap).
inline const GameProfile &lollipopProfile()
{
    static const GameProfile profile{
        QStringLiteral("Lollipop Chainsaw"),
        QStringLiteral("re_cherry"),
        QStringLiteral("565707D0"),
        {
            QStringLiteral("--gpu_plugin=xenos"),
            QStringLiteral("--license_mask=1"),
            // Title requirement (same class as Real Steel): the game
            // issues texture fetches with 'invalid' fetch constants
            // constantly, and without the runtime's tolerance flag
            // those draws flash white (proven in the first verdict
            QStringLiteral("--gpu_allow_invalid_fetch_constants=true"),
            // Perf (measured 2026-10-11): the OOB vfetch trace scans
            // every draw when on; it is off by default in the SDK now
            // too, passed here so the profile is explicit. Submitting
            // on the primary buffer end stalled the GPU thread on
            // fences during streaming bursts.
            QStringLiteral("--vfetch_oob_trace=false"),
            QStringLiteral("--vulkan_submit_on_primary_buffer_end=false"),
            // Perf (2026-10-11): synchronous memexport readback after each
            // memexport draw stalled the GPU thread during streaming
            // bursts (moving 47.5 -> 57.4 fps, >50 ms frames 99 -> 0).
            QStringLiteral("--readback_memexport=false"),
            // Teal tint fix (2026-10-12): estimate unclipped draws'
            // extent on the CPU (upstream Xenia default; this SDK has it
            // off). Without it UE3's shadow-map draws claim the whole
            // EDRAM and the scene's red channel is lost to stencil data.
            QStringLiteral("--execute_unclipped_draw_vs_on_cpu=true"),
            // Real occlusion queries flicker in this title (tiled
            // begin/end records not reproduced yet); off = every query
            // visible, as Test2 effectively ran.
            QStringLiteral("--occlusion_query_enable=false"),
        },
        QStringLiteral("#ff4fa3"), // hot pink (drives the .qss)
        QStringLiteral("Re-Cherry Launcher"),
        QStringLiteral(":/theme/lollipop.qss"),
        linuxPulseAudioEnv(),
    };
    return profile;
}

// Bejeweled Blitz Live (PopCap / Torpex Games, XBLA). Title id 58410A57.
// XBLA title: data comes from the LIVE package or an extracted copy,
// and the import step pre-decodes its XCTD-compressed content files
// (xctdprep.h) - the runtime has no transparent XCTD support.
//
// Only the proven essentials are forced. No fetch-constant tolerance
// or Lollipop perf flags: none were needed in the verification runs
// (2026-10-12, title -> menus -> full rounds, locked 60 fps).
inline const GameProfile &bejeweledProfile()
{
    static const GameProfile profile{
        QStringLiteral("Bejeweled Blitz Live"),
        QStringLiteral("bejeweled"),
        QStringLiteral("58410A57"),
        {
            QStringLiteral("--gpu_plugin=xenos"),
            QStringLiteral("--license_mask=1"),
        },
        QStringLiteral("#2f8cff"), // gem blue (drives the .qss)
        QStringLiteral("Re-Jeweled Launcher"),
        QStringLiteral(":/theme/bejeweled.qss"),
        // Same runtime audio path as Lollipop Chainsaw: SDL's native
        // PipeWire backend can record silence on 6-channel streams;
        // PipeWire's PulseAudio layer plays correctly.
        linuxPulseAudioEnv(),
    };
    return profile;
}

// The profile this build serves, selected by the TP_GAME_* compile
// definition (CMakeLists.txt). Maw is the default/fallback.
#if defined(TP_GAME_SPLOSIONMAN)
inline const GameProfile &activeProfile()
{
    return splosionProfile();
}
#elif defined(TP_GAME_REALSTEEL)
inline const GameProfile &activeProfile()
{
    return realsteelProfile();
}
#elif defined(TP_GAME_LOLLIPOP)
inline const GameProfile &activeProfile()
{
    return lollipopProfile();
}
#elif defined(TP_GAME_BEJEWELED)
inline const GameProfile &activeProfile()
{
    return bejeweledProfile();
}
#else
inline const GameProfile &activeProfile()
{
    return mawProfile();
}
#endif

inline QVector<GameProfile> allProfiles()
{
    return {mawProfile(), splosionProfile(), realsteelProfile(), lollipopProfile(),
            bejeweledProfile()};
}
