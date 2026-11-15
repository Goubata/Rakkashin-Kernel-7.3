/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint16_t __le16;
#define U32_MAX UINT32_MAX
#define unlikely(x) (x)
#define ts_err(...) ((void)0)
@CONSTANTS@
struct driver_request_pkg { u32 size;struct {u32 id,type;}request; };
struct goodix_thp_core { bool pm_suspend; };
struct platform_device { struct goodix_thp_core *data; };
struct spi_device { struct platform_device *data; };
struct thp_ts_device {struct spi_device *spi_dev;u8 *tx_buff,*rx_buff;int spi_mutex;struct {u32 chip_type,frame_addr;}board_data;};
struct spi_transfer { void *tx_buf,*rx_buf;unsigned int len;bool cs_change; };
struct spi_message {struct spi_transfer *x;};
static struct platform_device *spi_get_drvdata(struct spi_device *d){return d->data;}
static struct goodix_thp_core *platform_get_drvdata(struct platform_device *p){return p->data;}
static int locked,calls,fail_call;static u16 wire_length;static bool bad_checksum;
static u16 le16_to_cpup(const __le16 *v){u16 n;memcpy(&n,v,2);return n;}
static void mutex_lock(int *m){(void)m;assert(!locked);locked=1;}
static void mutex_unlock(int *m){(void)m;assert(locked);locked=0;}
static void spi_message_init(struct spi_message *m){m->x=NULL;}
static void spi_message_add_tail(struct spi_transfer *x,struct spi_message *m){assert(!m->x);m->x=x;}
static int spi_sync(struct spi_device *spi,struct spi_message *msg)
{
 (void)spi;assert(locked);calls++;if(calls==fail_call)return -ETIMEDOUT;
 struct spi_transfer *x=msg->x;assert(x && x->len<=GOODIX_SPI_BUFF_MAX_SIZE);
 u8 *tx=x->tx_buf,*rx=x->rx_buf;
 if(tx[0]==SPI_FLAG_RD){
  unsigned int skip=x->cs_change?9:8;memset(rx,0,x->len);
  if(x->len==FRAME_HEAD_LEN+skip){u8 *p=rx+skip;p[0]=0x91;p[6]=wire_length&255;p[7]=wire_length>>8;u16 sum=0;for(int i=0;i<12;i++)sum+=p[i];if(bad_checksum)sum++;memcpy(p+12,&sum,2);}
  else memset(rx+skip,0x4d,x->len-skip);
 }
 return 0;
}
@FUNCTIONS@
static unsigned int checks;
#define CHECK(x) do {assert(x);assert(!locked);checks++;}while(0)
int main(void)
{
 struct goodix_thp_core core={};struct platform_device pdev={&core};struct spi_device spi={&pdev};
 u8 tx[GOODIX_SPI_BUFF_MAX_SIZE+16],rx[GOODIX_SPI_BUFF_MAX_SIZE+16],buf[GOODIX_THP_MAX_FRAME_LEN+16];
 struct thp_ts_device dev={.spi_dev=&spi,.tx_buff=tx,.rx_buff=rx,.board_data={CHIP_TYPE_9897,0x1000}};
 for(int chip=0;chip<2;chip++){
  dev.board_data.chip_type=chip?CHIP_TYPE_9916:CHIP_TYPE_9897;
  unsigned int overhead=chip?8:9;memset(tx,0xa5,sizeof(tx));memset(rx,0xa5,sizeof(rx));
  CHECK(goodix_thp_spi_read(&dev,0x1000,buf,0)==-EINVAL);
  CHECK(goodix_thp_spi_read(&dev,0x1000,buf,UINT_MAX)==-EINVAL);
  CHECK(goodix_thp_spi_read(&dev,0x1000,buf,GOODIX_SPI_BUFF_MAX_SIZE-overhead+1)==-EINVAL);
  CHECK(goodix_thp_spi_read(&dev,0x1000,NULL,1)==-EINVAL);
  /* Large boundary needs a caller buffer of the full transport capacity. */
  u8 *big=malloc(GOODIX_SPI_BUFF_MAX_SIZE);assert(big);
  CHECK(goodix_thp_spi_read(&dev,0x1000,big,GOODIX_SPI_BUFF_MAX_SIZE-overhead)==0);
  CHECK(rx[GOODIX_SPI_BUFF_MAX_SIZE]==0xa5 && tx[GOODIX_SPI_BUFF_MAX_SIZE]==0xa5);
  memset(big,0x5a,GOODIX_SPI_BUFF_MAX_SIZE);
  CHECK(goodix_thp_spi_write(&dev,0x1000,big,GOODIX_SPI_BUFF_MAX_SIZE-5)==0);
  CHECK(tx[GOODIX_SPI_BUFF_MAX_SIZE]==0xa5);
  CHECK(goodix_thp_spi_write(&dev,0x1000,big,GOODIX_SPI_BUFF_MAX_SIZE-4)==-EINVAL);
  CHECK(goodix_thp_spi_write(&dev,0x1000,NULL,1)==-EINVAL);
  free(big);
  for(unsigned int len=0;len<FRAME_HEAD_LEN;len++){calls=0;wire_length=len;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==-EINVAL && calls==1);}
  calls=0;wire_length=FRAME_HEAD_LEN;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==FRAME_HEAD_LEN && calls==1);
  calls=0;wire_length=GOODIX_THP_MAX_FRAME_LEN-sizeof(struct driver_request_pkg);CHECK(goodix_thp_get_frame(&dev,(char*)buf)==wire_length && calls==2 && buf[wire_length-1]==0x4d);
  calls=0;wire_length++;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==-EINVAL && calls==1);
  calls=0;wire_length=100;fail_call=1;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==-ETIMEDOUT && calls==1);
  calls=0;fail_call=2;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==-ETIMEDOUT && calls==2);fail_call=0;
  bad_checksum=true;calls=0;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==-EINVAL && calls==1);bad_checksum=false;
 }
 CHECK(goodix_thp_get_frame(NULL,(char*)buf)==-EINVAL);
 CHECK(goodix_thp_get_frame(&dev,NULL)==-EINVAL);
 dev.board_data.frame_addr=UINT_MAX;CHECK(goodix_thp_get_frame(&dev,(char*)buf)==-EINVAL);
 spi.data=NULL;CHECK(goodix_thp_spi_read(&dev,0x1000,buf,1)==-ENODEV);CHECK(goodix_thp_spi_write(&dev,0x1000,buf,1)==-ENODEV);
 spi.data=&pdev;core.pm_suspend=true;CHECK(goodix_thp_spi_read(&dev,0x1000,buf,1)==-EINVAL);CHECK(goodix_thp_spi_write(&dev,0x1000,buf,1)==-EINVAL);
 printf("PASS %u wire source-method assertions; recorded SPI only\n",checks);return 0;
}
