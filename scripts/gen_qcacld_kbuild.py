#!/usr/bin/env python3
"""Generate ports/wlan-qcacld/qcacld-3.0/Kbuild from the bazel (DDK) definition the
phone module was built with (wlan_qcacld3_modules.bzl), instead of the legacy Kbuild,
which is out of sync for this release (missing mgmt_rx_srng objects, typos such as
CONFIG_CONFIG_WLAN_DUMP_IN_PROGRESS, MOTO_UTAGS_MAC only in cppflags).

DDK semantics reproduced:
  - Kconfig (module-local) evaluated with the defconfig the "perf" variant uses
    (configs/<target>_gki_<chipset>_defconfig); symbols outside the module Kconfig
    are taken from the phone kernel config (device-baseline/config.gz) so features
    match the shipped module;
  - autoconf header with CONFIG_* for y (CONFIG_*_MODULE for m), ints and strings;
  - srcs = _fixed_srcs + _conditional_srcs selected by the evaluated config;
  - includes = chipset + hw + _fixed_ipaths + _private_ipaths + ".";
  - copts = -include default_config.h, config_to_feature.h, grep defines,
    -Wunused-but-set-parameter -Wno-enum-compare.
Usage: gen_qcacld_kbuild.py [target chipset]   (default: canoe wcn7750)
"""
import ast, gzip, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
Q = ROOT / 'ports/wlan-qcacld/qcacld-3.0'
K73 = ROOT.parent / 'avr-upstream-audit/android-7.3'
target, chipset = (sys.argv[1:3] if len(sys.argv) >= 3 else ('canoe', 'wcn7750'))
bzl = (Q / 'wlan_qcacld3_modules.bzl').read_text()


def bzl_value(name):
    """Evaluate a top-level literal assignment of the .bzl (lists/dicts of strings)."""
    m = re.search(r'^' + name + r'\s*=\s*', bzl, re.M)
    i = m.end()
    depth, j, instr = 0, i, False
    while True:
        c = bzl[j]
        if c == '"' and bzl[j - 1] != '\\':
            instr = not instr
        elif not instr:
            if c in '[{(':
                depth += 1
            elif c in ']})':
                depth -= 1
                if depth == 0:
                    break
        j += 1
    text = bzl[i:j + 1]
    text = '\n'.join(l.split('#', 1)[0] if '"' not in l.split('#', 1)[0] or l.strip().startswith('#') else l
                     for l in text.split('\n'))
    text = re.sub(r'^\s*#.*$', '', text, flags=re.M)
    return ast.literal_eval(text)


fixed_srcs = bzl_value('_fixed_srcs')
cond_srcs = bzl_value('_conditional_srcs')
fixed_ipaths = bzl_value('_fixed_ipaths')
private_ipaths = bzl_value('_private_ipaths')
chipset_hdr = bzl_value('_chipset_header_map')[chipset]
hw = bzl_value('_chipset_hw_map')[chipset]
hw_hdr = bzl_value('_hw_header_map')[hw]
fixed_includes = bzl_value('_fixed_includes')

# --- external (kernel) symbols: phone kernel config ---------------------------------
ext = {}
for line in gzip.open(ROOT / 'device-baseline/config.gz', 'rt'):
    m = re.match(r'CONFIG_(\w+)=(.*)', line.strip())
    if m:
        ext[m.group(1)] = m.group(2).strip('"')
# DDK resolves kernel symbols from the vendor kernel build: GKI config + canoe_perf.bzl
for k, v in re.findall(r'"CONFIG_([A-Z0-9_]+)":\s*"(y|m)"',
                       (ROOT.parent / 'avr-upstream-audit/qcom-soc/configs/canoe_perf.bzl').read_text()):
    ext.setdefault(k, v)
# ... and the wlan platform DDK target (ports/wlan-platform defines)
for k in re.findall(r'-DCONFIG_([A-Z0-9_]+)=1', (ROOT / 'ports/wlan-platform/Makefile').read_text()):
    ext.setdefault(k[:-len('_MODULE')] if k.endswith('_MODULE') else k, 'm' if k.endswith('_MODULE') else 'y')

