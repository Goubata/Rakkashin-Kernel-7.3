// SPDX-License-Identifier: GPL-2.0-only
/*
 * sg on the QEMU UFS LU: SG_IO INQUIRY works, and the AVR vendor change holds a
 * runtime-PM reference on the scsi_device for as long as /dev/sgN is open.
 * usage: sg-test /dev/sgN /sys/.../device/power/runtime_usage
 */
#include <fcntl.h>
#include <scsi/sg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int usage(const char *p)
{
	FILE *f = fopen(p, "r");
	int v = -1;

	if (f) {
		if (fscanf(f, "%d", &v) != 1)
			v = -1;
		fclose(f);
	}
	return v;
}

int main(int argc, char **argv)
{
	unsigned char cdb[6] = { 0x12, 0, 0, 0, 96, 0 }, buf[96] = {}, sense[32];
	struct sg_io_hdr io = { .interface_id = 'S', .dxfer_direction = SG_DXFER_FROM_DEV,
		.cmd_len = 6, .mx_sb_len = sizeof(sense), .dxfer_len = sizeof(buf),
		.dxferp = buf, .cmdp = cdb, .sbp = sense, .timeout = 5000 };
	int before, during, after, fd, fails = 0;

	if (argc < 3)
		return 2;
	before = usage(argv[2]);
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	during = usage(argv[2]);
	if (ioctl(fd, SG_IO, &io) < 0 || io.status || io.host_status || io.driver_status) {
		printf("sg-test FAIL: INQUIRY status %d/%d/%d\n", io.status, io.host_status, io.driver_status);
		fails++;
	}
	printf("sg-test inquiry vendor='%.8s' product='%.16s' type=%d\n", buf + 8, buf + 16, buf[0] & 0x1f);
	close(fd);
	usleep(100000);
	after = usage(argv[2]);
	printf("sg-test runtime_usage before=%d open=%d after=%d\n", before, during, after);
	if (before < 0 || during != before + 1 || after != before) {
		puts("sg-test FAIL: runtime-PM reference not held exactly while open");
		fails++;
	}
	puts(fails ? "AVR_SG_USER_FAIL" : "AVR_SG_USER_PASS");
	return fails != 0;
}
