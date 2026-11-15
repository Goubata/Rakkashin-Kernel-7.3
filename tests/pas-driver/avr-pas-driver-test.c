// SPDX-License-Identifier: GPL-2.0-only
/* Exact driver methods; only external hardware/firmware primitives recorded.
 * Critical-error panic is NOT replaced; failing cleanup cases use noncritical
 * mode to exercise the returning error path without panicking the guest. */
#include <kunit/test.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/firmware/qcom/qcom_pas.h>
#include <linux/iommu.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/qcom/mdt_loader.h>
#include "qcom_q6v5.h"
#include "qcom_common.h"
#include "qcom_pil_info.h"
enum fault {F_PREP,F_XO,F_AGGRE,F_ARC_ENABLE,F_DTB_MAP,F_DTB_AUTH,F_MAIN_LOAD,F_MAIN_MAP,F_MAIN_AUTH,F_WAIT,F_MAIN_STOP,F_DTB_STOP,F_REQUEST,F_DTB_LOAD};
struct recording {
	unsigned long faults;
	int clocks[2],regs[2];
	bool live[2],maps[2],prepared,bad,inside_auth,disabled_inside_auth;
	bool early_handover,auth_handover,critical_armed;
	bool ready;
	unsigned int stops[2],metas[2],firmware_releases;
	int trace[256];unsigned int traces;
	struct qcom_q6v5 *q6;
	const struct firmware *dtb;
};
static struct recording *rec;
static int critical_mode;
module_param(critical_mode,int,0400);
static bool fail(unsigned int fault){return rec->faults & BIT(fault);}
static void event(int value){if(rec->traces<ARRAY_SIZE(rec->trace))rec->trace[rec->traces]=value;rec->traces++;}
static int image_index(u32 id){return id==65?1:0;}
static int clk_index(struct clk *clk){return clk==(void *)0x201?1:0;}
static int reg_index(struct regulator *reg){return reg==(void *)0x101?1:0;}
static int record_prepare(struct qcom_q6v5 *q6)
{
	event(1);if(fail(F_PREP))return -EIO;
	rec->prepared=true;rec->q6=q6;q6->running=true;q6->handover_issued=false;
	if(rec->early_handover){q6->handover_issued=true;q6->handover(q6);}
	return 0;
}
static int record_unprepare(struct qcom_q6v5 *q6){event(2);rec->prepared=false;return !q6->handover_issued;}
static int record_wait(struct qcom_q6v5 *q6,int timeout){event(3);return fail(F_WAIT)?-ETIMEDOUT:0;}
static int record_request_stop(struct qcom_q6v5 *q6,struct qcom_sysmon *mon){event(4);q6->running=false;return 0;}
static int record_clock_on(struct clk *clk)
{
	int i=clk_index(clk);event(10+i);
	if(fail(i?F_AGGRE:F_XO))return -EIO;
	if(++rec->clocks[i]!=1)rec->bad=true;
	return 0;
}
static void record_clock_off(struct clk *clk)
{
	int i=clk_index(clk);event(12+i);
	if(rec->critical_armed)pr_err("AVR_PAS_CRITICAL_CLEANUP clock\n");
	if(rec->inside_auth)rec->disabled_inside_auth=true;
	if(--rec->clocks[i]!=0)rec->bad=true;
}
static int record_reg_on(struct regulator *reg)
{
	int i=reg_index(reg);event(20+i);
	if(i==1 && fail(F_ARC_ENABLE))return -EIO;
	if(++rec->regs[i]!=1)rec->bad=true;
	return 0;
}
static int record_reg_off(struct regulator *reg)
{
	int i=reg_index(reg);event(22+i);
	if(rec->critical_armed)pr_err("AVR_PAS_CRITICAL_CLEANUP regulator\n");
	if(rec->inside_auth)rec->disabled_inside_auth=true;
	if(--rec->regs[i]!=0)rec->bad=true;
	return 0;
}
static int record_voltage(struct regulator *reg,int min,int max){if(min!=0 && min!=64)rec->bad=true;return 0;}
static int record_load(struct regulator *reg,int value){if(value)rec->bad=true;return 0;}
static int record_map(struct iommu_domain *d,unsigned long iova,phys_addr_t phys,size_t size,int prot,gfp_t gfp)
{
	int i=phys==0xa4a00000?1:0;event(30+i);
	if(fail(i?F_DTB_MAP:F_MAIN_MAP))return -EIO;
	if(rec->maps[i])rec->bad=true;rec->maps[i]=true;return 0;
}
static size_t record_unmap(struct iommu_domain *d,unsigned long iova,size_t size)
{
	int i=iova==0xa4a00000?1:0;event(32+i);
	if(rec->critical_armed)pr_err("AVR_PAS_CRITICAL_CLEANUP unmap\n");
	if(rec->live[0] || rec->live[1] || !rec->maps[i])rec->bad=true;
	rec->maps[i]=false;return size;
}
static int record_auth(struct qcom_pas_context *ctx)
{
	int i=image_index(ctx->pas_id);int ret=fail(i?F_DTB_AUTH:F_MAIN_AUTH)?-EKEYREJECTED:0;
	event(40+i);rec->live[i]=true;rec->inside_auth=true;
	if(!i && rec->auth_handover){rec->q6->handover_issued=true;rec->q6->handover(rec->q6);}
	rec->inside_auth=false;
	if(ret && rec->critical_armed)pr_info("AVR_PAS_CRITICAL_FAILED_AUTH image=%d live=%d,%d maps=%d,%d\n",i,rec->live[0],rec->live[1],rec->maps[0],rec->maps[1]);
	return ret;
}
static int record_shutdown(u32 id)
{
	int i=image_index(id);event(50+i);rec->stops[i]++;
	if(i && rec->live[0])rec->bad=true;
	if(fail(i?F_DTB_STOP:F_MAIN_STOP)) {
		if(rec->critical_armed)pr_info("AVR_PAS_CRITICAL_FAILED_SHUTDOWN live=%d,%d maps=%d,%d\n",rec->live[0],rec->live[1],rec->maps[0],rec->maps[1]);
		return -EIO;
	}
	rec->live[i]=false;return 0;
}
static int record_mdt(struct qcom_pas_context *ctx,const struct firmware *fw,const char *name,phys_addr_t *reloc)
{
	int i=image_index(ctx->pas_id);event(60+i);ctx->ptr=(void *)(unsigned long)(i+1);
	return fail(i?F_DTB_LOAD:F_MAIN_LOAD)?-EIO:0;
}
static void record_metadata(struct qcom_pas_context *ctx)
{
	if(ctx && ctx->ptr){int i=image_index(ctx->pas_id);event(62+i);if(rec->critical_armed)pr_err("AVR_PAS_CRITICAL_CLEANUP metadata\n");rec->metas[i]++;ctx->ptr=NULL;}
}
static int record_pil(const char *name,phys_addr_t base,size_t size){return 0;}
static int record_firmware(const struct firmware **fw,const char *name,struct device *dev)
{
	if(fail(F_REQUEST)){*fw=NULL;return -ENOENT;}*fw=rec->dtb;return 0;
}
static void record_firmware_release(const struct firmware *fw){if(fw!=rec->dtb)rec->bad=true;else rec->firmware_releases++;}
static int record_irq_state(unsigned int irq,enum irqchip_irq_state which,bool *state)
{
	if(which!=IRQCHIP_STATE_LINE_LEVEL || irq<10 || irq>12)return -EINVAL;
	*state=irq==12 ? rec->ready : false;
	return 0;
}
#define irq_get_irqchip_state record_irq_state
#define qcom_q6v5_prepare record_prepare
#define qcom_q6v5_unprepare record_unprepare
#define qcom_q6v5_wait_for_start record_wait
#define qcom_q6v5_request_stop record_request_stop
#define clk_prepare_enable record_clock_on
#define clk_disable_unprepare record_clock_off
#define regulator_enable record_reg_on
#define regulator_disable record_reg_off
#define regulator_set_voltage record_voltage
#define regulator_set_load record_load
#define iommu_map record_map
#define iommu_unmap record_unmap
#define qcom_pas_prepare_and_auth_reset record_auth
#define qcom_pas_shutdown record_shutdown
#define qcom_mdt_pas_load record_mdt
#define qcom_pas_metadata_release record_metadata
#define qcom_pil_info_store record_pil
#define request_firmware record_firmware
#define release_firmware record_firmware_release
#undef module_platform_driver
#define module_platform_driver(...)
#include "../../ports/qcom-pas-alor/qcom_q6v5_pas.c"
struct fixture {
	struct recording rec;
	struct qcom_pas pas;
	struct rproc rproc;
	struct qcom_pas_context main_ctx,dtb_ctx;
	struct firmware main_fw,dtb_fw;
	u8 region[128];
};
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
	if(!f)return -ENOMEM;
	rec=&f->rec;rec->dtb=&f->dtb_fw;rec->ready=true;
	f->pas.dev=root_device_register("avr_pas_driver_unit");
	if(IS_ERR(f->pas.dev))return PTR_ERR(f->pas.dev);
	f->pas.rproc=&f->rproc;f->rproc.priv=&f->pas;
	f->rproc.firmware="soccp.mbn";f->rproc.name="avr-soccp-unit";f->rproc.has_iommu=true;
	f->pas.pas_id=f->main_ctx.pas_id=51;f->pas.dtb_pas_id=f->dtb_ctx.pas_id=65;
	f->pas.pas_ctx=&f->main_ctx;f->pas.dtb_pas_ctx=&f->dtb_ctx;
	f->pas.dtb_firmware_name="soccp_dtb.mbn";f->pas.info_name="soccp";
	f->pas.mem_phys=f->pas.mem_reloc=0xa4400000;f->pas.mem_size=6*1024*1024;
	f->pas.dtb_mem_phys=0xa4a00000;f->pas.dtb_mem_size=256*1024;
	f->pas.xo=(void *)0x200;f->pas.aggre2_clk=(void *)0x201;
	f->pas.arc_rails=true;f->pas.critical_fw=true;
	f->pas.q6v5.handover=qcom_pas_handover;
	f->pas.q6v5.fatal_irq=10;f->pas.q6v5.stop_irq=11;f->pas.q6v5.ready_irq=12;
	mutex_init(&f->pas.rails.lock);mutex_init(&f->pas.proxy.lock);
	f->pas.proxy.ops=&qcom_pas_proxy_ops;f->pas.proxy.ctx=&f->pas;
	f->pas.rails.rail[0].reg=(void *)0x100;f->pas.rails.rail[1].reg=(void *)0x101;
	f->pas.rails.rail[0].level=f->pas.rails.rail[1].level=64;
	test->priv=f;return 0;
}
static void cleanup(struct kunit *test){struct fixture *f=test->priv;root_device_unregister(f->pas.dev);rec=NULL;}
static void clean(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	KUNIT_EXPECT_FALSE(test,rec->bad);KUNIT_EXPECT_FALSE(test,rec->prepared);
	KUNIT_EXPECT_FALSE(test,avr_pas_images_busy(&f->pas.images));KUNIT_EXPECT_EQ(test,f->pas.proxy.owned,0UL);
	for(i=0;i<2;i++){KUNIT_EXPECT_EQ(test,rec->clocks[i],0);KUNIT_EXPECT_EQ(test,rec->regs[i],0);KUNIT_EXPECT_FALSE(test,rec->maps[i]);KUNIT_EXPECT_FALSE(test,rec->live[i]);}
}
static int load(struct fixture *f){return qcom_pas_load(&f->rproc,&f->main_fw);}
static void success_and_early_handover(struct kunit *test)
{
	struct fixture *f=test->priv;
	if(critical_mode) {
		KUNIT_ASSERT_EQ(test,load(f),0);
		if(critical_mode==1) {
			rec->faults=BIT(F_DTB_AUTH);rec->critical_armed=true;
			pr_info("AVR_PAS_CRITICAL_ARMED mode=auth\n");
			qcom_pas_start(&f->rproc);
		} else if(critical_mode==2) {
			KUNIT_ASSERT_EQ(test,qcom_pas_start(&f->rproc),0);
			rec->faults=BIT(F_MAIN_STOP);rec->critical_armed=true;
			pr_info("AVR_PAS_CRITICAL_ARMED mode=shutdown\n");
			qcom_pas_stop(&f->rproc);
		} else {KUNIT_FAIL(test,"invalid critical mode");return;}
		pr_err("AVR_PAS_CRITICAL_RETURNED\n");
		KUNIT_FAIL(test,"critical path returned instead of panic");return;
	}
	rec->early_handover=true;rec->auth_handover=true;
	KUNIT_ASSERT_EQ(test,load(f),0);KUNIT_EXPECT_EQ(test,rec->firmware_releases,1U);KUNIT_EXPECT_PTR_EQ(test,f->pas.dtb_firmware,NULL);
	KUNIT_EXPECT_EQ(test,qcom_pas_start(&f->rproc),0);
	KUNIT_EXPECT_FALSE(test,rec->disabled_inside_auth);KUNIT_EXPECT_EQ(test,f->pas.proxy.owned,0UL);
	KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),0);KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),0);
	KUNIT_EXPECT_EQ(test,rec->metas[0],1U);KUNIT_EXPECT_EQ(test,rec->metas[1],1U);clean(test);
}
static void forward_failure_matrix(struct kunit *test)
{
	struct fixture *f=test->priv;
	unsigned int faults[]={F_PREP,F_XO,F_AGGRE,F_ARC_ENABLE,F_DTB_MAP,F_DTB_AUTH,F_MAIN_LOAD,F_MAIN_MAP,F_MAIN_AUTH,F_WAIT};int i,expected;
	f->pas.critical_fw=false; /* Do not execute the intentional panic policy. */
	for(i=0;i<ARRAY_SIZE(faults);i++) {
		rec->faults=0;KUNIT_ASSERT_EQ(test,load(f),0);rec->faults=BIT(faults[i]);
		expected=faults[i]==F_WAIT?-ETIMEDOUT:(faults[i]==F_DTB_AUTH || faults[i]==F_MAIN_AUTH)?-EKEYREJECTED:-EIO;
		KUNIT_EXPECT_EQ(test,qcom_pas_start(&f->rproc),expected);
		KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),0);clean(test);
	}
}
static void main_load_failure_stops_dtb(struct kunit *test)
{
	struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,load(f),0);rec->faults=BIT(F_MAIN_LOAD);
	KUNIT_EXPECT_EQ(test,qcom_pas_start(&f->rproc),-EIO);
	KUNIT_EXPECT_EQ(test,rec->stops[0],0U);KUNIT_EXPECT_EQ(test,rec->stops[1],1U);clean(test);
}
static void failed_stop_retains_resources_until_retry(struct kunit *test)
{
	struct fixture *f=test->priv;f->pas.critical_fw=false;
	KUNIT_ASSERT_EQ(test,load(f),0);KUNIT_ASSERT_EQ(test,qcom_pas_start(&f->rproc),0);
	rec->faults=BIT(F_MAIN_STOP);KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),-EIO);
	KUNIT_EXPECT_TRUE(test,rec->maps[0]);KUNIT_EXPECT_TRUE(test,rec->maps[1]);KUNIT_EXPECT_TRUE(test,rec->prepared);
	KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),-EBUSY);KUNIT_EXPECT_EQ(test,load(f),-EBUSY);
	rec->faults=0;KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),0);KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),0);clean(test);
}
static void failed_dtb_stop_does_not_repeat_main(struct kunit *test)
{
	struct fixture *f=test->priv;f->pas.critical_fw=false;
	KUNIT_ASSERT_EQ(test,load(f),0);KUNIT_ASSERT_EQ(test,qcom_pas_start(&f->rproc),0);
	rec->faults=BIT(F_DTB_STOP);KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),-EIO);
	KUNIT_EXPECT_FALSE(test,rec->live[0]);KUNIT_EXPECT_TRUE(test,rec->live[1]);KUNIT_EXPECT_TRUE(test,rec->maps[0]);
	rec->faults=0;KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),0);KUNIT_EXPECT_EQ(test,rec->stops[0],1U);KUNIT_EXPECT_EQ(test,rec->stops[1],2U);clean(test);
}
static void startup_and_shutdown_failure_preserves_metadata(struct kunit *test)
{
	struct fixture *f=test->priv;f->pas.critical_fw=false;
	KUNIT_ASSERT_EQ(test,load(f),0);rec->faults=BIT(F_MAIN_LOAD)|BIT(F_DTB_STOP);
	KUNIT_EXPECT_EQ(test,qcom_pas_start(&f->rproc),-EIO);KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),-EBUSY);
	KUNIT_EXPECT_NOT_NULL(test,f->dtb_ctx.ptr);KUNIT_EXPECT_NOT_NULL(test,f->main_ctx.ptr);
	KUNIT_EXPECT_TRUE(test,rec->maps[1]);KUNIT_EXPECT_TRUE(test,rec->prepared);
	rec->faults=0;KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),0);KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),0);
	KUNIT_EXPECT_PTR_EQ(test,f->dtb_ctx.ptr,NULL);KUNIT_EXPECT_PTR_EQ(test,f->main_ctx.ptr,NULL);clean(test);
}
static void dtb_source_reference_released_on_copy_failure(struct kunit *test)
{
	struct fixture *f=test->priv;rec->faults=BIT(F_DTB_LOAD);
	KUNIT_EXPECT_EQ(test,load(f),-EIO);KUNIT_EXPECT_EQ(test,rec->firmware_releases,1U);
	KUNIT_EXPECT_PTR_EQ(test,f->pas.dtb_firmware,NULL);KUNIT_EXPECT_PTR_EQ(test,f->pas.firmware,NULL);KUNIT_EXPECT_PTR_EQ(test,f->dtb_ctx.ptr,NULL);
	clean(test);
}
static void firmware_request_failure_has_no_reference(struct kunit *test)
{
	struct fixture *f=test->priv;rec->faults=BIT(F_REQUEST);
	KUNIT_EXPECT_EQ(test,load(f),-ENOENT);KUNIT_EXPECT_EQ(test,rec->firmware_releases,0U);
	KUNIT_EXPECT_PTR_EQ(test,f->pas.firmware,NULL);KUNIT_EXPECT_PTR_EQ(test,f->pas.dtb_firmware,NULL);clean(test);
}
static void no_iommu_alor_path(struct kunit *test)
{
	struct fixture *f=test->priv;f->rproc.has_iommu=false;
	KUNIT_ASSERT_EQ(test,load(f),0);KUNIT_ASSERT_EQ(test,qcom_pas_start(&f->rproc),0);
	KUNIT_EXPECT_TRUE(test,f->pas.images.mapped[0]);KUNIT_EXPECT_TRUE(test,f->pas.images.mapped[1]);
	KUNIT_EXPECT_FALSE(test,rec->maps[0]);KUNIT_EXPECT_FALSE(test,rec->maps[1]);
	KUNIT_EXPECT_EQ(test,qcom_pas_stop(&f->rproc),0);KUNIT_EXPECT_EQ(test,qcom_pas_unprepare(&f->rproc),0);clean(test);
}
static void real_remoteproc_framework_boot_cycles(struct kunit *test)
{
	struct fixture *f=test->priv;
	struct rproc *rproc;
	int ret,i;bool added=false;
	rproc=rproc_alloc(f->pas.dev,"avr-soccp-framework",&qcom_pas_minidump_ops,"avr-test/soccp.mbn",0);
	KUNIT_ASSERT_NOT_NULL(test,rproc);
	/* priv is driver-owned; keep the existing managed fixture, not a copy of mutexes. */
	rproc->priv=&f->pas;rproc->auto_boot=false;rproc->has_iommu=false;
	rproc_coredump_set_elf_info(rproc,ELFCLASS64,EM_NONE);
	f->pas.rproc=rproc;
	ret=rproc_add(rproc);KUNIT_EXPECT_EQ(test,ret,0);
	if(ret)goto out;
	added=true;
	for(i=0;i<2;i++) {
		ret=rproc_boot(rproc);KUNIT_EXPECT_EQ(test,ret,0);
		if(ret)break;
		KUNIT_EXPECT_EQ(test,rproc->state,RPROC_RUNNING);
		KUNIT_EXPECT_EQ(test,atomic_read(&rproc->power),1);
		ret=rproc_shutdown(rproc);KUNIT_EXPECT_EQ(test,ret,0);
		if(ret)break;
		KUNIT_EXPECT_EQ(test,rproc->state,RPROC_OFFLINE);
		KUNIT_EXPECT_EQ(test,atomic_read(&rproc->power),0);
		clean(test);
	}
	if(rproc->state==RPROC_RUNNING || rproc->state==RPROC_ATTACHED)rproc_shutdown(rproc);
	KUNIT_EXPECT_EQ(test,rec->firmware_releases,2U);
	KUNIT_EXPECT_EQ(test,rec->metas[0],2U);KUNIT_EXPECT_EQ(test,rec->metas[1],2U);
	clean(test);
out:
	if(added)rproc_del(rproc);
	rproc_free(rproc);f->pas.rproc=&f->rproc;
}
static void real_framework_bootloader_attach_and_shutdown(struct kunit *test)
{
	struct fixture *f=test->priv;
	struct rproc *rproc=rproc_alloc(f->pas.dev,"avr-soccp-attached",&qcom_pas_minidump_ops,"avr-test/soccp.mbn",0);
	int ret;bool added=false;
	KUNIT_ASSERT_NOT_NULL(test,rproc);
	rproc->priv=&f->pas;rproc->auto_boot=false;rproc->has_iommu=false;rproc->state=RPROC_DETACHED;f->pas.rproc=rproc;
	rec->live[0]=rec->live[1]=true; /* Already started by the modeled bootloader. */
	ret=rproc_add(rproc);KUNIT_EXPECT_EQ(test,ret,0);if(ret)goto out;added=true;
	ret=rproc_boot(rproc);KUNIT_EXPECT_EQ(test,ret,0);
	if(!ret) {
		KUNIT_EXPECT_EQ(test,rproc->state,RPROC_ATTACHED);
		KUNIT_EXPECT_EQ(test,atomic_read(&rproc->power),1);
		KUNIT_EXPECT_TRUE(test,f->pas.images.needs_shutdown[0]);KUNIT_EXPECT_TRUE(test,f->pas.images.needs_shutdown[1]);
		KUNIT_EXPECT_EQ(test,f->pas.proxy.owned,0UL);
		KUNIT_EXPECT_EQ(test,rproc_shutdown(rproc),0);
		KUNIT_EXPECT_EQ(test,rproc->state,RPROC_OFFLINE);KUNIT_EXPECT_EQ(test,atomic_read(&rproc->power),0);
		KUNIT_EXPECT_EQ(test,rec->stops[0],1U);KUNIT_EXPECT_EQ(test,rec->stops[1],1U);
		KUNIT_EXPECT_EQ(test,rec->firmware_releases,0U);KUNIT_EXPECT_EQ(test,rec->metas[0]+rec->metas[1],0U);
		clean(test);
	}
out:
	if(added)rproc_del(rproc);rproc_free(rproc);f->pas.rproc=&f->rproc;
}
static void real_framework_not_ready_falls_back_to_cold_boot(struct kunit *test)
{
	struct fixture *f=test->priv;
	struct rproc *rproc=rproc_alloc(f->pas.dev,"avr-soccp-fallback",&qcom_pas_minidump_ops,"avr-test/soccp.mbn",0);
	int ret;bool added=false;
	KUNIT_ASSERT_NOT_NULL(test,rproc);
	rproc->priv=&f->pas;rproc->auto_boot=false;rproc->has_iommu=false;rproc->state=RPROC_DETACHED;f->pas.rproc=rproc;
	rec->ready=false;
	ret=rproc_add(rproc);KUNIT_EXPECT_EQ(test,ret,0);if(ret)goto out;added=true;
	KUNIT_EXPECT_EQ(test,rproc_boot(rproc),-EINVAL);
	KUNIT_EXPECT_EQ(test,rproc->state,RPROC_OFFLINE);KUNIT_EXPECT_EQ(test,atomic_read(&rproc->power),0);
	KUNIT_EXPECT_FALSE(test,avr_pas_images_busy(&f->pas.images));
	rec->ready=true;
	ret=rproc_boot(rproc);KUNIT_EXPECT_EQ(test,ret,0);
	if(!ret){KUNIT_EXPECT_EQ(test,rproc->state,RPROC_RUNNING);KUNIT_EXPECT_EQ(test,rproc_shutdown(rproc),0);clean(test);}
out:
	if(added)rproc_del(rproc);rproc_free(rproc);f->pas.rproc=&f->rproc;
}
static void device_address_conversion_does_not_wrap(struct kunit *test)
{
	struct fixture *f=test->priv;bool io=false;u64 base=0x100000000ULL;
	f->pas.mem_reloc=base;f->pas.mem_size=sizeof(f->region);f->pas.mem_region=(void __iomem *)f->region;
	KUNIT_EXPECT_PTR_EQ(test,qcom_pas_da_to_va(&f->rproc,base+8,16,&io),f->region+8);KUNIT_EXPECT_TRUE(test,io);
	io=false;KUNIT_EXPECT_PTR_EQ(test,qcom_pas_da_to_va(&f->rproc,base+0x100000000ULL,1,&io),NULL);KUNIT_EXPECT_FALSE(test,io);
	KUNIT_EXPECT_PTR_EQ(test,qcom_pas_da_to_va(&f->rproc,base+8,SIZE_MAX,NULL),NULL);
	KUNIT_EXPECT_PTR_EQ(test,qcom_pas_da_to_va(&f->rproc,base-1,1,NULL),NULL);
}
static struct kunit_case cases[]={KUNIT_CASE(success_and_early_handover),KUNIT_CASE(forward_failure_matrix),KUNIT_CASE(main_load_failure_stops_dtb),KUNIT_CASE(failed_stop_retains_resources_until_retry),KUNIT_CASE(failed_dtb_stop_does_not_repeat_main),KUNIT_CASE(startup_and_shutdown_failure_preserves_metadata),KUNIT_CASE(dtb_source_reference_released_on_copy_failure),KUNIT_CASE(firmware_request_failure_has_no_reference),KUNIT_CASE(device_address_conversion_does_not_wrap),KUNIT_CASE(no_iommu_alor_path),KUNIT_CASE(real_remoteproc_framework_boot_cycles),KUNIT_CASE(real_framework_bootloader_attach_and_shutdown),KUNIT_CASE(real_framework_not_ready_falls_back_to_cold_boot),{}};
static struct kunit_suite suite={.name="avr-pas-driver-source",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
