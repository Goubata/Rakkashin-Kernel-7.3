// SPDX-License-Identifier: GPL-2.0-only
/* Exact message prepare/unprepare methods; native runtime PM and SPI objects.
 * Hardware mode, sequencer locks, FIFO setup and DMA mapping are recorded. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/qcom-geni-se-common.h>
#define SPI_LOG_ERR(...) ((void)0)
#define NUM_SPI_XFER 4
struct spi_geni_gsi { u8 unused[16]; };
struct spi_geni_master {
 struct device *dev;struct geni_se spi_rsc;void *ipc,*ipc_log_kpi;
 bool spi_kpi,shared_ee,setup,dis_autosuspend,shared_se,is_le_vm;
 bool is_xfer_in_progress,gsi_mode,is_la_vm;
 struct pinctrl *geni_pinctrl;struct pinctrl_state *geni_gpio_active,*geni_gpio_sleep;
 int cur_xfer_mode,cur_speed_hz,cur_word_len;struct spi_geni_gsi gsi[NUM_SPI_XFER];
};
struct fixture {
 struct device *dev;struct spi_controller *ctl;struct spi_geni_master *mas;struct spi_message msg;
 int mode,map_error,fifo_error,lock_error,resume_error,unlock_usage;
 unsigned int locks,unlocks,maps,unmaps,resumes,suspends;
 int pin_error,sleep_error,setup_error;unsigned int active_pins,sleep_pins,setups;
};
static struct fixture *backend;
static int runtime_resume(struct device *dev){struct fixture *f=dev_get_drvdata(dev);f->resumes++;return f->resume_error;}
static int runtime_suspend(struct device *dev){struct fixture *f=dev_get_drvdata(dev);f->suspends++;return 0;}
static const struct dev_pm_ops pm_ops={.runtime_resume=runtime_resume,.runtime_suspend=runtime_suspend};
static const struct device_type pm_type={.name="avr-geni-pm-ram",.pm=&pm_ops};
static int spi_geni_lock_bus(struct spi_controller *ctl){(void)ctl;backend->locks++;return backend->lock_error;}
static void spi_geni_unlock_bus(struct spi_controller *ctl){(void)ctl;backend->unlocks++;backend->unlock_usage=atomic_read(&backend->dev->power.usage_count);}
static int select_xfer_mode(struct spi_controller *ctl,struct spi_message *msg){(void)ctl;(void)msg;return backend->mode;}
static int setup_fifo_params(struct spi_device *spi,struct spi_controller *ctl){(void)spi;(void)ctl;return backend->fifo_error;}
static int spi_geni_map_buf(struct spi_geni_master *mas,struct spi_message *msg){(void)mas;(void)msg;backend->maps++;return backend->map_error;}
static void spi_geni_unmap_buf(struct spi_geni_master *mas,struct spi_message *msg){(void)mas;(void)msg;backend->unmaps++;}
static void record_select_mode(struct geni_se *se,enum geni_se_xfer_mode mode){(void)se;(void)mode;}
static int record_pinctrl(struct pinctrl *p,struct pinctrl_state *state)
{
 (void)p;
 if(state==backend->mas->geni_gpio_active){backend->active_pins++;return backend->pin_error;}
 backend->sleep_pins++;return backend->sleep_error;
}
static int spi_geni_mas_setup(struct spi_controller *ctl)
{
 struct spi_geni_master *mas=spi_controller_get_devdata(ctl);backend->setups++;
 /* Record only a clean setup result; production DMA setup partial failures
  * and firmware discovery are outside this fixture. */
 if(!backend->setup_error && !mas->shared_ee)mas->setup=true;
 return backend->setup_error;
}
#define pinctrl_select_state record_pinctrl
#define geni_se_select_mode record_select_mode
#include "generated.h"
#undef geni_se_select_mode
#undef pinctrl_select_state
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
 if(!f)return -ENOMEM;test->priv=f;f->dev=root_device_register("avr-geni-pm-ram");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 f->dev->type=&pm_type;dev_set_drvdata(f->dev,f);f->ctl=spi_alloc_host(f->dev,sizeof(*f->mas));
 if(!f->ctl){root_device_unregister(f->dev);return -ENOMEM;}
 f->mas=spi_controller_get_devdata(f->ctl);f->mas->dev=f->dev;f->mas->spi_rsc.dev=f->dev;
 f->mas->shared_ee=f->mas->shared_se=f->mas->setup=f->mas->dis_autosuspend=true;f->mode=GENI_GPI_DMA;
 f->mas->geni_gpio_active=(struct pinctrl_state *)&f->active_pins;
 f->mas->geni_gpio_sleep=(struct pinctrl_state *)&f->sleep_pins;
 spi_message_init(&f->msg);backend=f;ret=pm_runtime_set_active(f->dev);
 if(ret){spi_controller_put(f->ctl);root_device_unregister(f->dev);return ret;}
 pm_runtime_enable(f->dev);return 0;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;pm_runtime_barrier(f->dev);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 while(atomic_read(&f->dev->power.usage_count)>0)pm_runtime_put_noidle(f->dev);
 pm_runtime_disable(f->dev);spi_controller_put(f->ctl);backend=NULL;root_device_unregister(f->dev);
}
static void lock_failure_releases_pm_and_retry_succeeds(struct kunit *test)
{
 struct fixture *f=test->priv;f->lock_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);KUNIT_EXPECT_EQ(test,f->unlocks,0u);
 f->lock_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);
 spi_geni_unprepare_message(f->ctl,&f->msg);KUNIT_EXPECT_EQ(test,f->unlocks,1u);KUNIT_EXPECT_GT(test,f->unlock_usage,0);
}
static void map_failure_unlocks_before_releasing_power(struct kunit *test)
{
 struct fixture *f=test->priv;f->map_error=-ENOMEM;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-ENOMEM);KUNIT_EXPECT_EQ(test,f->unlocks,1u);KUNIT_EXPECT_GT(test,f->unlock_usage,0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 f->map_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);
}
static void fifo_failure_releases_only_held_resources(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_se=false;f->mode=GENI_SE_FIFO;f->fifo_error=-EINVAL;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EINVAL);KUNIT_EXPECT_EQ(test,f->unlocks,0u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static void mode_failure_unwinds_lock_and_pm(struct kunit *test)
{
 struct fixture *f=test->priv;f->mode=-EINVAL;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EINVAL);KUNIT_EXPECT_EQ(test,f->unlocks,1u);KUNIT_EXPECT_EQ(test,f->maps,0u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static void resume_failure_is_recoverable_without_warning(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_GE(test,pm_runtime_suspend(f->dev),0);f->resume_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,f->locks,0u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 f->resume_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);
}
static void first_shared_message_consumes_inherited_reference(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_GE(test,pm_runtime_get_sync(f->dev),0);f->mas->setup=false;f->lock_error=-EBUSY;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EBUSY);KUNIT_EXPECT_TRUE(test,f->mas->setup);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 f->lock_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);spi_geni_unprepare_message(f->ctl,&f->msg);
}
static void nonshared_hardware_reference_is_not_released(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=f->mas->shared_se=false;KUNIT_ASSERT_GE(test,pm_runtime_get_sync(f->dev),0);f->map_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);pm_runtime_put_sync(f->dev);
}
static void fallback_local_resume_unwinds_on_failure(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=f->mas->shared_se=false;KUNIT_ASSERT_GE(test,pm_runtime_suspend(f->dev),0);f->map_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static void disabled_pm_never_acquires_hardware_resources(struct kunit *test)
{
 struct fixture *f=test->priv;pm_runtime_disable(f->dev);
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),-EACCES);
 KUNIT_EXPECT_FALSE(test,f->mas->is_xfer_in_progress);KUNIT_EXPECT_EQ(test,f->active_pins,0u);
 spi_geni_unprepare_transfer_hardware(f->ctl);pm_runtime_enable(f->dev);
}
static void active_pin_failure_stops_prepare_and_can_retry(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=false;f->mas->gsi_mode=true;f->pin_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),-EIO);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);KUNIT_EXPECT_EQ(test,f->sleep_pins,0u);
 KUNIT_EXPECT_FALSE(test,f->mas->is_xfer_in_progress);spi_geni_unprepare_transfer_hardware(f->ctl);
 f->pin_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);
 spi_geni_unprepare_transfer_hardware(f->ctl);spi_geni_unprepare_transfer_hardware(f->ctl);
 KUNIT_EXPECT_EQ(test,f->sleep_pins,1u);KUNIT_EXPECT_FALSE(test,f->mas->is_xfer_in_progress);
}
static void hardware_resume_failure_restores_pins_without_double_put(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=false;f->mas->gsi_mode=true;
 KUNIT_ASSERT_GE(test,pm_runtime_suspend(f->dev),0);f->resume_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),-EIO);KUNIT_EXPECT_EQ(test,f->sleep_pins,1u);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);spi_geni_unprepare_transfer_hardware(f->ctl);
 f->resume_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);spi_geni_unprepare_transfer_hardware(f->ctl);
}
static void setup_error_releases_hardware_reference_once(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=false;f->mas->gsi_mode=true;f->mas->setup=false;f->setup_error=-ENXIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),-ENXIO);KUNIT_EXPECT_EQ(test,f->setups,1u);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);KUNIT_EXPECT_EQ(test,f->sleep_pins,1u);
 KUNIT_EXPECT_FALSE(test,f->mas->is_xfer_in_progress);spi_geni_unprepare_transfer_hardware(f->ctl);
 f->setup_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);spi_geni_unprepare_transfer_hardware(f->ctl);
}
static void cleanup_pin_error_preserves_primary_error(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=false;f->mas->gsi_mode=true;f->mas->setup=false;
 f->setup_error=-ENXIO;f->sleep_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),-ENXIO);
 KUNIT_EXPECT_EQ(test,f->sleep_pins,1u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 spi_geni_unprepare_transfer_hardware(f->ctl);
}
static void nonshared_phase_sequence_balances_one_reference(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=f->mas->shared_se=false;
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);spi_geni_unprepare_transfer_hardware(f->ctl);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static void first_shared_phase_sequence_balances_handoff(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->setup=false;
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);
 spi_geni_unprepare_transfer_hardware(f->ctl);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);spi_geni_unprepare_transfer_hardware(f->ctl);
}
static void first_shared_hardware_setup_error_does_not_leak(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->setup=false;f->setup_error=-ENOMEM;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),-ENOMEM);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);spi_geni_unprepare_transfer_hardware(f->ctl);
}
static void le_vm_hardware_phase_keeps_client_reference(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->is_le_vm=true;KUNIT_ASSERT_GE(test,pm_runtime_get_sync(f->dev),0);
 KUNIT_ASSERT_EQ(test,spi_geni_prepare_transfer_hardware(f->ctl),0);spi_geni_unprepare_transfer_hardware(f->ctl);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);pm_runtime_put_sync(f->dev);
}
/* Also drive the real registered SPI core: spi_sync may unprepare hardware
 * after its prepare callback failed. The transfer itself remains recorded. */
