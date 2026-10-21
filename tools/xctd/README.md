# XCTD pre-decoder (Bejeweled Blitz Live)

The 360 kernel decompresses XCTD files transparently; the ReXGlue runtime
does not. Bejeweled Blitz Live ships its four main content files XCTD-encoded
(blitz.pak, blitzgfx.pak, blitztitle.pak, blitz.xpr). The fix used here
(same strategy as rexautos transparent decode): decode the files OFFLINE to
plaintext, keep originals in gamedata/xctd_originals/, and let the games
existing XFileXctdCompressionInformation query report "not compressed".

## Build

    SDK=/path/to/rexglue-sdk   # patched and built, see ../../patches
    clang-20 -O1 -c lzxd_xmem.c -I$SDK/thirdparty/libmspack/cabextract/mspack \
        -DREX_PLATFORM_LINUX=1 -fPIC -w -o lzxd_xmem.o
    g++ -O1 xctd.cpp $SDK/out/build/src/system/CMakeFiles/rexruntime.dir/lzx.cpp.o \
        lzxd_xmem.o -I$SDK/include -o xctd

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

## Packaged build (2026-10-12)

The AppImage ships `xctd` in usr/bin; the launcher's import step runs it on
every file with the XCTD magic (src/xctdprep.cpp). Build used for the
package (libstdc++/libgcc static so the binary has no C++ runtime deps; a
fully static link fails because the SDK's lzx.cpp.o is PIC):

    B=project/out/build/linux-amd64-release
    clang-20 -O2 -c lzxd_xmem.c -I$SDK/thirdparty/libmspack/cabextract/mspack \
        -DREX_PLATFORM_LINUX=1 -fPIC -w -o build/lzxd_xmem.o
    clang++-20 -O2 -std=c++20 -static-libstdc++ -static-libgcc xctd.cpp \
        $B/rexglue-sdk/src/system/CMakeFiles/rexruntime.dir/lzx.cpp.o \
        build/lzxd_xmem.o -I$SDK/include -o build/xctd

Verified: decoding the stock files reproduces the run/gamedata plaintext
byte-exact for all four (blitz.pak, blitzgfx.pak, blitztitle.pak, blitz.xpr).
