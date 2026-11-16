// SPDX-License-Identifier: GPL-2.0-only
/*
 * Real binder transaction through the C driver on binderfs: a SCHED_FIFO client
 * calls a context manager registered with FLAT_BINDER_FLAG_INHERIT_RT. The server
 * thread must run the transaction at the client's RT priority and drop back to
 * SCHED_OTHER after BC_REPLY (Android binder priority inheritance, 6.12 parity).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/android/binder.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FLAT_BINDER_FLAG_INHERIT_RT
#define FLAT_BINDER_FLAG_INHERIT_RT 0x800
#endif
#define DEV "/dev/binderfs/binder"
#define MAP (128 * 1024)
#define CLIENT_PRIO 42
#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK 0x40000000
#endif
/* Binder applies inherited policies with reset-on-fork; compare the base policy. */
#define BASE(pol) ((pol) & ~SCHED_RESET_ON_FORK)

static int fail(const char *what)
{
	fprintf(stderr, "binder-pi: %s: %s\n", what, strerror(errno));
	return 1;
}

static int open_binder(void)
{
	int fd = open(DEV, O_RDWR | O_CLOEXEC);

	if (fd < 0)
		return -1;
	if (mmap(NULL, MAP, PROT_READ, MAP_PRIVATE, fd, 0) == MAP_FAILED)
		return -1;
	return fd;
}

static int wr(int fd, const void *w, size_t wl, void *r, size_t rl, size_t *consumed)
{
	struct binder_write_read bwr = {
		.write_size = wl, .write_buffer = (uintptr_t)w,
		.read_size = rl, .read_buffer = (uintptr_t)r,
	};
	int ret;

	do
		ret = ioctl(fd, BINDER_WRITE_READ, &bwr);
	while (ret < 0 && errno == EINTR);
	if (consumed)
		*consumed = bwr.read_consumed;
	return ret;
}

/* Find the first command of interest in a read buffer. */
static uint8_t *find(uint8_t *buf, size_t len, uint32_t want, size_t payload_hint)
{
	size_t off = 0;

	while (off + 4 <= len) {
		uint32_t cmd = *(uint32_t *)(buf + off);

		if (cmd == want)
			return buf + off + 4;
		off += 4 + _IOC_SIZE(cmd);
	}
	(void)payload_hint;
	return NULL;
}

static int server(int ready_pipe)
{
	struct flat_binder_object obj = {
		.hdr.type = BINDER_TYPE_BINDER,
		.flags = FLAT_BINDER_FLAG_INHERIT_RT | FLAT_BINDER_FLAG_ACCEPTS_FDS,
	};
	uint8_t rbuf[512];
	uint32_t enter = BC_ENTER_LOOPER;
	struct sched_param sp;
	int fd = open_binder(), policy, prio;
	size_t got;
	struct binder_transaction_data *tr;

	if (fd < 0)
		return fail("server open");
	if (ioctl(fd, BINDER_SET_CONTEXT_MGR_EXT, &obj) < 0)
		return fail("SET_CONTEXT_MGR_EXT");
	if (wr(fd, &enter, sizeof(enter), NULL, 0, NULL) < 0)
		return fail("ENTER_LOOPER");
	if (write(ready_pipe, "r", 1) != 1)
		return fail("ready");
	for (;;) {
		if (wr(fd, NULL, 0, rbuf, sizeof(rbuf), &got) < 0)
			return fail("server read");
		tr = (void *)find(rbuf, got, BR_TRANSACTION, 0);
		if (tr)
			break;
	}
	policy = BASE(sched_getscheduler(0));
	sched_getparam(0, &sp);
	prio = sp.sched_priority;
	printf("binder-pi: in-transaction policy=%d prio=%d\n", policy, prio);
	{
		struct {
			uint32_t free_cmd;
			binder_uintptr_t buf;
			uint32_t reply_cmd;
			struct binder_transaction_data reply;
		} __attribute__((packed)) out = {
			.free_cmd = BC_FREE_BUFFER, .buf = tr->data.ptr.buffer,
			.reply_cmd = BC_REPLY,
		};

		if (wr(fd, &out, sizeof(out), NULL, 0, NULL) < 0)
			return fail("reply");
	}
	{
		int after = BASE(sched_getscheduler(0));

		printf("binder-pi: after-reply policy=%d\n", after);
		if (policy != SCHED_FIFO || prio != CLIENT_PRIO || after != SCHED_OTHER)
			return 2;
	}
	return 0;
}

static int client(void)
{
	struct sched_param sp = { .sched_priority = CLIENT_PRIO };
	uint8_t rbuf[512];
	int fd, i;
	size_t got;
	struct {
		uint32_t cmd;
		struct binder_transaction_data tr;
	} __attribute__((packed)) txn = { .cmd = BC_TRANSACTION, .tr = { .target.handle = 0, .code = 1 } };

	if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0)
		return fail("client setscheduler");
	fd = open_binder();
	if (fd < 0)
		return fail("client open");
	if (wr(fd, &txn, sizeof(txn), NULL, 0, NULL) < 0)
		return fail("transaction");
	for (i = 0; i < 16; i++) {
		struct binder_transaction_data *rep;

		if (wr(fd, NULL, 0, rbuf, sizeof(rbuf), &got) < 0)
			return fail("client read");
		rep = (void *)find(rbuf, got, BR_REPLY, 0);
		if (rep) {
			struct {
				uint32_t cmd;
				binder_uintptr_t buf;
			} __attribute__((packed)) fr = { BC_FREE_BUFFER, rep->data.ptr.buffer };

			wr(fd, &fr, sizeof(fr), NULL, 0, NULL);
			return 0;
		}
		if (find(rbuf, got, BR_DEAD_REPLY, 0) || find(rbuf, got, BR_FAILED_REPLY, 0))
			return 3;
	}
	return 4;
}

int main(void)
{
	int p[2], st_s, st_c;
	pid_t s, c;
	char ch;

	if (pipe(p) < 0)
		return fail("pipe");
	s = fork();
	if (!s)
		_exit(server(p[1]));
	if (read(p[0], &ch, 1) != 1)
		return fail("server not ready");
	c = fork();
	if (!c)
		_exit(client());
	waitpid(c, &st_c, 0);
	waitpid(s, &st_s, 0);
	printf("binder-pi: server=%d client=%d\n", WEXITSTATUS(st_s), WEXITSTATUS(st_c));
	if (!WIFEXITED(st_s) || !WIFEXITED(st_c) || WEXITSTATUS(st_s) || WEXITSTATUS(st_c)) {
		puts("AVR_BINDER_PI_FAIL");
		return 1;
	}
	puts("AVR_BINDER_PI_PASS");
	return 0;
}
