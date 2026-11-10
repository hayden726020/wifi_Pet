#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>

struct arm_regs {
    unsigned long uregs[18];
};

static int mem_fd = -1;
static int fb_fd = -1;
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void read_cstr(pid_t pid, unsigned long addr, char *out, size_t n)
{
    size_t i = 0;
    out[0] = 0;
    while (i + 1 < n) {
        unsigned long word;
        if (pread(mem_fd, &word, sizeof(word), addr + i) != (ssize_t)sizeof(word))
            break;
        char *p = (char *)&word;
        for (int j = 0; j < 4 && i + 1 < n; j++) {
            out[i++] = p[j];
            if (p[j] == 0)
                return;
        }
    }
    out[n - 1] = 0;
}

static int peek32(pid_t pid, unsigned long addr, unsigned int *out)
{
    (void)pid;
    return pread(mem_fd, out, 4, addr) == 4 ? 0 : -1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: tracer PID [seconds]\n");
        return 1;
    }
    pid_t pid = (pid_t)atoi(argv[1]);
    int seconds = argc > 2 ? atoi(argv[2]) : 75;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/mem", (int)pid);
    mem_fd = open(path, O_RDONLY);
    if (mem_fd < 0) {
        perror("open mem");
        return 1;
    }
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) {
        perror("PTRACE_ATTACH");
        return 1;
    }
    int status;
    waitpid(pid, &status, 0);
    printf("ATTACHED pid=%d\n", (int)pid);
    fflush(stdout);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);

    long long end = now_ms() + (long long)seconds * 1000;
    int entering = 1;
    char open_path[128] = "";
    while (!g_stop && now_ms() < end) {
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0)
            break;
        if (waitpid(pid, &status, 0) < 0)
            break;
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            printf("TARGET_EXIT status=%d\n", status);
            break;
        }
        struct arm_regs regs;
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0)
            break;
        unsigned int nr = (unsigned int)regs.uregs[7];
        if (entering) {
            if (nr == 5) {
                read_cstr(pid, regs.uregs[0], open_path, sizeof(open_path));
                printf("E open(%s, %lu)\n", open_path, regs.uregs[1]);
            } else if (nr == 54) {
                unsigned int v = 0;
                int ok = peek32(pid, regs.uregs[2], &v);
                printf("E ioctl(fd=%ld, req=%08lx, arg=%08lx [%s%08x])\n",
                       (long)regs.uregs[0], regs.uregs[1], regs.uregs[2],
                       ok == 0 ? "" : "?", v);
            } else if (nr == 4 && regs.uregs[2] >= 1024) {
                printf("E write(fd=%ld, count=%lu)\n", (long)regs.uregs[0], regs.uregs[2]);
            } else if (nr == 192 && regs.uregs[1] >= 4096) {
                printf("E mmap2(len=%lu, prot=%lu, flags=%lu)\n",
                       regs.uregs[1], regs.uregs[2], regs.uregs[3]);
            } else if (nr == 6) {
                printf("E close(fd=%ld)\n", (long)regs.uregs[0]);
            }
            fflush(stdout);
        } else {
            if (nr == 5) {
                long ret = (long)regs.uregs[0];
                printf("X open ret=%ld %s\n", ret, open_path);
                if (ret >= 0 && strstr(open_path, "/dev/fb0")) {
                    fb_fd = (int)ret;
                    printf("  FB_FD=%d\n", fb_fd);
                }
            } else if (nr == 54) {
                printf("X ioctl ret=%ld\n", (long)regs.uregs[0]);
            } else if (nr == 4 && regs.uregs[2] >= 1024) {
                printf("X write ret=%ld\n", (long)regs.uregs[0]);
            } else if (nr == 192 && regs.uregs[1] >= 4096) {
                printf("X mmap2 ret=%08lx\n", regs.uregs[0]);
            }
            fflush(stdout);
        }
        entering = !entering;
    }
    ptrace(PTRACE_DETACH, pid, 0, 0);
    close(mem_fd);
    printf("DONE\n");
    return 0;
}