# --- module Kconfig ---------------------------------------------------------------------
syms, order = {}, []
ifstack = []
cur = None
for raw in (Q / 'Kconfig').read_text().split('\n'):
    line = raw.split('#', 1)[0].rstrip() if not raw.strip().startswith('help') else raw
    s = line.strip()
    if not s:
        continue
    m = re.match(r'(menu)?config\s+(\w+)$', s)
    if m:
        # a symbol may be defined several times (per chipset if-block): Kconfig makes it
        # visible if any definition is, defaults come from the visible definitions
        cur = {'type': 'bool', 'defaults': [], 'depends': list(ifstack), 'selects': []}
        if m.group(2) not in syms:
            syms[m.group(2)] = {'type': 'bool', 'defs': [], 'selects': []}
            order.append(m.group(2))
        syms[m.group(2)]['defs'].append(cur)
        continue
    if s.startswith('if '):
        ifstack.append(s[3:].strip())
        cur = None
        continue
    if s == 'endif' or s.startswith('endif '):
        ifstack.pop()
        cur = None
        continue
    if cur is None:
        continue
    m = re.match(r'(bool|tristate|int|string|hex)\b', s)
    if m:
        cur['type'] = m.group(1)
        syms[order[-1] if False else [k for k, v in syms.items() if cur in v['defs']][0]]['type'] = m.group(1)
        continue
    m = re.match(r'default\s+(.+?)(?:\s+if\s+(.+))?$', s)
    if m:
        cur['defaults'].append((m.group(1).strip().strip('"'), m.group(2)))
        continue
    m = re.match(r'depends\s+on\s+(.+)$', s)
    if m:
        cur['depends'].append(m.group(1))
        continue
    m = re.match(r'select\s+(\w+)', s)
    if m:
        cur['selects'].append(m.group(1))

defcfg = {}
for line in (Q / f'configs/{target}_gki_{chipset}_defconfig').read_text().split('\n'):
    m = re.match(r'CONFIG_(\w+)=(.*)', line.strip())
    if m:
        defcfg[m.group(1)] = m.group(2).strip().strip('"')
defcfg['QCA_CLD_WLAN'] = 'm'   # built as a module (DDK)

val = {}


def sym_val(name):
    if name in ('y', 'm', 'n'):
        return name
    if name in syms:
        return val.get(name, 'n')
    return ext.get(name, 'n')


def ev(expr):
    """Tiny Kconfig expression evaluator -> bool."""
    toks = re.findall(r'\(|\)|!=|=|&&|\|\||!|[\w]+', expr)
    pos = 0

    def atom():
        nonlocal pos
        t = toks[pos]
        pos += 1
        if t == '!':
            return not atom()
        if t == '(':
            r = orx()
            pos += 1
            return r
        v = sym_val(t)
        if pos < len(toks) and toks[pos] in ('=', '!='):
            op = toks[pos]
            rhs = sym_val(toks[pos + 1]) if toks[pos + 1] not in ('y', 'm', 'n') else toks[pos + 1]
            pos += 2
            return (v == rhs) if op == '=' else (v != rhs)
        return v not in ('n', '', '0') if v else False

    def andx():
        nonlocal pos
        r = atom()
        while pos < len(toks) and toks[pos] == '&&':
            pos += 1
            r = atom() and r
        return r

    def orx():
        nonlocal pos
        r = andx()
        while pos < len(toks) and toks[pos] == '||':
            pos += 1
            r = andx() or r
        return r
    return orx()


for _ in range(20):
    old = dict(val)
    selected = {t for n, c in syms.items() if val.get(n, 'n') not in ('n', None)
                for d in c['defs'] for t in d['selects']}
    for n in order:
        c = syms[n]
        vis = [d for d in c['defs'] if all(ev(x) for x in d['depends'])]
        if not vis:
            val[n] = 'n' if c['type'] in ('bool', 'tristate') else None
            continue
        if n in defcfg:
            v = defcfg[n]
        elif n in ext:
            # the module Kconfig re-declares some kernel symbols (SMP, NL80211_TESTMODE);
            # combined with the kernel Kconfig the kernel's value wins
            v = ext[n]
        else:
            v = None
            for d, cond in [x for dd in vis for x in dd['defaults']]:
                if cond is None or ev(cond):
                    v = sym_val(d) if (c['type'] in ('bool', 'tristate') and d in syms) else d
                    break
            if v is None:
                v = 'n' if c['type'] in ('bool', 'tristate') else None
        if n in selected and c['type'] in ('bool', 'tristate') and v == 'n':
            v = 'y'
        val[n] = v
    if val == old:
        break
unknown = sorted(k for k in defcfg if k not in syms)

# --- autoconf ---------------------------------------------------------------------------
ac = ['/* generated by scripts/gen_qcacld_kbuild.py: DDK autoconf for '
      f'{target}_gki_{chipset} (module Kconfig + defconfig) */']
for n in order:
    v, t = val.get(n), syms[n]['type']
    if v in (None, 'n'):
        continue
    if t in ('bool', 'tristate'):
        ac.append(f'#define CONFIG_{n}{"_MODULE" if v == "m" else ""} 1')
    elif t == 'string':
        ac.append(f'#define CONFIG_{n} "{v}"')
    else:
        ac.append(f'#define CONFIG_{n} {v}')
