/* Host-only integration test: cc -std=c11 -O2 -pthread -o test_ui_hold
 * test_ui_hold.c; ./test_ui_hold ./ui_hold_host
 * Only the synthetic child below is traced. No device access is used. */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct state {
    atomic_int initial_ready, create_late, late_tid, stop;
    atomic_uint initial_ticks, late_ticks, watchdog_ticks, main_ticks;
};
static struct state *shared;

static long long milliseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void *counter(void *arg)
{
    int kind = (int)(long)arg;
    const char *name = kind == 0 ? "mmi" : kind == 1 ? "mmi_lcd_main" : "zte_watchdog";
    atomic_uint *ticks = kind == 0 ? &shared->initial_ticks :
                         kind == 1 ? &shared->late_ticks : &shared->watchdog_ticks;
    prctl(PR_SET_NAME, name);
    if (kind == 0) atomic_store(&shared->initial_ready, 1);
    if (kind == 1) atomic_store(&shared->late_tid, (int)syscall(SYS_gettid));
    while (!atomic_load(&shared->stop)) {
        atomic_fetch_add(ticks, 1);
        usleep(1000);
    }
    return NULL;
}

static void target(void)
{
    /* Permit only this disposable test process to be traced by its sibling. */
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) _exit(2);
    pthread_t initial, watchdog, late;
    if (pthread_create(&initial, NULL, counter, (void *)0L) ||
        pthread_create(&watchdog, NULL, counter, (void *)2L)) _exit(2);
    int created = 0;
    while (!atomic_load(&shared->stop)) {
        if (!created && atomic_load(&shared->create_late)) {
            if (pthread_create(&late, NULL, counter, (void *)1L)) _exit(2);
            created = 1;
        }
        atomic_fetch_add(&shared->main_ticks, 1);
        usleep(1000);
    }
    _exit(0);
}

static int stopped_by(pid_t target_pid, int tid, pid_t holder)
{
    char path[128], line[256];
    snprintf(path, sizeof(path), "/proc/%d/task/%d/status", target_pid, tid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int tracer = 0, stopped = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "TracerPid: %d", &tracer) == 1) continue;
        char state;
        if (sscanf(line, "State: %c", &state) == 1 && (state == 't' || state == 'T')) stopped = 1;
    }
    fclose(f);
    return tracer == holder && stopped;
}

static int wait_holding(int fd)
{
    char output[4096] = {0};
    size_t used = 0;
    long long deadline = milliseconds() + 3000;
    while (milliseconds() < deadline && used < sizeof(output) - 1) {
        struct pollfd pfd = {fd, POLLIN, 0};
        if (poll(&pfd, 1, 100) <= 0) continue;
        ssize_t count = read(fd, output + used, sizeof(output) - used - 1);
        if (count <= 0) break;
        used += (size_t)count;
        output[used] = '\0';
        if (strstr(output, "HOLDING ")) return 1;
    }
    fprintf(stderr, "holder output: %s\n", output);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s UI_HOLD_BINARY\n", argv[0]); return 2; }
    shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) return 2;
    int pipes[2];
    if (pipe(pipes)) return 2;
    pid_t target_pid = fork();
    if (target_pid < 0) return 2;
    if (target_pid == 0) { close(pipes[0]); close(pipes[1]); target(); }
    long long deadline = milliseconds() + 3000;
    while (!atomic_load(&shared->initial_ready) && milliseconds() < deadline) usleep(1000);
    pid_t holder = fork();
    if (holder < 0) { kill(target_pid, SIGKILL); waitpid(target_pid, NULL, 0); return 2; }
    if (holder == 0) {
        char pid[32];
        snprintf(pid, sizeof(pid), "%d", target_pid);
        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[1]);
        execl(argv[1], argv[1], pid, (char *)NULL);
        _exit(127);
    }
    close(pipes[1]);
    int ready = wait_holding(pipes[0]);
    unsigned main_before = atomic_load(&shared->main_ticks);
    unsigned watchdog_before = atomic_load(&shared->watchdog_ticks);
    long long start = milliseconds();
    atomic_store(&shared->create_late, 1);
    int held = 0;
    while (ready && milliseconds() - start < 500) {
        int tid = atomic_load(&shared->late_tid);
        if (tid && stopped_by(target_pid, tid, holder)) { held = 1; break; }
        usleep(5000);
    }
    long long latency = milliseconds() - start;
    unsigned late_before = atomic_load(&shared->late_ticks);
    usleep(150000);
    int frozen = held && atomic_load(&shared->late_ticks) == late_before;
    int running = atomic_load(&shared->main_ticks) > main_before &&
                  atomic_load(&shared->watchdog_ticks) > watchdog_before;
    printf("%s late UI held within 500 ms (observed %lld ms)\n", held && frozen ? "PASS" : "FAIL", latency);
    printf("%s main and watchdog continue running\n", running ? "PASS" : "FAIL");
    kill(holder, SIGTERM);
    int status = 0, exited = 0;
    deadline = milliseconds() + 2000;
    while (milliseconds() < deadline) {
        if (waitpid(holder, &status, WNOHANG) == holder) { exited = 1; break; }
        usleep(10000);
    }
    if (!exited) { kill(holder, SIGKILL); waitpid(holder, &status, 0); }
    usleep(100000);
    int resumed = exited && WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                  atomic_load(&shared->late_ticks) > late_before;
    printf("%s late UI resumes after holder termination\n", resumed ? "PASS" : "FAIL");
    kill(target_pid, SIGKILL);
    waitpid(target_pid, NULL, 0);
    close(pipes[0]);
    munmap(shared, sizeof(*shared));
    return ready && held && frozen && running && resumed ? 0 : 1;
}
