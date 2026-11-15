/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define __user
#define unlikely(x) (x)
#define likely(x) (x)
#define ts_err(...) ((void)0)
#define ts_info(...) ((void)0)
#define ts_debug(...) ((void)0)
#define GOODIX_THP_MAX_FRAME_LEN (10 * 1024)
#define GOODIX_THP_MAX_FRAME_BUF_COUNT 20
#define WAKEUP_STATE 1
#define KOBJ_CHANGE 0
#define PEN_MESSAGE_BLE_MAC 1
#define PEN_MESSAGE_BATTERY 2
#define PEN_MESSAGE_PEN_INFO 3
#define PEN_MESSAGE_PEN_QPID 4
#define GET_FRAME_NONBLOCK_MODE 1
#define WAIT_STATE 0
#define wait_event_interruptible(q,c) (-EINTR)
#define wait_event_interruptible_timeout(q,c,t) (-EINTR)
#define msecs_to_jiffies(t) (t)
#define ktime_get_real_ns() 123000ull
#define ATOMIC_INIT(x) (x)
typedef unsigned int atomic_t;
static unsigned int atomic_fetch_inc(atomic_t *n) { return (*n)++; }
@STRUCTS@
struct thp_frame_mmap_list { char *buf; u32 head,tail; };
struct thp_ts_device { int dev; struct { char thp_ver[100]; } board_data; };
struct platform_device { struct { int kobj; } dev; };
struct goodix_thp_core {
 struct thp_ts_device *ts_dev; struct platform_device *pdev;
 int frame_mutex,frame_wq,frame_waitq_state,get_frame_wait_mode,frame_wait_time;
 struct thp_frame_mmap_list frame_mmap_list;
 u8 ble_mac[6],pen_info[9],open_status,battery_level,quick_pid;
 int uevent_message_type;
};
static int locked,wakes,events,esd_calls,fail_copy;
static void mutex_lock(int *m) { (void)m;assert(!locked);locked=1; }
static void mutex_unlock(int *m) { (void)m;assert(locked);locked=0; }
static void wake_up_interruptible(int *q) { (void)q;assert(locked);wakes++; }
static unsigned long copy_from_user(void *dst,const void *src,size_t n)
{ if(fail_copy)return n;memcpy(dst,src,n);return 0; }
static unsigned long copy_to_user(void *dst,const void *src,size_t n)
{ if(fail_copy)return n;memcpy(dst,src,n);return 0; }
static void kobject_uevent(int *k,int action) { (void)k;(void)action;events++; }
static void goodix_thp_esd_on(struct goodix_thp_core *cd,bool on) { (void)cd;assert(on);esd_calls++; }
@FUNCTIONS@
static unsigned int checks;
#define CHECK(x) do { assert(x);checks++; } while(0)
int main(void)
{
 struct thp_ts_device dev={};struct platform_device pdev={};
 struct goodix_thp_core cd={.ts_dev=&dev,.pdev=&pdev};
 size_t bytes=GOODIX_THP_MAX_FRAME_LEN*GOODIX_THP_MAX_FRAME_BUF_COUNT;
 char *storage=malloc(bytes+32);assert(storage);memset(storage,0xa5,bytes+32);cd.frame_mmap_list.buf=storage+16;
 u8 data[GOODIX_THP_MAX_FRAME_LEN];memset(data,0x3c,sizeof(data));
 put_frame_list(&cd,1,data,-1);CHECK(!wakes && !cd.frame_mmap_list.tail);
 put_frame_list(&cd,1,data,INT_MAX);CHECK(!wakes);
 put_frame_list(&cd,1,NULL,1);CHECK(!wakes);
 put_frame_list(&cd,1,data,GOODIX_THP_MAX_FRAME_LEN-sizeof(struct driver_request_pkg)+1);CHECK(!wakes);
 put_frame_list(&cd,1,data,GOODIX_THP_MAX_FRAME_LEN-sizeof(struct driver_request_pkg));CHECK(wakes==1 && cd.frame_mmap_list.tail==1);
 struct driver_request_pkg *pkg=(void*)(storage+16);CHECK(pkg->size==GOODIX_THP_MAX_FRAME_LEN-4 && pkg->request.id==0 && !memcmp(pkg->request.data,data,GOODIX_THP_MAX_FRAME_LEN-sizeof(*pkg)));
 for(size_t i=0;i<16;i++) CHECK((u8)storage[i]==0xa5 && (u8)storage[bytes+16+i]==0xa5);
 cd.frame_mmap_list.head=0;cd.frame_mmap_list.tail=19;
 put_frame_list(&cd,2,NULL,0);CHECK(cd.frame_mmap_list.head==1 && cd.frame_mmap_list.tail==0 && wakes==2);
 cd.frame_mmap_list.head=20;put_frame_list(&cd,1,data,1);CHECK(wakes==2 && !locked);
 cd.frame_mmap_list.head=0;cd.frame_mmap_list.tail=20;put_frame_list(&cd,1,data,1);CHECK(wakes==2 && !locked);
 cd.frame_mmap_list.tail=1;cd.frame_mmap_list.buf=NULL;put_frame_list(&cd,1,data,1);CHECK(wakes==2 && !locked);cd.frame_mmap_list.buf=storage+16;
 struct thp_ioctl_frame frame;memset(&frame,0xa5,sizeof(frame));
 CHECK(goodix_thp_ioctl_get_frame(&cd,(unsigned long)&frame)==0 && frame.pos==0 && frame.size==0 && frame.tv_us==123);
 fail_copy=1;CHECK(goodix_thp_ioctl_get_frame(&cd,(unsigned long)&frame)==-EFAULT && !locked);fail_copy=0;
 cd.get_frame_wait_mode=GET_FRAME_NONBLOCK_MODE;cd.frame_mmap_list.tail=0;
 CHECK(goodix_thp_ioctl_get_frame(&cd,(unsigned long)&frame)==-ENODATA && !locked);
 struct thp_ioctl_tsc_msg msg={.cmd=SVC_CMD_UPDATE_VERSION};
 for(unsigned int n=0;n<=99;n++) {
  memset(msg.value,'A',sizeof(msg.value));msg.len=n;
  CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==0);
  CHECK(strlen(dev.board_data.thp_ver)==n);
 }
 msg.len=100;memset(msg.value,'B',sizeof(msg.value));
 CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==-EINVAL);
 msg.value[99]=0;CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==0 && strlen(dev.board_data.thp_ver)==99);
 unsigned int bad[]={101,128,129,UINT16_MAX};
 char before[100];memcpy(before,dev.board_data.thp_ver,100);
 for(unsigned int i=0;i<4;i++){msg.len=bad[i];CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==-EINVAL && !memcmp(before,dev.board_data.thp_ver,100));}
 msg.cmd=SVC_CMD_GAME_FILTER;msg.len=128;CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==0);
 msg.len=129;CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==-EINVAL);
 msg.cmd=SVC_CMD_BATTERY;msg.value[0]=50;msg.len=129;CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==-EINVAL && !events);
 msg.len=1;CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==0 && events==1 && cd.battery_level==50);
 fail_copy=1;CHECK(goodix_thp_ioctl_recv_tsc_msg(&cd,(unsigned long)&msg)==-EFAULT && events==1);
 CHECK(!locked);free(storage);
 printf("PASS %u production payload assertions; recorded primitives only\n",checks);return 0;
}
