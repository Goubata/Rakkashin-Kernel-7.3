// SPDX-License-Identifier: GPL-2.0-only
/*
 * Userspace exercise of the ported smcinvoke_dlkm through /dev/smcinvoke, in the
 * order Android's libminkdescriptor uses it: root fd, a callback server
 * (SERVER_REQ + ACCEPT_REQ thread), IClientEnv registerWithCredentials with a
 * userspace credentials object, open a service, buffers in/out, a QTEE->user
 * callback, output objects, SCM busy back-off, error results and fd lifetime.
 * Backend: tests/qcom-tee/avr_fake_tz.c (QEMU-only).
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../../ports/securemsm/include/uapi/linux/smcinvoke.h"

#define COUNTS(bi, bo, oi, oo) ((bi) | ((bo) << 4) | ((oi) << 8) | ((oo) << 12))
static int fails, srv;
static volatile int cb_calls, cb_releases, cb_other;
#define CHECK(c, ...) do { if (!(c)) { printf("smcinvoke-test FAIL: " __VA_ARGS__); puts(""); fails++; } } while (0)

static int invoke(int fd, unsigned int op, unsigned int counts, union smcinvoke_arg *a, int *result)
{
	struct smcinvoke_cmd_req r = {
		.op = op, .counts = counts, .argsize = sizeof(*a), .args = (uintptr_t)a,
	};
	int ret = ioctl(fd, SMCINVOKE_IOCTL_INVOKE_REQ, &r);

	*result = r.result;
	return ret < 0 ? -errno : 0;
}

static void *accept_loop(void *unused __attribute__((unused)))
{
	static uint8_t buf[4096] __attribute__((aligned(8)));
	struct smcinvoke_accept acc = { .argsize = sizeof(union smcinvoke_arg),
		.buf_len = sizeof(buf), .buf_addr = (uintptr_t)buf };

	for (;;) {
		union smcinvoke_arg *a = (void *)buf;

		if (ioctl(srv, SMCINVOKE_IOCTL_ACCEPT_REQ, &acc) < 0) {
			if (errno == EINTR)
				continue;
			return NULL;
		}
		acc.has_resp = 1;
		acc.result = 0;
		if (acc.op == 7 && acc.counts == COUNTS(1, 1, 0, 0)) {
			char *in = (char *)(uintptr_t)a[0].b.addr;
			char *out = (char *)(uintptr_t)a[1].b.addr;

			cb_calls++;
			a[1].b.size = snprintf(out, a[1].b.size, "pong:%s", in) + 1;
		} else if ((acc.op & 0xffff) == 0xffff) {
			cb_releases++;
		} else {
			cb_other++;
			acc.result = 2;
		}
	}
}

static int open_svc(int env, uint32_t uid, int *result)
{
	union smcinvoke_arg a[2] = {
		{ .b = { (uintptr_t)&uid, sizeof(uid) } },
		{ .o = { .fd = -1 } },
	};

	if (invoke(env, 0, COUNTS(1, 0, 0, 1), a, result) || *result)
		return -1;
	return a[1].o.fd;
}

static void *storm(void *arg)
{
	int env = (int)(long)arg, i, r, s;
	char in[32], out[32];

	for (i = 0; i < 100; i++) {
		union smcinvoke_arg e[2];

		s = open_svc(env, 0x77, &r);
		if (s < 0) {
			__sync_fetch_and_add(&fails, 1);
			continue;
		}
		snprintf(in, sizeof(in), "%lx-%d", (unsigned long)pthread_self(), i);
		e[0].b.addr = (uintptr_t)in;
		e[0].b.size = strlen(in) + 1;
		e[1].b.addr = (uintptr_t)out;
		e[1].b.size = sizeof(out);
		if (invoke(s, 0, COUNTS(1, 1, 0, 0), e, &r) || r || strcmp(in, out))
			__sync_fetch_and_add(&fails, 1);
		close(s);
	}
	return NULL;
}

int main(void)
{
	struct smcinvoke_server sreq = { .cb_buf_size = 4096 };
	int root, env, svc, svc2, r, i;
	char in[] = "hello from android", out[64];
	pthread_t th, st[4];
	FILE *f;

	root = open("/dev/smcinvoke", O_RDWR);
	CHECK(root >= 0, "open /dev/smcinvoke: %d", errno);
	if (root < 0)
		goto done;

	/* the root fd refuses ADCI ops from userspace */
	{
		union smcinvoke_arg a[1];

		CHECK(invoke(root, 8, 0, a, &r) == -EINVAL, "adciAccept allowed from userspace");
	}

	srv = ioctl(root, SMCINVOKE_IOCTL_SERVER_REQ, &sreq);
	CHECK(srv >= 0, "SERVER_REQ: %d", errno);
	pthread_create(&th, NULL, accept_loop, NULL);

	/* registerWithCredentials needs a real (callback) credentials object */
	{
		union smcinvoke_arg a[2] = { { .o = { .fd = -1 } }, { .o = { .fd = -1 } } };

		CHECK(invoke(root, 5, COUNTS(0, 0, 1, 1), a, &r) == -EINVAL,
		      "NULL credentials accepted");
		a[0].o.fd = -2;
		a[0].o.cb_server_fd = srv;
		CHECK(invoke(root, 5, COUNTS(0, 0, 1, 1), a, &r) == 0 && r == 0,
		      "registerWithCredentials r=%d", r);
		env = a[1].o.fd;
		CHECK(env >= 0, "no client env fd");
	}

	svc = open_svc(env, 0x1234, &r);
	CHECK(svc >= 0, "open service r=%d", r);
	CHECK(open_svc(env, 0xdead, &r) < 0 && r == 10, "refusing service r=%d", r);

	{	/* echo */
		union smcinvoke_arg a[2] = {
			{ .b = { (uintptr_t)in, sizeof(in) } },
			{ .b = { (uintptr_t)out, sizeof(out) } },
		};

		memset(out, 0, sizeof(out));
		CHECK(invoke(svc, 0, COUNTS(1, 1, 0, 0), a, &r) == 0 && r == 0, "echo r=%d", r);
		CHECK(!strcmp(in, out) && a[1].b.size == sizeof(in), "echo data '%s' %llu", out,
		      (unsigned long long)a[1].b.size);
		a[1].b.size = 4;	/* too small: QTEE says SIZE_OUT */
		CHECK(invoke(svc, 0, COUNTS(1, 1, 0, 0), a, &r) == 0 && r == 4, "short out r=%d", r);
	}

	{	/* QTEE invokes a userspace callback object, then releases it */
		char ping[] = "ping";
		union smcinvoke_arg a[3] = {
			{ .b = { (uintptr_t)ping, sizeof(ping) } },
			{ .b = { (uintptr_t)out, sizeof(out) } },
			{ .o = { .fd = -3, .cb_server_fd = srv } },
		};

		memset(out, 0, sizeof(out));
		CHECK(invoke(svc, 1, COUNTS(1, 1, 1, 0), a, &r) == 0 && r == 0, "callback r=%d", r);
		CHECK(!strcmp(out, "pong:ping"), "callback data '%s'", out);
	}

	{	/* output object -> new fd, usable, then closed */
		union smcinvoke_arg a[1] = { { .o = { .fd = -1 } } };
		union smcinvoke_arg e[2] = {
			{ .b = { (uintptr_t)in, sizeof(in) } },
			{ .b = { (uintptr_t)out, sizeof(out) } },
		};

		CHECK(invoke(svc, 2, COUNTS(0, 0, 0, 1), a, &r) == 0 && r == 0, "out object r=%d", r);
		svc2 = a[0].o.fd;
		CHECK(svc2 >= 0 && svc2 != svc, "out object fd %d", svc2);
		CHECK(invoke(svc2, 0, COUNTS(1, 1, 0, 0), e, &r) == 0 && r == 0, "echo on out object");
		close(svc2);
	}

	{	/* SCM busy twice: si_core backs off, call completes */
		union smcinvoke_arg a[1];

		CHECK(invoke(svc, 3, 0, a, &r) == 0 && r == 0, "busy r=%d", r);
	}

	for (i = 0; i < 4; i++)
		pthread_create(&st[i], NULL, storm, (void *)(long)env);
	for (i = 0; i < 4; i++)
		pthread_join(st[i], NULL);

	close(svc);
	close(env);
	sleep(1);
	CHECK(cb_calls == 1, "callbacks %d", cb_calls);
	CHECK(cb_releases >= 2, "callback-object releases %d", cb_releases);
	CHECK(cb_other == 0, "unexpected callback ops %d", cb_other);
	f = fopen("/sys/kernel/si_core/release", "r");
	if (f) {
		int pending = -1, failed = -1;

		CHECK(fscanf(f, "%d %d", &pending, &failed) == 2 && pending == 0 && failed == 0,
		      "si_core releases pending=%d failed=%d", pending, failed);
		fclose(f);
	} else {
		CHECK(0, "/sys/kernel/si_core/release missing");
	}
	printf("smcinvoke-test callbacks=%d releases=%d\n", cb_calls, cb_releases);
done:
	puts(fails ? "AVR_SMCINVOKE_USER_FAIL" : "AVR_SMCINVOKE_USER_PASS");
	fflush(stdout);
	_exit(fails != 0);
}
