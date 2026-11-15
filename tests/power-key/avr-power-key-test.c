// SPDX-License-Identifier: GPL-2.0-only
/* Real driver IRQ methods, input core/handler and regmap; RAM bus only. */
#include <kunit/test.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#undef module_platform_driver
#define module_platform_driver(...)
#include "../../ports/qcom-power-key/pm8941-pwrkey.c"
#include "../../ports/qcom-power-key/qcom-pon.c"
struct fixture {
	struct device *dev;
	struct pm8941_pwrkey key;
	struct input_handler handler;
	struct input_handle handle;
	u8 ram[0x1500];unsigned int events,calls;bool io_error;
	unsigned int codes[32];int values[32];
};
static int ram_read(void *ctx,unsigned int reg,unsigned int *value)
{
	struct fixture *f=ctx;f->calls++;if(f->io_error)return -EIO;
	*value=f->ram[reg];return 0;
}
static int ram_write(void *ctx,unsigned int reg,unsigned int value)
{
	struct fixture *f=ctx;f->calls++;if(f->io_error)return -EIO;
	f->ram[reg]=value;return 0;
}
static const struct regmap_bus bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config config={.reg_bits=16,.val_bits=8,.max_register=0x14ff,.cache_type=REGCACHE_NONE};
static void capture(struct input_handle *handle,unsigned int type,unsigned int code,int value)
{
	struct fixture *f=handle->private;
	if(type!=EV_KEY)return;
	if(f->events<ARRAY_SIZE(f->codes)){f->codes[f->events]=code;f->values[f->events]=value;}
	f->events++;
}
static int connect(struct input_handler *handler,struct input_dev *dev,const struct input_device_id *id)
{
	struct fixture *f=handler->private;int ret;
	if(dev!=f->key.input)return -ENODEV;
	f->handle=(struct input_handle){.dev=dev,.handler=handler,.name="avr-power-key-capture",.private=f};
	ret=input_register_handle(&f->handle);if(ret)return ret;
	ret=input_open_device(&f->handle);if(ret)input_unregister_handle(&f->handle);
	return ret;
}
static void disconnect(struct input_handle *handle){input_close_device(handle);input_unregister_handle(handle);}
static const struct input_device_id ids[]={{.flags=INPUT_DEVICE_ID_MATCH_EVBIT,.evbit={BIT_MASK(EV_KEY)}},{}};
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
	if(!f)return -ENOMEM;
	f->dev=root_device_register("avr_power_key_test");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
	f->key.dev=f->dev;f->key.baseaddr=0x1300;f->key.pon_pbs_baseaddr=0x800;f->key.subtype=PON_SUBTYPE_GEN3_HLOS;
	f->key.data=&pon_gen3_pwrkey_data;f->key.code=KEY_POWER;
	f->key.regmap=regmap_init(f->dev,&bus,f,&config);
	if(IS_ERR(f->key.regmap)){ret=PTR_ERR(f->key.regmap);goto unregister;}
	f->key.input=input_allocate_device();if(!f->key.input){ret=-ENOMEM;goto unmap;}
	f->key.input->name="avr-power-key-ram";f->key.input->dev.parent=f->dev;
	input_set_capability(f->key.input,EV_KEY,KEY_POWER);input_set_capability(f->key.input,EV_KEY,KEY_VOLUMEDOWN);
	ret=input_register_device(f->key.input);if(ret){input_free_device(f->key.input);goto unmap;}
	f->handler=(struct input_handler){.name="avr-power-key-capture",.private=f,.event=capture,.connect=connect,.disconnect=disconnect,.id_table=ids};
	ret=input_register_handler(&f->handler);if(ret){input_unregister_device(f->key.input);goto unmap;}
	if(!f->handle.open){input_unregister_handler(&f->handler);input_unregister_device(f->key.input);ret=-ENODEV;goto unmap;}
	test->priv=f;return 0;
