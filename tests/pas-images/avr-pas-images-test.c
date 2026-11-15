// SPDX-License-Identifier: GPL-2.0-only
/* Secure-state decisions use recording callbacks; no secure firmware API call. */
#include <kunit/test.h>
#include <linux/module.h>
#include "../../ports/qcom-pas-alor/avr_pas_images.h"
struct fixture {
	struct avr_pas_images images;
	int error[AVR_IMAGE_COUNT];
	unsigned int shutdown[AVR_IMAGE_COUNT],unmap[AVR_IMAGE_COUNT];
	int trace[4096];unsigned int traces;
};
static int shutdown(void *ctx,unsigned int image)
{
	struct fixture *f=ctx;
	if(f->traces<ARRAY_SIZE(f->trace))f->trace[f->traces]=image+1;
	f->traces++;f->shutdown[image]++;
	return f->error[image];
}
static void unmap(void *ctx,unsigned int image)
{
	struct fixture *f=ctx;
	if(f->traces<ARRAY_SIZE(f->trace))f->trace[f->traces]=-(image+1);
	f->traces++;f->unmap[image]++;
}
static const struct avr_pas_image_ops ops={.shutdown=shutdown,.unmap=unmap};
static int setup(struct kunit *test)
{
	test->priv=kunit_kzalloc(test,sizeof(struct fixture),GFP_KERNEL);
	return test->priv?0:-ENOMEM;
}
static void activate(struct fixture *f)
{
	f->images.needs_shutdown[0]=f->images.needs_shutdown[1]=true;
	f->images.mapped[0]=f->images.mapped[1]=true;
}
static void quiet_state_is_idempotent(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_EXPECT_FALSE(test,avr_pas_images_busy(&f->images));
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,f->traces,0U);
}
static void stop_main_then_dtb_before_unmap(struct kunit *test)
{
	struct fixture *f=test->priv;activate(f);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),-EBUSY);
	KUNIT_EXPECT_EQ(test,f->traces,0U);
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
	KUNIT_EXPECT_FALSE(test,avr_pas_images_active(&f->images));
	KUNIT_EXPECT_TRUE(test,avr_pas_images_busy(&f->images));
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,f->trace[0],1);KUNIT_EXPECT_EQ(test,f->trace[1],2);
	KUNIT_EXPECT_EQ(test,f->trace[2],-1);KUNIT_EXPECT_EQ(test,f->trace[3],-2);
	KUNIT_EXPECT_FALSE(test,avr_pas_images_busy(&f->images));
}
static void main_failure_retains_both_images(struct kunit *test)
{
	struct fixture *f=test->priv;activate(f);f->error[0]=-EIO;
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),-EIO);
	KUNIT_EXPECT_TRUE(test,f->images.needs_shutdown[0]);KUNIT_EXPECT_TRUE(test,f->images.needs_shutdown[1]);
	KUNIT_EXPECT_EQ(test,f->shutdown[0],1U);KUNIT_EXPECT_EQ(test,f->shutdown[1],0U);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),-EBUSY);
	KUNIT_EXPECT_EQ(test,f->unmap[0]+f->unmap[1],0U);
	f->error[0]=0;
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
}
static void dtb_failure_does_not_repeat_main_shutdown(struct kunit *test)
{
	struct fixture *f=test->priv;activate(f);f->error[1]=-ETIMEDOUT;
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),-ETIMEDOUT);
	KUNIT_EXPECT_FALSE(test,f->images.needs_shutdown[0]);KUNIT_EXPECT_TRUE(test,f->images.needs_shutdown[1]);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),-EBUSY);
	f->error[1]=0;
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,f->shutdown[0],1U);KUNIT_EXPECT_EQ(test,f->shutdown[1],2U);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
}
static void unmap_only_owned_mapping(struct kunit *test)
{
	struct fixture *f=test->priv;f->images.mapped[1]=true;
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,f->unmap[0],0U);KUNIT_EXPECT_EQ(test,f->unmap[1],1U);
	KUNIT_EXPECT_EQ(test,f->shutdown[0]+f->shutdown[1],0U);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,f->unmap[1],1U);
}
static void uncertain_auth_state_must_be_quiesced(struct kunit *test)
{
	struct fixture *f=test->priv;
	f->images.needs_shutdown[1]=true;f->images.mapped[1]=true;f->error[1]=7;
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),7);
	KUNIT_EXPECT_TRUE(test,f->images.needs_shutdown[1]);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),-EBUSY);
	f->error[1]=0;
	KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
	KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
	KUNIT_EXPECT_FALSE(test,avr_pas_images_busy(&f->images));
}
static void repeated_cycles_do_not_double_release(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	for(i=0;i<200;i++) {
		activate(f);
		KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
		KUNIT_EXPECT_EQ(test,avr_pas_images_quiesce(&f->images,&ops,f),0);
		KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
		KUNIT_EXPECT_EQ(test,avr_pas_images_unmap(&f->images,&ops,f),0);
	}
	KUNIT_EXPECT_EQ(test,f->shutdown[0],200U);KUNIT_EXPECT_EQ(test,f->shutdown[1],200U);
	KUNIT_EXPECT_EQ(test,f->unmap[0],200U);KUNIT_EXPECT_EQ(test,f->unmap[1],200U);
}
static struct kunit_case cases[]={KUNIT_CASE(quiet_state_is_idempotent),KUNIT_CASE(stop_main_then_dtb_before_unmap),
 KUNIT_CASE(main_failure_retains_both_images),KUNIT_CASE(dtb_failure_does_not_repeat_main_shutdown),
 KUNIT_CASE(unmap_only_owned_mapping),KUNIT_CASE(uncertain_auth_state_must_be_quiesced),
 KUNIT_CASE(repeated_cycles_do_not_double_release),{}};
static struct kunit_suite suite={.name="avr-pas-images",.init=setup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_DESCRIPTION("AVR secure PAS image ownership decision tests");
MODULE_LICENSE("GPL");
