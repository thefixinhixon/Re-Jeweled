# XCTD pre-decoder (Bejeweled Blitz Live)

The 360 kernel decompresses XCTD files transparently; the ReXGlue runtime
does not. Bejeweled Blitz Live ships its four main content files XCTD-encoded
(blitz.pak, blitzgfx.pak, blitztitle.pak, blitz.xpr). The fix used here
(same strategy as rexautos transparent decode): decode the files OFFLINE to
plaintext, keep originals in gamedata/xctd_originals/, and let the games
existing XFileXctdCompressionInformation query report "not compressed".

## Build

It is a standalone CMake project: no SDK build is needed, only the
libmspack headers from the SDK source tree (thirdparty/libmspack is a
submodule - run `git submodule update --init --recursive` once).

    cmake -S tools/xctd -B tools/xctd/build -DREXSDK_DIR=/path/to/rexglue-sdk
    cmake --build tools/xctd/build

That is the whole build on Linux and on Windows (MSVC or clang). It used
to compile against the SDK's `lzx.cpp` object out of a built runtime tree,
which pinned the tool to one host platform; xctd.cpp now calls libmspack's
decoder itself, so the same source builds anywhere.

lzxd_xmem.c is a copy of the SDKs mspack lzxd.c with one change: a global
`int lzxd_xmem_no_uncompressed_realign` that, when set, skips the CAB
realignment pad byte after odd-sized uncompressed blocks. XMemCompress
(the encoder Microsoft used for these files) omits that pad byte; mspack
stock assumes it. xctd.cpp tries stock first, retries the segment with the
flag set on failure.

## Usage

    ./xctd <in.xctd> <out.plain>     # zoned and contiguous layouts auto-detected

## Format notes (beyond the public spec)

- Stored segments: a zone whose first u16 is 0x0000 followed by exactly the
  segments uncompressed size in raw bytes is STORED, not LZX (incompressible
  payload, e.g. JPEGs, where size == zone_size - 2). blitz.pak segments
  93/94/95 are stored.
- Sizes verified byte-exact on all four game files (2026-10-04).

## Packaged builds

The AppImage ships `xctd` in usr/bin and the Windows package ships
`xctd.exe` beside the launcher; the launcher's import step runs it on every
file with the XCTD magic (src/xctdprep.cpp). The CMake build links
libstdc++/libgcc statically on Linux and uses the static CRT on MSVC, so
the tool has no C++ runtime dependency of its own.

Verified: decoding the stock files reproduces the run/gamedata plaintext
byte-exact for all four (blitz.pak, blitzgfx.pak, blitztitle.pak, blitz.xpr),
by the shipped Linux build. After the libmspack-direct rewrite (2026-10-31)
the decoder paths that can be exercised without an encoded sample - zoned
stored segments, the raw layout, and the reject paths - were diffed against
the shipped binary and are byte-identical, including the MinGW-built Windows
exe run under Wine.
