# Qualcomm system statistics on 7.3

Source: Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1,
kernel/msm_sysstats.c and matching kernel/UAPI headers.

The SYSSTATS generic-netlink name, version, commands and struct layouts are
retained. DMA-buffer identification uses the actual target is_dma_buf_file API,
with the same heap-name filter. Swap information comes from si_swapinfo and
vmalloc usage from global_node_page_state(NR_VMALLOC), matching target meminfo;
the old NONEXPORTED_SYMBOLS zero-statistics stub config is not selected.

The GPU callback uses SRCU and a writer mutex. Unregister/replacement drains
existing calls before returning so GPU code can be removed safely. Registering
NULL restores the original no-GPU callback. Callbacks must not synchronously
register or unregister themselves. Test-only AVR_SYSSTATS_TEST omits module
init/exit so the same code can be exercised by a KUnit fixture.

The family now declares the attribute policy to the 7.3 core; validation in
pre_doit alone does not allocate info->attrs. A real userspace test exposed a
NULL dereference before this fix, preserved in the failed QEMU log. Missing PID
attributes now return EINVAL. KUnit covers callback values, default handling,
real vmalloc/memory counters and draining a sleeping reader during unregister.
The userspace fixture resolves the real generic-netlink family, reads memory
and self-process stats and verifies missing-PID rejection. Runtime results are
recorded under reports/. Real ROM requests and physical GPU accounting remain
unverified. Module state is never loaded into the phone's current kernel.
