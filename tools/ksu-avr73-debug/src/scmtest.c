// AF_UNIX credential behaviour probe for the 7.3 bring-up (logd receives nothing).
// 1) DGRAM socketpair-like: receiver bound + SO_PASSCRED, sender sendto() -> SCM_CREDENTIALS?
// 2) direct write to /dev/socket/logdw (what liblog does) -> errno
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(void)
{
	int r = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0), s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0), on = 1;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	char buf[64], cbuf[CMSG_SPACE(sizeof(struct ucred))];
	struct iovec iov = { buf, sizeof(buf) };
	struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = cbuf, .msg_controllen = sizeof(cbuf) };
	struct cmsghdr *c;
	ssize_t n;

	strcpy(a.sun_path + 1, "avr73-scmtest");
	if (bind(r, (void *)&a, sizeof(a.sun_family) + 1 + strlen("avr73-scmtest")))
		printf("bind: %s\n", strerror(errno));
	printf("SO_PASSCRED set: %d (%s)\n", setsockopt(r, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)), strerror(errno));
	n = sendto(s, "hi", 2, 0, (void *)&a, sizeof(a.sun_family) + 1 + strlen("avr73-scmtest"));
	printf("sendto: %zd (%s)\n", n, n < 0 ? strerror(errno) : "ok");
	n = recvmsg(r, &m, MSG_DONTWAIT);
	printf("recvmsg: %zd (%s) controllen=%zu\n", n, n < 0 ? strerror(errno) : "ok", (size_t)m.msg_controllen);
	for (c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c)) {
		if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_CREDENTIALS) {
			struct ucred *u = (void *)CMSG_DATA(c);
			printf("SCM_CREDENTIALS pid=%d uid=%d gid=%d (self %d)\n", u->pid, u->uid, u->gid, getpid());
		} else {
			printf("cmsg level=%d type=%d\n", c->cmsg_level, c->cmsg_type);
		}
	}
	/* liblog path */
	int l = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	struct sockaddr_un la = { .sun_family = AF_UNIX, .sun_path = "/dev/socket/logdw" };
	printf("connect logdw: %d (%s)\n", connect(l, (void *)&la, sizeof(la)), strerror(errno));
	unsigned char pkt[64] = { 0 };	/* android_log_header_t + priority + tag + msg */
	pkt[0] = 0;	/* LOG_ID_MAIN */
	int len = 11;
	pkt[len++] = 6;	/* ERROR */
	memcpy(pkt + len, "AVR73\0probe-direct\0", 19); len += 19;
	n = write(l, pkt, len);
	printf("write logdw: %zd (%s)\n", n, n < 0 ? strerror(errno) : "ok");
	return 0;
}
