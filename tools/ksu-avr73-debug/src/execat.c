/* execat: probe execveat(fd, "", AT_EMPTY_PATH) the way KernelSU sucompat does.
 * usage: execat <path> [opath] [cloexec]  -> execs <path> --version style "id" */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int flags = O_RDONLY;
	int fd;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <path> [opath] [cloexec]\n", argv[0]);
		return 2;
	}
	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "opath"))
			flags = (flags & ~O_RDONLY) | O_PATH;
		if (!strcmp(argv[i], "cloexec"))
			flags |= O_CLOEXEC;
	}
	fd = open(argv[1], flags);
	if (fd < 0) {
		printf("open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	char *nargv[] = { argv[1], NULL };
	char *nenvp[] = { NULL };
	printf("fd=%d flags=0x%x -> execveat\n", fd, flags);
	fflush(stdout);
	long r = syscall(__NR_execveat, fd, "", nargv, nenvp, AT_EMPTY_PATH);
	printf("execveat ret=%ld errno=%d (%s)\n", r, errno, strerror(errno));
	return 1;
}
