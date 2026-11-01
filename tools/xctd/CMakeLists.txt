cmake_minimum_required(VERSION 3.16)
project(xctd LANGUAGES C CXX)

# xctd - offline decoder for the game's XCTD (XCompress LZX) content files.
#
# Standalone on purpose: it needs libmspack's decoder core (vendored here as
# lzxd_xmem.c, the SDK's lzxd.c plus the XMemCompress pad-byte tweak) and the
# libmspack headers from the ReXGlue SDK source tree - not a built SDK. That
# keeps the same build on Linux and Windows; the launcher runs the result at
# import time (tp-launcher/src/xctdprep.cpp).
#
#   cmake -S tools/xctd -B tools/xctd/build -DREXSDK_DIR=/path/to/rexglue-sdk
#   cmake --build tools/xctd/build

set(REXSDK_DIR "" CACHE PATH "Path to the rexglue-sdk source tree (for thirdparty/libmspack)")

if(NOT REXSDK_DIR)
    message(FATAL_ERROR
        "Set REXSDK_DIR to the rexglue-sdk source tree, e.g.\n"
        "  cmake -S tools/xctd -B tools/xctd/build -DREXSDK_DIR=/path/to/rexglue-sdk\n"
        "The libmspack headers this tool compiles against live in\n"
        "  <sdk>/thirdparty/libmspack/cabextract/mspack")
endif()

set(MSPACK_DIR "${REXSDK_DIR}/thirdparty/libmspack/cabextract/mspack")
if(NOT EXISTS "${MSPACK_DIR}/lzx.h")
    message(FATAL_ERROR
        "libmspack headers not found in ${MSPACK_DIR}.\n"
        "Initialise the SDK's submodules (git submodule update --init --recursive);\n"
        "thirdparty/libmspack is one of them.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(xctd
    xctd.cpp
    lzxd_xmem.c
)
target_include_directories(xctd PRIVATE "${MSPACK_DIR}")

# The vendored decoder core is third-party C; don't drown the build log in it.
if(MSVC)
    target_compile_options(xctd PRIVATE /w)
    # Static C runtime: the tool ships beside the launcher and must run with no
    # Visual C++ redistributable installed or deployed next to it.
    set_property(TARGET xctd PROPERTY MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(xctd PRIVATE -w)
endif()

# Linux packages ship this next to the launcher, where a libstdc++ dependency
# would be the one thing that can still fail to resolve; link it statically
# (a fully static link is not possible - the decoder is fine either way).
if(NOT WIN32 AND NOT APPLE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(xctd PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endif()
