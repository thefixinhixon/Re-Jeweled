#!/usr/bin/env python3
"""Fix baked REX_FATAL unresolved call sites in generated code.

Codegen emits REX_FATAL("Unresolved call from A to B") at call/branch
sites whose target lacked a CallTarget edge in its graph - even when
the target function WAS emitted (body + registration exist). All such
targets are verified to have bodies before this pass runs.

Rewrites, per site (instruction kind from the '// bl|b 0x...' comment):
  bl to other fn : ctx.lr = <A+4>; sub_B(ctx, base);      (call, continues)
  bl to own entry: ctx.lr = <A+4>; sub_F(ctx, base);      (true recursion)
  b  to other fn : sub_B(ctx, base); return;              (tail call)
  b  to own entry: goto loc_B_entry; + entry label after the prologue
                   (exact loop semantics)

Any site not matching the expected shape is left untouched and
reported as an anomaly.
"""
import glob
import os
import re
import sys

FATAL_RE = re.compile(
    r'^(\s*)REX_FATAL\("Unresolved call from 0x([0-9A-F]+) to 0x([0-9A-F]+)"\);')
INSN_RE = re.compile(r'//\s*(bl|b)\s+0x[0-9A-Fa-f]+')
FUNC_RE = re.compile(r'DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)')
COMMENT_FATAL_RE = re.compile(r'// FATAL: unresolved function')


def bodies_present(gen_dir):
    bodies = set()
    for path in glob.glob(os.path.join(gen_dir, '*.cpp')):
        bodies |= set(re.findall(r'DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)',
                                 open(path, errors='replace').read()))
    return bodies


def fix_file(path, bodies):
    lines = open(path).read().split('\n')
    out = []
    cur_func = None
    stats = {'bl': 0, 'b': 0, 'self_b': 0, 'self_bl': 0, 'anomaly': []}
    entry_label_pending = set()  # funcs needing an entry label inserted
    i = 0
    while i < len(lines):
        line = lines[i]
        m = FUNC_RE.match(line)
        if m:
            cur_func = m.group(1)
            out.append(line)
            i += 1
            continue
        fm = FATAL_RE.match(line)
        if fm and cur_func:
            indent, frm, to = fm.group(1), fm.group(2), fm.group(3)
            # instruction kind: search up to 3 lines back for the insn comment
            kind = None
            for back in range(1, 4):
                if i - back >= 0:
                    im = INSN_RE.search(lines[i - back])
                    if im:
                        kind = im.group(1)
                        break
            if to not in bodies or kind is None:
                stats['anomaly'].append((path, i + 1, frm, to, kind))
                out.append(line)
                i += 1
                continue
            ret = f"{int(frm, 16) + 4:08X}"
            if kind == 'bl':
                out.append(f"{indent}ctx.lr = 0x{ret};")
                out.append(f"{indent}sub_{to}(ctx, base);  // (house fix: was baked unresolved-call fatal)")
                stats['self_bl' if to == cur_func else 'bl'] += 1
            else:  # b
                if to == cur_func:
                    out.append(f"{indent}goto loc_{to}_entry;  // (house fix: branch to own entry = loop)")
                    entry_label_pending.add(cur_func)
                    stats['self_b'] += 1
                else:
                    out.append(f"{indent}sub_{to}(ctx, base);  // (house fix: was baked unresolved-call fatal)")
                    stats['b'] += 1
                    # tail branch: the baked block's own return; (if present
                    # on the next line) stays as the terminator; if absent,
                    # add one.
                    nxt = lines[i + 1].strip() if i + 1 < len(lines) else ''
                    if nxt != 'return;':
                        out.append(f"{indent}return;")
            i += 1
            continue
        out.append(line)
        # insert entry label right after a prologue when needed (decided
        # later in the same pass - handled by second sweep below)
        i += 1

    if entry_label_pending:
        text = '\n'.join(out)
        for fn in entry_label_pending:
            pat = re.compile(r'(DEFINE_REX_FUNC\(sub_' + fn + r'\) \{\n\tREX_FUNC_PROLOGUE\(\);\n)')
            text, n = pat.subn(r'\1\tloc_' + fn + '_entry:\n', text)
            if n == 0:
                stats['anomaly'].append((path, 0, fn, fn, 'entry-label-insert'))
        out = text.split('\n')
    # drop the now-stale '// FATAL: unresolved function' comment lines
    out = [l for l in out if not COMMENT_FATAL_RE.search(l)]
    if any(stats[k] for k in ('bl', 'b', 'self_b', 'self_bl')):
        open(path, 'w').write('\n'.join(out))
    return stats


def main():
    gen_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           '..', 'generated', 'default')
    bodies = bodies_present(gen_dir)
    total = {'bl': 0, 'b': 0, 'self_b': 0, 'self_bl': 0, 'anomaly': []}
    for path in sorted(glob.glob(os.path.join(gen_dir, 'bejeweled_recomp.*.cpp'))):
        s = fix_file(path, bodies)
        for k in ('bl', 'b', 'self_b', 'self_bl'):
            total[k] += s[k]
        total['anomaly'] += s['anomaly']
    print(f"bl={total['bl']} b={total['b']} self_b={total['self_b']} "
          f"self_bl={total['self_bl']} anomalies={len(total['anomaly'])}")
    for a in total['anomaly'][:20]:
        print('ANOMALY:', a)


if __name__ == '__main__':
    sys.exit(main())
