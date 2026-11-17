/* AVR: the phone gh_irq_lend.ko does not import gunyah_qtvm_* (built without
 * QTVM_WITH_AVF, unlike gh_rm_drv); undo the canoe_perf define for this object. */
#undef CONFIG_QTVM_WITH_AVF
