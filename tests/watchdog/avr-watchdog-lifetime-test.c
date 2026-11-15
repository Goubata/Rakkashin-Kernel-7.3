// SPDX-License-Identifier: GPL-2.0-only
/* Exact dump/free methods, native devres and the real unready minidump provider.
 * Faulted backend scenarios are explicitly recorded; no hardware watchdog. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/overflow.h>
#include <soc/qcom/watchdog.h>
#include <soc/qcom/minidump.h>
struct fixture {
 struct device *dev;void *soc;struct md_region entry;bool native,owned;
 int add_error,remove_error;unsigned int adds,removes,frees;
};
static struct fixture *backend;
static void record_free(const void *data)
{
 if(backend && backend->soc==data){backend->frees++;backend->soc=NULL;}
 kfree(data);
}
#define kfree record_free
#include "generated-mmio.h"
#include "generated-free.h"
#undef kfree
static int backend_add(const struct md_region *entry)
{
 struct fixture *f=backend;int ret;f->adds++;
 if(f->add_error)return f->add_error;
 if(f->native){ret=msm_minidump_add_region(entry);if(ret<0)return ret;}
 else ret=0;
 f->entry=*entry;f->owned=true;return ret;
}
static int backend_remove(const struct md_region *entry)
{
 struct fixture *f=backend;int ret=0;f->removes++;
 if(f->remove_error){if(f->remove_error==-ENOENT)f->owned=false;return f->remove_error;}
 if(f->native){ret=msm_minidump_remove_region(entry);if(ret)return ret;}
 f->owned=false;return 0;
}
#define msm_minidump_add_region backend_add
#define msm_minidump_remove_region backend_remove
#include "generated-dump.h"
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);struct qcom_soc_wdt *soc;
 if(!f)return -ENOMEM;test->priv=f;f->dev=root_device_register("avr-wdt-dump-ram");
 if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 soc=kzalloc(sizeof(*soc),GFP_KERNEL);if(!soc){root_device_unregister(f->dev);return -ENOMEM;}
 f->soc=soc;soc->core.dev=f->dev;soc->core.bark_time=11000;backend=f;return 0;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;
 f->remove_error=0;
 if(soc){KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);qcom_soc_wdt_free(soc);}
 KUNIT_EXPECT_FALSE(test,f->owned);KUNIT_EXPECT_EQ(test,f->frees,1u);
 if(f->dev)root_device_unregister(f->dev);backend=NULL;
}
static void actual_pending_provider_removes_before_allocation_release(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;f->native=true;
 KUNIT_ASSERT_FALSE(test,msm_minidump_enabled());KUNIT_ASSERT_GE(test,qcom_wdt_add_dump(&soc->core),0);
 KUNIT_EXPECT_TRUE(test,soc->core.dump_registered);KUNIT_EXPECT_STREQ(test,f->entry.name,"KWDOGDATA");
 KUNIT_EXPECT_EQ(test,f->entry.id,0u);KUNIT_EXPECT_EQ(test,f->entry.virt_addr,(u64)(uintptr_t)&soc->core);
 KUNIT_EXPECT_EQ(test,f->entry.phys_addr,(u64)virt_to_phys(&soc->core));KUNIT_EXPECT_EQ(test,f->entry.size,(u64)sizeof(soc->core));
 KUNIT_ASSERT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);KUNIT_EXPECT_FALSE(test,soc->core.dump_registered);
 qcom_soc_wdt_free(soc);KUNIT_EXPECT_EQ(test,f->frees,1u);
}
static void actual_pending_entry_keeps_memory_alive_when_removal_is_faulted(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;f->native=true;
 KUNIT_ASSERT_GE(test,qcom_wdt_add_dump(&soc->core),0);f->remove_error=-EIO;
 KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),-EIO);qcom_soc_wdt_free(soc);
 KUNIT_ASSERT_EQ(test,f->frees,0u);KUNIT_EXPECT_TRUE(test,soc->core.dump_registered);KUNIT_EXPECT_EQ(test,soc->core.bark_time,11000u);
 f->remove_error=0;KUNIT_ASSERT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);qcom_soc_wdt_free(soc);KUNIT_EXPECT_EQ(test,f->frees,1u);
}
static void failed_add_does_not_remove_somebody_elses_region(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;f->add_error=-EEXIST;
 KUNIT_EXPECT_EQ(test,qcom_wdt_add_dump(&soc->core),-EEXIST);KUNIT_EXPECT_FALSE(test,soc->core.dump_registered);
 KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);KUNIT_EXPECT_EQ(test,f->removes,0u);
 qcom_soc_wdt_free(soc);KUNIT_EXPECT_EQ(test,f->frees,1u);
}
static void duplicate_add_and_repeated_cleanup_have_single_ownership(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;
 KUNIT_ASSERT_GE(test,qcom_wdt_add_dump(&soc->core),0);KUNIT_EXPECT_EQ(test,qcom_wdt_add_dump(&soc->core),-EBUSY);
 KUNIT_EXPECT_EQ(test,f->adds,1u);KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);
 KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);KUNIT_EXPECT_EQ(test,f->removes,1u);
 qcom_soc_wdt_free(soc);KUNIT_EXPECT_EQ(test,f->frees,1u);
}
static void confirmed_absent_record_can_be_freed(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;KUNIT_ASSERT_GE(test,qcom_wdt_add_dump(&soc->core),0);
 f->remove_error=-ENOENT;KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),0);
 KUNIT_EXPECT_FALSE(test,soc->core.dump_registered);qcom_soc_wdt_free(soc);KUNIT_EXPECT_EQ(test,f->frees,1u);
}
static void unregister_error_retains_the_exact_recorded_payload(struct kunit *test)
{
 struct fixture *f=test->priv;struct qcom_soc_wdt *soc=f->soc;struct md_region saved;
 KUNIT_ASSERT_GE(test,qcom_wdt_add_dump(&soc->core),0);saved=f->entry;f->remove_error=-EBUSY;
 KUNIT_EXPECT_EQ(test,qcom_wdt_remove_dump(&soc->core),-EBUSY);qcom_soc_wdt_free(soc);
 KUNIT_ASSERT_EQ(test,f->frees,0u);KUNIT_EXPECT_MEMEQ(test,&soc->core.dump_entry,&saved,sizeof(saved));
}
static void native_devres_releases_an_unregistered_probe_allocation(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,devm_add_action_or_reset(f->dev,qcom_soc_wdt_free,f->soc),0);
 root_device_unregister(f->dev);f->dev=NULL;KUNIT_EXPECT_PTR_EQ(test,f->soc,NULL);KUNIT_EXPECT_EQ(test,f->frees,1u);
}
static struct kunit_case cases[]={KUNIT_CASE(actual_pending_provider_removes_before_allocation_release),KUNIT_CASE(actual_pending_entry_keeps_memory_alive_when_removal_is_faulted),KUNIT_CASE(failed_add_does_not_remove_somebody_elses_region),KUNIT_CASE(duplicate_add_and_repeated_cleanup_have_single_ownership),KUNIT_CASE(confirmed_absent_record_can_be_freed),KUNIT_CASE(unregister_error_retains_the_exact_recorded_payload),KUNIT_CASE(native_devres_releases_an_unregistered_probe_allocation),{}};
static struct kunit_suite suite={.name="avr-watchdog-lifetime",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("Watchdog minidump lifetime with native pending-region APIs");
