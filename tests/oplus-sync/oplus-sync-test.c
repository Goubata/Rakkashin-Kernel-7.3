// SPDX-License-Identifier: GPL-2.0-only
/* Exercise the ROM's existing ioctl layout, including its deadline field order. */
#include <errno.h>
#include <fcntl.h>
#include <linux/sync_file.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

struct create { uint32_t value; char name[32]; int32_t fence; };
struct deadline { uint64_t ns; uint32_t fd, pad; };
#define CREATE _IOWR('W', 0, struct create)
#define INC _IOW('W', 1, uint32_t)
#define DEADLINE _IOWR('W', 2, struct deadline)
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL line %d: %s (errno %d)\n",__LINE__,#x,errno); exit(1); } } while (0)
static int timeline(void) { int fd=open("/dev/oplus_sync",O_RDWR|O_CLOEXEC); CHECK(fd>=0); return fd; }
static int fence(int t,uint32_t value) { struct create c={.value=value,.name="avr-test",.fence=-1}; CHECK(ioctl(t,CREATE,&c)==0); CHECK(c.fence>=0); CHECK(fcntl(c.fence,F_GETFD)&FD_CLOEXEC); return c.fence; }
static void inc(int t,uint32_t value) { CHECK(ioctl(t,INC,&value)==0); }
static int ready(int fd) { struct pollfd p={.fd=fd,.events=POLLIN}; int r=poll(&p,1,0); CHECK(r>=0); return r!=0; }
static int status(int fd) { struct sync_file_info info={0}; CHECK(ioctl(fd,SYNC_IOC_FILE_INFO,&info)==0); return info.status; }
int main(void)
{
 int a=timeline(),b=timeline(),x=fence(a,2),dup=fence(a,2),y=fence(b,2),zero=fence(a,0);
 CHECK(ready(zero)); CHECK(status(zero)==1); close(zero);
 CHECK(!ready(x)); CHECK(status(x)==0); inc(a,1); CHECK(!ready(x));
 inc(a,1); CHECK(ready(x)); CHECK(ready(dup)); CHECK(!ready(y));
 CHECK(status(x)==1); close(x); close(dup);
 CHECK(ioctl(a,INC,(void *)1)==-1 && errno==EFAULT);
 CHECK(ioctl(a,CREATE,(void *)1)==-1 && errno==EFAULT);
 CHECK(ioctl(a,_IO('W',127),0)==-1 && errno==ENOTTY);
 struct deadline d={.fd=y};
 CHECK(ioctl(b,DEADLINE,&d)==-1 && errno==ENOENT);
 d.pad=1; CHECK(ioctl(b,DEADLINE,&d)==-1 && errno==EINVAL);
 d.pad=0;d.fd=0xffffffff;CHECK(ioctl(b,DEADLINE,&d)==-1 && errno==EINVAL);
 /* Closed timeline reports an error to pending fence users. */
 close(b); CHECK(ready(y)); CHECK(status(y)==-ENOENT);
 close(a);
 /* A fence fd keeps module text alive even after both timeline fds close. */
 CHECK(syscall(SYS_delete_module,"avr_oplus_sync",O_NONBLOCK)==-1 && errno==EWOULDBLOCK);
 close(y);
 /* Exercise signed sequence wrap and the > INT_MAX increment path. */
 a=timeline(); inc(a,0x7fffffffU); inc(a,0x7ffffffeU); x=fence(a,0);
 CHECK(!ready(x)); inc(a,3); CHECK(ready(x)); close(x); close(a);
 a=timeline(); inc(a,0xffffffffU); x=fence(a,0); CHECK(!ready(x)); inc(a,1);CHECK(ready(x));close(x);close(a);
 /* Repeated out-of-order creates, duplicate values, close and cancellation. */
 for(int i=0;i<64;i++) { a=timeline(); x=fence(a,9); y=fence(a,3); int z=fence(a,6); inc(a,6); CHECK(ready(y));CHECK(ready(z));CHECK(!ready(x));close(y);close(z);close(a);CHECK(status(x)==-ENOENT);close(x); }
 CHECK(syscall(SYS_delete_module,"avr_oplus_sync",O_NONBLOCK)==0);
 printf("AVR_OPLUS_SYNC_PASS checks=%d\n",checks);
 return 0;
}
