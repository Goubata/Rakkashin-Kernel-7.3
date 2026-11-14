# SYNX, IPCLite and Qualcomm hardware fences: work in progress

This directory is not part of the 56 successfully linked support modules.
All selected C translation units compile. Final modpost now fails on three
Gunyah RM API names, down from the previous eight. The real SSR helpers and
Gunyah doorbell/message queue modules have been linked separately. No final
fence .ko or physical hardware operation is claimed. See
reports/qti-fence-dependencies.json and reports/qti-fence-stage8-v2-link.log.

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

Remaining actual provider: Gunyah resource manager.

- `gh_rm_register_notifier`, `ghd_rm_get_vmid`, `ghd_rm_mem_share`.
- The adapter in `../gunyah-rm` compiles, but still needs the real common RM
  transport and SCM assign-memory notifier; it is not a successfully linked module.

Static analysis of the saved phone modules corrected an earlier assumption:
stock qcom_q6v5_pas does not export rproc_set_state, and stock msm_hw_fence does
not import it. The latter contains the real V2 power queue/IPCC/ack path.
The saved AVR DT has an enabled Alor SOCCP and primary hw-fence controller.
The port retains that V2 path unchanged. V1 routines are compiled only with
CONFIG_QCOM_Q6V5_PAS_SOCCP_V1 and require the real V1 provider. An unsupported
V1 selection now fails with EOPNOTSUPP rather than silently succeeding through
the old optional-header stub. No V1 functionality is claimed for this build.
See reports/soccp-binary-analysis.json; disassembly stays in ignored artifacts.

Object name coverage does not prove module CRCs or firmware compatibility.
Complete providers, final link, lifecycle/security audit, IPC tests, GPU/display/
camera clients and actual SOCCP reset/suspend paths still need verification.
Do not load these objects into the phone's running 6.12 kernel.
