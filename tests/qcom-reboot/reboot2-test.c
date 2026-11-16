// SPDX-License-Identifier: GPL-2.0-only
/* reboot(2) with LINUX_REBOOT_CMD_RESTART2 and a command, like Android's init. */
#include <linux/reboot.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	sync();
	syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		LINUX_REBOOT_CMD_RESTART2, argc > 1 ? argv[1] : "");
	perror("reboot");
	return 1;
}
