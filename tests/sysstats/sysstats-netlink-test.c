// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "../../ports/msm-sysstats/include/uapi/linux/msm_sysstats.h"

static unsigned char response[8192];
static unsigned int sequence;
static int fd;
static void check(int condition,const char *what)
{
 if (!condition) {fprintf(stderr,"AVR_SYSSTATS_NETLINK_FAIL %s errno=%d\n",what,errno);exit(1);}
}
static struct nlattr *find_attr(size_t length,unsigned int type)
{
 size_t off=NLMSG_HDRLEN+GENL_HDRLEN;
 while (off+NLA_HDRLEN<=length) {
  struct nlattr *a=(void *)(response+off);
  check(a->nla_len>=NLA_HDRLEN && a->nla_len<=length-off,"attribute bounds");
  if ((a->nla_type & NLA_TYPE_MASK)==type) return a;
  off+=NLA_ALIGN(a->nla_len);
 }
 return NULL;
}
static int query(unsigned int family,unsigned int cmd,unsigned int attr,
                 const void *data,size_t length)
{
 unsigned char request[512]={};
 struct nlmsghdr *n=(void *)request;
 struct genlmsghdr *g=(void *)(request+NLMSG_HDRLEN);
 struct sockaddr_nl kernel={.nl_family=AF_NETLINK};
 struct nlmsghdr *reply;
 ssize_t got;
 n->nlmsg_len=NLMSG_HDRLEN+GENL_HDRLEN;
 n->nlmsg_type=family;n->nlmsg_flags=NLM_F_REQUEST;n->nlmsg_seq=++sequence;
 g->cmd=cmd;g->version=family==GENL_ID_CTRL ? 1 : SYSSTATS_GENL_VERSION;
 if (attr) {
  struct nlattr *a=(void *)(request+n->nlmsg_len);
  check(n->nlmsg_len+NLA_HDRLEN+length<=sizeof(request),"request size");
  a->nla_type=attr;a->nla_len=NLA_HDRLEN+length;
  memcpy((unsigned char *)a+NLA_HDRLEN,data,length);
  n->nlmsg_len+=NLA_ALIGN(a->nla_len);
 }
 check(sendto(fd,request,n->nlmsg_len,0,(void *)&kernel,sizeof(kernel))==(ssize_t)n->nlmsg_len,"send");
 got=recv(fd,response,sizeof(response),0);
 check(got>=(ssize_t)NLMSG_HDRLEN,"receive");
 reply=(void *)response;
 check(reply->nlmsg_len<=(size_t)got && reply->nlmsg_len>=NLMSG_HDRLEN,"reply bounds");
 check(reply->nlmsg_seq==sequence,"reply sequence");
 if (reply->nlmsg_type==NLMSG_ERROR) {
  struct nlmsgerr *e=(void *)(response+NLMSG_HDRLEN);
  check(reply->nlmsg_len>=NLMSG_LENGTH(sizeof(*e)),"error size");
  return e->error;
 }
 check(reply->nlmsg_type==family,"reply family");
 return reply->nlmsg_len;
}
int main(void)
{
 struct sockaddr_nl local={.nl_family=AF_NETLINK};
 struct timeval timeout={.tv_sec=5};
 struct nlattr *a;
 struct sysstats_mem memory;
 struct sysstats_task task;
 uint16_t family;
 uint32_t pid=getpid();
 int size;
 fd=socket(AF_NETLINK,SOCK_RAW,NETLINK_GENERIC);check(fd>=0,"socket");
 check(!setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)),"timeout");
 check(!bind(fd,(void *)&local,sizeof(local)),"bind");
 size=query(GENL_ID_CTRL,CTRL_CMD_GETFAMILY,CTRL_ATTR_FAMILY_NAME,SYSSTATS_GENL_NAME,sizeof(SYSSTATS_GENL_NAME));
 check(size>0,"resolve");a=find_attr(size,CTRL_ATTR_FAMILY_ID);
 check(a && a->nla_len>=NLA_HDRLEN+sizeof(family),"family id");
 memcpy(&family,(unsigned char *)a+NLA_HDRLEN,sizeof(family));
 size=query(family,SYSSTATS_MEMINFO_CMD_GET,0,NULL,0);check(size>0,"meminfo");
 a=find_attr(size,SYSSTATS_MEMINFO_TYPE_STATS);
 check(a && a->nla_len==NLA_HDRLEN+sizeof(memory),"memory ABI");
 memcpy(&memory,(unsigned char *)a+NLA_HDRLEN,sizeof(memory));
 check(memory.memtotal>0 && memory.vmalloc_total>0,"live memory counters");
 check(memory.swap_used<=memory.swap_total,"swap counters");
 size=query(family,SYSSTATS_TASK_CMD_GET,SYSSTATS_TASK_CMD_ATTR_PID,&pid,sizeof(pid));
 check(size>0,"task");a=find_attr(size,SYSSTATS_TASK_TYPE_STATS);
 check(a && a->nla_len==NLA_HDRLEN+sizeof(task),"task ABI");
 memcpy(&task,(unsigned char *)a+NLA_HDRLEN,sizeof(task));check(task.pid==pid,"pid");
 check(query(family,SYSSTATS_TASK_CMD_GET,0,NULL,0)==-EINVAL,"missing pid rejected");
 close(fd);puts("AVR_SYSSTATS_NETLINK_PASS checks=4");return 0;
}
