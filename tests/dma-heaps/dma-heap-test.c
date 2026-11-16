// SPDX-License-Identifier: GPL-2.0-only
/*
 * Userspace exercise of the ported qcom_dma_heaps through /dev/dma_heap (the
 * Android allocator path): alloc/mmap/CPU sync/verify/free on the system, CMA
 * (display) and carveout (qseecom-ta) heaps, concurrent allocation, and the
 * restored /sys/kernel/dma_heap/total_pools_kb accounting.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("dma-heap-test FAIL: " __VA_ARGS__); puts(""); fails++; } } while (0)

static int heap_fd(const char *name)
{
	char path[128];

	snprintf(path, sizeof(path), "/dev/dma_heap/%s", name);
	return open(path, O_RDONLY | O_CLOEXEC);
}

static int alloc_buf(int heap, size_t len)
{
	struct dma_heap_allocation_data d = { .len = len, .fd_flags = O_RDWR | O_CLOEXEC };

	if (ioctl(heap, DMA_HEAP_IOCTL_ALLOC, &d) < 0)
		return -errno;
	return d.fd;
}

static int sync_buf(int fd, uint64_t flags)
{
	struct dma_buf_sync s = { .flags = flags };

	return ioctl(fd, DMA_BUF_IOCTL_SYNC, &s);
}

/* alloc, map, fill a pattern under CPU-access sync, re-map and verify */
static int roundtrip(int heap, size_t len, unsigned seed)
{
	uint32_t *p;
	size_t i, n = len / 4;
	int fd = alloc_buf(heap, len), bad = 0;

	if (fd < 0)
		return fd;
	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		close(fd);
		return -errno;
	}
	sync_buf(fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE);
	for (i = 0; i < n; i++)
		p[i] = seed * 2654435761u + i;
	sync_buf(fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE);
	munmap(p, len);
	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		close(fd);
		return -errno;
	}
	sync_buf(fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ);
	for (i = 0; i < n; i++)
		bad += p[i] != seed * 2654435761u + i;
	sync_buf(fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
	munmap(p, len);
	close(fd);
	return bad ? -EIO : 0;
}

static long pools_kb(void)
{
	FILE *f = fopen("/sys/kernel/dma_heap/total_pools_kb", "r");
	long v = -1;

	if (f) {
		if (fscanf(f, "%ld", &v) != 1)
			v = -1;
		fclose(f);
	}
	return v;
}

static void *worker(void *arg)
{
	int heap = heap_fd("qcom,system"), i;
	long id = (long)arg;

	for (i = 0; i < 100 && heap >= 0; i++)
		if (roundtrip(heap, (size_t)(1 + (i % 7)) << 16, id * 1000 + i)) {
			__sync_fetch_and_add(&fails, 1);
			break;
		}
	if (heap >= 0)
		close(heap);
	return NULL;
}

int main(void)
{
	static const char *const all[] = { "system", "qcom,system", "qcom,display",
		"qcom,qseecom-ta", "qcom,secure-pixel", "qcom,secure-non-pixel" };
	static const size_t sizes[] = { 4096, 1 << 20, 8 << 20 };
	pthread_t t[4];
	long before, after;
	int h, i, j, r;

	for (i = 0; i < 6; i++) {
		h = heap_fd(all[i]);
		CHECK(h >= 0, "heap %s missing (errno %d)", all[i], errno);
		if (h >= 0)
			close(h);
	}
	CHECK(pools_kb() >= 0, "/sys/kernel/dma_heap/total_pools_kb missing");

	/* system, qcom,system, CMA display, carveout qseecom-ta: real CPU roundtrips */
	for (i = 0; i < 4; i++) {
		h = heap_fd(all[i]);
		if (h < 0)
			continue;
		for (j = 0; j < 3; j++) {
			if (i == 3 && sizes[j] > (4 << 20))
				continue;	/* fixture carveout is 8 MiB */
			r = roundtrip(h, sizes[j], i * 10 + j);
			CHECK(r == 0, "%s %zu bytes: %d", all[i], sizes[j], r);
		}
		close(h);
	}

	/* freed system-heap pages are pooled and reported by the restored accounting */
	before = pools_kb();
	h = heap_fd("qcom,system");
	for (i = 0; h >= 0 && i < 32; i++)
		roundtrip(h, 1 << 20, 500 + i);
	if (h >= 0)
		close(h);
	sleep(1);
	after = pools_kb();
	printf("dma-heap-test pools_kb before=%ld after=%ld\n", before, after);
	CHECK(after > 0, "no pooled pages reported after frees");

	for (i = 0; i < 4; i++)
		pthread_create(&t[i], NULL, worker, (void *)(long)i);
	for (i = 0; i < 4; i++)
		pthread_join(t[i], NULL);

	/* carveout exhaustion must fail cleanly and recover */
	h = heap_fd("qcom,qseecom-ta");
	if (h >= 0) {
		int fds[8], n = 0;

		while (n < 8 && (fds[n] = alloc_buf(h, 2 << 20)) >= 0)
			n++;
		CHECK(n >= 1 && n < 8, "carveout did not report exhaustion (%d)", n);
		while (n--)
			close(fds[n]);
		CHECK(roundtrip(h, 1 << 20, 99) == 0, "carveout unusable after exhaustion");
		close(h);
	}

	puts(fails ? "AVR_DMA_HEAP_USER_FAIL" : "AVR_DMA_HEAP_USER_PASS");
	return fails != 0;
}
