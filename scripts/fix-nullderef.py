#!/usr/bin/env python3
"""Null-guards for guest accessors whose own contract already defines a
null/empty result. A worker-thread chain in this PopCap binary calls them
with a null pointer argument; on the fault the rex handler retries forever
and the thread spins. Each guard returns exactly what the function itself
returns for its empty case:
  sub_822701F8: if ([r4] == 0) return 0        -> guard r4 == 0
  sub_82270218: if ([r7] == 0) return 0        -> guard r7 == 0
Applied by scripts/refix.sh so codegen regens keep them.
"""
import glob, os, sys

GEN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "generated", "default")
GUARDS = [
    ("sub_822701F8", "r4",
     "\t// lwz r11,0(r4)"),
    ("sub_82270218", "r7",
     "\t// lwz r10,0(r7)"),
]
MARKER = "house fix: null arg"

done = 0
for path in glob.glob(os.path.join(GEN, "bejeweled_recomp.*.cpp")):
    src = open(path, errors="replace").read()
    orig = src
    for name, reg, first_insn in GUARDS:
        anchor = f"DEFINE_REX_FUNC({name})"
        i = src.find(anchor)
        if i < 0:
            continue
        j = src.find(first_insn, i)
        assert j > 0, (name, path)
        if "house fix" in src[i:j]:
            continue
        guard = (f"\t// (house fix: null arg: {name} called with {reg}==0; its own empty\n"
                 f"\t// case returns 0, so guard instead of faulting on guest null)\n"
                 f"\tif (ctx.{reg}.u32 == 0) {{ ctx.r3.u64 = 0; return; }}\n")
        src = src[:j] + guard + src[j:]
        done += 1
        print("guard applied:", name, "in", os.path.basename(path))
    if src != orig:
        open(path, "w").write(src)
print("guards applied this run:", done)
