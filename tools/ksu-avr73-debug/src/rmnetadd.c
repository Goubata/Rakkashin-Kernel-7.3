// 7.3 bring-up: create (and delete) an rmnet link over a real device with a mux id,
// like the vendor data daemons do, and print the kernel's netlink error + extack text.
// Phone test 40: 6.12 has rmnet_data0..3 at boot, 7.3 has none (4G data / VoLTE fail).
// Usage: rmnetadd <real_dev> <name> <mux_id> [flags]   (deletes the link again unless KEEP=1)
#include <errno.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef IFLA_RMNET_MUX_ID
#define IFLA_RMNET_MUX_ID 1
#define IFLA_RMNET_FLAGS 2
#endif
#ifndef NLMSGERR_ATTR_MSG
#define NLMSGERR_ATTR_MSG 1
#endif

struct req { struct nlmsghdr n; struct ifinfomsg i; char buf[512]; };

static struct rtattr *addattr(struct nlmsghdr *n, int type, const void *data, int len)
{
	struct rtattr *rta = (struct rtattr *)((char *)n + NLMSG_ALIGN(n->nlmsg_len));
	rta->rta_type = type;
	rta->rta_len = RTA_LENGTH(len);
	if (len)
		memcpy(RTA_DATA(rta), data, len);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(rta->rta_len);
	return rta;
}

static void nest_end(struct nlmsghdr *n, struct rtattr *nest)
{
	nest->rta_len = (char *)n + NLMSG_ALIGN(n->nlmsg_len) - (char *)nest;
}

static int talk(int fd, struct nlmsghdr *n, const char *what)
{
	char rbuf[4096];
	int len;

	if (send(fd, n, n->nlmsg_len, 0) < 0) { perror("send"); return -1; }
	len = recv(fd, rbuf, sizeof(rbuf), 0);
	if (len < 0) { perror("recv"); return -1; }
	for (struct nlmsghdr *h = (struct nlmsghdr *)rbuf; NLMSG_OK(h, len); h = NLMSG_NEXT(h, len)) {
		if (h->nlmsg_type != NLMSG_ERROR)
			continue;
		struct nlmsgerr *e = NLMSG_DATA(h);
		printf("%s: error=%d (%s)", what, e->error, e->error ? strerror(-e->error) : "ok");
		if (h->nlmsg_flags & NLM_F_ACK_TLVS) {
			int off = sizeof(*e) + ((h->nlmsg_flags & NLM_F_CAPPED) ? 0 : e->msg.nlmsg_len - NLMSG_HDRLEN);
			struct rtattr *a = (struct rtattr *)((char *)e + NLMSG_ALIGN(off));
			int alen = h->nlmsg_len - NLMSG_HDRLEN - NLMSG_ALIGN(off);
			for (; RTA_OK(a, alen); a = RTA_NEXT(a, alen))
				if (a->rta_type == NLMSGERR_ATTR_MSG)
					printf(" extack=\"%s\"", (char *)RTA_DATA(a));
		}
		printf("\n");
		return e->error;
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: rmnetadd real_dev name mux_id [flags]\n"); return 2; }
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE), one = 1;
	setsockopt(fd, SOL_NETLINK, NETLINK_EXT_ACK, &one, sizeof(one));
	unsigned int link = if_nametoindex(argv[1]);
	if (!link) { perror(argv[1]); return 1; }

	struct req r = { 0 };
	r.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	r.n.nlmsg_type = RTM_NEWLINK;
	r.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
	addattr(&r.n, IFLA_LINK, &link, 4);
	addattr(&r.n, IFLA_IFNAME, argv[2], strlen(argv[2]) + 1);
	struct rtattr *li = addattr(&r.n, IFLA_LINKINFO, NULL, 0);
	addattr(&r.n, IFLA_INFO_KIND, "rmnet", 6);
	struct rtattr *data = addattr(&r.n, IFLA_INFO_DATA, NULL, 0);
	unsigned short mux = atoi(argv[3]);
	addattr(&r.n, IFLA_RMNET_MUX_ID, &mux, 2);
	if (argc > 4) {
		struct { unsigned int flags, mask; } fl = { strtoul(argv[4], 0, 0), 0xffffffff };
		addattr(&r.n, IFLA_RMNET_FLAGS, &fl, sizeof(fl));
	}
	nest_end(&r.n, data);
	nest_end(&r.n, li);
	int err = talk(fd, &r.n, "newlink");
	if (!err && !getenv("KEEP")) {
		struct req d = { 0 };
		d.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
		d.n.nlmsg_type = RTM_DELLINK;
		d.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		d.i.ifi_index = if_nametoindex(argv[2]);
		talk(fd, &d.n, "dellink");
	}
	return err ? 1 : 0;
}
