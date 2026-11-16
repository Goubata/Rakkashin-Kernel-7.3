// SPDX-License-Identifier: GPL-2.0-only
/* PR_SET_VMA_ANON_NAME through the restored pr_set_vma_name_bypass hook path. */
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0
#endif
int main(void)
{
	char line[512];
	int found = 0;
	size_t len = 1 << 20;
	void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	FILE *f;

	if (p == MAP_FAILED || prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, (unsigned long)p, len,
				     (unsigned long)"avr-moto") != 0) {
		perror("prctl");
		return 1;
	}
	f = fopen("/proc/self/maps", "r");
	while (f && fgets(line, sizeof(line), f))
		if (strstr(line, "[anon:avr-moto]"))
			found = 1;
	puts(found ? "AVR_VMA_NAME_OK" : "AVR_VMA_NAME_MISSING");
	return !found;
}
