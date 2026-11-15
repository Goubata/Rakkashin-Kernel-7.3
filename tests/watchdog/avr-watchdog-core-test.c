// SPDX-License-Identifier: GPL-2.0-only
/* Full production watchdog core with native private IRQ descriptors, notifier
 * chains, sysfs, timers and kthread. Only hardware operations are recorded. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/kthread.h>
#include <linux/suspend.h>
#include <linux/panic_notifier.h>
#include <linux/cpu_pm.h>
#include <linux/reboot.h>
#include <linux/kernfs.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <soc/qcom/watchdog.h>
enum failure {F_IRQ=1,F_PM,F_BARK,F_BITE,F_PANIC,F_RESTART,F_CPU_PM,F_ENABLE,F_RESET,F_SYSFS,F_THREAD};
struct fixture {
 struct platform_device *pdev;struct msm_watchdog_data *dd;int irq,fail_at,scm_ret;
 unsigned long seen;atomic_t resets;unsigned int enables,disables;wait_queue_head_t events;struct kunit *test;
};
static struct fixture *backend;
static bool fail_here(int point){backend->seen|=BIT(point);return backend->fail_at==point;}
static int test_irq(struct device *dev,unsigned int irq,irq_handler_t fn,unsigned long flags,const char *name,void *data)
{return fail_here(F_IRQ) ? -EIO : devm_request_irq(dev,irq,fn,flags,name,data);}
static int test_pm(struct notifier_block *nb){return fail_here(F_PM) ? -EIO : register_pm_notifier(nb);}
static int test_panic(struct atomic_notifier_head *head,struct notifier_block *nb)
{return fail_here(F_PANIC) ? -EIO : atomic_notifier_chain_register(head,nb);}
static int test_restart(struct notifier_block *nb){return fail_here(F_RESTART) ? -EIO : register_restart_handler(nb);}
static int test_cpu_pm(struct notifier_block *nb){return fail_here(F_CPU_PM) ? -EIO : cpu_pm_register_notifier(nb);}
static int test_sysfs(struct kobject *obj,const struct attribute_group *group)
{return fail_here(F_SYSFS) ? -EIO : sysfs_create_group(obj,group);}
static struct task_struct *test_thread(int (*fn)(void *),void *arg,const char *name)
{return fail_here(F_THREAD) ? ERR_PTR(-ENOMEM) : kthread_create(fn,arg,"%s",name);}
static int recorded_scm_disable(void){return backend->scm_ret;}
static int recorded_shared_reset(void){KUNIT_FAIL(backend->test,"unexpected firmware reset path");return -EPERM;}
int avr_core_register(struct platform_device *,struct msm_watchdog_data *,char *);
void avr_core_remove(struct platform_device *);
void avr_core_bite(void);
int avr_core_suspend(struct device *);
int avr_core_resume(struct device *);
#define qcom_wdt_register avr_core_register
#define qcom_wdt_remove avr_core_remove
#define qcom_wdt_trigger_bite avr_core_bite
#define qcom_wdt_pet_suspend avr_core_suspend
#define qcom_wdt_pet_resume avr_core_resume
#define qcom_sdei_shared_reset recorded_shared_reset
#define qcom_scm_sec_wdog_deactivate recorded_scm_disable
#define devm_request_irq test_irq
#define register_pm_notifier test_pm
#define atomic_notifier_chain_register test_panic
#define register_restart_handler test_restart
#define cpu_pm_register_notifier test_cpu_pm
#define sysfs_create_group test_sysfs
#undef kthread_create
#define kthread_create(fn,arg,name) test_thread(fn,arg,name)
#include "../../ports/qcom-watchdog/qcom_wdt_core.c"
#undef devm_request_irq
#undef atomic_notifier_chain_register
#undef register_pm_notifier
#undef register_restart_handler
#undef cpu_pm_register_notifier
#undef sysfs_create_group
static void ram_irq_nop(struct irq_data *data){(void)data;}
static int ram_irq_type(struct irq_data *data,unsigned int type){(void)data;(void)type;return 0;}
static struct irq_chip ram_chip={.name="avr-wdt-private",.irq_mask=ram_irq_nop,.irq_unmask=ram_irq_nop,.irq_ack=ram_irq_nop,.irq_set_type=ram_irq_type};
static int hw_bark(u32 ms,struct msm_watchdog_data *dd){(void)ms;(void)dd;return fail_here(F_BARK) ? -EIO : 0;}
static int hw_bite(u32 ms,struct msm_watchdog_data *dd){(void)ms;(void)dd;return fail_here(F_BITE) ? -EIO : 0;}
static int hw_enable(u32 val,struct msm_watchdog_data *dd){(void)val;(void)dd;backend->enables++;return fail_here(F_ENABLE) ? -EIO : 0;}
static int hw_disable(struct msm_watchdog_data *dd){(void)dd;backend->disables++;return 0;}
static int hw_reset(struct msm_watchdog_data *dd)
{(void)dd;if(fail_here(F_RESET))return -EIO;atomic_inc(&backend->resets);wake_up_all(&backend->events);return 0;}
static int hw_status(struct msm_watchdog_data *dd){(void)dd;return 0;}
static struct qcom_wdt_ops hw_ops={.set_bark_time=hw_bark,.set_bite_time=hw_bite,.enable_wdt=hw_enable,.disable_wdt=hw_disable,.reset_wdt=hw_reset,.show_wdt_status=hw_status};
static int new_data(struct fixture *f)
{
 f->dd=kzalloc(sizeof(*f->dd),GFP_KERNEL);if(!f->dd)return -ENOMEM;
 f->dd->dev=&f->pdev->dev;f->dd->ops=&hw_ops;return 0;
}
static irqreturn_t dummy_irq(int irq,void *data){(void)irq;(void)data;return IRQ_HANDLED;}
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);struct resource res={.flags=IORESOURCE_IRQ};int ret;
 if(!f)return -ENOMEM;test->priv=f;f->test=test;init_waitqueue_head(&f->events);
 f->irq=irq_alloc_descs(-1,32,1,NUMA_NO_NODE);if(f->irq<0)return f->irq;
 irq_set_chip_and_handler(f->irq,&ram_chip,handle_simple_irq);irq_clear_status_flags(f->irq,IRQ_NOREQUEST);
 ret=irq_set_irq_type(f->irq,IRQ_TYPE_LEVEL_HIGH);if(ret){irq_free_descs(f->irq,1);return ret;}
 res.start=res.end=f->irq;f->pdev=platform_device_register_simple("avr-wdt-core-ram",PLATFORM_DEVID_AUTO,&res,1);
 if(IS_ERR(f->pdev)){irq_free_descs(f->irq,1);return PTR_ERR(f->pdev);}
 backend=f;ret=new_data(f);if(ret){platform_device_unregister(f->pdev);irq_free_descs(f->irq,1);}return ret;
}
static void expect_native_registrations_gone(struct kunit *test)
{
 struct fixture *f=test->priv;int ret;struct kernfs_node *kn;
 ret=request_irq(f->irq,dummy_irq,IRQF_NO_SUSPEND,"avr-wdt-reclaim",f);
 KUNIT_EXPECT_EQ(test,ret,0);if(!ret)free_irq(f->irq,f);
 KUNIT_EXPECT_EQ(test,atomic_notifier_chain_unregister(&panic_notifier_list,&f->dd->panic_blk),-ENOENT);
 KUNIT_EXPECT_EQ(test,unregister_pm_notifier(&qcom_wdt_notif_block),-ENOENT);
 KUNIT_EXPECT_EQ(test,unregister_restart_handler(&f->dd->restart_blk),-ENOENT);
 KUNIT_EXPECT_EQ(test,cpu_pm_unregister_notifier(&f->dd->wdog_cpu_pm_nb),-ENOENT);
 kn=sysfs_get_dirent(f->pdev->dev.kobj.sd,"disable");KUNIT_EXPECT_PTR_EQ(test,kn,NULL);if(kn)kernfs_put(kn);
 kn=sysfs_get_dirent(f->pdev->dev.kobj.sd,"wakeup_enable");KUNIT_EXPECT_PTR_EQ(test,kn,NULL);if(kn)kernfs_put(kn);
 KUNIT_EXPECT_FALSE(test,f->dd->irq_registered||f->dd->pm_registered||f->dd->panic_registered||f->dd->restart_registered||f->dd->cpu_pm_registered||f->dd->timers_initialized||f->dd->sysfs_created||f->dd->dump_registered);
 KUNIT_EXPECT_PTR_EQ(test,f->dd->watchdog_task,NULL);
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;f->fail_at=0;
 if(platform_get_drvdata(f->pdev))avr_core_remove(f->pdev);
 if(f->dd){
  if(f->dd->dump_registered)qcom_wdt_remove_dump(f->dd);
  if(!f->dd->dump_registered)kfree(f->dd);
 }
 platform_device_unregister(f->pdev);irq_free_descs(f->irq,1);backend=NULL;
}
static void full_native_core_pets_and_removes_without_callback_leases(struct kunit *test)
{
 struct fixture *f=test->priv;int before;
 KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-test"),0);
 KUNIT_EXPECT_TRUE(test,f->dd->ready);KUNIT_EXPECT_TRUE(test,f->dd->irq_registered);
 before=atomic_read(&f->resets);qcom_wdt_pet_task_wakeup(&f->dd->pet_timer);
 KUNIT_EXPECT_NE(test,wait_event_timeout(f->events,atomic_read(&f->resets)>before,2*HZ),0ul);
 avr_core_remove(f->pdev);expect_native_registrations_gone(test);
}
static void every_init_registration_or_hardware_failure_unwinds(struct kunit *test)
{
 struct fixture *f=test->priv;int point;
 for(point=F_IRQ;point<=F_SYSFS;point++){
  f->fail_at=point;f->seen=0;
  KUNIT_EXPECT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-fault"),-EIO);
  KUNIT_EXPECT_TRUE(test,f->seen&BIT(point));expect_native_registrations_gone(test);
  KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev),NULL);
  kfree(f->dd);f->dd=NULL;KUNIT_ASSERT_EQ(test,new_data(f),0);
 }
}
static void thread_creation_error_leaves_no_published_driver(struct kunit *test)
{
 struct fixture *f=test->priv;f->fail_at=F_THREAD;
 KUNIT_EXPECT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-thread-fault"),-ENOMEM);
 KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev),NULL);expect_native_registrations_gone(test);
}
static void suspended_counter_still_releases_irq_and_notifiers_on_remove(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-suspend"),0);
 KUNIT_ASSERT_EQ(test,avr_core_suspend(&f->pdev->dev),0);KUNIT_EXPECT_FALSE(test,f->dd->enabled);
 KUNIT_EXPECT_TRUE(test,f->dd->irq_registered);avr_core_remove(f->pdev);expect_native_registrations_gone(test);
}
static void permanent_disable_does_not_restart_on_resume(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int enables;
 KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-disable"),0);
 KUNIT_ASSERT_EQ(test,qcom_wdt_disable_set(&f->pdev->dev,&dev_attr_disable,"1",1),1l);
 enables=f->enables;KUNIT_EXPECT_EQ(test,avr_core_resume(&f->pdev->dev),0);KUNIT_EXPECT_EQ(test,f->enables,enables);
 KUNIT_EXPECT_EQ(test,qcom_wdt_user_pet_enabled_set(&f->pdev->dev,&dev_attr_user_pet_enabled,"1",1),-ESHUTDOWN);
 KUNIT_EXPECT_EQ(test,qcom_wdt_disable_set(&f->pdev->dev,&dev_attr_disable,"1",1),1l);
 avr_core_remove(f->pdev);expect_native_registrations_gone(test);
}
static void secure_disable_failure_preserves_an_active_controller(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-scm-fault"),0);f->scm_ret=-EACCES;
 KUNIT_EXPECT_EQ(test,qcom_wdt_disable_set(&f->pdev->dev,&dev_attr_disable,"1",1),-EACCES);
 KUNIT_EXPECT_TRUE(test,f->dd->enabled);KUNIT_EXPECT_TRUE(test,f->dd->irq_registered);
 avr_core_remove(f->pdev);expect_native_registrations_gone(test);
}
static void not_ready_sysfs_rejects_hardware_mutations(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int enables;KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-closing"),0);
 smp_store_release(&f->dd->ready,false);enables=f->enables;
 KUNIT_EXPECT_EQ(test,qcom_wdt_disable_set(&f->pdev->dev,&dev_attr_disable,"1",1),-ENODEV);
 KUNIT_EXPECT_EQ(test,wakeup_enable_store(&f->pdev->dev,&dev_attr_wakeup_enable,"1",1),-ENODEV);
 KUNIT_EXPECT_EQ(test,qcom_wdt_user_pet_enabled_set(&f->pdev->dev,&dev_attr_user_pet_enabled,"1",1),-ENODEV);
 KUNIT_EXPECT_EQ(test,f->enables,enables);avr_core_remove(f->pdev);expect_native_registrations_gone(test);
}
static void remove_stops_a_worker_waiting_for_userspace_pet(struct kunit *test)
{
 struct fixture *f=test->priv;int i;KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-user-wait"),0);
 WRITE_ONCE(f->dd->user_pet_enabled,true);WRITE_ONCE(f->dd->user_pet_complete,false);qcom_wdt_pet_task_wakeup(&f->dd->pet_timer);
 for(i=0;i<100 && !READ_ONCE(f->dd->thread_start);i++)msleep(1);
 KUNIT_EXPECT_NE(test,READ_ONCE(f->dd->thread_start),0ull);
 avr_core_remove(f->pdev);expect_native_registrations_gone(test);
}
static void repeated_register_remove_reuses_the_platform_irq_mapping(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 for(i=0;i<3;i++){
  KUNIT_ASSERT_EQ(test,avr_core_register(f->pdev,f->dd,"avr-wdt-repeat"),0);avr_core_remove(f->pdev);avr_core_remove(f->pdev);
  expect_native_registrations_gone(test);KUNIT_EXPECT_NOT_NULL(test,irq_get_irq_data(f->irq));
  kfree(f->dd);f->dd=NULL;KUNIT_ASSERT_EQ(test,new_data(f),0);
 }
}
static struct kunit_case cases[]={KUNIT_CASE(full_native_core_pets_and_removes_without_callback_leases),KUNIT_CASE(every_init_registration_or_hardware_failure_unwinds),KUNIT_CASE(thread_creation_error_leaves_no_published_driver),KUNIT_CASE(suspended_counter_still_releases_irq_and_notifiers_on_remove),KUNIT_CASE(permanent_disable_does_not_restart_on_resume),KUNIT_CASE(secure_disable_failure_preserves_an_active_controller),KUNIT_CASE(not_ready_sysfs_rejects_hardware_mutations),KUNIT_CASE(remove_stops_a_worker_waiting_for_userspace_pet),KUNIT_CASE(repeated_register_remove_reuses_the_platform_irq_mapping),{}};
static struct kunit_suite suite={.name="avr-watchdog-core",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("Full watchdog core with native private IRQ/notifier/thread lifetime");