unmap:regmap_exit(f->key.regmap);
unregister:root_device_unregister(f->dev);return ret;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;input_unregister_handler(&f->handler);input_unregister_device(f->key.input);regmap_exit(f->key.regmap);root_device_unregister(f->dev);
}
static void irq(struct fixture *f,unsigned int status){f->ram[f->key.baseaddr+PON_RT_STS]=status;pm8941_pwrkey_irq(1,&f->key);}
static void expect_pair(struct kunit *test,unsigned int code)
{
	struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,f->events,2U);
	KUNIT_EXPECT_EQ(test,f->codes[0],code);KUNIT_EXPECT_EQ(test,f->codes[1],code);
	KUNIT_EXPECT_EQ(test,f->values[0],1);KUNIT_EXPECT_EQ(test,f->values[1],0);
}
static void power_key_input_events(struct kunit *test)
{
	struct fixture *f=test->priv;irq(f,PON_GEN3_KPDPWR_N_SET);irq(f,0);expect_pair(test,KEY_POWER);
}
static void resin_volume_down_events(struct kunit *test)
{
	struct fixture *f=test->priv;f->key.data=&pon_gen3_resin_data;f->key.code=KEY_VOLUMEDOWN;
	irq(f,PON_GEN3_RESIN_N_SET);irq(f,0);expect_pair(test,KEY_VOLUMEDOWN);
}
static void release_without_press_synthesizes_pair(struct kunit *test)
{
	struct fixture *f=test->priv;irq(f,0);expect_pair(test,KEY_POWER);
}
static void duplicate_press_is_filtered_by_input_core(struct kunit *test)
{
	struct fixture *f=test->priv;irq(f,0x80);irq(f,0x80);irq(f,0);expect_pair(test,KEY_POWER);
}
static void regmap_read_error_does_not_emit_events(struct kunit *test)
{
	struct fixture *f=test->priv;f->io_error=true;irq(f,0x80);
	KUNIT_EXPECT_EQ(test,f->events,0U);KUNIT_EXPECT_FALSE(test,f->key.last_status);
}
static void pending_software_debounce_skips_bus(struct kunit *test)
{
	struct fixture *f=test->priv;f->key.sw_debounce_time_us=1000000;
	f->key.sw_debounce_end_time=ktime_add_ms(ktime_get(),60000);irq(f,0x80);
	KUNIT_EXPECT_EQ(test,f->events,0U);KUNIT_EXPECT_EQ(test,f->calls,0U);
}
static void pbs_debounce_uses_second_parent_region(struct kunit *test)
{
	struct fixture *f=test->priv;f->ram[0x871]=5;f->ram[0x1371]=14;
	KUNIT_ASSERT_EQ(test,pm8941_pwrkey_sw_debounce_init(&f->key),0);
	KUNIT_EXPECT_EQ(test,f->key.sw_debounce_time_us,1953U);
	f->io_error=true;KUNIT_EXPECT_EQ(test,pm8941_pwrkey_sw_debounce_init(&f->key),-EIO);
}
static void gen2_reboot_mode_mask_and_errors(struct kunit *test)
{
	struct fixture *f=test->priv;struct qcom_pon pon={.dev=f->dev,.regmap=f->key.regmap,.baseaddr=0x1300,.reason_shift=GEN2_REASON_SHIFT};
	f->ram[0x138f]=1;KUNIT_ASSERT_EQ(test,qcom_pon_reboot_mode_write(&pon.reboot_mode,0x2a),0);
	KUNIT_EXPECT_EQ(test,f->ram[0x138f],(u8)0x55);
	f->io_error=true;KUNIT_EXPECT_EQ(test,qcom_pon_reboot_mode_write(&pon.reboot_mode,3),-EIO);
}
static struct kunit_case cases[]={KUNIT_CASE(power_key_input_events),KUNIT_CASE(resin_volume_down_events),KUNIT_CASE(release_without_press_synthesizes_pair),KUNIT_CASE(duplicate_press_is_filtered_by_input_core),KUNIT_CASE(regmap_read_error_does_not_emit_events),KUNIT_CASE(pending_software_debounce_skips_bus),KUNIT_CASE(pbs_debounce_uses_second_parent_region),KUNIT_CASE(gen2_reboot_mode_mask_and_errors),{}};
static struct kunit_suite suite={.name="avr-power-key-input-ram",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
