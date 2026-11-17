#!/usr/bin/env python3
"""Wrap vendor '#define NAME VALUE' lines that clash with a 7.3 header macro:
  #ifdef NAME / static_assert(NAME == (VALUE)); / #else / #define ... / #endif
so the build proves the values agree instead of silently redefining them.
Usage: guard_macro_redefs.py BUILD_LOG PORT_DIR"""
import re, sys
from pathlib import Path
log, port = Path(sys.argv[1]).read_text(errors='ignore'), Path(sys.argv[2])
hits = {}
for f, name in re.findall(r'([\w/.+-]+\.[ch]):\d+:\d+: error: \'(\w+)\' macro redefined', log):
    f = re.sub(r'^.*?/ports/[^/]+/', '', f)
    hits.setdefault(f, set()).add(name)
for f, names in hits.items():
    p = port / f
    s = p.read_text()
    for n in sorted(names):
        m = re.search(rf'^#define\s+{n}\s+(.+?)\s*$', s, re.M)
        if not m or f'#ifdef {n}\nstatic_assert' in s:
            continue
        val = re.sub(r'\s*/\*.*?\*/\s*$', '', m.group(1))
        s = s[:m.start()] + f'#ifdef {n}\nstatic_assert({n} == ({val}));\n#else\n{m.group(0)}\n#endif' + s[m.end():]
    p.write_text(s)
    print(f, len(names))
