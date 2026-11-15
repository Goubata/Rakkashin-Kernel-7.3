// SPDX-License-Identifier: GPL-2.0-only
/* Native wrapper actually binds to reserved guest RAM, never phone MMIO. */
#include <kunit/test.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/overflow.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/qcom-geni-se-common.h>
#define SPI_NUM_CHIPSELECT 4
/* Reduced private field fixture; actual geni_se and CCF APIs are native. */
struct spi_geni_master { struct geni_se spi_rsc;struct device *dev;int oversampling; };
#include "generated.h"
struct fixture {struct platform_device *pdev;struct geni_se se;void __iomem *base;struct clk *clk;};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);struct device_node *node;
 if(!f)return -ENOMEM;test->priv=f;
 node=of_find_node_by_path("/avr-geni-wrapper@60000000");if(!node)return -ENODEV;
 f->pdev=of_find_device_by_node(node);of_node_put(node);if(!f->pdev)return -ENODEV;
 if(!f->pdev->dev.driver || strcmp(f->pdev->dev.driver->name,"geni_se_qup")){put_device(&f->pdev->dev);return -ENODEV;}
 f->se.wrapper=dev_get_drvdata(&f->pdev->dev);f->base=geni_se_get_wrapper_base(&f->se);
 if(!f->base){put_device(&f->pdev->dev);return -ENODEV;}return 0;
}
static void cleanup(struct kunit *test){struct fixture *f=test->priv;if(!IS_ERR_OR_NULL(f->clk))clk_put(f->clk);put_device(&f->pdev->dev);}
static void accessor_uses_actual_native_wrapper(struct kunit *test)
{
 struct fixture *f=test->priv;struct geni_se empty={};
 KUNIT_EXPECT_PTR_EQ(test,geni_se_get_wrapper_base(NULL),NULL);KUNIT_EXPECT_PTR_EQ(test,geni_se_get_wrapper_base(&empty),NULL);
 writel_relaxed(0x12345678,f->base+4);KUNIT_EXPECT_EQ(test,geni_se_get_qup_hw_version(&f->se),0x12345678u);
}
static void wrapper_test_bus_writes_correct_mapping(struct kunit *test)
{
 struct fixture *f=test->priv;writel_relaxed(0xa5a55a5a,f->base+0x210);
 test_bus_enable_per_qupv3(&f->pdev->dev,NULL);KUNIT_EXPECT_EQ(test,readl_relaxed(f->base+QUPV3_TEST_BUS_EN),0x11u);
 test_bus_select_per_qupv3(&f->pdev->dev,5,NULL);KUNIT_EXPECT_EQ(test,readl_relaxed(f->base+QUPV3_TEST_BUS_SEL),5u);
 writel_relaxed(0x87654321,f->base+QUPV3_TEST_BUS_REG);test_bus_read_per_qupv3(&f->pdev->dev,NULL);
 KUNIT_EXPECT_EQ(test,readl_relaxed(f->base+0x210),0xa5a55a5au);
}
static void missing_wrapper_data_is_not_mmio(struct kunit *test)
{
 struct device *dev=root_device_register("avr-geni-empty");KUNIT_ASSERT_FALSE(test,IS_ERR(dev));
 test_bus_enable_per_qupv3(dev,NULL);test_bus_select_per_qupv3(dev,5,NULL);test_bus_read_per_qupv3(dev,NULL);root_device_unregister(dev);
}
static void spi_chip_select_uses_native_array_bounds(struct kunit *test)
{
 struct spi_device *spi=kunit_kzalloc(test,sizeof(*spi),GFP_KERNEL);u8 cs=0xff;unsigned int i;
 KUNIT_ASSERT_NOT_NULL(test,spi);memset(spi->chip_select,0xff,sizeof(spi->chip_select));
 KUNIT_EXPECT_LT(test,spi_geni_get_chip_select_num(spi,&cs),0);
 for(i=0;i<ARRAY_SIZE(spi->chip_select);i++){
  memset(spi->chip_select,0xff,sizeof(spi->chip_select));spi->chip_select[i]=i;
  KUNIT_EXPECT_EQ(test,spi_geni_get_chip_select_num(spi,&cs),0);KUNIT_EXPECT_EQ(test,cs,(u8)i);
 }
 memset(spi->chip_select,SPI_NUM_CHIPSELECT,sizeof(spi->chip_select));KUNIT_EXPECT_LT(test,spi_geni_get_chip_select_num(spi,&cs),0);
}
static int prepare_spi_clock(struct fixture *f,struct spi_geni_master *mas)
{
 f->clk=clk_get(&f->pdev->dev,"m-ahb");if(IS_ERR(f->clk))return PTR_ERR(f->clk);
 mas->dev=&f->pdev->dev;mas->spi_rsc.dev=mas->dev;mas->spi_rsc.clk=f->clk;mas->oversampling=1;return 0;
}
static void spi_clock_rejects_zero_and_invalid_sampling(struct kunit *test)
{
 struct fixture *f=test->priv;struct spi_geni_master mas={};int index=-1,div=-1;
 KUNIT_ASSERT_EQ(test,prepare_spi_clock(f,&mas),0);
 KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(0,&mas,&index,&div),-EINVAL);
 mas.oversampling=0;KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(400000,&mas,&index,&div),-EINVAL);
 mas.oversampling=-1;KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(400000,&mas,&index,&div),-EINVAL);
 KUNIT_EXPECT_EQ(test,clk_get_rate(f->clk),19200000ul);
}
static void spi_clock_uses_native_ccf_and_wide_product(struct kunit *test)
{
 struct fixture *f=test->priv;struct spi_geni_master mas={};int index=-1,div=-1;
 KUNIT_ASSERT_EQ(test,prepare_spi_clock(f,&mas),0);
 KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(400000,&mas,&index,&div),0);KUNIT_EXPECT_EQ(test,div,48);
 mas.oversampling=2;KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(400000,&mas,&index,&div),0);KUNIT_EXPECT_EQ(test,div,24);
 /* Old u32 product wraps to zero here and reaches native divide-by-zero. */
 KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(0x80000000u,&mas,&index,&div),0);KUNIT_EXPECT_EQ(test,div,1);
 KUNIT_EXPECT_EQ(test,clk_get_rate(f->clk),19200000ul);
}
static void spi_clock_divider_fits_native_register(struct kunit *test)
{
 struct fixture *f=test->priv;struct spi_geni_master mas={};int index=-1,div=-1;
 KUNIT_ASSERT_EQ(test,prepare_spi_clock(f,&mas),0);
 KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(5000,&mas,&index,&div),0);KUNIT_EXPECT_EQ(test,div,3840);
 KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(4000,&mas,&index,&div),-ERANGE);
 KUNIT_EXPECT_EQ(test,get_spi_clk_cfg(1,&mas,&index,&div),-ERANGE);
 KUNIT_EXPECT_EQ(test,clk_get_rate(f->clk),19200000ul);
}
static struct kunit_case cases[]={KUNIT_CASE(accessor_uses_actual_native_wrapper),KUNIT_CASE(wrapper_test_bus_writes_correct_mapping),KUNIT_CASE(missing_wrapper_data_is_not_mmio),KUNIT_CASE(spi_chip_select_uses_native_array_bounds),KUNIT_CASE(spi_clock_rejects_zero_and_invalid_sampling),KUNIT_CASE(spi_clock_uses_native_ccf_and_wide_product),KUNIT_CASE(spi_clock_divider_fits_native_register),{}};
static struct kunit_suite suite={.name="avr-geni-bus",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("Native GENI wrapper and selected client helper tests");
