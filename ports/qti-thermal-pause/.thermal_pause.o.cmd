savedcmd_thermal_pause.o := clang -Wp,-MMD,./.thermal_pause.o.d -nostdinc -I/work/avr-src/arch/arm64/include -I/work/avr-port/out/native-gki/arch/arm64/include/generated -I/work/avr-src/include -I/work/avr-port/out/native-gki/include -I/work/avr-src/arch/arm64/include/uapi -I/work/avr-port/out/native-gki/arch/arm64/include/generated/uapi -I/work/avr-src/include/uapi -I/work/avr-port/out/native-gki/include/generated/uapi -include /work/avr-src/include/linux/compiler-version.h -include /work/avr-src/include/linux/kconfig.h -include /work/avr-src/include/linux/compiler_types.h -D__KERNEL__ --target=aarch64-linux-gnu -fintegrated-as -Werror=unknown-warning-option -Werror=ignored-optimization-argument -Werror=option-ignored -Werror=unused-command-line-argument -mlittle-endian -DKASAN_SHADOW_SCALE_SHIFT= -Werror -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -std=gnu11 -fms-extensions -Wno-gnu -Wno-microsoft-anon-tag -mgeneral-regs-only -DCONFIG_CC_HAS_K_CONSTRAINT=1 -Wno-psabi -fasynchronous-unwind-tables -mbranch-protection=pac-ret -Wa,-march=armv8.5-a -DARM64_ASM_ARCH='"armv8.5-a"' -ffixed-x18 -DKASAN_SHADOW_SCALE_SHIFT= -fno-delete-null-pointer-checks -O2 -fstack-protector-strong -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftrivial-auto-var-init=zero -fno-stack-clash-protection -fsanitize=kcfi -fsanitize-cfi-icall-experimental-normalize-integers -falign-functions=4 -fstrict-flex-arrays=3 -fno-strict-overflow -fno-stack-check -fno-builtin-wcslen -Wall -Wextra -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wmissing-declarations -Wmissing-prototypes -Wframe-larger-than=2048 -Wno-format-overflow-non-kprintf -Wno-format-truncation-non-kprintf -Wno-default-const-init-unsafe -Wno-type-limits -Wno-pointer-sign -Wcast-function-type -Wno-unterminated-string-initialization -Wimplicit-fallthrough -Werror=date-time -Werror=incompatible-pointer-types -Wenum-conversion -Wunused -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-format-overflow -Wno-override-init -Wno-pointer-to-enum-cast -Wno-tautological-constant-out-of-range-compare -Wno-unaligned-access -Wno-enum-compare-conditional -Wno-missing-field-initializers -Wno-shift-negative-value -Wno-enum-enum-conversion -Wno-sign-compare -Wno-unused-parameter -g -gdwarf-5 -gz=zstd -mstack-protector-guard=sysreg -mstack-protector-guard-reg=sp_el0 -mstack-protector-guard-offset=1928 -I././include -I././../sched-walt/include -DCONFIG_QTI_CPU_PAUSE_COOLING_DEVICE_MODULE=1 -DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8  -fsanitize=array-bounds -fsanitize=local-bounds -fsanitize-trap=undefined    -DMODULE  -DKBUILD_BASENAME='"thermal_pause"' -DKBUILD_MODNAME='"thermal_pause"' -D__KBUILD_MODNAME=thermal_pause -c -o thermal_pause.o thermal_pause.c  

source_thermal_pause.o := thermal_pause.c

