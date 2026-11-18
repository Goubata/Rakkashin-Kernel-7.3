// 7.3 bring-up: attach to init's netd1shot as soon as it appears, log failing syscalls,
// and turn a failing exit_group() status into 0 so init does not "reboot,netd1shot-fail".
// The point is to keep the test boot alive long enough for adbd (live debugging).
// Usage: rescue1shot <logfile> <wait_seconds>
#define _GNU_SOURCE
#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <asm/ptrace.h>
#include <linux/ptrace.h>

static const char *want = "netd";
static int anyparent;

static int is_target(int pid)
{
	char p[64], comm[32] = "", st[512];
	FILE *f;
	int ppid = -1;

	snprintf(p, sizeof(p), "/proc/%d/comm", pid);
	if (!(f = fopen(p, "r"))) return 0;
	if (!fgets(comm, sizeof(comm), f)) comm[0] = 0;
	fclose(f);
	comm[strcspn(comm, "\n")] = 0;
	if (strcmp(comm, want)) return 0;
	snprintf(p, sizeof(p), "/proc/%d/stat", pid);
	if (!(f = fopen(p, "r"))) return 0;
	if (fgets(st, sizeof(st), f)) sscanf(strrchr(st, ')') + 2, "%*c %d", &ppid);
	fclose(f);
	return anyparent || ppid == 1;
}

static void seize_all(int pid, FILE *o)
{
	char p[64];
	DIR *d;
	struct dirent *e;

	snprintf(p, sizeof(p), "/proc/%d/task", pid);
	if (!(d = opendir(p))) return;
	while ((e = readdir(d))) {
		int t = atoi(e->d_name);
		if (t <= 0) continue;
		if (ptrace(PTRACE_SEIZE, t, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE) == 0)
			ptrace(PTRACE_INTERRUPT, t, 0, 0);
		else
			fprintf(o, "seize %d: %s\n", t, strerror(errno));
	}
	closedir(d);
}

int main(int argc, char **argv)
{
	FILE *o = fopen(argc > 1 ? argv[1] : "/dev/null", "w");
	time_t end = time(NULL) + (argc > 2 ? atoi(argv[2]) : 120);

	if (argc > 3) want = argv[3];
	if (argc > 4) anyparent = 1;	/* test only */
	int target = 0, st, insys[65536] = { 0 };
	static unsigned long long svc_pc[65536];
	struct user_pt_regs r;
	struct iovec iov = { &r, sizeof(r) };

	while (!target && time(NULL) < end) {
		DIR *d = opendir("/proc");
		struct dirent *e;
		while (d && (e = readdir(d)))
			if (atoi(e->d_name) > 1 && is_target(atoi(e->d_name))) { target = atoi(e->d_name); break; }
		if (d) closedir(d);
		if (!target) usleep(2000);
	}
	if (!target) { fprintf(o, "netd1shot never seen\n"); return 1; }
	fprintf(o, "attaching to %d\n", target); fflush(o);
	seize_all(target, o);
	for (;;) {
		pid_t p = waitpid(-1, &st, __WALL);
		if (p < 0) break;
		if (WIFEXITED(st) || WIFSIGNALED(st)) {
			fprintf(o, "pid %d %s %d\n", p, WIFEXITED(st) ? "exit" : "signal", WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st));
			fflush(o);
			if (p == target) break;
			continue;
		}
		int sig = 0, ev = st >> 16;
		if (WIFSTOPPED(st) && WSTOPSIG(st) == (SIGTRAP | 0x80)) {
			ptrace(PTRACE_GETREGSET, p, (void *)NT_PRSTATUS, &iov);
			int i = p & 0xffff;
			if (!insys[i]) {
				insys[i] = 1;
				svc_pc[i] = r.pc - 4;	/* aarch64: pc is past the svc at syscall entry */
				if ((r.regs[8] == 94 || r.regs[8] == 93) && r.regs[0] != 0) {	/* exit_group / exit */
					fprintf(o, "[%d] exit%s(%llu) -> rewritten to 0\n", p, r.regs[8] == 94 ? "_group" : "",
						(unsigned long long)r.regs[0]);
					r.regs[0] = 0;
					ptrace(PTRACE_SETREGSET, p, (void *)NT_PRSTATUS, &iov);
				}
			} else {
				insys[i] = 0;
				long ret = (long)r.regs[0];
				if (ret < 0 && ret > -4096 && ret != -EAGAIN && ret != -ENOENT && ret != -EINTR)
					fprintf(o, "[%d] sys %llu = %ld\n", p, (unsigned long long)r.regs[8], ret);
			}
		} else if (WIFSTOPPED(st) && ev == PTRACE_EVENT_STOP) {
			sig = 0;	/* group-stop / interrupt stop */
		} else if (WIFSTOPPED(st) && ev == PTRACE_EVENT_CLONE) {
			sig = 0;
		} else if (WIFSTOPPED(st)) {
			sig = WSTOPSIG(st);
			if (sig == SIGABRT && svc_pc[p & 0xffff]) {
				/* abort(): swallow the signal and run exit_group(0) at a known svc instead */
				ptrace(PTRACE_GETREGSET, p, (void *)NT_PRSTATUS, &iov);
				fprintf(o, "[%d] SIGABRT at pc %llx -> exit_group(0)\n", p, (unsigned long long)r.pc);
				r.pc = svc_pc[p & 0xffff];
				r.regs[8] = 94;
				r.regs[0] = 0;
				ptrace(PTRACE_SETREGSET, p, (void *)NT_PRSTATUS, &iov);
				sig = 0;
			}
		}
		fflush(o);
		ptrace(PTRACE_SYSCALL, p, 0, sig);
	}
	return 0;
}
