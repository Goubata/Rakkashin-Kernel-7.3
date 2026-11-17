#!/usr/bin/env python3
"""Restore android16-6.12 ACK vendor hooks used by Motorola modules on 7.3:
  moto_binder.ko:     android_vh_binder_preset / _trans / _reply / _alloc_new_buf_locked
  moto_mmap_fault.ko: android_vh_adjust_alloc_flags (dma-iommu page alloc),
                      android_vh_throttle_direct_reclaim_bypass (vmscan),
                      android_vh_tune_mmap_readaround (filemap mmap read-around)
Prototypes and call sites as in 6.12; hooks are no-ops without a vendor handler.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:70]!r}')
    p.write_text(s.replace(old, new))


def add_decl(rel, decl):
    p = SRC / rel
    s = p.read_text()
    if decl.split('(')[1].split(',')[0] in s:
        return
    i = s.rindex('#endif /* _TRACE_HOOK_')
    p.write_text(s[:i] + decl + '\n' + s[i:])


add_decl('include/trace/hooks/binder.h', '''/* ANDROID: android16-6.12 parity (moto_binder) */
struct binder_thread;
struct binder_transaction_data;
struct hlist_head;
struct mutex;
DECLARE_HOOK(android_vh_binder_preset,
	TP_PROTO(struct hlist_head *hhead, struct mutex *lock, struct binder_proc *proc),
	TP_ARGS(hhead, lock, proc));
DECLARE_HOOK(android_vh_binder_reply,
	TP_PROTO(struct binder_proc *target_proc, struct binder_proc *proc,
		struct binder_thread *thread, struct binder_transaction_data *tr),
	TP_ARGS(target_proc, proc, thread, tr));
DECLARE_HOOK(android_vh_binder_trans,
	TP_PROTO(struct binder_proc *target_proc, struct binder_proc *proc,
		struct binder_thread *thread, struct binder_transaction_data *tr),
	TP_ARGS(target_proc, proc, thread, tr));
DECLARE_HOOK(android_vh_binder_alloc_new_buf_locked,
	TP_PROTO(size_t size, size_t *free_async_space, int is_async, bool *should_fail),
	TP_ARGS(size, free_async_space, is_async, should_fail));
''')
add_decl('include/trace/hooks/iommu.h', '''/* ANDROID: android16-6.12 parity (moto_mmap_fault) */
DECLARE_HOOK(android_vh_adjust_alloc_flags,
	TP_PROTO(unsigned int order, gfp_t *alloc_flags),
	TP_ARGS(order, alloc_flags));
''')
add_decl('include/trace/hooks/vmscan.h', '''/* ANDROID: android16-6.12 parity (moto_mmap_fault) */
DECLARE_HOOK(android_vh_throttle_direct_reclaim_bypass,
	TP_PROTO(bool *bypass),
	TP_ARGS(bypass));
''')
add_decl('include/trace/hooks/mm.h', '''/* ANDROID: android16-6.12 parity (moto_mmap_fault) */
DECLARE_HOOK(android_vh_tune_mmap_readaround,
	TP_PROTO(unsigned int ra_pages, pgoff_t pgoff,
		pgoff_t *start, unsigned int *size, unsigned int *async_size),
	TP_ARGS(ra_pages, pgoff, start, size, async_size));
''')

V = 'drivers/android/vendor_hooks.c'
s = (SRC / V).read_text()
for h in ['binder_preset', 'binder_reply', 'binder_trans', 'binder_alloc_new_buf_locked',
          'adjust_alloc_flags', 'throttle_direct_reclaim_bypass', 'tune_mmap_readaround']:
    line = f'EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_{h});\n'
    if line not in s:
        s = s.rstrip('\n') + '\n' + line
(SRC / V).write_text(s)

B = 'drivers/android/binder.c'
edit(B, '''		target_proc = target_thread->proc;
		target_proc->tmp_ref++;
		binder_inner_proc_unlock(target_thread->proc);
	} else {''', '''		target_proc = target_thread->proc;
		target_proc->tmp_ref++;
		binder_inner_proc_unlock(target_thread->proc);
		trace_android_vh_binder_reply(target_proc, proc, thread, tr);
	} else {''')
edit(B, '''			goto err_invalid_target_handle;
		}
		if (security_binder_transaction(proc->cred,''', '''			goto err_invalid_target_handle;
		}
		trace_android_vh_binder_trans(target_proc, proc, thread, tr);
		if (security_binder_transaction(proc->cred,''')
edit(B, '''	filp->private_data = proc;

	mutex_lock(&binder_procs_lock);''', '''	filp->private_data = proc;

	trace_android_vh_binder_preset(&binder_procs, &binder_procs_lock, proc);
	mutex_lock(&binder_procs_lock);''')
A = 'drivers/android/binder_alloc.c'
edit(A, '''	unsigned long curr_last_page;
	size_t buffer_size;

	if (is_async && alloc->free_async_space < size) {''', '''	unsigned long curr_last_page;
	size_t buffer_size;
	bool should_fail = false;

	/* ANDROID: 6.12 parity */
	trace_android_vh_binder_alloc_new_buf_locked(size, &alloc->free_async_space, is_async,
			&should_fail);
	if (should_fail) {
		binder_alloc_debug(BINDER_DEBUG_BUFFER_ALLOC,
			     "%d: binder_alloc_buf failed, not allowed to alloc more async space\\n",
			      alloc->pid);
		buffer = ERR_PTR(-EPERM);
		goto out;
	}

	if (is_async && alloc->free_async_space < size) {''')
s = (SRC / A).read_text()
if '#include <trace/hooks/binder.h>' not in s:
    i = s.index('#include "binder_alloc.h"')
    s = s[:i] + '#include <trace/hooks/binder.h>\n' + s[i:]
    (SRC / A).write_text(s)

edit('drivers/iommu/dma-iommu.c', '''				alloc_flags |= __GFP_NORETRY;
			page = alloc_pages_node(nid, alloc_flags, order);''', '''				alloc_flags |= __GFP_NORETRY;
			trace_android_vh_adjust_alloc_flags(order, &alloc_flags);
			page = alloc_pages_node(nid, alloc_flags, order);''')
edit('mm/vmscan.c', '''	/* If no zone was usable by the allocation flags then do not throttle */
	if (!pgdat)
		goto out;

	/* Account for the throttling */''', '''	/* If no zone was usable by the allocation flags then do not throttle */
	if (!pgdat)
		goto out;

	{
		bool bypass = false;	/* ANDROID: 6.12 parity */

		trace_android_vh_throttle_direct_reclaim_bypass(&bypass);
		if (bypass)
			goto out;
	}

	/* Account for the throttling */''')
F = 'mm/filemap.c'
edit(F, '''		ra->async_size = ra->ra_pages / 4;
		ra->order = 0;
	}
''', '''		ra->async_size = ra->ra_pages / 4;
		ra->order = 0;
		/* ANDROID: 6.12 parity */
		trace_android_vh_tune_mmap_readaround(ra->ra_pages, vmf->pgoff,
				&ra->start, &ra->size, &ra->async_size);
	}
''')
s = (SRC / F).read_text()
if '#include <trace/hooks/mm.h>' not in s:
    # after #undef CREATE_TRACE_POINTS (as in 6.12), or the mm hooks would be defined here again
    s = s.replace('#define CREATE_TRACE_POINTS\n#include <trace/events/filemap.h>\n', '#define CREATE_TRACE_POINTS\n#include <trace/events/filemap.h>\n#undef CREATE_TRACE_POINTS\n#include <trace/hooks/mm.h>\t/* ANDROID: as in 6.12 */\n', 1)
    (SRC / F).write_text(s)
print('applied moto vendor hooks to', SRC)
