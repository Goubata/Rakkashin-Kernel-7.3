// SPDX-License-Identifier: GPL-2.0-only
/*
 * Userspace exercise of the ported sync_fence.ko (/dev/spec_sync) on 7.3, with
 * sw_sync timelines as the bound fences. Checks the 6.12 semantics that the
 * port had to re-create after dma_fence_array_create() lost signal_on_any:
 *   SIGNAL_ANY  - spec fence signals when the first bound fence signals
 *   SIGNAL_ALL  - spec fence signals only after every bound fence signals
 * plus GET_VER, double bind refusal and invalid-fd bind (error-signals).
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/sync_file.h>
#include "../../ports/mm-drivers/sync_fence/include/uapi/sync_fence/qcom_sync_file.h"

struct sw_sync_create_fence_data { uint32_t value; char name[32]; int32_t fence; };
#define SW_SYNC_IOC_CREATE_FENCE _IOWR('W', 0, struct sw_sync_create_fence_data)
#define SW_SYNC_IOC_INC _IOW('W', 1, uint32_t)

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("spec-sync-test FAIL: " __VA_ARGS__); puts(""); fails++; } } while (0)

static int signaled(int fd, int ms)
{
	struct pollfd p = { .fd = fd, .events = POLLIN };

	return poll(&p, 1, ms) == 1 && (p.revents & POLLIN);
}

static int status(int fd)
{
	struct sync_fence_info fi[4];
	struct sync_file_info info = { .num_fences = 0 };

	memset(fi, 0, sizeof(fi));
	if (ioctl(fd, SYNC_IOC_FILE_INFO, &info) < 0)
		return -1000;
	return info.status;
}

static int sw_fence(int tl, uint32_t value)
{
	struct sw_sync_create_fence_data d = { .value = value };

	snprintf(d.name, sizeof(d.name), "avr-%u", value);
	if (ioctl(tl, SW_SYNC_IOC_CREATE_FENCE, &d) < 0)
		return -1;
	return d.fence;
}

static int spec(int dev, uint32_t n, uint32_t flags)
{
	struct fence_create_data c = { .num_fences = n, .flags = flags };

	if (ioctl(dev, SPEC_SYNC_IOC_CREATE_FENCE, &c) < 0)
		return -errno;
	return c.out_bind_fd;
}

static int bind(int dev, int sfd, int *fds)
{
	struct fence_bind_data b = { .out_bind_fd = sfd, .fds = (uintptr_t)fds };

	return ioctl(dev, SPEC_SYNC_IOC_BIND, &b) < 0 ? -errno : 0;
}

static void run(int dev, uint32_t flags, const char *name)
{
	uint32_t one = 1;
	int tl = open("/sys/kernel/debug/sync/sw_sync", O_RDWR);
	int fds[2], sfd;

	CHECK(tl >= 0, "%s: sw_sync open %d", name, errno);
	if (tl < 0)
		return;
	fds[0] = sw_fence(tl, 1);
	fds[1] = sw_fence(tl, 2);
	sfd = spec(dev, 2, flags);
	CHECK(fds[0] >= 0 && fds[1] >= 0 && sfd >= 0, "%s: create %d %d %d", name, fds[0], fds[1], sfd);
	CHECK(!signaled(sfd, 0), "%s: spec signaled before bind", name);
	CHECK(bind(dev, sfd, fds) == 0, "%s: bind", name);
	CHECK(bind(dev, sfd, fds) == -EINVAL, "%s: second bind accepted", name);
	CHECK(!signaled(sfd, 50), "%s: spec signaled with no bound fence signaled", name);
	ioctl(tl, SW_SYNC_IOC_INC, &one);	/* first bound fence signals */
	if (flags == SPEC_FENCE_SIGNAL_ALL) {
		CHECK(!signaled(sfd, 100), "%s: ALL signaled after one of two", name);
		ioctl(tl, SW_SYNC_IOC_INC, &one);
	}
	CHECK(signaled(sfd, 2000), "%s: spec fence not signaled", name);
	CHECK(status(sfd) == 1, "%s: status %d", name, status(sfd));
	printf("spec-sync-test %s ok=%d\n", name, !fails);
	close(sfd);
	close(fds[0]);
	close(fds[1]);
	close(tl);
}

int main(void)
{
	int dev = open("/dev/spec_sync", O_RDWR), sfd, bad[2] = { -1, -1 };
	uint64_t ver = 0;

	CHECK(dev >= 0, "open /dev/spec_sync %d", errno);
	if (dev < 0)
		goto done;
	CHECK(ioctl(dev, SPEC_SYNC_IOC_GET_VER, &ver) == 0 && ver, "GET_VER %llu", (unsigned long long)ver);
	CHECK(spec(dev, 0, SPEC_FENCE_SIGNAL_ANY) == -ERANGE, "0 fences accepted");
	run(dev, SPEC_FENCE_SIGNAL_ANY, "any");
	run(dev, SPEC_FENCE_SIGNAL_ALL, "all");
	run(dev, 0, "default-any");
	/* invalid bound fd: bind fails, spec fence is signaled with an error */
	sfd = spec(dev, 2, SPEC_FENCE_SIGNAL_ALL);
	CHECK(sfd >= 0, "create for invalid bind");
	CHECK(bind(dev, sfd, bad) == -EINVAL, "invalid fds bound");
	CHECK(signaled(sfd, 2000) && status(sfd) < 0, "invalid bind status %d", status(sfd));
	close(sfd);
	printf("spec-sync-test version=%llu\n", (unsigned long long)ver);
done:
	puts(fails ? "AVR_SPEC_SYNC_USER_FAIL" : "AVR_SPEC_SYNC_USER_PASS");
	fflush(stdout);
	_exit(fails != 0);
}
