/* Exact selected constructor instructions from pinned HAL. */
.text

.global ctor_original
.type ctor_original, %function
ctor_original:
 stp x26, x30, [sp, #-16]!
 mov x9, x0
 mov x26, x1
 .inst 0x52800548
 .inst 0x3dc00120
 .inst 0x39030348
 .inst 0xf840d128
 .inst 0x3c8c1340
 .inst 0xf80ce348
 .inst 0x39035b5f
 ldp x26, x30, [sp], #16
 ret
.size ctor_original, .-ctor_original

.global ctor_candidate
.type ctor_candidate, %function
ctor_candidate:
 stp x26, x30, [sp, #-16]!
 mov x9, x0
 mov x26, x1
 .inst 0x52800448
 .inst 0x3dc00120
 .inst 0x39030348
 .inst 0xf840d128
 .inst 0x3c8c1340
 .inst 0xf80ce348
 .inst 0x39035b5f
 ldp x26, x30, [sp], #16
 ret
.size ctor_candidate, .-ctor_candidate
