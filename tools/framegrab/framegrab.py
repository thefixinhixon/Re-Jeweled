#!/usr/bin/env python3
"""Grab a guest texture / render target out of a rexglue (xenia-style) guest
memory arena (/dev/shm/xenia_memory_*) and write it as a PNG.

Arena layout (proven 2026-10-04): RAM physical address P lives at file
offset 0x100000000 + P. The 0x80000000+ system/XEX regions live at
offset == VA.

Usage:
  framegrab.py --phys 0x1F403000 --w 1280 --h 720 --tiled --out frame.png
  framegrab.py --trace /path/to/game.log --out frame.png   (auto: last
      'Loaded tiled 1280x720' texture base from the trace)
Tiled detiling ports the SDK's texture_util::GetTiledOffset2D exactly.
Guest 32bpp texels are big-endian ARGB words; output is RGBA PNG.
"""
import argparse, glob, os, re, struct, sys, zlib

def tiled_offset(x, y, pitch, bpb_log2):
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) + \
           (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)

def write_png(path, w, h, rgba):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arena", default=None)
    ap.add_argument("--phys", default=None, help="guest physical address, hex")
    ap.add_argument("--trace", default=None, help="auto-pick texture base from a trace log")
    ap.add_argument("--w", type=int, default=1280)
    ap.add_argument("--h", type=int, default=720)
    ap.add_argument("--bpp", type=int, default=4, help="bytes per texel")
    ap.add_argument("--tiled", action="store_true")
    ap.add_argument("--out", default="frame.png")
    a = ap.parse_args()

    arena = a.arena
    if arena is None:
        files = sorted(glob.glob("/dev/shm/xenia_memory_*"), key=os.path.getmtime)
        if not files:
            sys.exit("no arena file found")
        arena = files[-1]
    phys = None
    if a.phys:
        phys = int(a.phys, 16)
    elif a.trace:
        txt = open(a.trace, errors="replace").read()
        pat = r"Loaded tiled %dx%d.*?base at 0x([0-9A-F]+)" % (a.w, a.h)
        m = re.findall(pat, txt)
        if not m:
            sys.exit("no matching Loaded line in trace")
        phys = int(m[-1], 16)
    else:
        sys.exit("need --phys or --trace")

    bpb_log2 = {1: 0, 2: 1, 4: 2}[a.bpp]
    fh = open(arena, "rb")
    base = 0x100000000 + phys
    out = bytearray(a.w * a.h * 4)
    for y in range(a.h):
        for x in range(a.w):
            if a.tiled:
                off = base + tiled_offset(x, y, a.w, bpb_log2)
            else:
                off = base + (y * a.w + x) * a.bpp
            fh.seek(off)
            px = fh.read(a.bpp)
            o = (y * a.w + x) * 4
            if a.bpp == 4:  # guest BE ARGB word -> RGBA
                out[o] = px[1]; out[o + 1] = px[2]; out[o + 2] = px[3]; out[o + 3] = px[0]
            elif a.bpp == 1:
                out[o] = out[o + 1] = out[o + 2] = px[0]; out[o + 3] = 255
    fh.close()
    write_png(a.out, a.w, a.h, bytes(out))
    print("wrote", a.out, "from arena", arena, "phys", hex(phys))

if __name__ == "__main__":
    main()
