/* SPDX-License-Identifier: GPL-2.0-only */
/* Exact production function is inserted by run.py. No SPI hardware is accessed. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef uint8_t u8;
typedef uint32_t u32;
#define __user
#define GFP_KERNEL 0
#define GOODIX_SPI_SPEED_WAKEUP 3500
struct spi_transfer { const void *tx_buf; void *rx_buf; unsigned int len; bool cs_change; };
struct spi_message { struct spi_transfer *items[5120]; unsigned int n; };
struct spi_device { int unused; };
struct thp_ts_device;
struct ops { int (*set_spi_speed)(struct thp_ts_device *, u32); };
struct thp_ts_device { struct spi_device *spi_dev; int spi_mutex; struct { struct { u32 spi_max_speed; } spi_setting; } board_data; struct ops *hw_ops; };
struct goodix_thp_core { struct thp_ts_device *ts_dev; };
/* UAPI structs are inserted directly from the production header. */
@STRUCTS@
static int alloc_calls, fail_alloc, live, in_calls, out_calls, fail_in, fail_out;
static int sync_calls, sync_error, speed_calls, fail_speed, locked;
static u32 speeds[2];
static void *kcalloc(size_t n, size_t size, int flags)
{
        (void)flags;
        if (++alloc_calls == fail_alloc) return NULL;
        void *p = calloc(n, size); if (p) live++; return p;
}
static void *kzalloc(size_t size, int flags) { return kcalloc(1, size, flags); }
static void kfree(void *p) { if (p) { live--; free(p); } }
static unsigned long copy_from_user(void *to, const void *from, size_t n)
{
        if (++in_calls == fail_in) return n;
        assert(from); memcpy(to, from, n); return 0;
}
static unsigned long copy_to_user(void *to, const void *from, size_t n)
{
        if (++out_calls == fail_out) return n;
        assert(to); memcpy(to, from, n); return 0;
}
static void mutex_lock(int *m) { (void)m; assert(!locked); locked=1; }
static void mutex_unlock(int *m) { (void)m; assert(locked); locked=0; }
static void spi_message_init(struct spi_message *m) { m->n=0; }
static void spi_message_add_tail(struct spi_transfer *x, struct spi_message *m) { assert(m->n<5120); m->items[m->n++]=x; }
static int spi_sync(struct spi_device *spi, struct spi_message *m)
{
        (void)spi; assert(locked); sync_calls++;
        if(sync_error) return sync_error;
        for(unsigned int i=0;i<m->n;i++) memcpy(m->items[i]->rx_buf,m->items[i]->tx_buf,m->items[i]->len);
        return 0;
}
static int set_speed(struct thp_ts_device *dev, u32 speed)
{
        (void)dev; assert(locked); assert(speed_calls<2); speeds[speed_calls++]=speed;
        return speed_calls==fail_speed ? -EIO : 0;
}
@FUNCTION@
static struct ops hw_ops={set_speed};
static struct spi_device spi;
static struct thp_ts_device dev={.spi_dev=&spi,.board_data.spi_setting.spi_max_speed=10000000,.hw_ops=&hw_ops};
static struct goodix_thp_core core={&dev};
static unsigned int checks;
static void reset(void)
{
        assert(!live && !locked);
        alloc_calls=fail_alloc=in_calls=out_calls=fail_in=fail_out=sync_calls=sync_error=speed_calls=fail_speed=0;
}
static void expect(struct thp_ioctl_multi_spi_trans_data *m, int result)
{
        assert(goodix_thp_ioctl_multi_spi_trans(&core,m)==result);
        assert(!live && !locked); checks++;
}
int main(void)
{
        char tx[5120],rx[5120];memset(tx,0x5a,sizeof(tx));
        struct thp_ioctl_spi_xfer_data x[2]={{.tx=tx,.rx=rx,.len=2560},{.tx=tx+2560,.rx=rx+2560,.len=2560}};
        struct thp_ioctl_multi_spi_trans_data m={.xfer_num=2,.xfer_data=x};
        reset();expect(&m,0);assert(sync_calls==1 && out_calls==2 && !memcmp(tx,rx,5120));
        reset();m.xfer_num=0;expect(&m,-EINVAL);assert(!alloc_calls);
        reset();m.xfer_num=UINT_MAX;expect(&m,-EINVAL);assert(!alloc_calls);m.xfer_num=2;
        reset();x[0].len=0;expect(&m,-EINVAL);assert(in_calls==2 && !sync_calls);
        reset();x[0].len=UINT_MAX;expect(&m,-EINVAL);assert(in_calls==2 && !sync_calls);
        reset();x[0].len=2560;x[1].len=2561;expect(&m,-EINVAL);assert(in_calls==2 && !sync_calls);x[1].len=2560;
        for(int i=1;i<=4;i++){reset();fail_alloc=i;expect(&m,-ENOMEM);assert(!sync_calls);}
        for(int i=1;i<=4;i++){reset();fail_in=i;expect(&m,-EFAULT);assert(!sync_calls);}
        for(int i=1;i<=2;i++){reset();fail_out=i;expect(&m,-EFAULT);assert(out_calls==i);}
        reset();sync_error=-EIO;expect(&m,-EIO);assert(!out_calls);
        reset();x[0].tx=NULL;x[0].rx=NULL;expect(&m,0);assert(out_calls==1);x[0].tx=tx;x[0].rx=rx;
        m.speed_hz=GOODIX_SPI_SPEED_WAKEUP;
        reset();expect(&m,0);assert(speed_calls==2 && sync_calls==1 && !out_calls && speeds[0]==GOODIX_SPI_SPEED_WAKEUP && speeds[1]==10000000);
        reset();fail_speed=1;expect(&m,-EIO);assert(speed_calls==2 && !sync_calls && !out_calls);
        reset();fail_speed=2;expect(&m,-EIO);assert(speed_calls==2 && sync_calls==1 && !out_calls);
        reset();sync_error=-ETIMEDOUT;fail_speed=2;expect(&m,-ETIMEDOUT);assert(speed_calls==2 && !out_calls);
        printf("PASS %u production multi-SPI cases; recorded primitives only\n",checks);
        return 0;
}
