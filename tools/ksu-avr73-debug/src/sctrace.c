// Minimal aarch64 syscall tracer for the 7.3 bring-up: runs a command under ptrace and
// prints every syscall that fails (nr, args, -errno), plus the text of writes/sendmsg to
// sockets (liblog payloads), and the exit status. Usage: sctrace <out> <timeout_s> cmd...
#define _GNU_SOURCE
#include <errno.h>
#include <elf.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <asm/ptrace.h>
#include <linux/ptrace.h>

static FILE *o;

static void peekstr(pid_t p, unsigned long a, size_t n, char *out)
{
	size_t i;

	for (i = 0; i < n; i += sizeof(long)) {
		errno = 0;
		long w = ptrace(PTRACE_PEEKDATA, p, (void *)(a + i), 0);
		if (errno) break;
		memcpy(out + i, &w, sizeof(long));
	}
	out[n] = 0;
	for (i = 0; i < n; i++) if ((out[i] < 32 || out[i] > 126) && out[i]) out[i] = '.';
	for (i = 0; i < n; i++) if (!out[i]) out[i] = '|';
}

int main(int argc, char **argv)
{
	pid_t c;
	int st, insys[65536] = { 0 };
	struct user_pt_regs r;
	struct iovec iov = { &r, sizeof(r) };
	time_t end;

	if (argc < 4) return 2;
	o = fopen(argv[1], "w");
	end = time(NULL) + atoi(argv[2]);
	/* init-style control sockets (ANDROID_SOCKET_<name>, bound at /dev/socket/<name>) from
	 * the comma list in $SCTRACE_SOCKETS, so daemons like netd start as under init */
	char *sl = getenv("SCTRACE_SOCKETS");
	if (sl) {
		char *dup = strdup(sl), *nm, *save;
		for (nm = strtok_r(dup, ",", &save); nm; nm = strtok_r(NULL, ",", &save)) {
			int fd = socket(AF_UNIX, SOCK_STREAM, 0);
			struct sockaddr_un a = { .sun_family = AF_UNIX };
			char env[64], val[16];
			snprintf(a.sun_path, sizeof(a.sun_path), "/dev/socket/%s", nm);
			unlink(a.sun_path);
			if (bind(fd, (void *)&a, sizeof(a)) || listen(fd, 8))
				fprintf(o, "socket %s: %s\n", nm, strerror(errno));
			snprintf(env, sizeof(env), "ANDROID_SOCKET_%s", nm);
			snprintf(val, sizeof(val), "%d", fd);
			setenv(env, val, 1);
		}
	}
	c = fork();
	if (!c) {
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		raise(SIGSTOP);
		execv(argv[3], argv + 3);
		_exit(127);
	}
	waitpid(c, &st, 0);
	ptrace(PTRACE_SETOPTIONS, c, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_EXITKILL);
	ptrace(PTRACE_SYSCALL, c, 0, 0);
	for (;;) {
		pid_t p = waitpid(-1, &st, __WALL);
		if (p < 0) break;
		if (time(NULL) > end) { fprintf(o, "timeout, killing\n"); kill(c, SIGKILL); }
		if (WIFEXITED(st) || WIFSIGNALED(st)) {
			fprintf(o, "pid %d %s %d\n", p, WIFEXITED(st) ? "exit" : "signal", WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st));
			if (p == c) break;
			continue;
		}
		int sig = 0;
		if (WIFSTOPPED(st) && WSTOPSIG(st) == (SIGTRAP | 0x80)) {
			ptrace(PTRACE_GETREGSET, p, (void *)NT_PRSTATUS, &iov);
			int i = p & 0xffff;
			if (!insys[i]) {
				insys[i] = 1;
				/* entry: dump socket payloads (write=64, writev=66, sendmsg=211, sendto=206) */
				if (r.regs[8] == 64 || r.regs[8] == 206) {
					char b[300]; size_t n = r.regs[2] < 280 ? r.regs[2] : 280;
					peekstr(p, r.regs[1], n, b);
					if (strstr(b, "rror") || strstr(b, "ail") || strstr(b, "nable")) fprintf(o, "[%d] write fd%llu: %s\n", p, (unsigned long long)r.regs[0], b);
				} else if (r.regs[8] == 66) {
					struct iovec v[6]; unsigned long k, cnt = r.regs[2] < 6 ? r.regs[2] : 6;
					for (k = 0; k < cnt * 2; k++) { errno = 0; ((long *)v)[k] = ptrace(PTRACE_PEEKDATA, p, (void *)(r.regs[1] + k * 8), 0); }
					fprintf(o, "[%d] writev fd%llu:", p, (unsigned long long)r.regs[0]);
					for (k = 0; k < cnt; k++) { char b[300]; size_t n = v[k].iov_len < 280 ? v[k].iov_len : 280; peekstr(p, (unsigned long)v[k].iov_base, n, b); fprintf(o, " <%s>", b); }
					fprintf(o, "\n");
				}
			} else {
				insys[i] = 0;
				long ret = (long)r.regs[0];
				if (ret < 0 && ret > -4096 && ret != -EAGAIN && ret != -ENOENT)
					fprintf(o, "[%d] sys %llu = %ld\n", p, (unsigned long long)r.regs[8], ret);
			}
		} else if (WIFSTOPPED(st) && (st >> 8) != (SIGTRAP | (PTRACE_EVENT_CLONE << 8))) {
			sig = WSTOPSIG(st);
			if (sig == SIGTRAP || sig == SIGSTOP) sig = 0;
		}
		fflush(o);
		ptrace(PTRACE_SYSCALL, p, 0, sig);
	}
	if (sl) {	/* leave /dev/socket clean for init's real netd */
		char *dup = strdup(sl), *nm, *save, path[108];
		for (nm = strtok_r(dup, ",", &save); nm; nm = strtok_r(NULL, ",", &save)) {
			snprintf(path, sizeof(path), "/dev/socket/%s", nm);
			unlink(path);
		}
	}
	return 0;
}
