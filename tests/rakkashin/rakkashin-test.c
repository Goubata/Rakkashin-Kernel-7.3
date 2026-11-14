// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/ntsync.h>
#include "../../ports/rakkashin/encore-fas/include/uapi/encore_fas_uapi.h"
static unsigned int checks;
static void check(int ok,const char *what)
{
 checks++;
 if (!ok) {fprintf(stderr,"AVR_RAKKASHIN_FAIL %s errno=%d\n",what,errno);exit(1);}
}
static void read_text(const char *path,char *buf,size_t size)
{
 int fd=open(path,O_RDONLY);ssize_t n;
 check(fd>=0,"open read");n=read(fd,buf,size-1);close(fd);
 check(n>=0,"read");buf[n]=0;
}
static int write_text(const char *path,const char *value)
{
 int fd=open(path,O_WRONLY);int n,saved;
 if (fd<0)return -1;
 n=write(fd,value,strlen(value));saved=errno;close(fd);errno=saved;
 return n;
}
static void profiles(void)
{
 const char *modes[]={"battery","balanced","performance","game","extreme"};
 unsigned int mult[]={1,2,4,4,4},wb[]={3000,1500,1500,1500,1500};
 char buf[4096],expected[80];
 for (unsigned int i=0;i<5;i++) {
  check(write_text("/sys/kernel/avr_profile/mode",modes[i])==(int)strlen(modes[i]),"mode write");
  read_text("/sys/kernel/avr_profile/mode",buf,sizeof(buf));
  snprintf(expected,sizeof(expected),"[%s]",modes[i]);check(strstr(buf,expected)!=NULL,"active mode");
  read_text("/proc/sys/kernel/sched_pelt_multiplier",buf,sizeof(buf));check(strtoul(buf,NULL,10)==mult[i],"PELT mode");
  read_text("/proc/sys/vm/dirty_writeback_centisecs",buf,sizeof(buf));check(strtoul(buf,NULL,10)==wb[i],"writeback mode");
 }
 check(write_text("/sys/kernel/avr_profile/mode","invalid")<0 && errno==EINVAL,"invalid mode");
 check(write_text("/proc/sys/kernel/sched_pelt_multiplier","3")<0 && errno==EINVAL,"invalid PELT");
 read_text("/proc/sys/kernel/sched_pelt_multiplier",buf,sizeof(buf));check(strtoul(buf,NULL,10)==4,"PELT unchanged");
 check(write_text("/sys/kernel/avr_profile/app_profiles","10001:game 10002:battery")>0,"app table");
 check(write_text("/sys/kernel/avr_profile/app_profiles","10001:invalid")<0 && errno==EINVAL,"invalid app mode");
 read_text("/sys/kernel/avr_profile/app_profiles",buf,sizeof(buf));check(strstr(buf,"10001:game") && strstr(buf,"10002:battery"),"app table unchanged");
 check(write_text("/sys/kernel/avr_profile/smart_cap","49")<0 && errno==EINVAL,"cap bounds");
 check(write_text("/sys/kernel/avr_profile/sustained_temp","51")<0 && errno==EINVAL,"temperature bounds");
 check(write_text("/sys/kernel/avr_profile/power_limit","1")<0 && errno==EINVAL,"power bounds");
 check(write_text("/sys/kernel/avr_profile/mode","balanced")>0,"restore mode");
}
static void launch_boost(void)
{
 pid_t child=fork();int status;
 check(child>=0,"launch fork");
 if (!child) {
  struct sched_attr attr={.size=sizeof(attr)};
  pid_t grand;int result;
  if (setresuid(10003,10003,10003))_exit(2);
  if (syscall(SYS_sched_getattr,0,&attr,sizeof(attr),0) || attr.sched_util_min!=1024 || !(attr.sched_flags&SCHED_FLAG_RESET_ON_FORK))_exit(3);
  grand=fork();if (grand<0)_exit(4);
  if (!grand) {
   struct sched_attr inherited={.size=sizeof(inherited)};
   _exit(syscall(SYS_sched_getattr,0,&inherited,sizeof(inherited),0) || inherited.sched_util_min!=0 ? 5 : 0);
  }
  if (waitpid(grand,&result,0)!=grand || !WIFEXITED(result) || WEXITSTATUS(result))_exit(6);
  usleep(2500000);
  if (syscall(SYS_sched_getattr,0,&attr,sizeof(attr),0) || attr.sched_util_min!=0 || (attr.sched_flags&SCHED_FLAG_RESET_ON_FORK))_exit(7);
  _exit(0);
 }
 check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"launch clamp, fork isolation and expiry");
 puts("AVR_LAUNCH_BOOST_PASS checks=4");
}
static void app_selection(void)
{
 char buf[4096],pidbuf[40],ready;int pipefd[2],status;pid_t child;
 check(!mkdir("/cpuset",0755),"cpuset directory");
 check(!mount("none","/cpuset","cgroup",0,"cpuset,noprefix"),"cpuset mount");
 check(!mkdir("/cpuset/top-app",0755),"top-app group");
 read_text("/cpuset/mems",buf,sizeof(buf));check(write_text("/cpuset/top-app/mems",buf)>0,"top-app memory nodes");
 read_text("/cpuset/cpus",buf,sizeof(buf));check(write_text("/cpuset/top-app/cpus",buf)>0,"top-app CPUs");
 check(write_text("/sys/kernel/avr_profile/per_app","1")>0,"per-app enable");
 check(write_text("/sys/module/launch_boost/parameters/enabled","0")>0,"isolate thread boost");
 check(!pipe(pipefd),"app pipe");child=fork();check(child>=0,"app fork");
 if (!child) {
  close(pipefd[0]);
  if (setresuid(10001,10001,10001))_exit(2);
  if (write(pipefd[1],"R",1)!=1)_exit(3);
  for (;;)asm volatile("" ::: "memory");
 }
 close(pipefd[1]);check(read(pipefd[0],&ready,1)==1 && ready=='R',"app ready");close(pipefd[0]);
 snprintf(pidbuf,sizeof(pidbuf),"%d",child);
 check(write_text("/cpuset/top-app/tasks",pidbuf)>0,"move foreground app");
 for (int i=0;i<500;i++) {
  read_text("/sys/kernel/avr_profile/mode",buf,sizeof(buf));
  if (strstr(buf,"[game]"))break;
  usleep(10000);
 }
 check(strstr(buf,"[game]")!=NULL,"foreground profile selected");
 read_text("/sys/kernel/avr_profile/top_pid",buf,sizeof(buf));check(atoi(buf)==child,"foreground PID");
 {
  struct sched_attr attr={.size=sizeof(attr)};int ret=-1;
  for (int i=0;i<500;i++) {
   ret=syscall(SYS_sched_getattr,child,&attr,sizeof(attr),0);
   if (!ret && attr.sched_util_min>0)break;
   usleep(10000);
  }
  check(!ret && attr.sched_util_min>0 && (attr.sched_flags&SCHED_FLAG_RESET_ON_FORK),"foreground thread headroom boost");
 }
 check(write_text("/sys/kernel/avr_profile/app_profiles","\n")>0,"remove app override");
 read_text("/sys/kernel/avr_profile/mode",buf,sizeof(buf));check(strstr(buf,"[balanced]")!=NULL,"global profile restored");
 {
  struct sched_attr attr={.size=sizeof(attr)};int ret=-1;
  for (int i=0;i<500;i++) {
   ret=syscall(SYS_sched_getattr,child,&attr,sizeof(attr),0);
   if (!ret && !attr.sched_util_min && !(attr.sched_flags&SCHED_FLAG_RESET_ON_FORK))break;
   usleep(10000);
  }
  check(!ret && !attr.sched_util_min && !(attr.sched_flags&SCHED_FLAG_RESET_ON_FORK),"thread boost restores clamp and flags");
 }
 check(write_text("/sys/module/launch_boost/parameters/enabled","1")>0,"restore launch enable");
 kill(child,SIGTERM);check(waitpid(child,&status,0)==child,"app cleanup");
 puts("AVR_APP_PROFILE_PASS");puts("AVR_THREAD_BOOST_PASS");
}
struct legacy_sem {uint32_t sem,count,max;};
#define LEGACY_CREATE _IOWR('N',0x80,struct legacy_sem)
static void ntsync_legacy(void)
{
 int dev=open("/dev/ntsync",O_RDWR),ret,dupfd,hole;
 struct legacy_sem old={.count=1,.max=5};
 struct ntsync_sem_args current={.count=2,.max=8},state={};
 uint32_t release=3;
 check(dev>=0,"ntsync open");
 check(ioctl(dev,LEGACY_CREATE,&old)==0,"legacy create");
 check((int)old.sem>=0 && fcntl(old.sem,F_GETFD)>=0,"legacy descriptor");
 check(ioctl(old.sem,NTSYNC_IOC_SEM_RELEASE,&release)==0 && release==1,"legacy post");
 check(ioctl(old.sem,NTSYNC_IOC_SEM_READ,&state)==0 && state.count==4 && state.max==5,"legacy object modern read");
 release=2;check(ioctl(old.sem,NTSYNC_IOC_SEM_RELEASE,&release)<0 && errno==EOVERFLOW,"semaphore overflow");
 ret=ioctl(dev,NTSYNC_IOC_CREATE_SEM,&current);check(ret>=0,"modern create");close(ret);
 old.count=6;check(ioctl(dev,LEGACY_CREATE,&old)<0 && errno==EINVAL,"legacy invalid count");
 {
  long page=sysconf(_SC_PAGESIZE);
  struct legacy_sem *ro=mmap(NULL,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  check(ro!=MAP_FAILED,"mmap");*ro=(struct legacy_sem){.count=1,.max=3};
  check(!mprotect(ro,page,PROT_READ),"readonly");
  hole=fcntl(dev,F_DUPFD_CLOEXEC,0);check(hole>=0,"fd hole");close(hole);
  check(ioctl(dev,LEGACY_CREATE,ro)<0 && errno==EFAULT,"copyout fault");
  dupfd=fcntl(dev,F_DUPFD_CLOEXEC,0);check(dupfd==hole,"no descriptor leak");close(dupfd);munmap(ro,page);
 }
 close(old.sem);close(dev);
}
__attribute__((noinline)) static void frame_probe(void) {asm volatile("" ::: "memory");}
static uint64_t probe_offset(void)
{
 int fd=open("/proc/self/exe",O_RDONLY);Elf64_Ehdr eh;Elf64_Phdr ph;uintptr_t addr=(uintptr_t)frame_probe;
 check(fd>=0,"open ELF");check(pread(fd,&eh,sizeof(eh),0)==sizeof(eh),"ELF header");
 for (unsigned int i=0;i<eh.e_phnum;i++) {
  check(pread(fd,&ph,sizeof(ph),eh.e_phoff+i*eh.e_phentsize)==sizeof(ph),"ELF program header");
  if (ph.p_type==PT_LOAD && addr>=ph.p_vaddr && addr<ph.p_vaddr+ph.p_filesz) {close(fd);return addr-ph.p_vaddr+ph.p_offset;}
 }
 check(0,"probe offset");return 0;
}
static void fas(void)
{
 int fd=open("/dev/encore_fas",O_RDWR),status;pid_t child;
 struct fas_version version={};
 struct fas_register_args req={.pid=getpid(),.cfg={.vsync_ns=16666667,.count=1,.fps={60}}};
 struct fas_state state={};struct fas_listener_list list={};struct fas_remove_args remove;
 check(fd>=0,"FAS open");check(!ioctl(fd,FAS_IOC_GET_VERSION,&version) && version.abi==FAS_ABI_VERSION && version.counter_hz>0,"FAS ABI");
 req.offset=probe_offset();strcpy(req.path,"/bin/rakkashin-test");
 check(!ioctl(fd,FAS_IOC_REGISTER,&req),"real uprobe register");state.ctx_id=req.ctx_id;
 check(!ioctl(fd,FAS_IOC_LIST,&list) && list.count==1 && list.listeners[0].pid==getpid(),"listener list");
 for (int i=0;i<30;i++){frame_probe();usleep(16667);}
 usleep(200000);
 check(!ioctl(fd,FAS_IOC_GET_STATE,&state) && state.fps==60 && state.seq>0,"uprobes and watchdog events");
 remove.ctx_id=req.ctx_id;check(!ioctl(fd,FAS_IOC_REMOVE,&remove),"uprobe removal");
 check(!ioctl(fd,FAS_IOC_LIST,&list) && list.count==0,"listener removed");
 child=fork();check(child>=0,"fork");
 if (!child) {
  int denied;close(fd);
  if (setresuid(10001,10001,10001))_exit(2);
  denied=open("/dev/encore_fas",O_RDWR);
  if (denied>=0){close(denied);_exit(3);}
  _exit(errno==EPERM || errno==EACCES ? 0 : 4);
 }
 check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"FAS access policy");close(fd);
}
int main(void)
{
 profiles();launch_boost();app_selection();ntsync_legacy();fas();
 printf("AVR_RAKKASHIN_PASS checks=%u\n",checks);return 0;
}