# defconfig entries that are not in Kconfig are dropped by kconfig (DDK) as well
# wlan platform symbols are part of the vendor kernel config the DDK module sees
for k in re.findall(r'-DCONFIG_([A-Z0-9_]+)=1', (ROOT / 'ports/wlan-platform/Makefile').read_text()):
    line = f'#define CONFIG_{k} 1'
    if line not in ac:
        ac.append(line)
(Q / 'configs/avr_ddk_autoconf.h').write_text('\n'.join(ac) + '\n')

# --- grep defines (same greps as the bzl genrule, against the 7.3 tree) ---------------
greps = re.findall(r'"pattern":\s*"([^"]+)",\s*"file":\s*"([^"]+)",\s*"flag":\s*"([^"]+)"', bzl)
gd = ['/* generated: bzl feature_grep_map evaluated on android18-7.3 */']
for pat, f, flag in greps:
    p = K73 / f
    if p.exists() and pat in p.read_text(errors='ignore'):
        gd.append(f'#define {flag} (1)')
(Q / 'configs/avr_grep_defines.h').write_text('\n'.join(gd) + '\n')

# --- sources ------------------------------------------------------------------------------
def on(key):
    k = key[len('CONFIG_'):] if key.startswith('CONFIG_') else key
    v = val.get(k) if k in syms else ext.get(k)
    return v not in (None, 'n', '', '0')


srcs = list(fixed_srcs)
for key, branches in cond_srcs.items():
    srcs += branches.get(on(key), [])
srcs = [s for s in dict.fromkeys(srcs) if s.endswith('.c')]
missing = [s for s in srcs if not (Q / s).exists()]
srcs = [s for s in srcs if (Q / s).exists()]
mod = f'qca_cld3_{chipset.replace("-", "_")}'
ipaths = list(dict.fromkeys(chipset_hdr + hw_hdr + fixed_ipaths + private_ipaths + ['.']))

kb = ['# SPDX-License-Identifier: GPL-2.0-only',
      '# Generated by scripts/gen_qcacld_kbuild.py from wlan_qcacld3_modules.bzl '
      f'({target} perf/gki, {chipset}); do not edit, edit the generator.',
      f'obj-m += {mod}.o',
      f'{mod}-y := ' + ' \\\n\t'.join(s[:-2] + '.o' for s in srcs),
      'ccflags-y += -include $(src)/configs/avr_ddk_autoconf.h',
      ''.join(f'ccflags-y += -include $(src)/{i}\n' for i in fixed_includes).rstrip('\n'),
      'ccflags-y += -include $(src)/configs/avr_grep_defines.h',
      'ccflags-y += -Wunused-but-set-parameter -Wno-enum-compare',
      '# 6.12 DDK builds did not make -Wmissing-prototypes fatal (7.3 does by default)',
      'ccflags-y += -Wno-missing-prototypes',
      '# ACK build.config.common: KCFLAGS += -D__ANDROID_COMMON_KERNEL__ (6.12 and 7.3 GKI builds)',
      'ccflags-y += -D__ANDROID_COMMON_KERNEL__',
      'ccflags-y += ' + ' '.join(f'-I$(src)/{i}' for i in ipaths),
      '# DDK deps: wlan-platform headers, dataipa include_headers (+ its vendor config),',
      '# WALT, minidump, iommu-util, rproc headers; AVR vendor CONFIG parity header',
      'ccflags-y += -I$(src)/../../wlan-platform/inc -I$(src)/../../dataipa/drivers/platform/msm/include '
      '-I$(src)/../../dataipa/drivers/platform/msm/include/uapi',
      'ccflags-y += -include $(src)/../../dataipa/config/dataipa_vendor.h',
      'ccflags-y += -I$(src)/../../sched-walt/include -DCONFIG_SCHED_WALT_NR_CPUS=8 '
      '-I$(src)/../../qcom-minidump/include -I$(src)/../../qcom-debug-diag/include '
      '-I$(src)/../../qcom-iommu-util/include -I$(src)/../../qcom-rproc-common/include',
      'ccflags-y += -include $(src)/avr-canoe-vendor-config.h',
      'KBUILD_EXTRA_SYMBOLS += ' + ' '.join(f'$(src)/../../{p}/Module.symvers' for p in (
          'wlan-platform', 'dataipa', 'sched-walt', 'qcom-iommu-util', 'qcom-debug-diag',
          'qcom-minidump', 'qcom-rproc-common', 'qcom-smem', 'qcom-ipc-logging')),
      '']
(Q / 'Kbuild').write_text('\n'.join(kb) + '\n')
print(f'{mod}: {len(srcs)} sources, {sum(1 for v in val.values() if v not in (None, "n"))} config symbols on, '
      f'{len(gd) - 1} grep defines; defconfig symbols not in Kconfig: {len(unknown)}; missing srcs: {missing[:5]}')
