// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include "../../ports/qti-fence/avr-fence-compat.h"
static const char *name(struct dma_fence *f) {return "avr-fence-test";}
static const struct dma_fence_ops ops={.get_driver_name=name,.get_timeline_name=name,.release=dma_fence_free};
struct fixture {struct dma_fence *first,*second;struct dma_fence_array *array;};
static void cleanup(void *arg)
{
 struct fixture *f=arg;
 if (f->first) dma_fence_signal(f->first);
 if (f->second) dma_fence_signal(f->second);
 if (f->array) dma_fence_put(&f->array->base);
 dma_fence_put(f->first);dma_fence_put(f->second);
}
static struct fixture *prepare(struct kunit *test,bool any)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
 struct dma_fence **children;
 if (!f) return NULL;
 if (kunit_add_action_or_reset(test,cleanup,f)) return NULL;
 f->first=kzalloc(sizeof(*f->first),GFP_KERNEL);
 if (!f->first) return NULL;
 dma_fence_init(f->first,&ops,NULL,dma_fence_context_alloc(1),1);
 f->second=kzalloc(sizeof(*f->second),GFP_KERNEL);
 if (!f->second) return NULL;
 dma_fence_init(f->second,&ops,NULL,dma_fence_context_alloc(1),1);
 children=kmalloc_array(2,sizeof(*children),GFP_KERNEL);
 if (!children) return NULL;
 children[0]=dma_fence_get(f->first);children[1]=dma_fence_get(f->second);
 f->array=avr_dma_fence_array_create(2,children,dma_fence_context_alloc(1),1,any);
 if (!f->array) {dma_fence_put(children[0]);dma_fence_put(children[1]);kfree(children);return NULL;}
 return f;
}
static void semantics(struct kunit *test,bool any,bool callbacks)
{
 struct fixture *f=prepare(test,any);
 KUNIT_ASSERT_NOT_NULL(test,f);
 KUNIT_EXPECT_FALSE(test,dma_fence_is_signaled(&f->array->base));
 if (callbacks) dma_fence_enable_signaling(&f->array->base);
 KUNIT_EXPECT_EQ(test,avr_dma_fence_signal(f->first),0);
 /* wait_timeout polls and enables callbacks, so use signaled() first to
  * also verify the pre-enable path. irq_work can complete after a signal. */
 if (any && callbacks)
  KUNIT_EXPECT_GT(test,dma_fence_wait_timeout(&f->array->base,false,HZ),0L);
 else
  KUNIT_EXPECT_EQ(test,dma_fence_is_signaled(&f->array->base),any);
 KUNIT_EXPECT_EQ(test,avr_dma_fence_signal(f->second),0);
 KUNIT_EXPECT_GT(test,dma_fence_wait_timeout(&f->array->base,false,HZ),0L);
}
static void any_poll(struct kunit *t) {semantics(t,true,false);}
static void any_callback(struct kunit *t) {semantics(t,true,true);}
static void all_poll(struct kunit *t) {semantics(t,false,false);}
static void all_callback(struct kunit *t) {semantics(t,false,true);}
static void repeat_signal(struct kunit *test)
{
 struct fixture *f=prepare(test,false);
 KUNIT_ASSERT_NOT_NULL(test,f);
 KUNIT_EXPECT_EQ(test,avr_dma_fence_signal(f->first),0);
 KUNIT_EXPECT_EQ(test,avr_dma_fence_signal(f->first),-EINVAL);
}
static struct kunit_case cases[]={KUNIT_CASE(any_poll),KUNIT_CASE(any_callback),
 KUNIT_CASE(all_poll),KUNIT_CASE(all_callback),KUNIT_CASE(repeat_signal),{}};
static struct kunit_suite suite={.name="avr-fence-compat",.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
