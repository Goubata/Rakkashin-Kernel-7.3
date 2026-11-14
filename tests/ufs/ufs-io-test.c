// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHUNK 65536U
#define TOTAL (4U * 1024 * 1024)

static uint64_t word(uint64_t offset, unsigned int round)
{
	return (offset / 8) * UINT64_C(0x9e3779b97f4a7c15) ^
		(round + 1) * UINT64_C(0xd1b54a32d192ed03);
}
int main(void)
{
	char model[128] = {};
	void *memory = NULL;
	uint64_t *buffer;
	int fd, identity;
	unsigned int round, off, i;

	/* This executable is for the virtual test machine, never the phone. */
	identity = open("/sys/firmware/devicetree/base/model", O_RDONLY);
	if (identity < 0 || read(identity, model, sizeof(model) - 1) <= 0 ||
	    !strstr(model, "dummy-virt")) {
		fprintf(stderr, "Requires the QEMU virt machine\n");
		return 1;
	}
	close(identity);
	if (posix_memalign(&memory, 4096, CHUNK))
		return 1;
	buffer = memory;
	fd = open("/dev/avr-ufs", O_RDWR | O_DIRECT | O_CLOEXEC);
	if (fd < 0) {
		perror("open virtual UFS");
		free(memory);
		return 1;
	}
	for (round = 0; round < 2; round++) {
		for (off = 0; off < TOTAL; off += CHUNK) {
			for (i = 0; i < CHUNK / 8; i++)
				buffer[i] = word(off + i * 8, round);
			if (pwrite(fd, buffer, CHUNK, off) != CHUNK || fsync(fd)) {
				perror("virtual UFS write/flush");
				goto failed;
			}
		}
		for (off = 0; off < TOTAL; off += CHUNK) {
			memset(buffer, 0, CHUNK);
			if (pread(fd, buffer, CHUNK, off) != CHUNK) {
				perror("virtual UFS read");
				goto failed;
			}
			for (i = 0; i < CHUNK / 8; i++)
				if (buffer[i] != word(off + i * 8, round)) {
					fprintf(stderr, "Data differs: round=%u off=%u\n", round, off + i * 8);
					goto failed;
				}
		}
	}
	close(fd);
	free(memory);
	puts("AVR_UFS_IO_PASS rounds=2 bytes_written=8388608 bytes_read=8388608");
	return 0;
failed:
	close(fd);
	free(memory);
	return 1;
}
