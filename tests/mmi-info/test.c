// SPDX-License-Identifier: GPL-2.0-only
/* Exact five production source files and real platform/proc/sysfs machinery;
 * SMEM uses private RAM; selected allocation/proc/sysfs faults are injected
 * through wrappers that otherwise call the real framework APIs. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <soc/qcom/mmi_boot_info.h>
#include <linux/mmi_device.h>
#include "test-api.h"
static DEFINE_MUTEX(backend_lock);
static struct {int alloc_error,ddr_error,unit_error;size_t ddr_size,unit_size;u32 ddr[6];u8 box[1088];bool bad;unsigned int allocs,ddr_gets,unit_gets,registrations,fail_registration;bool fail_storage_group,fail_ram_group,fail_unit_proc,fail_boot_proc;} backend;
int avr_info_smem_alloc(unsigned int host,unsigned int item,size_t size)
{
	int ret;mutex_lock(&backend_lock);backend.allocs++;
	if(host!=QCOM_SMEM_HOST_ANY || item!=134 || size!=1024)backend.bad=true;
	ret=backend.alloc_error;mutex_unlock(&backend_lock);return ret;
}
void *avr_info_smem_get(unsigned int host,unsigned int item,size_t *size)
{
	void *ret;mutex_lock(&backend_lock);if(host!=QCOM_SMEM_HOST_ANY)backend.bad=true;
	if(item==135){backend.ddr_gets++;*size=backend.ddr_size;ret=backend.ddr_error?ERR_PTR(backend.ddr_error):backend.ddr;}
	else if(item==134){backend.unit_gets++;*size=backend.unit_size;ret=backend.unit_error?ERR_PTR(backend.unit_error):backend.box+32;}
	else{backend.bad=true;ret=ERR_PTR(-EINVAL);}
	mutex_unlock(&backend_lock);return ret;
}
struct proc_dir_entry *avr_info_proc_create(const char *name,umode_t mode,struct proc_dir_entry *parent,const struct proc_ops *ops)
{
	bool fail;mutex_lock(&backend_lock);fail=(!strcmp(name,"unitinfo") && backend.fail_unit_proc)||(!strcmp(name,"bootinfo") && backend.fail_boot_proc);mutex_unlock(&backend_lock);
	return fail?NULL:proc_create(name,mode,parent,ops);
}
int avr_info_sysfs_group(struct kobject *kobj,const struct attribute_group *group)
{
	bool fail;mutex_lock(&backend_lock);fail=(!strcmp(kobject_name(kobj),"storage") && backend.fail_storage_group)||(!strcmp(kobject_name(kobj),"ram") && backend.fail_ram_group);mutex_unlock(&backend_lock);
	return fail?-EIO:sysfs_create_group(kobj,group);
}
struct platform_device *avr_info_register_device(const char *name,int id,const struct resource *res,unsigned int count)
{
	bool fail;mutex_lock(&backend_lock);backend.registrations++;fail=backend.fail_registration && backend.registrations==backend.fail_registration;mutex_unlock(&backend_lock);
	return fail?ERR_PTR(-ENOMEM):platform_device_register_simple(name,id,res,count);
}
static void ready(void)
{
	mutex_lock(&backend_lock);backend.alloc_error=backend.ddr_error=backend.unit_error=0;mutex_unlock(&backend_lock);
}
static int reprobe(unsigned int index){return device_reprobe(&avr_info_component(index)->dev);}
static bool bound(unsigned int index){return avr_info_component(index)->dev.driver!=NULL;}
static int setup(struct kunit *test)
{
	memset(&backend,0,sizeof(backend));backend.alloc_error=backend.ddr_error=-EPROBE_DEFER;backend.ddr_size=24;backend.unit_size=1024;
	backend.ddr[0]=6;backend.ddr[1]=1;backend.ddr[2]=2;backend.ddr[3]=3;backend.ddr[4]=12288;backend.ddr[5]=9;memset(backend.box,0x5a,sizeof(backend.box));
	return avr_info_start();
}
static void cleanup(struct kunit *test){avr_info_stop();KUNIT_EXPECT_FALSE(test,backend.bad);}
static bool exists(const char *path)
{
	struct file *file=filp_open(path,O_RDONLY,0);if(IS_ERR(file))return false;filp_close(file,NULL);return true;
}
static void read_sysfs(struct kunit *test,const char *path,const char *expected)
{
	struct file *file=filp_open(path,O_RDONLY,0);char buf[128]={};loff_t pos=0;ssize_t count;
	KUNIT_ASSERT_FALSE(test,IS_ERR(file));count=kernel_read(file,buf,sizeof(buf)-1,&pos);filp_close(file,NULL);
	KUNIT_ASSERT_GT(test,count,0);KUNIT_EXPECT_STREQ(test,buf,expected);
}
static void independent_components_defer_without_false_endpoints(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test,bound(0));KUNIT_EXPECT_FALSE(test,bound(1));KUNIT_EXPECT_FALSE(test,bound(2));KUNIT_EXPECT_TRUE(test,bound(3));
	KUNIT_EXPECT_TRUE(test,exists("/sys/storage/type"));KUNIT_EXPECT_FALSE(test,exists("/sys/ram/type"));KUNIT_EXPECT_FALSE(test,exists("/proc/unitinfo"));KUNIT_EXPECT_TRUE(test,exists("/proc/bootinfo"));
	KUNIT_EXPECT_STREQ(test,bi_bootmode(),"normal");KUNIT_EXPECT_STREQ(test,bi_bootreason(),"test-boot");
}
static void real_reprobe_publishes_ready_components(struct kunit *test)
{
	ready();KUNIT_EXPECT_GE(test,reprobe(1),0);KUNIT_EXPECT_GE(test,reprobe(2),0);
	KUNIT_EXPECT_TRUE(test,bound(1));KUNIT_EXPECT_TRUE(test,bound(2));KUNIT_EXPECT_TRUE(test,exists("/proc/unitinfo"));
	read_sysfs(test,"/sys/ram/type","LPDDR5X\n");read_sysfs(test,"/sys/ram/size","12288\n");read_sysfs(test,"/sys/storage/type","UFS\n");
}
static void ddr_error_pointer_and_short_item_do_not_publish(struct kunit *test)
{
	mutex_lock(&backend_lock);backend.ddr_error=-EIO;mutex_unlock(&backend_lock);reprobe(1);
	KUNIT_EXPECT_FALSE(test,bound(1));KUNIT_EXPECT_FALSE(test,exists("/sys/ram/type"));
	mutex_lock(&backend_lock);backend.ddr_error=0;backend.ddr_size=19;mutex_unlock(&backend_lock);reprobe(1);
	KUNIT_EXPECT_FALSE(test,bound(1));KUNIT_EXPECT_FALSE(test,exists("/sys/ram/type"));
	mutex_lock(&backend_lock);backend.ddr_size=24;mutex_unlock(&backend_lock);reprobe(1);KUNIT_EXPECT_TRUE(test,bound(1));
}
static void ddr_sysfs_uses_owned_snapshot(struct kunit *test)
{
	ready();reprobe(1);KUNIT_ASSERT_TRUE(test,bound(1));
	mutex_lock(&backend_lock);backend.ddr[0]=0xfe;mutex_unlock(&backend_lock);
	read_sysfs(test,"/sys/ram/mr5","0x06\n");
}
static void unit_failure_size_checks_preserve_backing(struct kunit *test)
{
	mutex_lock(&backend_lock);backend.alloc_error=-ENOMEM;mutex_unlock(&backend_lock);reprobe(2);KUNIT_EXPECT_FALSE(test,bound(2));
	mutex_lock(&backend_lock);backend.alloc_error=0;backend.unit_error=-EIO;mutex_unlock(&backend_lock);reprobe(2);KUNIT_EXPECT_FALSE(test,bound(2));
	mutex_lock(&backend_lock);backend.unit_error=0;backend.unit_size=512;mutex_unlock(&backend_lock);reprobe(2);KUNIT_EXPECT_FALSE(test,bound(2));
	KUNIT_EXPECT_FALSE(test,exists("/proc/unitinfo"));KUNIT_EXPECT_EQ(test,backend.box[32],(u8)0x5a);
}
static void unit_payload_and_1024_byte_guards(struct kunit *test)
{
	struct mmi_unit_info *info=(void *)(backend.box+32);int i;
	ready();reprobe(2);KUNIT_ASSERT_TRUE(test,bound(2));
	KUNIT_EXPECT_EQ(test,info->version,3U);KUNIT_EXPECT_EQ(test,info->radio,0x7fU);KUNIT_EXPECT_EQ(test,info->system_rev,0x1234U);
	KUNIT_EXPECT_STREQ(test,info->device,"avr-test");KUNIT_EXPECT_STREQ(test,info->barcode,"TESTSERIAL");KUNIT_EXPECT_STREQ(test,info->baseband,"test-baseband");
	for(i=0;i<32;i++){KUNIT_EXPECT_EQ(test,backend.box[i],(u8)0x5a);KUNIT_EXPECT_EQ(test,backend.box[1056+i],(u8)0x5a);}
	for(i=sizeof(*info);i<1024;i++)KUNIT_EXPECT_EQ(test,backend.box[32+i],(u8)0);
}
static void bootarg_exact_keys_quotes_empty_and_stable_values(struct kunit *test)
{
	char *value,*saved;
	KUNIT_ASSERT_EQ(test,avr_info_lookup("androidboot.mode=",&value,"avr-test-lines",true),0);saved=value;KUNIT_EXPECT_STREQ(test,value,"line value");
	KUNIT_ASSERT_EQ(test,avr_info_lookup("first=",&value,"avr-test-lines",true),0);KUNIT_EXPECT_STREQ(test,value,"one");KUNIT_EXPECT_STREQ(test,saved,"line value");
	KUNIT_ASSERT_EQ(test,avr_info_lookup("empty=",&value,"avr-test-lines",true),0);KUNIT_EXPECT_STREQ(test,value,"");
	KUNIT_ASSERT_EQ(test,avr_info_lookup("androidboot.mode=",&value,"avr-test-command",false),0);KUNIT_EXPECT_STREQ(test,value,"command value");
	KUNIT_ASSERT_EQ(test,mmi_get_bootarg("androidboot.radio=",&value),0);KUNIT_EXPECT_STREQ(test,value,"7f");
}
static void malformed_bootarg_properties_and_keys_are_rejected(struct kunit *test)
{
	char *value=(void *)1;
	KUNIT_EXPECT_LT(test,avr_info_lookup("key=",&value,"avr-test-invalid",true),0);KUNIT_EXPECT_PTR_EQ(test,value,NULL);
	KUNIT_EXPECT_EQ(test,avr_info_lookup("key=",&value,"avr-test-unmatched-quote",true),-EINVAL);
	KUNIT_EXPECT_EQ(test,avr_info_lookup("first",&value,"avr-test-lines",true),-EINVAL);
	KUNIT_EXPECT_EQ(test,avr_info_lookup("absent=",&value,"avr-test-lines",true),-ENOENT);
}
static void selector_exact_contains_and_malformed_data(struct kunit *test)
{
	const char *paths[]={"/avr-status-exact","/avr-status-contains","/avr-status-reject","/avr-status-malformed","/avr-status-no-value"};bool expected[]={true,true,false,false,false};int i;
	for(i=0;i<ARRAY_SIZE(paths);i++){struct device_node *node=of_find_node_by_path(paths[i]);KUNIT_ASSERT_NOT_NULL(test,node);KUNIT_EXPECT_EQ(test,mmi_device_is_available(node),expected[i]);of_node_put(node);}
	KUNIT_EXPECT_FALSE(test,mmi_check_dynamic_device_node("avr-status-reject"));KUNIT_EXPECT_TRUE(test,mmi_check_dynamic_device_node("avr-status-exact"));KUNIT_EXPECT_FALSE(test,mmi_check_dynamic_device_node(NULL));
}
static void endpoint_teardown_and_restart_drains_sysfs(struct kunit *test)
{
	ready();reprobe(1);reprobe(2);avr_info_stop();KUNIT_EXPECT_FALSE(test,exists("/sys/storage/type"));KUNIT_EXPECT_FALSE(test,exists("/sys/ram/type"));KUNIT_EXPECT_FALSE(test,exists("/proc/unitinfo"));KUNIT_EXPECT_FALSE(test,exists("/proc/bootinfo"));
	KUNIT_ASSERT_EQ(test,avr_info_start(),0);KUNIT_EXPECT_TRUE(test,bound(0));KUNIT_EXPECT_TRUE(test,bound(1));KUNIT_EXPECT_TRUE(test,bound(2));KUNIT_EXPECT_TRUE(test,bound(3));
}
static void proc_and_sysfs_failures_leave_no_partial_endpoints(struct kunit *test)
{
	ready();
	mutex_lock(&backend_lock);backend.fail_storage_group=true;backend.fail_ram_group=true;backend.fail_unit_proc=true;backend.fail_boot_proc=true;mutex_unlock(&backend_lock);
	reprobe(0);reprobe(1);reprobe(2);reprobe(3);
	KUNIT_EXPECT_FALSE(test,exists("/sys/storage"));KUNIT_EXPECT_FALSE(test,exists("/sys/ram"));KUNIT_EXPECT_FALSE(test,exists("/proc/unitinfo"));KUNIT_EXPECT_FALSE(test,exists("/proc/bootinfo"));
	mutex_lock(&backend_lock);backend.fail_storage_group=false;backend.fail_ram_group=false;backend.fail_unit_proc=false;backend.fail_boot_proc=false;mutex_unlock(&backend_lock);
	reprobe(0);reprobe(1);reprobe(2);reprobe(3);KUNIT_EXPECT_TRUE(test,bound(0));KUNIT_EXPECT_TRUE(test,bound(1));KUNIT_EXPECT_TRUE(test,bound(2));KUNIT_EXPECT_TRUE(test,bound(3));
}
static void registration_failure_unwinds_created_devices(struct kunit *test)
{
	unsigned int i;
	avr_info_stop();
	for(i=1;i<=4;i++) {
		mutex_lock(&backend_lock);backend.registrations=0;backend.fail_registration=i;mutex_unlock(&backend_lock);
		KUNIT_EXPECT_EQ(test,avr_info_start(),-ENOMEM);
		KUNIT_EXPECT_FALSE(test,exists("/sys/storage"));KUNIT_EXPECT_FALSE(test,exists("/sys/ram"));KUNIT_EXPECT_FALSE(test,exists("/proc/unitinfo"));KUNIT_EXPECT_FALSE(test,exists("/proc/bootinfo"));
	}
	mutex_lock(&backend_lock);backend.registrations=0;backend.fail_registration=0;mutex_unlock(&backend_lock);
	KUNIT_ASSERT_EQ(test,avr_info_start(),0);
}
struct reader {struct completion done;const char *key,*expected;int failed;};
static int read_cached_property(void *arg)
{
	struct reader *r=arg;char *value=NULL,*first=NULL;int i;
	for(i=0;i<100;i++) {
		if(avr_info_lookup(r->key,&value,"avr-test-race",true) || strcmp(value,r->expected)){r->failed=1;break;}
		if(!first)first=value;else if(first!=value){r->failed=1;break;}
		cond_resched();
	}
	complete(&r->done);while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static void immutable_cache_is_safe_for_concurrent_readers(struct kunit *test)
{
	struct reader readers[3]={{.key="one=",.expected="1"},{.key="two=",.expected="2"},{.key="three=",.expected="3"}};
	struct task_struct *tasks[3]={};int i,j;
	for(i=0;i<3;i++){init_completion(&readers[i].done);tasks[i]=kthread_run(read_cached_property,&readers[i],"avr_info_reader");if(IS_ERR(tasks[i])){tasks[i]=NULL;KUNIT_FAIL(test,"reader did not start");break;}}
	for(j=0;j<i;j++){KUNIT_EXPECT_NE(test,wait_for_completion_timeout(&readers[j].done,2*HZ),0UL);kthread_stop(tasks[j]);KUNIT_EXPECT_EQ(test,readers[j].failed,0);}
}
static struct kunit_case cases[]={KUNIT_CASE(independent_components_defer_without_false_endpoints),KUNIT_CASE(real_reprobe_publishes_ready_components),KUNIT_CASE(ddr_error_pointer_and_short_item_do_not_publish),KUNIT_CASE(ddr_sysfs_uses_owned_snapshot),KUNIT_CASE(unit_failure_size_checks_preserve_backing),KUNIT_CASE(unit_payload_and_1024_byte_guards),KUNIT_CASE(bootarg_exact_keys_quotes_empty_and_stable_values),KUNIT_CASE(malformed_bootarg_properties_and_keys_are_rejected),KUNIT_CASE(selector_exact_contains_and_malformed_data),KUNIT_CASE(endpoint_teardown_and_restart_drains_sysfs),KUNIT_CASE(proc_and_sysfs_failures_leave_no_partial_endpoints),KUNIT_CASE(registration_failure_unwinds_created_devices),KUNIT_CASE(immutable_cache_is_safe_for_concurrent_readers),{}};
static struct kunit_suite suite={.name="avr-mmi-info-source",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
