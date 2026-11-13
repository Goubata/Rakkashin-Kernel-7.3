#!/usr/bin/env python3
"""One-time migration of user KGSL fence API to the checked-out Android 7.3 API."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]/'ports/kgsl'
def remove_function(s,name):
    start=s.index('static void '+name+'(')
    brace=s.index('{',start);pos=brace+1;depth=1
    while depth:
        if s[pos]=='{':depth+=1
        elif s[pos]=='}':depth-=1
        pos+=1
    return s[:start]+s[pos:]
if 'void kgsl_get_fence_value(' in (ROOT/'kgsl_sync.c').read_text():
    print('KGSL fence migration already applied')
    raise SystemExit(0)
for p in ROOT.glob('*.c'):
    s=p.read_text()
    s=s.replace('spin_lock_irqsave(fence->lock, flags)','dma_fence_lock_irqsave(fence, flags)')
    s=s.replace('spin_unlock_irqrestore(fence->lock, flags)','dma_fence_unlock_irqrestore(fence, flags)')
    s=s.replace('if (fence->ops->timeline_value_str)\n\t\tfence->ops->timeline_value_str(fence, value, sizeof(value));','kgsl_get_fence_value(fence, value, sizeof(value));')
    s=s.replace('if (fence->ops->timeline_value_str)\n\t\t\tfence->ops->timeline_value_str(fence, value, sizeof(value));','kgsl_get_fence_value(fence, value, sizeof(value));')
    s=re.sub(r'^\s*\.(?:timeline_value_str|fence_value_str|use_64bit_seqno)\s*=.*\n','\n',s,flags=re.M)
    if p.name=='kgsl_sync.c':
        s=remove_function(s,'kgsl_sync_fence_value_str')
        s=remove_function(s,'kgsl_syncsource_fence_value_str')
        start=s.index('void kgsl_get_fence_name(');end=s.index('void kgsl_get_fence_info(',start)
        s=s[:start]+'''/* The old value_str ops were removed in 7.3. Keep detailed KGSL
 * diagnostics locally, and use sequence/status for other fence providers. */
void kgsl_get_fence_value(struct dma_fence *f, char *value, u32 size)
{
    if (rcu_access_pointer(f->ops) == &kgsl_sync_fence_ops)
        kgsl_sync_timeline_value_str(f, value, size);
    else
        scnprintf(value, size, "%llu status:%d", f->seqno, dma_fence_get_status(f));
}

void kgsl_get_fence_name(struct dma_fence *f, char *name, u32 size)
{
    rcu_read_lock();
    scnprintf(name, size, "%s %s: %llu",
        rcu_dereference(dma_fence_driver_name(f)),
        rcu_dereference(dma_fence_timeline_name(f)), f->seqno);
    rcu_read_unlock();
}
''' + s[end:]
    if p.name=='kgsl_timeline.c':
        s=remove_function(s,'timeline_get_value_str')
        s=s.replace('__dma_fence_is_later(fence->seqno, f->timeline->value,\n\t\tfence->ops)','__dma_fence_is_later(fence, fence->seqno, f->timeline->value)')
        s=s.replace('dma_fence_init(&fence->base, &timeline_fence_ops,','dma_fence_init64(&fence->base, &timeline_fence_ops,')
    if p.name=='adreno_gen7_hwsched_hfi.c':
        s=s.replace('int gen7_hwsched_warmboot_init_gmu(', 'static int gen7_hwsched_warmboot_init_gmu(')
    p.write_text(s)
p=ROOT/'kgsl_sync.h';s=p.read_text();s=s.replace('void kgsl_get_fence_name(', 'void kgsl_get_fence_value(struct dma_fence *f, char *value, u32 size);\nvoid kgsl_get_fence_name(');p.write_text(s)
