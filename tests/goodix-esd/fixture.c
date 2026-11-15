/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define to_delayed_work(p) container_of(p,struct delayed_work,work)
#define ts_err(...) ((void)0)
#define GOODIX_ESD_CHECK_INTERVAL 800
#define GOODIX_ESD_TICK_WRITE_DATA 0xaa
struct work_struct { int dummy; };
struct delayed_work { struct work_struct work; bool initialized; void (*fn)(struct work_struct *); };
#define INIT_DELAYED_WORK(p,func) do { (p)->initialized=true;(p)->fn=func; } while(0)
struct thp_ts_device;
struct hw_ops { int (*read)(struct thp_ts_device *,u32,u8 *,int);int (*write)(struct thp_ts_device *,u32,u8 *,int); };
struct thp_ts_device { struct hw_ops *hw_ops; struct {u32 esd_addr;bool esd_enable;}board_data; };
struct goodix_thp_core { struct delayed_work esd_work; struct thp_ts_device *ts_dev;bool esd_on,suspended,pm_suspend; };
static int reads,writes,off,on,queues,delay,read_error;
static u8 read_value;
static int read_item(struct thp_ts_device *dev,u32 addr,u8 *buf,int len)
{assert(dev && addr==0x1234 && len==1);reads++;if(read_error)return read_error;*buf=read_value;return 0;}
static int write_item(struct thp_ts_device *dev,u32 addr,u8 *buf,int len)
{assert(dev && addr==0x1234 && len==1 && *buf==0xaa);writes++;return 0;}
static int goodix_thp_power_off(struct goodix_thp_core *cd){assert(cd);off++;return 0;}
static int goodix_thp_power_on(struct goodix_thp_core *cd){assert(cd);on++;return 0;}
static void msleep(int ms){assert(ms==200);delay++;}
static bool schedule_delayed_work(struct delayed_work *w,int ticks){assert(w->initialized && ticks==800);queues++;return true;}
@FUNCTIONS@
static unsigned int assertions;
#define CHECK(x) do {assert(x);assertions++;}while(0)
int main(void)
{
 struct hw_ops ops={read_item,write_item};struct thp_ts_device dev={.hw_ops=&ops,.board_data={0x1234,true}};
 struct goodix_thp_core cd={.ts_dev=&dev};
 CHECK(goodix_thp_esd_init(&cd)==0 && cd.esd_work.initialized && !queues);
 cd.esd_on=true;
 read_error=-ETIMEDOUT;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==1 && !writes && !off && !on && queues==1);
 read_error=-EIO;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==2 && !writes && !off && !on && queues==2);
 read_error=0;read_value=0;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==3 && writes==1 && !off && queues==3);
 read_value=0xaa;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==4 && writes==1 && off==1 && on==1 && delay==1 && queues==4);
 cd.suspended=true;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==4 && queues==4);
 cd.suspended=false;cd.pm_suspend=true;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==4 && queues==4);
 cd.pm_suspend=false;cd.esd_on=false;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==4 && queues==4);
 cd.esd_on=true;dev.board_data.esd_addr=0;goodix_thp_esd_work(&cd.esd_work.work);CHECK(reads==4 && queues==4);
 memset(&cd.esd_work,0,sizeof(cd.esd_work));dev.board_data.esd_enable=false;
 CHECK(goodix_thp_esd_init(&cd)==0 && cd.esd_work.initialized && !cd.esd_work.work.dummy && queues==4);
 printf("PASS %u ESD source-method assertions; recorded primitives only\n",assertions);return 0;
}
