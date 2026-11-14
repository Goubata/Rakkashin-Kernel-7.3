// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define WORKERS 4
#define BLOCKS 128
#define SIZE 4096
static int fd;
static unsigned int seed;
static atomic_int errors;
static void *worker(void *arg)
{
 uintptr_t id=(uintptr_t)arg;
 unsigned char *writebuf=NULL,*readbuf=NULL;
 if (posix_memalign((void **)&writebuf,SIZE,SIZE) || posix_memalign((void **)&readbuf,SIZE,SIZE)) {
  free(writebuf);free(readbuf);atomic_fetch_add(&errors,1);return NULL;
 }
 for (unsigned int block=0;block<BLOCKS;block++) {
  off_t off=(id*BLOCKS+block)*SIZE;
  for (unsigned int i=0;i<SIZE;i++)writebuf[i]=(i*17+block*13+id*37+seed*79)&255;
  memset(readbuf,0,SIZE);
  if (pwrite(fd,writebuf,SIZE,off)!=SIZE || pread(fd,readbuf,SIZE,off)!=SIZE || memcmp(writebuf,readbuf,SIZE)) {
   fprintf(stderr,"AVR_ADIOS_IO_FAIL worker=%lu block=%u errno=%d\n",(unsigned long)id,block,errno);
   atomic_fetch_add(&errors,1);break;
  }
 }
 free(writebuf);free(readbuf);return NULL;
}
int main(int argc,char **argv)
{
 pthread_t threads[WORKERS];unsigned int started=0;
 seed=argc==2 ? strtoul(argv[1],NULL,10) : 1;
 fd=open("/dev/nullb0",O_RDWR|O_DIRECT|O_CLOEXEC);
 if (fd<0){perror("AVR_ADIOS_IO_FAIL open");return 1;}
 for (uintptr_t i=0;i<WORKERS;i++) {
  if (pthread_create(&threads[i],NULL,worker,(void *)i)){atomic_fetch_add(&errors,1);break;}
  started++;
 }
 for (unsigned int i=0;i<started;i++)if(pthread_join(threads[i],NULL))atomic_fetch_add(&errors,1);
 if (fdatasync(fd))atomic_fetch_add(&errors,1);
 close(fd);
 if (atomic_load(&errors))return 1;
 printf("AVR_ADIOS_IO_PASS seed=%u bytes_written=%u workers=%u\n",seed,WORKERS*BLOCKS*SIZE,WORKERS);return 0;
}
