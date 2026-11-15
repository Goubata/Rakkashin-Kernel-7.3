/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned int checks;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d errno %d\n",__LINE__,errno);return 1;}checks++; } while(0)
int main(void)
{
        long page=sysconf(_SC_PAGESIZE);size_t bytes=10*1024*20;
        int fd=open("/dev/avr_goodix_mmap",O_RDWR);CHECK(fd>=0 && page==4096);
        unsigned char *all=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);CHECK(all!=MAP_FAILED);
        for(size_t i=0;i<bytes;i++)CHECK(all[i]==0);
        all[0]=0x17;all[bytes-1]=0x69;
        unsigned char *last=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_SHARED,fd,bytes-page);CHECK(last!=MAP_FAILED && last[page-1]==0x69);
        last[0]=0x5a;CHECK(all[bytes-page]==0x5a);
        errno=0;CHECK(mmap(NULL,page,PROT_READ,MAP_SHARED,fd,bytes)==MAP_FAILED && errno==EINVAL);
        errno=0;CHECK(mmap(NULL,page*2,PROT_READ,MAP_SHARED,fd,bytes-page)==MAP_FAILED && errno==EINVAL);
        errno=0;CHECK(mmap(NULL,page,PROT_READ,MAP_SHARED,fd,(off_t)0x7ffffffffffff000LL)==MAP_FAILED && errno==EINVAL);
        CHECK(ioctl(fd,1,0)==0); /* Drop actual allocation-owner page refs. */
        CHECK(all[0]==0x17 && all[bytes-1]==0x69 && last[0]==0x5a);
        errno=0;CHECK(mmap(NULL,page,PROT_READ,MAP_SHARED,fd,0)==MAP_FAILED && errno==ENODEV);
        CHECK(ioctl(fd,1,0)==0); /* Idempotent owner detach. */
        CHECK(mprotect(all+page,page,PROT_READ)==0); /* Split VMA retains pages. */
        pid_t child=fork();CHECK(child>=0);
        if(child==0){if(all[0]!=0x17 || last[page-1]!=0x69)_exit(1);last[page-1]=0x33;_exit(0);}
        int status;CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
        CHECK(all[bytes-1]==0x33);
        CHECK(close(fd)==0);CHECK(all[0]==0x17);
        CHECK(munmap(all,bytes)==0);CHECK(last[page-1]==0x33);CHECK(munmap(last,page)==0);
        printf("AVR_GOODIX_MMAP_USER_PASS assertions=%u scope=real-vm-selected-source-only\n",checks);
        return 0;
}
