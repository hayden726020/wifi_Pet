#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(void)
{
    char buf[2048];
    memset(buf, 0x41, sizeof(buf));
    for (int i = 0; i < 60; i++) {
        int fd = open("/tmp/sysloop.tmp", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            write(fd, buf, sizeof(buf));
            close(fd);
        }
        sleep(1);
    }
    return 0;
}
