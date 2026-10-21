#!/usr/bin/env python3
"""Forward-declare house-fix call targets in their calling TUs.

fix-callsites.py converts baked unresolved-call fatals into direct
sub_X(ctx, base) calls. Each recomp TU only includes its OWN
bejeweled_funcs.N.h, so targets declared in other TUs' headers are
undeclared identifiers here. This pass inserts the missing
DECLARE_REX_FUNC(sub_X); lines (codegen's own declaration idiom)
right after the TU's include. Idempotent.
"""
import glob
import os
import re

GEN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', 'generated', 'default')
CALL_RE = re.compile(r'sub_([0-9A-F]+)\(ctx, base\);\s*// \(house fix')
DECL_RE = re.compile(r'DECLARE_REX_FUNC\(sub_([0-9A-F]+)\)')
INCLUDE_RE = re.compile(r'#include "(bejeweled_funcs\.\d+\.h)"')


def header_decls(name):
    path = os.path.join(GEN, name)
    if not os.path.exists(path):
        return set()
    return set(DECL_RE.findall(open(path, errors='replace').read()))


def main():
    total_files = total_decls = 0
    for path in sorted(glob.glob(os.path.join(GEN, 'bejeweled_recomp.*.cpp'))):
        src = open(path, errors='replace').read()
        targets = set(CALL_RE.findall(src))
        if not targets:
            continue
        inc = INCLUDE_RE.search(src)
        own = header_decls(inc.group(1)) if inc else set()
        already = set(DECL_RE.findall(src))
        missing = sorted(t for t in targets if t not in own and t not in already)
        if not missing:
            continue
        block = ['// (house fix: declarations for repaired call sites)']
        block += [f'DECLARE_REX_FUNC(sub_{t});' for t in missing]
        if inc:
            pos = src.index('\n', inc.start()) + 1
            src = src[:pos] + '\n'.join(block) + '\n' + src[pos:]
        else:
            src = '\n'.join(block) + '\n' + src
        open(path, 'w').write(src)
        total_files += 1
        total_decls += len(missing)
    print(f'declarations: +{total_decls} across {total_files} files')


if __name__ == '__main__':
    main()
