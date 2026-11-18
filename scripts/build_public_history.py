#!/usr/bin/env python3
"""Build the public combined history (kit at the repository root, kernel source
under kernel/) without copying the Android Common Kernel history.

  build_public_history.py <pub-repo> <kit-repo> <kernel-repo> <kernel-base>

- commit 0: snapshot of the kernel at <kernel-base> (one commit, no ACK history)
- then every kit commit and every kernel commit after <kernel-base>, in author
  date order, each with the combined tree at that point; author/committer and
  dates are kept, the message is kept except for AI co-author trailers.
The objects come from the source repositories through objects/info/alternates;
`git repack -a -d` afterwards makes the result self-contained.
"""
import os, re, subprocess, sys

pub, kit, krn, kbase = sys.argv[1:5]

def git(repo, *args, inp=None, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(['git', '-c', 'safe.directory=*', '-C', repo] + list(args),
                       input=inp, capture_output=True, env=e)
    if r.returncode:
        sys.exit('git %s failed: %s' % (' '.join(args), r.stderr.decode(errors='replace')))
    return r.stdout.decode(errors='replace')

STRIP = [re.compile(r'^\s*Co-Authored-By:\s*Claude.*$', re.I),
         re.compile(r'^\s*Co-authored-by:.*anthropic\.com.*$', re.I)]

def clean_msg(msg):
    lines = [l for l in msg.split('\n') if not any(p.match(l) for p in STRIP)]
    out = '\n'.join(lines)
    out = re.sub(r'\bClaude Code\b', 'the assistant', out)
    out = re.sub(r'\bClaude\b', 'the assistant', out)
    out = re.sub(r'\n{3,}', '\n\n', out).rstrip() + '\n'
    return out

def commits(repo, rng):
    fmt = '%H%x00%T%x00%an%x00%ae%x00%aI%x00%cn%x00%ce%x00%cI%x00%B%x01'
    raw = git(repo, 'log', '--reverse', '--topo-order', '--format=' + fmt, rng)
    out = []
    for rec in raw.split('\x01'):
        rec = rec.lstrip('\n')
        if not rec:
            continue
        f = rec.split('\x00')
        out.append(dict(h=f[0], tree=f[1], an=f[2], ae=f[3], ad=f[4], cn=f[5],
                        ce=f[6], cd=f[7], msg=f[8]))
    return out

# objects of both sources are reachable from the new repository
alt = os.path.join(pub, '.git', 'objects', 'info', 'alternates')
with open(alt, 'w') as fh:
    fh.write(os.path.join(kit, '.git', 'objects') + '\n')
    fh.write(os.path.join(krn, '.git', 'objects') + '\n')

def combined_tree(kit_tree, kernel_tree):
    entries = []
    if kit_tree:
        for line in git(pub, 'ls-tree', kit_tree).splitlines():
            meta, name = line.split('\t', 1)
            if name == 'kernel':
                sys.exit('kit tree already has kernel/')
            entries.append(line)
    entries.append('040000 tree %s\tkernel' % kernel_tree)
    return git(pub, 'mktree', inp='\n'.join(entries).encode() + b'\n').strip()

def commit(tree, parent, c, msg):
    env = {'GIT_AUTHOR_NAME': c['an'], 'GIT_AUTHOR_EMAIL': c['ae'], 'GIT_AUTHOR_DATE': c['ad'],
           'GIT_COMMITTER_NAME': c['cn'], 'GIT_COMMITTER_EMAIL': c['ce'], 'GIT_COMMITTER_DATE': c['cd']}
    args = ['commit-tree', tree] + (['-p', parent] if parent else [])
    return git(pub, *args, inp=msg.encode(), env=env).strip()

base_tree = git(krn, 'rev-parse', kbase + '^{tree}').strip()
bl = git(krn, 'log', '-1', '--format=%h %s%x00%cI', kbase).strip().split('\x00')
base_c = dict(an='Android Common Kernel', ae='android-kernel@google.com', ad=bl[1],
              cn='Android Common Kernel', ce='android-kernel@google.com', cd=bl[1])
msg0 = ('Base: Android Common Kernel android-7.3 snapshot\n\n'
        'Source tree of https://android.googlesource.com/kernel/common (android-7.3)\n'
        'at %s, imported as one commit without its upstream history.\n'
        'Everything after this commit is the avr port.\n' % bl[0])
head = commit(combined_tree(None, base_tree), None, base_c, msg0)

seq = [('kit', c) for c in commits(kit, 'HEAD')] + \
      [('kernel', c) for c in commits(krn, kbase + '..HEAD')]
seq.sort(key=lambda x: (x[1]['ad'], 0 if x[0] == 'kernel' else 1))
cur_kit, cur_krn = None, base_tree
for kind, c in seq:
    if kind == 'kit':
        cur_kit = c['tree']
    else:
        cur_krn = c['tree']
    head = commit(combined_tree(cur_kit, cur_krn), head, c, clean_msg(c['msg']))
git(pub, 'update-ref', 'refs/heads/main', head)
git(pub, 'symbolic-ref', 'HEAD', 'refs/heads/main')
print('commits:', len(seq) + 1, 'head:', head)
left = git(pub, 'log', '--format=%an%n%ae%n%cn%n%ce%n%B', 'main')
print('claude mentions left:', len(re.findall(r'claude|anthropic', left, re.I)))
