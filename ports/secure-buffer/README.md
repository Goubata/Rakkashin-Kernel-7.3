# Secure scatterlist assignment on 7.3

Source: Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1,
drivers/soc/qcom/secure_buffer.c and matching trace/public headers.

Requires core patches/scm-memory-batch-7.3.patch. The real firmware command and
single-command batch behavior remain. CPU-owned descriptors are copied by SCM
to target TZMEM, replacing the vendor's raw physical descriptor mapping.
Original redundant streaming DMA mappings were removed accordingly.

Fixes: reject null/negative inputs before notifier or table dereference; use
correct destination count and completed-region prefix during rollback; bound
batches and ownership flags to the requested prefix; avoid divide-by-zero when
the first assignment fails; replace nth_page; clear readiness on unbind.

Five KUnit cases include this production implementation and replace ONLY the
SCM transport: invalid input, first-call failure, multi-destination rollback,
bounded-prefix page flags and full batches. They passed in RAM-only SMP4 QEMU.
No secure-world ownership was changed. A test-only define omits driver init/exit
registration in that fixture. Real transport/rollback, device policy and secure
content behavior remain unverified. Never load this 7.3 module into the phone's
current 6.12 kernel.
