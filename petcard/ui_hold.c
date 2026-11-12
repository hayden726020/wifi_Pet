/*
 * Hold only the stock-UI threads of zte_mifi with ptrace, leaving the
 * watchdog and AP threads running. Usage:
 *   ui_hold [--list] [PID]
 * Default target threads: mmi_lcd_main, mmihandlekey, mmi, mmihandlemsg.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>

#ifndef __WALL
#define __WALL 0x40000000
#endif

static volatile sig_atomic_t g_stop = 0;
static pid_t g_held[64];
static int g_nheld = 0;

static long long monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int read_comm(const char *path, char *out, size_t n)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = read(fd, out, n - 1);
    close(fd);
    if (r <= 0) return -1;
    out[r] = '\0';
    char *nl = strchr(out, '\n');
    if (nl) *nl = '\0';
    return 0;
}

static int find_pid(const char *name)
{
    DIR *d = opendir("/proc");
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[64], comm[64];
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        if (read_comm(path, comm, sizeof(comm)) == 0 && strcmp(comm, name) == 0) {
            int pid = atoi(e->d_name);
            closedir(d);
            return pid;
        }
    }
    closedir(d);
    return -1;
}

static int is_target(const char *name)
{
    return strcmp(name, "mmi_lcd_main") == 0 ||
           strcmp(name, "mmihandlekey") == 0 ||
           strcmp(name, "mmi") == 0 ||
           strcmp(name, "mmihandlemsg") == 0;
}

static int already_held(pid_t tid)
{
    for (int i = 0; i < g_nheld; i++)
        if (g_held[i] == tid) return 1;
    return 0;
}

static int scan_hold(int pid)
{
    char tdir[64];
    snprintf(tdir, sizeof(tdir), "/proc/%d/task", pid);
    DIR *d = opendir(tdir);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        pid_t tid = (pid_t)atoi(e->d_name);
        if (already_held(tid)) continue;
        char cpath[128], comm[64];
        snprintf(cpath, sizeof(cpath), "%s/%s/comm", tdir, e->d_name);
        if (read_comm(cpath, comm, sizeof(comm)) != 0) continue;
        if (!is_target(comm)) continue;
        if (ptrace(PTRACE_ATTACH, tid, 0, 0) < 0) {
            fprintf(stderr, "attach %d (%s): %s\n", (int)tid, comm, strerror(errno));
            continue;
        }
        int st = 0;
        waitpid(tid, &st, __WALL);
        if (g_nheld < (int)(sizeof(g_held) / sizeof(g_held[0]))) {
            g_held[g_nheld++] = tid;
            printf("HOLD tid=%d %s uptime_ms=%lld\n", (int)tid, comm, monotonic_ms());
            fflush(stdout);
        }
    }
    closedir(d);
    return 0;
}

int main(int argc, char **argv)
{
    int list_only = 0;
    int pid = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) list_only = 1;
        else if (argv[i][0] >= '0' && argv[i][0] <= '9') pid = atoi(argv[i]);
    }
    if (pid <= 0) pid = find_pid("zte_mifi");
    if (pid <= 0) {
        fprintf(stderr, "zte_mifi not found\n");
        return 1;
    }

    if (list_only) {
        char tdir[64];
        snprintf(tdir, sizeof(tdir), "/proc/%d/task", pid);
        DIR *d = opendir(tdir);
        if (!d) { perror(tdir); return 1; }
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
            char cpath[128], comm[64];
            snprintf(cpath, sizeof(cpath), "%s/%s/comm", tdir, e->d_name);
            if (read_comm(cpath, comm, sizeof(comm)) == 0)
                printf("%s %s\n", e->d_name, comm);
        }
        closedir(d);
        return 0;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    long long fast_scan_until = monotonic_ms() + 10000;
    scan_hold(pid);
    if (g_nheld == 0) {
        fprintf(stderr, "no UI threads held\n");
        return 1;
    }
    printf("HOLDING %d threads\n", g_nheld);
    fflush(stdout);

    while (!g_stop) {
        /* MMI creates/names its display threads after the first UI thread.
         * Keep discovering them during startup instead of leaving a 5s gap. */
        if (monotonic_ms() < fast_scan_until) usleep(100000);
        else sleep(1);
        if (g_stop) break;
        int st;
        while (waitpid(-1, &st, __WALL | WNOHANG) > 0) { }
        if (scan_hold(pid) != 0) break;
    }

    for (int i = 0; i < g_nheld; i++)
        ptrace(PTRACE_DETACH, g_held[i], 0, 0);
    printf("RELEASED %d\n", g_nheld);
    fflush(stdout);
    return 0;
}
