/* Host-only regression tests: only harmless commands, never /sbin/poweroff. */
#define PET_SIM 1
#include <sys/wait.h>
static long long g_test_now = 1000000;
static long long now_ms(void) { return g_test_now; }
#include "petcard.c"

static int failures;
static void check(int condition, const char *label)
{
    printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    failures += !condition;
}

static void finish_command(const char *marker)
{
    for (int i = 0; i < 1000 && g_poweroff_child > 0; i++) {
        poll_system_poweroff(marker);
        usleep(1000);
    }
    check(g_poweroff_child == -1, "poweroff child is reaped");
}

int main(void)
{
    char dir[] = "/tmp/petcard-shutdown-test-XXXXXX";
    if (!mkdtemp(dir)) return 2;
    char marker[128];
    snprintf(marker, sizeof(marker), "%s/guard", dir);
    pid_t caller = getpid();
    g_poweroff_requested = 1;
    check(request_system_poweroff("/bin/true", marker) == 0 && getpid() == caller,
          "poweroff request preserves its caller process");
    check(access(marker, F_OK) == 0 && g_poweroff_started,
          "shutdown guard exists before waiting for command completion");
    finish_command(marker);
    check(g_poweroff_started && g_poweroff_requested && access(marker, F_OK) == 0,
          "successful request retains shutdown frame and respawn guard");
    unlink(marker);
    g_poweroff_started = 0;
    g_poweroff_started_at = 0;

    check(request_system_poweroff("/bin/false", marker) == 0, "nonzero command starts");
    finish_command(marker);
    check(!g_poweroff_started && !g_poweroff_requested && access(marker, F_OK) < 0,
          "nonzero command restores normal operation and removes guard");

    g_poweroff_requested = 1;
    check(request_system_poweroff("/no/such/petcard-test-command", marker) == 0,
          "missing command is handled in child");
    finish_command(marker);
    check(!g_poweroff_started && !g_poweroff_requested && access(marker, F_OK) < 0,
          "exec failure restores normal operation and removes guard");

    char sleeper[128];
    snprintf(sleeper, sizeof(sleeper), "%s/helper", dir);
    FILE *script = fopen(sleeper, "w");
    if (!script) return 2;
    fputs("#!/bin/sh\nexec sleep 60\n", script);
    fclose(script);
    chmod(sleeper, 0700);
    g_poweroff_requested = 1;
    check(request_system_poweroff(sleeper, marker) == 0, "signal test helper starts");
    if (g_poweroff_child > 0) kill(g_poweroff_child, SIGKILL);
    finish_command(marker);
    check(g_poweroff_started && g_poweroff_requested && access(marker, F_OK) == 0,
          "terminated helper retains guard because shutdown acceptance is uncertain");
    unlink(marker);
    unlink(sleeper);
    g_poweroff_started = 0;
    g_poweroff_started_at = 0;

    script = fopen(sleeper, "w");
    if (!script) return 2;
    fputs("#!/bin/sh\nexec sleep 60\n", script);
    fclose(script);
    chmod(sleeper, 0700);
    g_poweroff_requested = 1;
    g_test_now += 1000;
    check(request_system_poweroff(sleeper, marker) == 0,
          "long-running poweroff helper starts");
    long long started_at = g_test_now;
    g_test_now = started_at + 1000;
    poll_system_poweroff_step(marker, g_test_now);
    check(g_poweroff_started && access(marker, F_OK) == 0,
          "poweroff under timeout keeps shutdown state");
    g_test_now = started_at + POWEROFF_WAIT_MS;
    poll_system_poweroff_step(marker, g_test_now);
    check(!g_poweroff_started && !g_poweroff_requested && g_poweroff_child == -1 &&
          access(marker, F_OK) < 0,
          "poweroff timeout restores normal operation and removes guard");
    unlink(sleeper);

    g_soft_off = 0;
    g_hard_poweroff = 0;
    g_poweroff_requested = 1;
    g_poweroff_started = 0;
    begin_shutdown_action("/bin/true", marker);
    check(g_soft_off && !g_poweroff_started && access(marker, F_OK) < 0,
          "default shutdown selects soft-off without marker");
    g_soft_off = 0;

    g_hard_poweroff = 1;
    g_poweroff_requested = 1;
    begin_shutdown_action("/bin/true", marker);
    check(!g_soft_off && g_poweroff_started && access(marker, F_OK) == 0,
          "hard flag selects real poweroff");
    finish_command(marker);
    unlink(marker);
    g_poweroff_started = 0;
    g_poweroff_requested = 0;
    g_poweroff_started_at = 0;
    g_hard_poweroff = 0;

    int fd = open(marker, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) close(fd);
    g_poweroff_requested = 1;
    check(request_system_poweroff("/bin/true", marker) == -1 &&
          access(marker, F_OK) == 0 && g_poweroff_child == -1,
          "existing guard is not cleared or used for a second shutdown request");
    unlink(marker);
    rmdir(dir);
    printf("RESULT %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
