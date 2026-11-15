// SPDX-License-Identifier: GPL-2.0-only
/* Full SDEI source, native module pins/CPU calls, recorded firmware calls only. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/arm_sdei.h>
#include <linux/arm-smccc.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include "drivers/firmware/qcom/qcom_scm.h"
struct fixture {
 struct platform_device *pdev;int initial_refs,fail_step,step,unregister_busy,hvc_ret,scm_ret;
 bool owned[2],enabled[2],unregister_fatal;u32 hvc_fn;u64 hvc_arg0,hvc_arg1;
 unsigned int registrations,enables,disables,unregistrations,scm_calls;
 struct qcom_scm_desc desc;
};
static struct fixture *backend;
static int event_index(u32 event){return event==0 ? 0 : 1;}
static int fake_register(u32 event,sdei_event_callback *callback,void *arg)
{
 struct fixture *f=backend;int i=event_index(event);(void)callback;(void)arg;f->registrations++;
 if(++f->step==f->fail_step)return -EIO;
 if(f->owned[i])return -EBUSY;f->owned[i]=true;return 0;
}
static int fake_enable(u32 event)
{
 struct fixture *f=backend;f->enables++;if(++f->step==f->fail_step)return -EIO;
 f->enabled[event_index(event)]=true;return 0;
}
static int fake_disable(u32 event){backend->disables++;backend->enabled[event_index(event)]=false;return 0;}
static int fake_unregister(u32 event)
{
 struct fixture *f=backend;f->unregistrations++;
 if(f->unregister_busy){f->unregister_busy--;return -EINPROGRESS;}
 if(f->unregister_fatal)return -EIO;f->owned[event_index(event)]=false;return 0;
}
static void fake_hvc_4(unsigned long fn,unsigned long arg0,unsigned long arg1,struct arm_smccc_res *res)
{backend->hvc_fn=fn;backend->hvc_arg0=arg0;backend->hvc_arg1=arg1;res->a0=backend->hvc_ret;}
static void fake_hvc_2(unsigned long fn,struct arm_smccc_res *res){fake_hvc_4(fn,0,0,res);}
#define PICK_HVC(_1,_2,_3,_4,NAME,...) NAME
#undef arm_smccc_1_1_hvc
#define arm_smccc_1_1_hvc(...) PICK_HVC(__VA_ARGS__,fake_hvc_4,invalid_hvc_3,fake_hvc_2,invalid_hvc_1)(__VA_ARGS__)
#define sdei_event_register fake_register
#define sdei_event_enable fake_enable
#define sdei_event_disable fake_disable
#define sdei_event_unregister fake_unregister
#define qcom_sdei_shared_reset avr_test_qcom_sdei_shared_reset
#undef module_platform_driver
#define module_platform_driver(driver)
#undef pr_fmt
#include "../../ports/qcom-watchdog/qcom_sdei.c"
/* A selected native SCM method with its transport recorded. */
static struct {struct device *dev;} scm_fixture;
static typeof(scm_fixture) *__scm;
static int qcom_scm_call(struct device *dev,const struct qcom_scm_desc *desc,struct qcom_scm_res *res)
{(void)dev;(void)res;backend->scm_calls++;backend->desc=*desc;return backend->scm_ret;}
#define qcom_scm_sec_wdog_deactivate avr_test_scm_secure_disable
int avr_test_scm_secure_disable(void);
#include "generated-scm.h"
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);if(!f)return -ENOMEM;test->priv=f;
 f->pdev=platform_device_register_simple("avr-sdei-recorded",PLATFORM_DEVID_AUTO,NULL,0);
 if(IS_ERR(f->pdev))return PTR_ERR(f->pdev);backend=f;f->initial_refs=module_refcount(THIS_MODULE);
 scm_fixture.dev=&f->pdev->dev;__scm=NULL;return 0;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;f->unregister_fatal=false;f->unregister_busy=0;
 qcom_sdei_driver_remove(f->pdev);KUNIT_EXPECT_EQ(test,registered_events,0ul);
 KUNIT_EXPECT_FALSE(test,event_owner_pinned);KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),f->initial_refs);
 __scm=NULL;platform_device_unregister(f->pdev);backend=NULL;
}
static void every_registration_stage_failure_unwinds_and_allows_retry(struct kunit *test)
{
 struct fixture *f=test->priv;int step;
 for(step=1;step<=4;step++){
  f->step=0;f->fail_step=step;
  KUNIT_EXPECT_EQ(test,qcom_sdei_driver_probe(f->pdev),-EIO);
  KUNIT_EXPECT_EQ(test,registered_events,0ul);KUNIT_EXPECT_FALSE(test,f->owned[0]);KUNIT_EXPECT_FALSE(test,f->owned[1]);
  KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),f->initial_refs);
 }
 f->fail_step=0;KUNIT_EXPECT_EQ(test,qcom_sdei_driver_probe(f->pdev),0);
 KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),f->initial_refs+1);
}
static void successful_registration_is_pinned_until_both_events_are_gone(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,qcom_sdei_driver_probe(f->pdev),0);
 KUNIT_EXPECT_EQ(test,registered_events,3ul);KUNIT_EXPECT_TRUE(test,event_owner_pinned);
 KUNIT_EXPECT_EQ(test,qcom_sdei_driver_probe(f->pdev),-EBUSY);
 qcom_sdei_driver_remove(f->pdev);qcom_sdei_driver_remove(f->pdev);
 KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),f->initial_refs);KUNIT_EXPECT_EQ(test,registered_events,0ul);
}
static void inprogress_unregister_is_drained_before_owner_release(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,qcom_sdei_driver_probe(f->pdev),0);f->unregister_busy=2;
 qcom_sdei_driver_remove(f->pdev);KUNIT_EXPECT_EQ(test,f->unregistrations,4u);
 KUNIT_EXPECT_EQ(test,registered_events,0ul);KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),f->initial_refs);
}
static void failed_unregister_retains_callbacks_and_the_module_pin(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,qcom_sdei_driver_probe(f->pdev),0);f->unregister_fatal=true;
 qcom_sdei_driver_remove(f->pdev);KUNIT_EXPECT_EQ(test,registered_events,3ul);KUNIT_EXPECT_TRUE(test,event_owner_pinned);
 KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),f->initial_refs+1);KUNIT_EXPECT_EQ(test,qcom_sdei_driver_probe(f->pdev),-EBUSY);
 f->unregister_fatal=false;qcom_sdei_driver_remove(f->pdev);KUNIT_EXPECT_EQ(test,registered_events,0ul);
}
static void shared_reset_reports_the_actual_recorded_hypervisor_error(struct kunit *test)
{
 struct fixture *f=test->priv;f->hvc_ret=-EOPNOTSUPP;
 KUNIT_EXPECT_EQ(test,avr_test_qcom_sdei_shared_reset(),-EOPNOTSUPP);KUNIT_EXPECT_EQ(test,f->hvc_fn,(u32)SDEI_1_0_FN_SDEI_SHARED_RESET);
 f->hvc_ret=0;KUNIT_EXPECT_EQ(test,avr_test_qcom_sdei_shared_reset(),0);
}
static void signal_and_reset_use_exact_native_firmware_arguments(struct kunit *test)
{
 struct fixture *f=test->priv;u64 mpidr=0x1234567880000000ull;
 KUNIT_EXPECT_EQ(test,qcom_sdei_event_signal(mpidr),0);KUNIT_EXPECT_EQ(test,f->hvc_fn,0xc400002fu);
 KUNIT_EXPECT_EQ(test,f->hvc_arg0,0ull);KUNIT_EXPECT_EQ(test,f->hvc_arg1,mpidr);
 KUNIT_EXPECT_EQ(test,psci_system_reset(),0);KUNIT_EXPECT_EQ(test,f->hvc_fn,(u32)PSCI_0_2_FN_SYSTEM_RESET);
}
static void unavailable_scm_defers_without_a_transport_call(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_EXPECT_EQ(test,avr_test_scm_secure_disable(),-EPROBE_DEFER);KUNIT_EXPECT_EQ(test,f->scm_calls,0u);
}
static void secure_disable_descriptor_and_transport_errors_are_preserved(struct kunit *test)
{
 struct fixture *f=test->priv;__scm=&scm_fixture;f->scm_ret=-EACCES;
 KUNIT_EXPECT_EQ(test,avr_test_scm_secure_disable(),-EACCES);KUNIT_EXPECT_EQ(test,f->scm_calls,1u);
 KUNIT_EXPECT_EQ(test,f->desc.svc,(u32)QCOM_SCM_SVC_BOOT);KUNIT_EXPECT_EQ(test,f->desc.cmd,7u);
 KUNIT_EXPECT_EQ(test,f->desc.owner,(u32)ARM_SMCCC_OWNER_SIP);KUNIT_EXPECT_EQ(test,f->desc.arginfo,(u32)QCOM_SCM_ARGS(1));KUNIT_EXPECT_EQ(test,f->desc.args[0],1ull);
 f->scm_ret=0;KUNIT_EXPECT_EQ(test,avr_test_scm_secure_disable(),0);
}
static struct kunit_case cases[]={KUNIT_CASE(every_registration_stage_failure_unwinds_and_allows_retry),KUNIT_CASE(successful_registration_is_pinned_until_both_events_are_gone),KUNIT_CASE(inprogress_unregister_is_drained_before_owner_release),KUNIT_CASE(failed_unregister_retains_callbacks_and_the_module_pin),KUNIT_CASE(shared_reset_reports_the_actual_recorded_hypervisor_error),KUNIT_CASE(signal_and_reset_use_exact_native_firmware_arguments),KUNIT_CASE(unavailable_scm_defers_without_a_transport_call),KUNIT_CASE(secure_disable_descriptor_and_transport_errors_are_preserved),{}};
static struct kunit_suite suite={.name="avr-sdei",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