static int record_transfer_message(struct spi_controller *ctl,struct spi_message *msg)
{
 msg->actual_length=0;msg->status=0;spi_finalize_current_message(ctl);return 0;
}
static struct spi_device *register_test_spi(struct kunit *test)
{
 struct fixture *f=test->priv;
 struct spi_board_info info={.modalias="avr-geni-pm-test",.max_speed_hz=1000000,.chip_select=0};
 struct spi_device *spi;int ret;
 f->ctl->bus_num=-1;f->ctl->num_chipselect=1;f->ctl->max_speed_hz=1000000;
 f->ctl->bits_per_word_mask=SPI_BPW_MASK(8);
 f->ctl->prepare_transfer_hardware=spi_geni_prepare_transfer_hardware;
 f->ctl->unprepare_transfer_hardware=spi_geni_unprepare_transfer_hardware;
 f->ctl->prepare_message=spi_geni_prepare_message;
 f->ctl->unprepare_message=spi_geni_unprepare_message;
 f->ctl->transfer_one_message=record_transfer_message;
 /* Retain the fixture's allocation reference across unregister. */
 spi_controller_get(f->ctl);ret=spi_register_controller(f->ctl);
 if(ret){spi_controller_put(f->ctl);KUNIT_FAIL(test,"SPI register failed: %d",ret);return NULL;}
 spi=spi_new_device(f->ctl,&info);
 if(!spi){spi_unregister_controller(f->ctl);KUNIT_FAIL(test,"SPI child registration failed");}
 return spi;
}
static int core_sync(struct spi_device *spi)
{
 struct spi_message msg;struct spi_transfer transfer={.len=0,.bits_per_word=8,.speed_hz=1000000};
 spi_message_init(&msg);spi_message_add_tail(&transfer,&msg);return spi_sync(spi,&msg);
}
static void native_spi_core_unprepares_failed_hardware_safely(struct kunit *test)
{
 struct fixture *f=test->priv;struct spi_device *spi;
 f->mas->shared_ee=f->mas->shared_se=false;f->mas->setup=false;f->setup_error=-ENXIO;
 spi=register_test_spi(test);KUNIT_ASSERT_NOT_NULL(test,spi);
 KUNIT_EXPECT_EQ(test,core_sync(spi),-ENXIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 KUNIT_EXPECT_FALSE(test,f->mas->is_xfer_in_progress);
 f->setup_error=0;KUNIT_EXPECT_EQ(test,core_sync(spi),0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 spi_unregister_device(spi);spi_unregister_controller(f->ctl);
}
static void native_spi_core_message_failure_preserves_hardware_owner(struct kunit *test)
{
 struct fixture *f=test->priv;struct spi_device *spi;
 f->mas->shared_ee=f->mas->shared_se=false;f->map_error=-ENOMEM;
 spi=register_test_spi(test);KUNIT_ASSERT_NOT_NULL(test,spi);
 KUNIT_EXPECT_EQ(test,core_sync(spi),-ENOMEM);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 KUNIT_EXPECT_EQ(test,f->unmaps,0u);
 f->map_error=0;KUNIT_EXPECT_EQ(test,core_sync(spi),0);KUNIT_EXPECT_EQ(test,f->unmaps,1u);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 spi_unregister_device(spi);spi_unregister_controller(f->ctl);
}
static struct kunit_case cases[]={KUNIT_CASE(lock_failure_releases_pm_and_retry_succeeds),KUNIT_CASE(map_failure_unlocks_before_releasing_power),KUNIT_CASE(fifo_failure_releases_only_held_resources),KUNIT_CASE(mode_failure_unwinds_lock_and_pm),KUNIT_CASE(resume_failure_is_recoverable_without_warning),KUNIT_CASE(first_shared_message_consumes_inherited_reference),KUNIT_CASE(nonshared_hardware_reference_is_not_released),KUNIT_CASE(fallback_local_resume_unwinds_on_failure),
 KUNIT_CASE(disabled_pm_never_acquires_hardware_resources),KUNIT_CASE(active_pin_failure_stops_prepare_and_can_retry),KUNIT_CASE(hardware_resume_failure_restores_pins_without_double_put),KUNIT_CASE(setup_error_releases_hardware_reference_once),KUNIT_CASE(cleanup_pin_error_preserves_primary_error),KUNIT_CASE(nonshared_phase_sequence_balances_one_reference),KUNIT_CASE(first_shared_phase_sequence_balances_handoff),KUNIT_CASE(first_shared_hardware_setup_error_does_not_leak),KUNIT_CASE(le_vm_hardware_phase_keeps_client_reference),KUNIT_CASE(native_spi_core_unprepares_failed_hardware_safely),KUNIT_CASE(native_spi_core_message_failure_preserves_hardware_owner),{}};
static struct kunit_suite suite={.name="avr-geni-pm",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("SPI prepare error unwind with native runtime PM");
