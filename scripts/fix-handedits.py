#!/usr/bin/env python3
"""Re-apply the behavioral hand edits to fresh codegen output.

The Test1 build carried two fixes that had been typed straight into
generated/ (Oct 2026). Codegen output is never committed, so they live
here and are applied by function name and anchor lines. That makes them
independent of how codegen partitions functions into files.

  XFALLBACK (sub_8226FF18, archive file find):
      The VFS 'current root' is overwritten as archives mount, so a find
      can run against the wrong archive's map. Remember every root seen.
      On a miss, or when the lower_bound node is a different file (the
      node key is validated with the game's own comparator sub_821977B0),
      retry the other roots and accept only an exact-name match.

  SKIP-FIX (sub_82274C50, movie preload):
      A failed preload took the game's fatal exit (sub_822884E8), which
      parks the thread and with it the whole GFx layer. Log it, release
      the key string and return instead.

Idempotent: a function that already carries its marker is skipped.
Exit 1 if an anchor is missing (codegen changed shape; re-derive).
"""
import glob
import os
import re
import sys

GEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "generated", "default")

XFALLBACK = """\
	// [XFALLBACK] The VFS 'current root' is overwritten as archives mount, so a
	// find can run against the wrong archive's map. Remember every root seen;
	// on a miss - or when the lower_bound node is a different file (validate the
	// node key with the game's own comparator) - retry the other roots and
	// accept only an exact-name match.
	{
		static uint32_t xfRoots[4] = {0, 0, 0, 0};
		static int xfCount = 0;
		bool known = false;
		for (int i = 0; i < xfCount; i++) if (xfRoots[i] == ctx.r29.u32) known = true;
		if (!known && xfCount < 4) xfRoots[xfCount++] = ctx.r29.u32;
		auto xfKeyEq = [&](uint32_t node) -> bool {
			uint32_t klen = REX_LOAD_U32(node + 32);
			uint32_t kdata = (klen < 16) ? (node + 12) : REX_LOAD_U32(node + 12);
			ctx.r3.u64 = ctx.r31.u32 + 128;
			ctx.r4.u64 = REX_LOAD_U32(ctx.r31.u32 + 128 + 16);
			ctx.r5.u64 = kdata;
			ctx.r6.u64 = REX_LOAD_U32(node + 28);
			ctx.lr = 0x8226FFAC;
			sub_821977B0(ctx, base);
			return ctx.r3.s32 == 0;
		};
		uint32_t node = REX_LOAD_U32(ctx.r31.u32 + 112);
		uint32_t end = REX_LOAD_U32(ctx.r29.u32 + 20);
		bool ok = (node != end) && xfKeyEq(node);
		if (!ok) {
			for (int i = 0; i < xfCount && !ok; i++) {
				uint32_t alt = xfRoots[i];
				if (alt == ctx.r29.u32) continue;
				ctx.r3.u64 = ctx.r31.u32 + 112;
				ctx.r4.u64 = alt + 16;
				ctx.r5.u64 = ctx.r31.u32 + 128;
				ctx.lr = 0x8226FFA4;
				sub_82270D18(ctx, base);
				uint32_t n2 = REX_LOAD_U32(ctx.r31.u32 + 112);
				uint32_t e2 = REX_LOAD_U32(alt + 20);
				if (n2 != e2 && xfKeyEq(n2)) ok = true;
			}
			if (!ok) REX_STORE_U32(ctx.r31.u32 + 112, end);
		}
	}
"""

SKIPFIX = """\
	// [SKIP-FIX] movie preload failed: log it, reset the key string, and return
	// instead of taking the game's fatal exit (sub_822884E8) which paralyzes
	// this thread and with it the whole GFx layer.
	{
		char kbuf[128]; int ki = 0;
		uint32_t slen = REX_LOAD_U32(ctx.r31.u32 + 116);
		uint32_t kp = (slen < 16) ? (ctx.r31.u32 + 96) : REX_LOAD_U32(ctx.r31.u32 + 96);
		if (kp) { while (ki < 127 && ki < (int)slen) { uint8_t c = REX_LOAD_U8(kp + ki); if (!c) break; kbuf[ki++] = (char)c; } }
		kbuf[ki] = 0;
		fprintf(stderr, "[SKIP-FIX] preload failed, skipping movie: %s\\n", kbuf);
	}
	ctx.r5.s64 = 0;
	ctx.r4.s64 = 1;
	ctx.r3.s64 = ctx.r31.s64 + 96;
	ctx.lr = 0x82274DA0;
	sub_821810F8(ctx, base);
	ctx.r3.u64 = ctx.r28.u64;
	ctx.r1.s64 = ctx.r31.s64 + 208;
	__restgprlr_28(ctx, base);
	return;
"""

# (function, marker, anchor, replacement, callees that need a declaration)
EDITS = [
    ("sub_8226FF18", "[XFALLBACK]",
     "\tctx.lr = 0x8226FFA4;\n\tsub_82270D18(ctx, base);\n",
     "\tctx.lr = 0x8226FFA4;\n\tsub_82270D18(ctx, base);\n" + XFALLBACK,
     ["sub_82270D18", "sub_821977B0"]),
    ("sub_82274C50", "[SKIP-FIX]",
     "\t// bl 0x822884e8\n\tctx.lr = 0x82274D30;\n\tsub_822884E8(ctx, base);\n",
     SKIPFIX,
     ["sub_821810F8", "__restgprlr_28"]),
]


def func_span(src, name):
    m = re.search(r"^(?:REX_WEAK_FUNC\s+)?DEFINE_REX_FUNC\(%s\)\s*\{" % name, src, re.M)
    if not m:
        return None
    nxt = re.search(r"^(?:REX_WEAK_FUNC\s+)?DEFINE_REX_FUNC\(", src[m.end():], re.M)
    return m.start(), (m.end() + nxt.start()) if nxt else len(src)


def main():
    files = glob.glob(os.path.join(GEN, "bejeweled_recomp.*.cpp"))
    if not files:
        sys.exit("fix-handedits: no generated sources in %s" % GEN)
    failed = 0
    for name, marker, anchor, repl, callees in EDITS:
        for path in files:
            src = open(path).read()
            span = func_span(src, name)
            if span is None:
                continue
            a, b = span
            body = src[a:b]
            if marker in body:
                print("%s: %s already applied" % (name, marker))
                break
            if body.count(anchor) != 1:
                print("%s: anchor for %s not found exactly once - codegen changed?" % (name, marker))
                failed = 1
                break
            body = body.replace(anchor, repl)
            decls = "".join("DECLARE_REX_FUNC(%s);\n" % c for c in callees)
            src = src[:a] + "// (house fix: declarations for %s)\n" % marker + decls + body + src[b:]
            open(path, "w").write(src)
            print("%s: %s applied in %s" % (name, marker, os.path.basename(path)))
            break
        else:
            print("%s: function not found" % name)
            failed = 1
    sys.exit(failed)


if __name__ == "__main__":
    main()
