# SYNX, IPCLite and Qualcomm hardware fences: work in progress

This directory is not part of the 50 successfully linked support modules.
All selected C translation units compile and the three aggregate objects exist,
but final modpost fails on eight external provider names. No final .ko or
physical hardware operation is claimed. See reports/qti-fence-dependencies.json
and reports/qti-fence-attempt12.log.

Sources:

- synx/: the user's kgsl-avr-1.0-source.zip staged vendor SYNX sources.
  Original archive SHA256:
  0b65d98f38556912f5fd486535f455b580287c178197fd07cc22b0448c656e1f.
  No firmware-matching public SYNX ref was found in the earlier ref audit.
- hw_fence/: Motorola vendor-qcom-opensource-mm-drivers,
  6ac06dd09262b3bbe28c6d8105f18a42f8650832, exact canoe firmware ref.
- Local vendor-only Gunyah and secure-buffer headers: Motorola kernel-msm
  037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1.
- Absent target linux/gunyah.h and matching UAPI: Motorola kernel-common
  479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4. These are declarations for the
  future real provider; they do not implement a substitute backend.

The config explicitly enables hardware-fence and SYNX interoperation, including
Gunyah doorbell/resource-manager and remoteproc notifier imports. No synx_stub
or disabled hardware backend is selected. The stock phone loads all three
modules; the relevant canoe hw_fence config also enables these paths.

Port changes:

- Target dma_fence_spinlock access and current signaling API. Compatibility
  wrappers preserve the old already-signalled error while issuing real target
  signals under the correct lock; owned external locks are released correctly.
- Array ALL and ANY behavior is preserved. Native 7.3 array callbacks and
  signaled() still support the pending-count-one path. The wrapper sets it
  before publication, without changing the target core. Five KUnit cases passed
  SMP4 QEMU: ALL/ANY with polling and callbacks, plus repeated-signal error.
- Private functions are scoped static; cross-file/exported callbacks gain
  declarations. Add the actual synx_ioctl implementation to the aggregate.
- Initialize an invalid-selection map pointer; reject unsupported merge-n
  request types and zero object counts rather than freeing an uninitialized
  pointer or returning success without performing a merge.

Remaining actual providers:

- qcom_register_ssr_notifier / qcom_unregister_ssr_notifier (remoteproc common)
- rproc_set_state: vendor qcom_q6v5_pas.c SOCCP power-vote implementation;
  assigning rproc->state is not an equivalent implementation.
- gh_dbl_read_and_clean / gh_dbl_rx_register (Gunyah doorbell)
- gh_rm_register_notifier / ghd_rm_get_vmid / ghd_rm_mem_share (Gunyah RM)

Object name coverage does not prove module CRCs or firmware compatibility.
Complete providers, final link, lifecycle/security audit, IPC tests, GPU/display/
camera clients and actual SOCCP reset/suspend paths still need verification.
Do not load these objects into the phone's running 6.12 kernel.