deps_thermal_pause.o := \
  /work/avr-src/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /work/avr-src/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /work/avr-src/include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/FUNCTION_ALIGNMENT) \
    $(wildcard include/config/CC_HAS_SANE_FUNCTION_ALIGNMENT) \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/LD_DEAD_CODE_DATA_ELIMINATION) \
    $(wildcard include/config/LTO_CLANG) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/CC_HAS_ASSUME) \
    $(wildcard include/config/CC_HAS_COUNTED_BY) \
    $(wildcard include/config/FORTIFY_SOURCE) \
    $(wildcard include/config/UBSAN_BOUNDS) \
    $(wildcard include/config/CC_HAS_COUNTED_BY_PTR) \
    $(wildcard include/config/CC_HAS_MULTIDIMENSIONAL_NONSTRING) \
    $(wildcard include/config/CFI) \
    $(wildcard include/config/ARCH_USES_CFI_GENERIC_LLVM_PASS) \
    $(wildcard include/config/CC_HAS_BROKEN_COUNTED_BY_REF) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /work/avr-src/include/linux/compiler-context-analysis.h \
  /work/avr-src/include/linux/compiler_attributes.h \
  /work/avr-src/include/linux/compiler-clang.h \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/CLANG_VERSION) \
    $(wildcard include/config/CC_HAS_TYPEOF_UNQUAL) \
  /work/avr-src/arch/arm64/include/asm/compiler.h \
    $(wildcard include/config/ARM64_PTR_AUTH_KERNEL) \
    $(wildcard include/config/ARM64_PTR_AUTH) \
    $(wildcard include/config/BUILTIN_RETURN_ADDRESS_STRIPS_PAC) \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/percpu_types.h \
  /work/avr-src/include/asm-generic/percpu_types.h \
  /work/avr-src/include/linux/cpu.h \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/GENERIC_CPU_DEVICES) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/PM_SLEEP_SMP) \
    $(wildcard include/config/PM_SLEEP_SMP_NONZERO_CPU) \
    $(wildcard include/config/ARCH_HAS_CPU_FINALIZE_INIT) \
    $(wildcard include/config/CPU_MITIGATIONS) \
  /work/avr-src/include/linux/node.h \
    $(wildcard include/config/HMEM_REPORTING) \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/NUMA) \
  /work/avr-src/include/linux/device.h \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
    $(wildcard include/config/GENERIC_MSI_IRQ) \
    $(wildcard include/config/ENERGY_MODEL) \
    $(wildcard include/config/PINCTRL) \
    $(wildcard include/config/ARCH_HAS_DMA_OPS) \
    $(wildcard include/config/DMA_DECLARE_COHERENT) \
    $(wildcard include/config/DMA_CMA) \
    $(wildcard include/config/SWIOTLB) \
    $(wildcard include/config/SWIOTLB_DYNAMIC) \
    $(wildcard include/config/PM) \
    $(wildcard include/config/PM_SLEEP) \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/OF) \
    $(wildcard include/config/DEVTMPFS) \
  /work/avr-src/include/linux/dev_printk.h \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /work/avr-src/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
  /work/avr-src/arch/arm64/include/asm/rwonce.h \
    $(wildcard include/config/LTO) \
  /work/avr-src/include/asm-generic/rwonce.h \
  /work/avr-src/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /work/avr-src/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
    $(wildcard include/config/KCOV) \
    $(wildcard include/config/KVFREE_RCU_BATCHED) \
  /work/avr-src/include/uapi/linux/types.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/types.h \
  /work/avr-src/include/uapi/asm-generic/types.h \
  /work/avr-src/include/asm-generic/int-ll64.h \
  /work/avr-src/include/uapi/asm-generic/int-ll64.h \
  /work/avr-src/arch/arm64/include/uapi/asm/bitsperlong.h \
  /work/avr-src/include/asm-generic/bitsperlong.h \
  /work/avr-src/include/uapi/asm-generic/bitsperlong.h \
  /work/avr-src/include/uapi/linux/posix_types.h \
  /work/avr-src/include/linux/stddef.h \
  /work/avr-src/include/uapi/linux/stddef.h \
  /work/avr-src/arch/arm64/include/uapi/asm/posix_types.h \
  /work/avr-src/include/uapi/asm-generic/posix_types.h \
  /work/avr-src/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /work/avr-src/include/linux/ratelimit.h \
  /work/avr-src/include/linux/ratelimit_types.h \
  /work/avr-src/include/linux/bits.h \
  /work/avr-src/include/vdso/bits.h \
  /work/avr-src/include/vdso/const.h \
  /work/avr-src/include/uapi/linux/const.h \
  /work/avr-src/include/uapi/linux/bits.h \
  /work/avr-src/include/linux/build_bug.h \
  /work/avr-src/include/linux/overflow.h \
  /work/avr-src/include/linux/limits.h \
  /work/avr-src/include/uapi/linux/limits.h \
  /work/avr-src/include/vdso/limits.h \
  /work/avr-src/include/linux/const.h \
  /work/avr-src/include/uapi/linux/param.h \
  /work/avr-src/arch/arm64/include/uapi/asm/param.h \
  /work/avr-src/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /work/avr-src/include/uapi/asm-generic/param.h \
  /work/avr-src/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
  /work/avr-src/arch/arm64/include/asm/spinlock_types.h \
  /work/avr-src/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/NR_CPUS) \
  /work/avr-src/include/asm-generic/qrwlock_types.h \
  /work/avr-src/arch/arm64/include/uapi/asm/byteorder.h \
  /work/avr-src/include/linux/byteorder/little_endian.h \
  /work/avr-src/include/uapi/linux/byteorder/little_endian.h \
  /work/avr-src/include/linux/swab.h \
  /work/avr-src/include/uapi/linux/swab.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/swab.h \
  /work/avr-src/include/uapi/asm-generic/swab.h \
  /work/avr-src/include/linux/byteorder/generic.h \
  /work/avr-src/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/LOCK_STAT) \
  /work/avr-src/include/linux/sched.h \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_NATIVE) \
    $(wildcard include/config/SCHED_INFO) \
    $(wildcard include/config/SCHEDSTATS) \
    $(wildcard include/config/SCHED_CORE) \
    $(wildcard include/config/FAIR_GROUP_SCHED) \
    $(wildcard include/config/RT_GROUP_SCHED) \
    $(wildcard include/config/RT_MUTEXES) \
    $(wildcard include/config/UCLAMP_TASK) \
    $(wildcard include/config/UCLAMP_BUCKETS_COUNT) \
    $(wildcard include/config/KMAP_LOCAL) \
    $(wildcard include/config/PREEMPTION) \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/MEM_ALLOC_PROFILING) \
    $(wildcard include/config/SCHED_CLASS_EXT) \
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/CFS_BANDWIDTH) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/TRIVIAL_PREEMPT_RCU) \
    $(wildcard include/config/MEMCG_V1) \
    $(wildcard include/config/LRU_GEN) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/ARCH_HAS_CPU_PASID) \
    $(wildcard include/config/X86_BUS_LOCK_DETECT) \
    $(wildcard include/config/TASK_DELAY_ACCT) \
    $(wildcard include/config/STACKPROTECTOR) \
    $(wildcard include/config/ARCH_HAS_SCALED_CPUTIME) \
    $(wildcard include/config/CPU_FREQ_TIMES) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_GEN) \
    $(wildcard include/config/NO_HZ_FULL) \
    $(wildcard include/config/POSIX_CPUTIMERS) \
    $(wildcard include/config/POSIX_CPU_TIMERS_TASK_WORK) \
    $(wildcard include/config/KEYS) \
    $(wildcard include/config/SYSVIPC) \
    $(wildcard include/config/DETECT_HUNG_TASK) \
    $(wildcard include/config/IO_URING) \
    $(wildcard include/config/AUDIT) \
    $(wildcard include/config/AUDITSYSCALL) \
    $(wildcard include/config/SCHED_PROXY_EXEC) \
    $(wildcard include/config/DETECT_HUNG_TASK_BLOCKER) \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/UBSAN) \
    $(wildcard include/config/UBSAN_TRAP) \
    $(wildcard include/config/COMPACTION) \
    $(wildcard include/config/TASK_XACCT) \
    $(wildcard include/config/CPUSETS) \
    $(wildcard include/config/X86_CPU_RESCTRL) \
    $(wildcard include/config/PERF_EVENTS) \
    $(wildcard include/config/DEBUG_PREEMPT) \
    $(wildcard include/config/NUMA_BALANCING) \
    $(wildcard include/config/SCHED_CACHE) \
    $(wildcard include/config/ARCH_HAS_LAZY_MMU_MODE) \
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/KMSAN) \
    $(wildcard include/config/KUNIT) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/UPROBES) \
    $(wildcard include/config/BCACHE) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/VMAP_STACK) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/SECURITY) \
    $(wildcard include/config/BPF_SYSCALL) \
    $(wildcard include/config/KSTACK_ERASE) \
    $(wildcard include/config/KSTACK_ERASE_METRICS) \
    $(wildcard include/config/X86_MCE) \
    $(wildcard include/config/KRETPROBES) \
    $(wildcard include/config/RETHOOK) \
    $(wildcard include/config/ARCH_HAS_PARANOID_L1D_FLUSH) \
    $(wildcard include/config/RV) \
    $(wildcard include/config/RV_PER_TASK_MONITORS) \
    $(wildcard include/config/USER_EVENTS) \
    $(wildcard include/config/UNWIND_USER) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
    $(wildcard include/config/MEM_ALLOC_PROFILING_DEBUG) \
    $(wildcard include/config/SCHED_MM_CID) \
  /work/avr-src/include/uapi/linux/sched.h \
  /work/avr-src/arch/arm64/include/asm/current.h \
  /work/avr-src/arch/arm64/include/asm/processor.h \
    $(wildcard include/config/COMPAT) \
    $(wildcard include/config/ARM64_64K_PAGES) \
    $(wildcard include/config/KUSER_HELPERS) \
    $(wildcard include/config/ARM64_FORCE_52BIT) \
    $(wildcard include/config/HAVE_HW_BREAKPOINT) \
    $(wildcard include/config/ARM64_MTE) \
    $(wildcard include/config/ARM64_GCS) \
    $(wildcard include/config/ARM64_TAGGED_ADDR_ABI) \
  /work/avr-src/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /work/avr-src/include/uapi/linux/kernel.h \
  /work/avr-src/include/uapi/linux/sysinfo.h \
  /work/avr-src/include/vdso/cache.h \
  /work/avr-src/arch/arm64/include/asm/cache.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
  /work/avr-src/include/linux/bitops.h \
  /work/avr-src/include/linux/typecheck.h \
  /work/avr-src/include/asm-generic/bitops/generic-non-atomic.h \
  /work/avr-src/arch/arm64/include/asm/barrier.h \
    $(wildcard include/config/ARM64_PSEUDO_NMI) \
  /work/avr-src/arch/arm64/include/asm/alternative-macros.h \
  /work/avr-src/arch/arm64/include/asm/cpucaps.h \
    $(wildcard include/config/ARM64_EPAN) \
    $(wildcard include/config/ARM64_SVE) \
    $(wildcard include/config/ARM64_SME) \
    $(wildcard include/config/ARM64_CNP) \
    $(wildcard include/config/ARM64_BTI) \
    $(wildcard include/config/ARM64_TLB_RANGE) \
    $(wildcard include/config/ARM64_POE) \
    $(wildcard include/config/ARM64_HAFT) \
    $(wildcard include/config/UNMAP_KERNEL_AT_EL0) \
    $(wildcard include/config/ARM64_ERRATUM_843419) \
    $(wildcard include/config/ARM64_ERRATUM_1742098) \
    $(wildcard include/config/ARM64_ERRATUM_2645198) \
    $(wildcard include/config/ARM64_ERRATUM_2658417) \
    $(wildcard include/config/CAVIUM_ERRATUM_23154) \
    $(wildcard include/config/ARM64_WORKAROUND_DISABLE_CNP) \
    $(wildcard include/config/ARM64_WORKAROUND_REPEAT_TLBI_SYNC) \
    $(wildcard include/config/ARM64_ERRATUM_3194386) \
    $(wildcard include/config/ARM64_ERRATUM_4193714) \
    $(wildcard include/config/HW_PERF_EVENTS) \
    $(wildcard include/config/ARM64_LSUI) \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/cpucap-defs.h \
  /work/avr-src/arch/arm64/include/asm/insn-def.h \
  /work/avr-src/arch/arm64/include/asm/brk-imm.h \
  /work/avr-src/include/linux/stringify.h \
  /work/avr-src/include/asm-generic/barrier.h \
  /work/avr-src/arch/arm64/include/asm/bitops.h \
  /work/avr-src/include/asm-generic/bitops/builtin-__ffs.h \
  /work/avr-src/include/asm-generic/bitops/builtin-ffs.h \
  /work/avr-src/include/asm-generic/bitops/builtin-__fls.h \
  /work/avr-src/include/asm-generic/bitops/builtin-fls.h \
  /work/avr-src/include/asm-generic/bitops/ffz.h \
  /work/avr-src/include/asm-generic/bitops/fls64.h \
  /work/avr-src/include/asm-generic/bitops/sched.h \
  /work/avr-src/include/asm-generic/bitops/hweight.h \
  /work/avr-src/include/asm-generic/bitops/arch_hweight.h \
  /work/avr-src/include/asm-generic/bitops/const_hweight.h \
  /work/avr-src/include/asm-generic/bitops/atomic.h \
  /work/avr-src/include/linux/atomic.h \
  /work/avr-src/arch/arm64/include/asm/atomic.h \
  /work/avr-src/arch/arm64/include/asm/cmpxchg.h \
  /work/avr-src/arch/arm64/include/asm/lse.h \
  /work/avr-src/arch/arm64/include/asm/atomic_ll_sc.h \
    $(wildcard include/config/CC_HAS_K_CONSTRAINT) \
  /work/avr-src/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/GENDWARFKSYMS) \
  /work/avr-src/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /work/avr-src/arch/arm64/include/asm/linkage.h \
  /work/avr-src/arch/arm64/include/asm/alternative.h \
    $(wildcard include/config/MODULES) \
  /work/avr-src/include/linux/init.h \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
  /work/avr-src/arch/arm64/include/asm/atomic_lse.h \
  /work/avr-src/include/linux/atomic/atomic-arch-fallback.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /work/avr-src/include/linux/atomic/atomic-long.h \
  /work/avr-src/include/linux/atomic/atomic-instrumented.h \
  /work/avr-src/include/linux/instrumented.h \
    $(wildcard include/config/DEBUG_ATOMIC) \
    $(wildcard include/config/DEBUG_ATOMIC_LARGEST_ALIGN) \
  /work/avr-src/include/linux/bug.h \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /work/avr-src/arch/arm64/include/asm/bug.h \
  /work/avr-src/arch/arm64/include/asm/asm-bug.h \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /work/avr-src/include/asm-generic/bug.h \
    $(wildcard include/config/DEBUG_BUGVERBOSE_DETAILED) \
    $(wildcard include/config/BUG) \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
  /work/avr-src/include/linux/instrumentation.h \
    $(wildcard include/config/NOINSTR_VALIDATION) \
  /work/avr-src/include/linux/once_lite.h \
  /work/avr-src/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /work/avr-src/include/linux/stdarg.h \
  /work/avr-src/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK_INDEX) \
  /work/avr-src/include/linux/kern_levels.h \
  /work/avr-src/include/linux/kmsan-checks.h \
  /work/avr-src/include/asm-generic/bitops/instrumented-atomic.h \
  /work/avr-src/include/asm-generic/bitops/lock.h \
  /work/avr-src/include/asm-generic/bitops/instrumented-lock.h \
  /work/avr-src/include/asm-generic/bitops/non-atomic.h \
  /work/avr-src/include/asm-generic/bitops/non-instrumented-non-atomic.h \
  /work/avr-src/include/asm-generic/bitops/le.h \
  /work/avr-src/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /work/avr-src/include/linux/kasan-enabled.h \
    $(wildcard include/config/ARCH_DEFER_KASAN) \
    $(wildcard include/config/KASAN) \
  /work/avr-src/include/linux/static_key.h \
  /work/avr-src/include/linux/jump_label.h \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /work/avr-src/include/linux/cleanup.h \
  /work/avr-src/include/linux/err.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/errno.h \
  /work/avr-src/include/uapi/asm-generic/errno.h \
  /work/avr-src/include/uapi/asm-generic/errno-base.h \
  /work/avr-src/include/linux/args.h \
  /work/avr-src/arch/arm64/include/asm/jump_label.h \
  /work/avr-src/arch/arm64/include/asm/insn.h \
  /work/avr-src/arch/arm64/include/asm/cputype.h \
  /work/avr-src/arch/arm64/include/asm/sysreg.h \
    $(wildcard include/config/BROKEN_GAS_INST) \
    $(wildcard include/config/ARM64_PA_BITS_52) \
    $(wildcard include/config/ARM64_4K_PAGES) \
    $(wildcard include/config/ARM64_16K_PAGES) \
    $(wildcard include/config/AMPERE_ERRATUM_AC04_CPU_23) \
  /work/avr-src/include/linux/kasan-tags.h \
  /work/avr-src/arch/arm64/include/asm/gpr-num.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/sysreg-defs.h \
  /work/avr-src/include/linux/bitfield.h \
  /work/avr-src/arch/arm64/include/asm/mte-def.h \
  /work/avr-src/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
  /work/avr-src/include/linux/array_size.h \
  /work/avr-src/include/linux/errno.h \
  /work/avr-src/include/uapi/linux/errno.h \
  /work/avr-src/include/uapi/linux/string.h \
  /work/avr-src/arch/arm64/include/asm/string.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  /work/avr-src/include/linux/fortify-string.h \
    $(wildcard include/config/CC_HAS_KASAN_MEMINTRINSIC_PREFIX) \
    $(wildcard include/config/GENERIC_ENTRY) \
  /work/avr-src/include/linux/thread_info.h \
    $(wildcard include/config/ARCH_HAS_PREEMPT_LAZY) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/SH) \
  /work/avr-src/include/linux/restart_block.h \
  /work/avr-src/include/linux/time64.h \
  /work/avr-src/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /work/avr-src/include/linux/math.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/div64.h \
  /work/avr-src/include/asm-generic/div64.h \
    $(wildcard include/config/CC_OPTIMIZE_FOR_PERFORMANCE) \
  /work/avr-src/include/vdso/math64.h \
  /work/avr-src/include/vdso/time64.h \
  /work/avr-src/include/uapi/linux/time.h \
  /work/avr-src/include/uapi/linux/time_types.h \
  /work/avr-src/arch/arm64/include/asm/thread_info.h \
    $(wildcard include/config/ARM64_SW_TTBR0_PAN) \
    $(wildcard include/config/SHADOW_CALL_STACK) \
    $(wildcard include/config/ARM64_MPAM) \
  /work/avr-src/arch/arm64/include/asm/memory.h \
    $(wildcard include/config/ARM64_VA_BITS) \
    $(wildcard include/config/KASAN_SHADOW_OFFSET) \
    $(wildcard include/config/RANDOMIZE_BASE) \
    $(wildcard include/config/DEBUG_VIRTUAL) \
    $(wildcard include/config/EFI) \
    $(wildcard include/config/ARM_GIC_V3_ITS) \
  /work/avr-src/include/linux/sizes.h \
  /work/avr-src/arch/arm64/include/asm/page-def.h \
  /work/avr-src/include/vdso/page.h \
    $(wildcard include/config/PAGE_SHIFT) \
  /work/avr-src/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/DEBUG_VM_IRQSOFF) \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /work/avr-src/arch/arm64/include/asm/boot.h \
  /work/avr-src/arch/arm64/include/asm/sections.h \
  /work/avr-src/include/asm-generic/sections.h \
    $(wildcard include/config/HAVE_FUNCTION_DESCRIPTORS) \
  /work/avr-src/include/asm-generic/memory_model.h \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
    $(wildcard include/config/SPARSEMEM) \
  /work/avr-src/include/linux/pfn.h \
  /work/avr-src/arch/arm64/include/asm/stack_pointer.h \
  /work/avr-src/include/linux/android_vendor.h \
    $(wildcard include/config/ANDROID_VENDOR_OEM_DATA) \
  /work/avr-src/include/vdso/processor.h \
  /work/avr-src/arch/arm64/include/asm/vdso/processor.h \
  /work/avr-src/arch/arm64/include/asm/cpufeature.h \
    $(wildcard include/config/ARM64_DEBUG_PRIORITY_MASKING) \
    $(wildcard include/config/ARM64_BTI_KERNEL) \
    $(wildcard include/config/ARM64_PA_BITS) \
    $(wildcard include/config/ARM64_HW_AFDBM) \
    $(wildcard include/config/ARM64_AMU_EXTN) \
    $(wildcard include/config/ARM64_LPA2) \
  /work/avr-src/arch/arm64/include/asm/hwcap.h \
  /work/avr-src/arch/arm64/include/uapi/asm/hwcap.h \
  /work/avr-src/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/kernel-hwcap.h \
  /work/avr-src/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/PROVE_LOCKING) \
    $(wildcard include/config/DYNAMIC_FTRACE) \
  /work/avr-src/include/linux/align.h \
  /work/avr-src/include/vdso/align.h \
  /work/avr-src/include/linux/container_of.h \
  /work/avr-src/include/linux/kstrtox.h \
  /work/avr-src/include/linux/minmax.h \
  /work/avr-src/include/linux/sprintf.h \
  /work/avr-src/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
  /work/avr-src/include/linux/trace_printk.h \
  /work/avr-src/include/linux/instruction_pointer.h \
  /work/avr-src/include/linux/util_macros.h \
    $(wildcard include/config/FOO_SUSPEND) \
  /work/avr-src/include/linux/wordpart.h \
  /work/avr-src/include/linux/cpumask.h \
    $(wildcard include/config/FORCE_NR_CPUS) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
  /work/avr-src/include/linux/bitmap.h \
  /work/avr-src/include/linux/find.h \
  /work/avr-src/include/linux/bitmap-str.h \
  /work/avr-src/include/linux/cpumask_types.h \
  /work/avr-src/include/linux/threads.h \
    $(wildcard include/config/BASE_SMALL) \
  /work/avr-src/include/linux/gfp_types.h \
  /work/avr-src/include/linux/numa.h \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
    $(wildcard include/config/HAVE_ARCH_NODE_DEV_GROUP) \
  /work/avr-src/include/linux/nodemask.h \
    $(wildcard include/config/HIGHMEM) \
  /work/avr-src/include/linux/nodemask_types.h \
    $(wildcard include/config/NODES_SHIFT) \
  /work/avr-src/include/linux/random.h \
    $(wildcard include/config/VMGENID) \
  /work/avr-src/include/linux/list.h \
    $(wildcard include/config/LIST_HARDENED) \
    $(wildcard include/config/DEBUG_LIST) \
  /work/avr-src/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /work/avr-src/include/uapi/linux/random.h \
  /work/avr-src/include/uapi/linux/ioctl.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/ioctl.h \
  /work/avr-src/include/asm-generic/ioctl.h \
  /work/avr-src/include/uapi/asm-generic/ioctl.h \
  /work/avr-src/include/linux/irqnr.h \
  /work/avr-src/include/uapi/linux/irqnr.h \
  /work/avr-src/arch/arm64/include/asm/hw_breakpoint.h \
    $(wildcard include/config/CPU_PM) \
  /work/avr-src/arch/arm64/include/asm/virt.h \
    $(wildcard include/config/KVM) \
  /work/avr-src/arch/arm64/include/asm/ptrace.h \
  /work/avr-src/arch/arm64/include/uapi/asm/ptrace.h \
  /work/avr-src/arch/arm64/include/uapi/asm/sve_context.h \
  /work/avr-src/include/linux/irqchip/arm-gic-v3-prio.h \
  /work/avr-src/arch/arm64/include/asm/stacktrace/frame.h \
  /work/avr-src/arch/arm64/include/asm/kasan.h \
  /work/avr-src/arch/arm64/include/asm/mte-kasan.h \
  /work/avr-src/arch/arm64/include/asm/pgtable-types.h \
    $(wildcard include/config/PGTABLE_LEVELS) \
  /work/avr-src/include/asm-generic/pgtable-nopud.h \
  /work/avr-src/include/asm-generic/pgtable-nop4d.h \
  /work/avr-src/arch/arm64/include/asm/pgtable-hwdef.h \
    $(wildcard include/config/ARM64_CONT_PTE_SHIFT) \
    $(wildcard include/config/ARM64_CONT_PMD_SHIFT) \
    $(wildcard include/config/ARM64_VA_BITS_52) \
  /work/avr-src/arch/arm64/include/asm/pointer_auth.h \
  /work/avr-src/include/uapi/linux/prctl.h \
  /work/avr-src/arch/arm64/include/asm/spectre.h \
  /work/avr-src/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
    $(wildcard include/config/CSD_LOCK_WAIT_DEBUG) \
  /work/avr-src/include/linux/smp_types.h \
  /work/avr-src/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /work/avr-src/include/linux/preempt.h \
    $(wildcard include/config/HAS_SEPARATE_PREEMPT_RESCHED_BITS) \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPT_NONE) \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT) \
    $(wildcard include/config/PREEMPT_LAZY) \
  /work/avr-src/arch/arm64/include/asm/preempt.h \
  /work/avr-src/arch/arm64/include/asm/smp.h \
    $(wildcard include/config/ARM64_ACPI_PARKING_PROTOCOL) \
  /work/avr-src/arch/arm64/include/asm/percpu.h \
  /work/avr-src/include/asm-generic/percpu.h \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /work/avr-src/include/linux/percpu-defs.h \
    $(wildcard include/config/ARCH_MODULE_NEEDS_WEAK_PER_CPU) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /work/avr-src/arch/arm64/include/asm/fpsimd.h \
  /work/avr-src/arch/arm64/include/uapi/asm/sigcontext.h \
  /work/avr-src/include/linux/futex_types.h \
    $(wildcard include/config/FUTEX) \
    $(wildcard include/config/FUTEX_PRIVATE_HASH) \
    $(wildcard include/config/FUTEX_ROBUST_UNLOCK) \
  /work/avr-src/include/linux/mutex_types.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_MUTEXES) \
  /work/avr-src/include/linux/osq_lock.h \
  /work/avr-src/include/linux/spinlock_types.h \
  /work/avr-src/include/linux/rwlock_types.h \
  /work/avr-src/include/linux/irqflags_types.h \
  /work/avr-src/include/linux/pid_types.h \
  /work/avr-src/include/linux/sem_types.h \
  /work/avr-src/include/linux/shm.h \
  /work/avr-src/arch/arm64/include/asm/page.h \
  /work/avr-src/include/linux/personality.h \
  /work/avr-src/include/uapi/linux/personality.h \
  /work/avr-src/include/asm-generic/getorder.h \
  /work/avr-src/arch/arm64/include/asm/shmparam.h \
  /work/avr-src/include/asm-generic/shmparam.h \
  /work/avr-src/include/linux/kmsan_types.h \
  /work/avr-src/include/linux/plist_types.h \
  /work/avr-src/include/linux/hrtimer_types.h \
  /work/avr-src/include/linux/timerqueue_types.h \
  /work/avr-src/include/linux/rbtree_types.h \
  /work/avr-src/include/linux/timer_types.h \
  /work/avr-src/include/linux/seccomp_types.h \
    $(wildcard include/config/SECCOMP) \
  /work/avr-src/include/linux/refcount_types.h \
  /work/avr-src/include/linux/resource.h \
  /work/avr-src/include/uapi/linux/resource.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/resource.h \
  /work/avr-src/include/asm-generic/resource.h \
  /work/avr-src/include/uapi/asm-generic/resource.h \
  /work/avr-src/include/linux/latencytop.h \
  /work/avr-src/include/linux/sched/prio.h \
  /work/avr-src/include/linux/sched/types.h \
  /work/avr-src/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /work/avr-src/include/uapi/linux/signal.h \
  /work/avr-src/arch/arm64/include/asm/signal.h \
  /work/avr-src/arch/arm64/include/uapi/asm/signal.h \
  /work/avr-src/include/asm-generic/signal.h \
  /work/avr-src/include/uapi/asm-generic/signal.h \
  /work/avr-src/include/uapi/asm-generic/signal-defs.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/siginfo.h \
  /work/avr-src/include/uapi/asm-generic/siginfo.h \
  /work/avr-src/include/linux/spinlock.h \
  /work/avr-src/include/linux/irqflags.h \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /work/avr-src/arch/arm64/include/asm/irqflags.h \
  /work/avr-src/include/linux/interrupt_rc.h \
  /work/avr-src/include/linux/processor.h \
  /work/avr-src/include/linux/bottom_half.h \
  /work/avr-src/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  /work/avr-src/include/linux/debug_locks.h \
  /work/avr-src/include/linux/stacktrace.h \
    $(wildcard include/config/ARCH_STACKWALK) \
    $(wildcard include/config/STACKTRACE) \
    $(wildcard include/config/HAVE_RELIABLE_STACKTRACE) \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/mmiowb.h \
  /work/avr-src/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /work/avr-src/arch/arm64/include/asm/spinlock.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/qspinlock.h \
  /work/avr-src/include/asm-generic/qspinlock.h \
    $(wildcard include/config/QUEUED_SPINLOCKS_TRACE_CONTENDED_RELEASE) \
  /work/avr-src/include/linux/tracepoint-defs.h \
    $(wildcard include/config/TRACEPOINTS) \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/qrwlock.h \
  /work/avr-src/include/asm-generic/qrwlock.h \
  /work/avr-src/include/linux/rwlock.h \
  /work/avr-src/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/INLINE_SPIN_LOCK) \
    $(wildcard include/config/INLINE_SPIN_LOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK_BH) \
    $(wildcard include/config/UNINLINE_SPIN_UNLOCK) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/GENERIC_LOCKBREAK) \
  /work/avr-src/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/INLINE_READ_LOCK) \
    $(wildcard include/config/INLINE_WRITE_LOCK) \
    $(wildcard include/config/INLINE_READ_LOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_LOCK_BH) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_READ_TRYLOCK) \
    $(wildcard include/config/INLINE_WRITE_TRYLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_BH) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQRESTORE) \
  /work/avr-src/include/linux/syscall_user_dispatch_types.h \
    $(wildcard include/config/SYSCALL_USER_DISPATCH) \
  /work/avr-src/include/linux/mm_types_task.h \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
  /work/avr-src/arch/arm64/include/asm/tlbbatch.h \
  /work/avr-src/include/linux/netdevice_xmit.h \
    $(wildcard include/config/NET_ACT_MIRRED) \
    $(wildcard include/config/NET_EGRESS) \
    $(wildcard include/config/NF_DUP_NETDEV) \
  /work/avr-src/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /work/avr-src/include/linux/posix-timers_types.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /work/avr-src/include/linux/rseq_types.h \
    $(wildcard include/config/RSEQ) \
    $(wildcard include/config/RSEQ_SLICE_EXTENSION) \
  /work/avr-src/include/linux/irq_work_types.h \
  /work/avr-src/include/linux/workqueue_types.h \
  /work/avr-src/include/linux/seqlock_types.h \
  /work/avr-src/include/linux/kcsan.h \
  /work/avr-src/include/linux/rv.h \
    $(wildcard include/config/RV_LTL_MONITOR) \
    $(wildcard include/config/RV_HA_MONITOR) \
    $(wildcard include/config/RV_REACTORS) \
  /work/avr-src/include/linux/uidgid_types.h \
  /work/avr-src/include/linux/unwind_deferred_types.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/kmap_size.h \
  /work/avr-src/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /work/avr-port/out/native-gki/include/generated/rq-offsets.h \
  /work/avr-src/include/linux/sched/ext.h \
    $(wildcard include/config/EXT_SUB_SCHED) \
    $(wildcard include/config/EXT_GROUP_SCHED) \
  /work/avr-src/include/linux/rhashtable-types.h \
  /work/avr-src/include/linux/alloc_tag.h \
    $(wildcard include/config/MEM_ALLOC_PROFILING_ENABLED_BY_DEFAULT) \
  /work/avr-src/include/linux/codetag.h \
    $(wildcard include/config/CODE_TAGGING) \
  /work/avr-src/include/linux/mutex.h \
  /work/avr-src/include/linux/energy_model.h \
  /work/avr-src/include/linux/kobject.h \
    $(wildcard include/config/UEVENT_HELPER) \
    $(wildcard include/config/DEBUG_KOBJECT_RELEASE) \
  /work/avr-src/include/linux/sysfs.h \
    $(wildcard include/config/SYSFS) \
  /work/avr-src/include/linux/kernfs.h \
    $(wildcard include/config/KERNFS) \
  /work/avr-src/include/linux/idr.h \
  /work/avr-src/include/linux/radix-tree.h \
  /work/avr-src/include/linux/percpu.h \
    $(wildcard include/config/KMALLOC_PARTITION_CACHES) \
    $(wildcard include/config/PAGE_SIZE_4KB) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
  /work/avr-src/include/linux/rcupdate.h \
    $(wildcard include/config/TINY_RCU) \
    $(wildcard include/config/RCU_STRICT_GRACE_PERIOD) \
    $(wildcard include/config/RCU_LAZY) \
    $(wildcard include/config/RCU_STALL_COMMON) \
    $(wildcard include/config/VIRT_XFER_TO_GUEST_WORK) \
    $(wildcard include/config/RCU_NOCB_CPU) \
    $(wildcard include/config/TASKS_RCU_GENERIC) \
    $(wildcard include/config/TASKS_RUDE_RCU) \
    $(wildcard include/config/TREE_RCU) \
    $(wildcard include/config/DEBUG_OBJECTS_RCU_HEAD) \
    $(wildcard include/config/PROVE_RCU) \
    $(wildcard include/config/ARCH_WEAK_RELEASE_ACQUIRE) \
  /work/avr-src/include/linux/context_tracking_irq.h \
    $(wildcard include/config/CONTEXT_TRACKING_IDLE) \
  /work/avr-src/include/linux/rcutree.h \
  /work/avr-src/include/linux/xarray.h \
    $(wildcard include/config/XARRAY_MULTI) \
  /work/avr-src/include/linux/gfp.h \
    $(wildcard include/config/ZONE_DMA) \
    $(wildcard include/config/ZONE_DMA32) \
    $(wildcard include/config/ZONE_DEVICE) \
    $(wildcard include/config/CONTIG_ALLOC) \
  /work/avr-src/include/linux/mmzone.h \
    $(wildcard include/config/ARCH_FORCE_MAX_ORDER) \
    $(wildcard include/config/PAGE_BLOCK_MAX_ORDER) \
    $(wildcard include/config/HAVE_GIGANTIC_FOLIOS) \
    $(wildcard include/config/HUGETLB_PAGE) \
    $(wildcard include/config/HUGETLB_PAGE_OPTIMIZE_VMEMMAP) \
    $(wildcard include/config/CMA) \
    $(wildcard include/config/MEMORY_ISOLATION) \
    $(wildcard include/config/ZSMALLOC) \
    $(wildcard include/config/UNACCEPTED_MEMORY) \
    $(wildcard include/config/IOMMU_SUPPORT) \
    $(wildcard include/config/SWAP) \
    $(wildcard include/config/TRANSPARENT_HUGEPAGE) \
    $(wildcard include/config/LRU_GEN_STATS) \
    $(wildcard include/config/LRU_GEN_WALKS_MMU) \
    $(wildcard include/config/MEMORY_FAILURE) \
    $(wildcard include/config/PAGE_EXTENSION) \
    $(wildcard include/config/DEFERRED_STRUCT_PAGE_INIT) \
    $(wildcard include/config/HAVE_MEMORYLESS_NODES) \
    $(wildcard include/config/SPARSEMEM_EXTREME) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP_PREINIT) \
    $(wildcard include/config/HAVE_ARCH_PFN_VALID) \
  /work/avr-src/include/linux/list_nulls.h \
  /work/avr-src/include/linux/wait.h \
  /work/avr-src/include/linux/seqlock.h \
    $(wildcard include/config/CC_IS_GCC) \
    $(wildcard include/config/GCC_VERSION) \
    $(wildcard include/config/UBSAN_ALIGNMENT) \
  /work/avr-src/include/linux/pageblock-flags.h \
    $(wildcard include/config/HUGETLB_PAGE_SIZE_VARIABLE) \
  /work/avr-src/include/linux/page-flags-layout.h \
  /work/avr-port/out/native-gki/include/generated/bounds.h \
  /work/avr-src/arch/arm64/include/asm/sparsemem.h \
  /work/avr-src/arch/arm64/include/asm/pgtable-prot.h \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_WP) \
  /work/avr-src/arch/arm64/include/asm/rsi.h \
  /work/avr-src/arch/arm64/include/asm/rsi_cmds.h \
  /work/avr-src/include/linux/arm-smccc.h \
    $(wildcard include/config/HAVE_ARM_SMCCC) \
    $(wildcard include/config/ARM) \
  /work/avr-src/include/linux/uuid.h \
  /work/avr-src/arch/arm64/include/asm/rsi_smc.h \
  /work/avr-src/include/linux/mm_types.h \
    $(wildcard include/config/HAVE_ALIGNED_STRUCT_PAGE) \
    $(wildcard include/config/SLAB_OBJ_EXT) \
    $(wildcard include/config/HUGETLB_PMD_PAGE_TABLE_SHARING) \
    $(wildcard include/config/SLAB_FREELIST_HARDENED) \
    $(wildcard include/config/USERFAULTFD) \
    $(wildcard include/config/ANON_VMA_NAME) \
    $(wildcard include/config/PER_VMA_LOCK) \
    $(wildcard include/config/HAVE_ARCH_COMPAT_MMAP_BASES) \
    $(wildcard include/config/MEMBARRIER) \
    $(wildcard include/config/ARCH_HAS_ELF_CORE_EFLAGS) \
    $(wildcard include/config/AIO) \
    $(wildcard include/config/MMU_NOTIFIER) \
    $(wildcard include/config/SPLIT_PMD_PTLOCKS) \
    $(wildcard include/config/IOMMU_MM_DATA) \
    $(wildcard include/config/KSM) \
    $(wildcard include/config/MM_ID) \
    $(wildcard include/config/CORE_DUMP_DEFAULT_ELF_HEADERS) \
  /work/avr-src/include/linux/auxvec.h \
  /work/avr-src/include/uapi/linux/auxvec.h \
  /work/avr-src/arch/arm64/include/uapi/asm/auxvec.h \
  /work/avr-src/include/linux/kref.h \
  /work/avr-src/include/linux/refcount.h \
  /work/avr-src/include/linux/rbtree.h \
  /work/avr-src/include/linux/maple_tree.h \
    $(wildcard include/config/DEBUG_MAPLE_TREE) \
  /work/avr-src/include/linux/rwsem.h \
    $(wildcard include/config/RWSEM_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_RWSEMS) \
  /work/avr-src/include/linux/completion.h \
  /work/avr-src/include/linux/swait.h \
  /work/avr-src/include/linux/uprobes.h \
  /work/avr-src/include/linux/timer.h \
    $(wildcard include/config/DEBUG_OBJECTS_TIMERS) \
    $(wildcard include/config/NO_HZ_COMMON) \
  /work/avr-src/include/linux/ktime.h \
  /work/avr-src/include/linux/jiffies.h \
  /work/avr-src/include/linux/time.h \
  /work/avr-src/include/linux/time32.h \
  /work/avr-src/include/linux/timex.h \
  /work/avr-src/include/uapi/linux/timex.h \
  /work/avr-src/arch/arm64/include/asm/timex.h \
  /work/avr-src/arch/arm64/include/asm/arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER_OOL_WORKAROUND) \
  /work/avr-src/include/clocksource/arm_arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER) \
  /work/avr-src/include/linux/timecounter.h \
  /work/avr-src/include/asm-generic/timex.h \
  /work/avr-src/include/vdso/time32.h \
  /work/avr-src/include/vdso/time.h \
  /work/avr-src/include/vdso/jiffies.h \
  /work/avr-port/out/native-gki/include/generated/timeconst.h \
  /work/avr-src/include/vdso/ktime.h \
  /work/avr-src/include/linux/timekeeping.h \
    $(wildcard include/config/POSIX_AUX_CLOCKS) \
    $(wildcard include/config/GENERIC_CMOS_UPDATE) \
  /work/avr-src/include/linux/clocksource_ids.h \
  /work/avr-src/include/linux/debugobjects.h \
    $(wildcard include/config/DEBUG_OBJECTS) \
    $(wildcard include/config/DEBUG_OBJECTS_FREE) \
  /work/avr-src/arch/arm64/include/asm/uprobes.h \
  /work/avr-src/arch/arm64/include/asm/debug-monitors.h \
  /work/avr-src/arch/arm64/include/asm/esr.h \
  /work/avr-src/arch/arm64/include/asm/probes.h \
    $(wildcard include/config/KPROBES) \
  /work/avr-src/include/linux/workqueue.h \
    $(wildcard include/config/DEBUG_OBJECTS_WORK) \
    $(wildcard include/config/FREEZER) \
    $(wildcard include/config/WQ_WATCHDOG) \
  /work/avr-src/include/linux/percpu_counter.h \
  /work/avr-src/arch/arm64/include/asm/mmu.h \
    $(wildcard include/config/ARM64_E0PD) \
  /work/avr-src/include/linux/page-flags.h \
    $(wildcard include/config/PAGE_IDLE_FLAG) \
    $(wildcard include/config/ARCH_USES_PG_ARCH_2) \
    $(wildcard include/config/ARCH_USES_PG_ARCH_3) \
    $(wildcard include/config/MIGRATION) \
    $(wildcard include/config/DEBUG_KMAP_LOCAL_FORCE_MAP) \
  /work/avr-src/include/linux/local_lock.h \
  /work/avr-src/include/linux/local_lock_internal.h \
  /work/avr-src/include/linux/zswap.h \
    $(wildcard include/config/ZSWAP) \
  /work/avr-src/include/linux/memory_hotplug.h \
    $(wildcard include/config/ARCH_HAS_ADD_PAGES) \
    $(wildcard include/config/MEMORY_HOTREMOVE) \
  /work/avr-src/include/linux/notifier.h \
    $(wildcard include/config/TREE_SRCU) \
  /work/avr-src/include/linux/srcu.h \
    $(wildcard include/config/TINY_SRCU) \
    $(wildcard include/config/NEED_SRCU_NMI_SAFE) \
  /work/avr-src/include/linux/rcu_segcblist.h \
  /work/avr-src/include/linux/srcutree.h \
  /work/avr-src/include/linux/rcu_node_tree.h \
    $(wildcard include/config/RCU_FANOUT) \
    $(wildcard include/config/RCU_FANOUT_LEAF) \
  /work/avr-src/include/linux/topology.h \
    $(wildcard include/config/USE_PERCPU_NUMA_NODE_ID) \
    $(wildcard include/config/SCHED_SMT) \
    $(wildcard include/config/GENERIC_ARCH_TOPOLOGY) \
  /work/avr-src/include/linux/arch_topology.h \
  /work/avr-src/arch/arm64/include/asm/topology.h \
  /work/avr-src/include/asm-generic/topology.h \
  /work/avr-src/include/linux/sched/mm.h \
    $(wildcard include/config/MMU_LAZY_TLB_REFCOUNT) \
    $(wildcard include/config/ARCH_HAS_MEMBARRIER_CALLBACKS) \
    $(wildcard include/config/ARCH_HAS_SYNC_CORE_BEFORE_USERMODE) \
  /work/avr-src/include/linux/sync_core.h \
    $(wildcard include/config/ARCH_HAS_PREPARE_SYNC_CORE_CMD) \
  /work/avr-src/include/linux/sched/coredump.h \
  /work/avr-src/include/linux/uidgid.h \
    $(wildcard include/config/MULTIUSER) \
    $(wildcard include/config/USER_NS) \
  /work/avr-src/include/linux/highuid.h \
  /work/avr-src/include/linux/kobject_ns.h \
  /work/avr-src/include/linux/stat.h \
  /work/avr-src/arch/arm64/include/asm/stat.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/uapi/asm/stat.h \
  /work/avr-src/include/uapi/asm-generic/stat.h \
  /work/avr-src/arch/arm64/include/asm/compat.h \
  /work/avr-src/include/asm-generic/compat.h \
    $(wildcard include/config/COMPAT_FOR_U64_ALIGNMENT) \
  /work/avr-src/include/linux/sched/task_stack.h \
    $(wildcard include/config/STACK_GROWSUP) \
    $(wildcard include/config/DEBUG_STACK_USAGE) \
  /work/avr-src/include/uapi/linux/magic.h \
  /work/avr-src/include/linux/kasan.h \
    $(wildcard include/config/KASAN_STACK) \
    $(wildcard include/config/KASAN_VMALLOC) \
  /work/avr-src/include/uapi/linux/stat.h \
  /work/avr-src/include/linux/sched/cpufreq.h \
    $(wildcard include/config/CPU_FREQ) \
  /work/avr-src/include/linux/sched/topology.h \
    $(wildcard include/config/SCHED_CLUSTER) \
    $(wildcard include/config/SCHED_MC) \
    $(wildcard include/config/CPU_FREQ_GOV_SCHEDUTIL) \
  /work/avr-src/include/linux/sched/idle.h \
  /work/avr-src/include/linux/sched/sd_flags.h \
  /work/avr-src/include/linux/ioport.h \
  /work/avr-src/include/linux/klist.h \
  /work/avr-src/include/linux/pm.h \
    $(wildcard include/config/VT_CONSOLE_SLEEP) \
    $(wildcard include/config/CXL_SUSPEND) \
    $(wildcard include/config/PM_CLK) \
    $(wildcard include/config/PM_GENERIC_DOMAINS) \
  /work/avr-src/include/linux/device/bus.h \
    $(wildcard include/config/ACPI) \
  /work/avr-src/include/linux/device/class.h \
  /work/avr-src/include/linux/device/devres.h \
    $(wildcard include/config/HAS_IOMEM) \
  /work/avr-src/include/linux/device/driver.h \
  /work/avr-src/include/linux/module.h \
    $(wildcard include/config/MODULES_TREE_LOOKUP) \
    $(wildcard include/config/STACKTRACE_BUILD_ID) \
    $(wildcard include/config/ARCH_USES_CFI_TRAPS) \
    $(wildcard include/config/KALLSYMS) \
    $(wildcard include/config/BPF_EVENTS) \
    $(wildcard include/config/DEBUG_INFO_BTF_MODULES) \
    $(wildcard include/config/EVENT_TRACING) \
    $(wildcard include/config/MODULE_UNLOAD) \
    $(wildcard include/config/CONSTRUCTORS) \
    $(wildcard include/config/FUNCTION_ERROR_INJECTION) \
    $(wildcard include/config/MITIGATION_RETPOLINE) \
    $(wildcard include/config/MODULE_SIG) \
  /work/avr-src/include/linux/buildid.h \
    $(wildcard include/config/VMCORE_INFO) \
  /work/avr-src/include/linux/kmod.h \
  /work/avr-src/include/linux/umh.h \
  /work/avr-src/include/linux/sysctl.h \
    $(wildcard include/config/SYSCTL) \
  /work/avr-src/include/uapi/linux/sysctl.h \
  /work/avr-src/include/linux/elf.h \
    $(wildcard include/config/ARCH_HAVE_EXTRA_ELF_NOTES) \
    $(wildcard include/config/ARCH_USE_GNU_PROPERTY) \
    $(wildcard include/config/ARCH_HAVE_ELF_PROT) \
  /work/avr-src/arch/arm64/include/asm/elf.h \
    $(wildcard include/config/COMPAT_VDSO) \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/user.h \
  /work/avr-src/include/asm-generic/user.h \
  /work/avr-src/include/uapi/linux/elf.h \
  /work/avr-src/include/uapi/linux/elf-em.h \
  /work/avr-src/include/linux/fs.h \
    $(wildcard include/config/FANOTIFY_ACCESS_PERMISSIONS) \
    $(wildcard include/config/FS_POSIX_ACL) \
    $(wildcard include/config/CGROUP_WRITEBACK) \
    $(wildcard include/config/IMA) \
    $(wildcard include/config/FILE_LOCKING) \
    $(wildcard include/config/FSNOTIFY) \
    $(wildcard include/config/EPOLL) \
    $(wildcard include/config/FS_DAX) \
    $(wildcard include/config/BLOCK) \
    $(wildcard include/config/UNICODE) \
  /work/avr-src/include/linux/fs/super.h \
  /work/avr-src/include/linux/fs/super_types.h \
    $(wildcard include/config/QUOTA) \
    $(wildcard include/config/FS_ENCRYPTION) \
    $(wildcard include/config/FS_VERITY) \
  /work/avr-src/include/linux/fs_dirent.h \
  /work/avr-src/include/linux/errseq.h \
  /work/avr-src/include/linux/list_lru.h \
  /work/avr-src/include/linux/shrinker.h \
    $(wildcard include/config/SHRINKER_DEBUG) \
  /work/avr-src/include/linux/list_bl.h \
  /work/avr-src/include/linux/bit_spinlock.h \
  /work/avr-src/include/linux/percpu-rwsem.h \
  /work/avr-src/include/linux/rcuwait.h \
  /work/avr-src/include/linux/sched/signal.h \
    $(wildcard include/config/SCHED_AUTOGROUP) \
    $(wildcard include/config/BSD_PROCESS_ACCT) \
    $(wildcard include/config/TASKSTATS) \
  /work/avr-src/include/linux/rculist.h \
    $(wildcard include/config/PROVE_RCU_LIST) \
  /work/avr-src/include/linux/signal.h \
    $(wildcard include/config/DYNAMIC_SIGFRAME) \
    $(wildcard include/config/PROC_FS) \
  /work/avr-src/include/linux/sched/jobctl.h \
  /work/avr-src/include/linux/sched/task.h \
    $(wildcard include/config/GKI_DYNAMIC_TASK_STRUCT_SIZE) \
    $(wildcard include/config/GKI_TASK_STRUCT_VENDOR_SIZE_MAX) \
    $(wildcard include/config/HAVE_EXIT_THREAD) \
    $(wildcard include/config/ARCH_WANTS_DYNAMIC_TASK_STRUCT) \
    $(wildcard include/config/HAVE_ARCH_THREAD_STRUCT_WHITELIST) \
  /work/avr-src/include/linux/uaccess.h \
    $(wildcard include/config/ARCH_HAS_SUBPAGE_FAULTS) \
    $(wildcard include/config/ARCH_MEMORY_ORDER_TSO) \
    $(wildcard include/config/HARDENED_USERCOPY) \
  /work/avr-src/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/FAULT_INJECTION_USERCOPY) \
  /work/avr-src/include/linux/nospec.h \
  /work/avr-src/include/linux/ucopysize.h \
    $(wildcard include/config/HARDENED_USERCOPY_DEFAULT_ON) \
  /work/avr-src/arch/arm64/include/asm/uaccess.h \
    $(wildcard include/config/CC_HAS_ASM_GOTO_OUTPUT) \
  /work/avr-src/arch/arm64/include/asm/kernel-pgtable.h \
    $(wildcard include/config/RELOCATABLE) \
  /work/avr-src/arch/arm64/include/asm/asm-extable.h \
  /work/avr-src/arch/arm64/include/asm/mte.h \
  /work/avr-src/arch/arm64/include/asm/extable.h \
    $(wildcard include/config/BPF_JIT) \
  /work/avr-src/include/asm-generic/access_ok.h \
    $(wildcard include/config/ALTERNATE_USER_ADDRESS_SPACE) \
  /work/avr-src/include/linux/cred.h \
  /work/avr-src/include/linux/capability.h \
  /work/avr-src/include/uapi/linux/capability.h \
  /work/avr-src/include/linux/key.h \
    $(wildcard include/config/KEY_NOTIFICATIONS) \
    $(wildcard include/config/NET) \
  /work/avr-src/include/linux/assoc_array.h \
    $(wildcard include/config/ASSOCIATIVE_ARRAY) \
  /work/avr-src/include/linux/sched/user.h \
    $(wildcard include/config/VFIO_PCI_ZDEV_KVM) \
    $(wildcard include/config/IOMMUFD) \
    $(wildcard include/config/SECRETMEM) \
    $(wildcard include/config/WATCH_QUEUE) \
  /work/avr-src/include/linux/pid.h \
  /work/avr-src/include/linux/posix-timers.h \
  /work/avr-src/include/linux/alarmtimer.h \
    $(wildcard include/config/RTC_CLASS) \
  /work/avr-src/include/linux/hrtimer.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
    $(wildcard include/config/TIME_LOW_RES) \
    $(wildcard include/config/TIMERFD) \
  /work/avr-src/include/linux/hrtimer_rearm.h \
    $(wildcard include/config/HRTIMER_REARM_DEFERRED) \
  /work/avr-src/include/linux/timerqueue.h \
  /work/avr-src/include/linux/rcuref.h \
  /work/avr-src/include/linux/rcu_sync.h \
  /work/avr-src/include/linux/quota.h \
    $(wildcard include/config/QUOTA_NETLINK_INTERFACE) \
  /work/avr-src/include/uapi/linux/dqblk_xfs.h \
  /work/avr-src/include/linux/dqblk_v1.h \
  /work/avr-src/include/linux/dqblk_v2.h \
  /work/avr-src/include/linux/dqblk_qtree.h \
  /work/avr-src/include/linux/projid.h \
  /work/avr-src/include/uapi/linux/quota.h \
  /work/avr-src/include/linux/unicode.h \
  /work/avr-src/include/linux/dcache.h \
  /work/avr-src/include/linux/rculist_bl.h \
  /work/avr-src/include/linux/lockref.h \
    $(wildcard include/config/ARCH_USE_CMPXCHG_LOCKREF) \
  /work/avr-src/include/linux/stringhash.h \
    $(wildcard include/config/DCACHE_WORD_ACCESS) \
  /work/avr-src/include/linux/hash.h \
    $(wildcard include/config/HAVE_ARCH_HASH) \
  /work/avr-src/include/linux/vfsdebug.h \
    $(wildcard include/config/DEBUG_VFS) \
  /work/avr-src/include/linux/wait_bit.h \
  /work/avr-src/include/linux/kdev_t.h \
  /work/avr-src/include/uapi/linux/kdev_t.h \
  /work/avr-src/include/linux/path.h \
  /work/avr-src/include/linux/semaphore.h \
  /work/avr-src/include/linux/fcntl.h \
    $(wildcard include/config/ARCH_32BIT_OFF_T) \
  /work/avr-src/include/uapi/linux/fcntl.h \
  /work/avr-src/arch/arm64/include/uapi/asm/fcntl.h \
  /work/avr-src/include/uapi/asm-generic/fcntl.h \
  /work/avr-src/include/uapi/linux/openat2.h \
  /work/avr-src/include/linux/migrate_mode.h \
  /work/avr-src/include/linux/delayed_call.h \
  /work/avr-src/include/linux/ioprio.h \
  /work/avr-src/include/linux/sched/rt.h \
    $(wildcard include/config/RT_SOFTIRQ_AWARE_SCHED) \
  /work/avr-src/include/linux/iocontext.h \
    $(wildcard include/config/BLK_ICQ) \
  /work/avr-src/include/uapi/linux/ioprio.h \
  /work/avr-src/include/linux/mount.h \
  /work/avr-src/include/linux/mnt_idmapping.h \
  /work/avr-src/include/linux/slab.h \
    $(wildcard include/config/FAILSLAB) \
    $(wildcard include/config/KFENCE) \
    $(wildcard include/config/SLUB_TINY) \
    $(wildcard include/config/SLUB_DEBUG) \
    $(wildcard include/config/KMALLOC_PARTITION_RANDOM) \
    $(wildcard include/config/KMALLOC_PARTITION_TYPED) \
    $(wildcard include/config/SLAB_BUCKETS) \
  /work/avr-src/include/linux/percpu-refcount.h \
  /work/avr-src/include/linux/rw_hint.h \
  /work/avr-src/include/linux/file_ref.h \
  /work/avr-src/include/uapi/linux/fs.h \
  /work/avr-src/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/PPC64) \
  /work/avr-src/include/linux/rbtree_latch.h \
  /work/avr-src/include/linux/error-injection.h \
  /work/avr-src/include/asm-generic/error-injection.h \
  /work/avr-src/include/linux/dynamic_debug.h \
  /work/avr-src/arch/arm64/include/asm/module.h \
  /work/avr-src/include/asm-generic/module.h \
    $(wildcard include/config/HAVE_MOD_ARCH_SPECIFIC) \
  /work/avr-src/include/linux/device-id/acpi.h \
  /work/avr-src/include/linux/device-id/of.h \
  /work/avr-port/out/native-gki/arch/arm64/include/generated/asm/device.h \
  /work/avr-src/include/asm-generic/device.h \
  /work/avr-src/include/linux/pm_wakeup.h \
  /work/avr-src/include/linux/cpuhotplug.h \
    $(wildcard include/config/HOTPLUG_CORE_SYNC_DEAD) \
  /work/avr-src/include/linux/cpuhplock.h \
  /work/avr-src/include/linux/cpu_smt.h \
    $(wildcard include/config/HOTPLUG_SMT) \
  /work/avr-src/include/linux/of.h \
    $(wildcard include/config/OF_DYNAMIC) \
    $(wildcard include/config/SPARC) \
    $(wildcard include/config/OF_PROMTREE) \
    $(wildcard include/config/OF_KOBJ) \
    $(wildcard include/config/OF_NUMA) \
    $(wildcard include/config/OF_OVERLAY) \
  /work/avr-src/include/linux/property.h \
  /work/avr-src/include/linux/fwnode.h \
  /work/avr-src/include/linux/platform_device.h \
    $(wildcard include/config/SUSPEND) \
    $(wildcard include/config/HIBERNATE_CALLBACKS) \
    $(wildcard include/config/SUPERH) \
  /work/avr-src/include/linux/device-id/platform.h \
  ../sched-walt/include/linux/sched/walt.h \
    $(wildcard include/config/SCHED_WALT) \
    $(wildcard include/config/SCHED_WALT_NR_CPUS) \
    $(wildcard include/config/SCHED_MOTO_UNFAIR) \
  include/linux/thermal_pause.h \
    $(wildcard include/config/QTI_CPU_PAUSE_COOLING_DEVICE) \
  /work/avr-src/include/linux/thermal.h \
    $(wildcard include/config/THERMAL_OF) \
    $(wildcard include/config/THERMAL_DEBUGFS) \
    $(wildcard include/config/THERMAL) \
  /work/avr-src/include/uapi/linux/thermal.h \

thermal_pause.o: $(deps_thermal_pause.o)

$(deps_thermal_pause.o):
#SYMVER thermal_pause_notifier_unregister 0xdd98a2ad
#SYMVER thermal_paused_cpumask 0x9cf1d65a
#SYMVER thermal_pause_notifier_register 0xdd98a2ad
