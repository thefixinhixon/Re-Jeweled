#!/usr/bin/env python3
"""Fix baked REX_FATAL unresolved BRANCH sites whose target label
exists in the same function.

Codegen's conditional-branch emitter failed to resolve targets that
its own unconditional emitter labelled fine: loc_<T>: is defined in
the same function, yet the site got
    if (cond) REX_FATAL("Unresolved branch from A to T");
Rewrite is exact:  if (cond) goto loc_<T>;   (bare form: goto loc_<T>;)
Sites whose target is NOT labelled in the same function are left
untouched (cross-function branches - handled case by case).
Idempotent.
"""
import glob
import os
import re

GEN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', 'generated', 'default')
FUNC_RE = re.compile(r'DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)')
LABEL_RE = re.compile(r'^\s*loc_([0-9A-F]+):')
IF_FATAL_RE = re.compile(
    r'^(\s*)if \((.*)\) REX_FATAL\("Unresolved branch from 0x[0-9A-F]+ '
    r'to 0x([0-9A-F]+)"\);')
BARE_FATAL_RE = re.compile(
    r'^(\s*)REX_FATAL\("Unresolved branch from 0x[0-9A-F]+ '
    r'to 0x([0-9A-F]+)"\);')
ERROR_COMMENT_RE = re.compile(r'// ERROR: .*branch to unknown address')


def main():
    fixed = left = 0
    for path in sorted(glob.glob(os.path.join(GEN, 'bejeweled_recomp.*.cpp'))):
        lines = open(path, errors='replace').read().split('\n')
        # pass 1: function ranges and their label sets
        func_labels = {}
        cur, start = None, 0
        for i, line in enumerate(lines):
            m = FUNC_RE.match(line)
            if m:
                if cur is not None:
                    func_labels[(start, i)] = (cur, labels)
                cur, start, labels = m.group(1), i, set()
                continue
            lm = LABEL_RE.match(line)
            if lm and cur is not None:
                labels.add(lm.group(1))
        if cur is not None:
            func_labels[(start, len(lines))] = (cur, labels)

        def labels_at(idx):
            for (s, e), (_, ls) in func_labels.items():
                if s <= idx < e:
                    return ls
            return set()

        changed = False
        for i, line in enumerate(lines):
            m = IF_FATAL_RE.match(line)
            if m:
                indent, cond, to = m.groups()
                if to in labels_at(i):
                    lines[i] = f'{indent}if ({cond}) goto loc_{to};'
                    changed = True
                    fixed += 1
                else:
                    left += 1
                continue
            m = BARE_FATAL_RE.match(line)
            if m:
                indent, to = m.groups()
                if to in labels_at(i):
                    lines[i] = f'{indent}goto loc_{to};'
                    changed = True
                    fixed += 1
                else:
                    left += 1
        if changed:
            lines = [l for l in lines if not ERROR_COMMENT_RE.search(l)]
            open(path, 'w').write('\n'.join(lines))
    print(f'branch sites fixed={fixed} left={left}')


if __name__ == '__main__':
    main()
