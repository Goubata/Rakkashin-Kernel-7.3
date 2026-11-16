# SPDX-License-Identifier: GPL-2.0-only
# canoe_perf: QCOM_DMABUF_HEAPS=m (SYSTEM, SYSTEM_SECURE, CMA, CARVEOUT, PAGE_POOL_REFILL;
# DEFERRED_FREE=m), QCOM_MEM_BUF(_DEV,_MSGQ)=m with GH, QCOM_LAZY_MAPPING=m,
# GH_MEM_NOTIFIER=m, GH_RM_HEAP_MANAGER=m (+ mem-prot).
obj-m += deferred-free-helper.o msm_dma_iommu_mapping.o mem-prot.o gh_mem_notifier.o \
	gh_rm_heap_manager.o mem_buf_msgq.o mem_buf_dev.o mem_buf.o qcom_dma_heaps.o
mem_buf_msgq-y := mem-buf-msgq.o
mem_buf_dev-y := mem-buf-dev.o mem-buf-ids.o mem_buf_dma_buf.o mem-buf-dev-gh.o
mem_buf-y := mem-buf.o mem-buf-gh.o
qcom_dma_heaps-y := qcom_dma_heap.o qcom_dt_parser.o qcom_sg_ops.o qcom_system_heap.o \
	qcom_dynamic_page_pool.o qcom_secure_system_heap.o qcom_dma_heap_secure_utils.o \
	qcom_cma_heap.o qcom_carveout_heap.o
ccflags-y += -I$(src) -I$(src)/include -I$(src)/../gunyah/include -I$(src)/../gunyah-rm \
	-I$(src)/../qti-fence/include -I$(src)/../gunyah-rm/include -I$(src)/../gunyah-core/include -I$(src)/../qcom-iommu-util/include -I$(src)/../secure-buffer/include \
	-I$(srctree)
ccflags-y += -DCONFIG_QCOM_DMABUF_HEAPS_MODULE=1 -DCONFIG_QCOM_DMABUF_HEAPS_SYSTEM=1 \
	-DCONFIG_QCOM_DMABUF_HEAPS_SYSTEM_SECURE=1 -DCONFIG_QCOM_DMABUF_HEAPS_CMA=1 \
	-DCONFIG_QCOM_DMABUF_HEAPS_CARVEOUT=1 -DCONFIG_QCOM_DMABUF_HEAPS_PAGE_POOL_REFILL=1 \
	-DCONFIG_QCOM_DMABUF_HEAPS_DEFERRED_FREE_MODULE=1 -DCONFIG_QCOM_MEM_BUF_MODULE=1 \
	-DCONFIG_QCOM_MEM_BUF_DEV_MODULE=1 -DCONFIG_QCOM_MEM_BUF_DEV_GH=1 -DCONFIG_QCOM_MEM_BUF_GH=1 \
	-DCONFIG_QCOM_MEM_BUF_MSGQ_MODULE=1 -DCONFIG_QCOM_LAZY_MAPPING_MODULE=1 \
	-DCONFIG_GH_MEM_NOTIFIER_MODULE=1 -DCONFIG_GH_RM_HEAP_MANAGER_MODULE=1 -DCONFIG_MEM_PROT_MODULE=1 \
	-DCONFIG_GH_RM_DRV_MODULE=1 -DCONFIG_GH_MSGQ_MODULE=1 -DCONFIG_GH_DBL_MODULE=1 -DCONFIG_GUNYAH_MODULE=1 \
	-DCONFIG_GH_ARM64_DRV_MODULE=1 -DCONFIG_QCOM_SECURE_BUFFER_MODULE=1 -DCONFIG_QTVM_WITH_AVF=1 \
	-Wno-missing-prototypes

CFLAGS_mem-buf-msgq.o := -I$(src)
CFLAGS_mem-buf-gh.o := -I$(src)
CFLAGS_mem-buf-dev.o := -I$(src)
KBUILD_EXTRA_SYMBOLS += $(src)/../gunyah/Module.symvers $(src)/../gunyah-core/Module.symvers \
	$(src)/../gunyah-rm/Module.symvers $(src)/../secure-buffer/Module.symvers \
	$(src)/../qcom-iommu-util/Module.symvers
