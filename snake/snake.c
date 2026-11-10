/*
 * One-button snake for adol D623+ (128x128 SPI framebuffer, /dev/event0).
 * Button: press to start; each press turns clockwise; press to restart.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define W 128
#define H 128
#define FB_BYTES (W * H * 2)
#define CELL 8
#define COLS (W / CELL)
#define STATUS_H 16
#define ROWS ((H - STATUS_H) / CELL)
#define NCELL (COLS * ROWS)

struct input_event_local {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};
typedef char input_event_size_check[(sizeof(struct input_event_local) == 16) ? 1 : -1];

#define EV_KEY 1
#define KEY_POWER 116

/* RGB565, little endian. */
#define C_BG     0x0000
#define C_HEAD   0x07E0
#define C_BODY   0x0320
#define C_FOOD   0xF800
#define C_TEXT   0xFFFF
#define C_YELLOW 0xFFE0
#define C_GRAY   0x8410
#define C_RED    0xF800
#define C_GREEN  0x07E0
#define C_BLUE   0x001F
#define C_WHITE  0xFFFF

enum { ST_READY = 0, ST_PLAY = 1, ST_OVER = 2 };

static volatile sig_atomic_t g_quit = 0;
static uint16_t *g_fb = NULL;
static uint16_t g_fb_save[W * H];
static int g_fb_fd = -1;
static int g_fb_saved = 0;
static int g_ev_fd = -1;
static int g_dirty = 0;
static int g_flush_verbose = 0;
static int g_lcd_err = 0;

static int g_state = ST_READY;
static uint8_t g_sx[NCELL], g_sy[NCELL];
static int g_head, g_tail, g_len, g_dir, g_score, g_pending;
static int g_fx = -1, g_fy = -1;
static long long g_next_tick, g_next_blink;
static int g_blink_on;
static int g_demo = 0;
static long long g_demo_next;

static const int DX[4] = {1, 0, -1, 0};
static const int DY[4] = {0, 1, 0, -1};

typedef struct {
    char ch;
    uint8_t row[5];
} Glyph;

static const Glyph FONT[] = {
    {'0', {0b111, 0b101, 0b101, 0b101, 0b111}},
    {'1', {0b010, 0b110, 0b010, 0b010, 0b111}},
    {'2', {0b111, 0b001, 0b111, 0b100, 0b111}},
    {'3', {0b111, 0b001, 0b111, 0b001, 0b111}},
    {'4', {0b101, 0b101, 0b111, 0b001, 0b001}},
    {'5', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'6', {0b111, 0b100, 0b111, 0b101, 0b111}},
    {'7', {0b111, 0b001, 0b001, 0b001, 0b001}},
    {'8', {0b111, 0b101, 0b111, 0b101, 0b111}},
    {'9', {0b111, 0b101, 0b111, 0b001, 0b111}},
    {'S', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'C', {0b111, 0b100, 0b100, 0b100, 0b111}},
    {'O', {0b111, 0b101, 0b101, 0b101, 0b111}},
    {'R', {0b111, 0b101, 0b110, 0b101, 0b101}},
    {'E', {0b111, 0b100, 0b111, 0b100, 0b111}},
    {'G', {0b111, 0b100, 0b101, 0b101, 0b111}},
    {'A', {0b111, 0b101, 0b111, 0b101, 0b101}},
    {'M', {0b101, 0b111, 0b111, 0b101, 0b101}},
    {'V', {0b101, 0b101, 0b101, 0b101, 0b010}},
    {'P', {0b111, 0b101, 0b111, 0b100, 0b100}},
    {'T', {0b111, 0b010, 0b010, 0b010, 0b010}},
    {'N', {0b101, 0b111, 0b111, 0b111, 0b101}},
    {'K', {0b101, 0b101, 0b110, 0b101, 0b101}},
    {':', {0b000, 0b010, 0b000, 0b010, 0b000}},
    {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
};

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

#define LCD_DEBUGFS "/sys/kernel/debug/lcd_zx29/lcd_lead"

static void lcd_cmd(const char *cmd)
{
    int fd = open(LCD_DEBUGFS, O_WRONLY);
    if (fd < 0) {
        if (!g_lcd_err) {
            fprintf(stderr, "lcd debugfs open failed: %s\n", strerror(errno));
            g_lcd_err = 1;
        }
        return;
    }
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%s\n", cmd);
    if (n > 0) {
        ssize_t w = write(fd, buf, (size_t)n);
        if (w != (ssize_t)n && !g_lcd_err) {
            fprintf(stderr, "lcd debugfs write failed: %s\n", strerror(errno));
            g_lcd_err = 1;
        }
    }
    close(fd);
}

static void lcd_wake(void)
{
    lcd_cmd("sleepout");
    lcd_cmd("blgon");
}

static int fb_init(int save)
{
    g_fb_fd = open("/dev/fb0", O_RDWR);
    if (g_fb_fd < 0) {
        perror("open /dev/fb0");
        return -1;
    }
    g_fb = mmap(NULL, FB_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    if (g_fb == MAP_FAILED) {
        perror("mmap /dev/fb0");
        g_fb = NULL;
        close(g_fb_fd);
        g_fb_fd = -1;
        return -1;
    }
    if (save) {
        memcpy(g_fb_save, g_fb, FB_BYTES);
        g_fb_saved = 1;
    }
    return 0;
}

static void flush_fb(void)
{
    if (!g_fb || g_fb_fd < 0)
        return;
    errno = 0;
    ssize_t n = write(g_fb_fd, g_fb, FB_BYTES);
    if (n == (ssize_t)FB_BYTES) {
        g_dirty = 0;
    } else {
        /* Panel may have gone to sleep; wake it and retry next loop. */
        lcd_wake();
    }
    if (g_flush_verbose) {
        printf("FBWRITE n=%ld errno=%d\n", (long)n, errno);
        fflush(stdout);
        g_flush_verbose = 0;
    }
}

static void cleanup_all(void)
{
    if (g_fb) {
        if (g_fb_saved) {
            memcpy(g_fb, g_fb_save, FB_BYTES);
            lcd_wake();
            if (g_fb_fd >= 0) {
                ssize_t nw = write(g_fb_fd, g_fb, FB_BYTES);
                (void)nw;
            }
        }
        munmap(g_fb, FB_BYTES);
        g_fb = NULL;
    }
    if (g_fb_fd >= 0) {
        close(g_fb_fd);
        g_fb_fd = -1;
    }
    if (g_ev_fd >= 0) {
        close(g_ev_fd);
        g_ev_fd = -1;
    }
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0 || !g_fb)
        return;
    uint16_t r5 = (color >> 11) & 0x1F;
    uint16_t g6 = (color >> 5) & 0x3F;
    uint16_t b5 = color & 0x1F;
    uint16_t r6 = (uint16_t)((r5 << 1) | (r5 >> 4));
    uint16_t g5 = (uint16_t)(g6 >> 1);
    uint16_t panel = (uint16_t)((b5 << 11) | (r6 << 5) | g5);
    for (int j = 0; j < h; j++) {
        uint16_t *row = g_fb + (size_t)(y + j) * W + x;
        for (int i = 0; i < w; i++)
            row[i] = panel;
    }
    g_dirty = 1;
}

static void draw_rect_outline(int x, int y, int w, int h, uint16_t color)
{
    fill_rect(x, y, w, 1, color);
    fill_rect(x, y + h - 1, w, 1, color);
    fill_rect(x, y, 1, h, color);
    fill_rect(x + w - 1, y, 1, h, color);
}

static const uint8_t *find_glyph(char c)
{
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++)
        if (FONT[i].ch == c)
            return FONT[i].row;
    return NULL;
}

static int text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    if (n <= 0)
        return 0;
    return n * 4 * scale - scale;
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const uint8_t *rows = find_glyph(c);
    if (!rows)
        return;
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 3; col++) {
            if (rows[r] & (1 << (2 - col)))
                fill_rect(x + col * scale, y + r * scale, scale, scale, color);
        }
    }
}

static void draw_text(int x, int y, const char *s, int scale, uint16_t color)
{
    for (; *s; s++, x += 4 * scale)
        draw_char(x, y, *s, scale, color);
}

static void draw_text_center(int y, const char *s, int scale, uint16_t color)
{
    int x = (W - text_width(s, scale)) / 2;
    draw_text(x, y, s, scale, color);
}

static void draw_cell(int cx, int cy, uint16_t color)
{
    fill_rect(cx * CELL + 1, STATUS_H + cy * CELL + 1, CELL - 2, CELL - 2, color);
}

static void draw_status(void)
{
    char buf[16];
    fill_rect(0, 0, W, STATUS_H - 1, C_BG);
    snprintf(buf, sizeof(buf), "S:%d", g_score);
    draw_text(4, 3, buf, 2, C_TEXT);
}

static int cell_on_snake(int x, int y)
{
    int idx = g_tail;
    for (int i = 0; i < g_len; i++) {
        if (g_sx[idx] == x && g_sy[idx] == y)
            return 1;
        idx = (idx + 1) % NCELL;
    }
    return 0;
}

static void place_food(void)
{
    int free_count = 0;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (!cell_on_snake(x, y))
                free_count++;
    if (free_count == 0) {
        g_fx = g_fy = -1;
        return;
    }
    int pick = rand() % free_count;
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (!cell_on_snake(x, y)) {
                if (pick-- == 0) {
                    g_fx = x;
                    g_fy = y;
                    return;
                }
            }
        }
    }
    g_fx = g_fy = -1;
}

static void reset_board(void)
{
    int cx = COLS / 2, cy = ROWS / 2;
    g_tail = 0;
    g_head = 2;
    g_len = 3;
    g_sx[0] = cx - 2; g_sy[0] = cy;
    g_sx[1] = cx - 1; g_sy[1] = cy;
    g_sx[2] = cx;     g_sy[2] = cy;
    g_dir = 0;
    g_score = 0;
    g_pending = 0;
    place_food();
}

static void full_redraw(void)
{
    fill_rect(0, 0, W, H, C_BG);
    draw_status();
    fill_rect(0, STATUS_H - 1, W, 1, C_GRAY);
    int idx = g_tail;
    for (int i = 0; i < g_len; i++) {
        draw_cell(g_sx[idx], g_sy[idx], idx == g_head ? C_HEAD : C_BODY);
        idx = (idx + 1) % NCELL;
    }
    if (g_fx >= 0)
        draw_cell(g_fx, g_fy, C_FOOD);
    if (g_state == ST_READY) {
        draw_text_center(38, "SNAKE", 3, C_HEAD);
        draw_text_center(86, "PRESS", 2, C_YELLOW);
    }
}

static void draw_press_line(int visible)
{
    fill_rect(6, 80, W - 12, 16, C_BG);
    if (visible)
        draw_text_center(83, "PRESS", 2, C_YELLOW);
}

static void draw_game_over(void)
{
    fill_rect(4, 30, W - 8, 70, C_BG);
    draw_rect_outline(4, 30, W - 8, 70, C_TEXT);
    draw_text_center(36, "GAME", 2, C_TEXT);
    draw_text_center(50, "OVER", 2, C_RED);
    char buf[16];
    snprintf(buf, sizeof(buf), "S:%d", g_score);
    draw_text_center(66, buf, 2, C_YELLOW);
    g_blink_on = 1;
    draw_press_line(g_blink_on);
}

static void game_over(void)
{
    g_state = ST_OVER;
    g_next_blink = now_ms() + 450;
    draw_game_over();
}

static void start_game(void)
{
    reset_board();
    g_state = ST_PLAY;
    full_redraw();
    g_next_tick = now_ms() + 450;
}

static void move_snake(void)
{
    if (g_pending) {
        g_dir = (g_dir + 1) & 3;
        g_pending = 0;
    }
    int hx = g_sx[g_head], hy = g_sy[g_head];
    int nx = hx + DX[g_dir], ny = hy + DY[g_dir];
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
        game_over();
        return;
    }
    int grow = (nx == g_fx && ny == g_fy);
    int idx = g_tail;
    for (int i = 0; i < g_len; i++) {
        if (!grow && idx == g_tail) {
            idx = (idx + 1) % NCELL;
            continue;
        }
        if (g_sx[idx] == nx && g_sy[idx] == ny) {
            game_over();
            return;
        }
        idx = (idx + 1) % NCELL;
    }
    draw_cell(hx, hy, C_BODY);
    if (!grow) {
        draw_cell(g_sx[g_tail], g_sy[g_tail], C_BG);
        g_tail = (g_tail + 1) % NCELL;
    }
    g_head = (g_head + 1) % NCELL;
    g_sx[g_head] = nx;
    g_sy[g_head] = ny;
    draw_cell(nx, ny, C_HEAD);
    if (grow) {
        g_len++;
        g_score++;
        draw_status();
        place_food();
        if (g_fx >= 0)
            draw_cell(g_fx, g_fy, C_FOOD);
        if (g_len >= NCELL) {
            game_over();
            return;
        }
    }
}

static void handle_press(void)
{
    if (g_state == ST_READY || g_state == ST_OVER)
        start_game();
    else if (g_state == ST_PLAY)
        g_pending = 1;
}

static void drain_events(void)
{
    struct input_event_local ev;
    while (read(g_ev_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_KEY && ev.value == 1)
            handle_press();
    }
}

static void run_game(void)
{
    long long lcd_next = now_ms() + 2000;
    while (!g_quit) {
        long long now = now_ms();
        if (g_state == ST_PLAY && now >= g_next_tick) {
            move_snake();
            int tick = 230 - g_score * 4;
            if (tick < 130)
                tick = 130;
            g_next_tick = now + tick;
        }
        if (g_state == ST_OVER && now >= g_next_blink) {
            g_blink_on = !g_blink_on;
            draw_press_line(g_blink_on);
            g_next_blink = now + 450;
        }
        if (g_demo && now >= g_demo_next) {
            if (g_state == ST_PLAY)
                g_pending = 1;
            else if (g_state == ST_OVER)
                start_game();
            g_demo_next = now + 1500;
        }
        if (now >= lcd_next) {
            lcd_wake();
            g_dirty = 1;
            lcd_next = now + 2000;
        }
        if (g_dirty)
            flush_fb();

        long long to = 500;
        if (g_state == ST_PLAY) {
            to = g_next_tick - now;
            if (to < 0) to = 0;
        } else if (g_state == ST_OVER) {
            long long bt = g_next_blink - now;
            if (bt < 0) bt = 0;
            if (bt < to) to = bt;
        }
        struct timeval tv;
        tv.tv_sec = (time_t)(to / 1000);
        tv.tv_usec = (suseconds_t)((to % 1000) * 1000);
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_ev_fd, &rfds);
        int r = select(g_ev_fd + 1, &rfds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(g_ev_fd, &rfds))
            drain_events();
        if (g_dirty)
            flush_fb();
    }
}

static void probe_pattern(void)
{
    fill_rect(0, 0, W, H, C_BG);
    fill_rect(0, 0, 24, 24, C_RED);
    fill_rect(W - 24, 0, 24, 24, C_GREEN);
    fill_rect(0, H - 24, 24, 24, C_BLUE);
    fill_rect(W - 24, H - 24, 24, 24, C_WHITE);
    draw_rect_outline(0, 0, W, H, C_TEXT);
    draw_text_center(30, "TEST", 3, C_TEXT);
}

static void run_probe(void)
{
    probe_pattern();
    g_flush_verbose = 1;
    flush_fb();
    printf("PROBE READY %dx%d\n", W, H);
    printf("FBREAD TL=%04X TR=%04X BL=%04X BR=%04X\n",
           g_fb[12 * W + 12], g_fb[12 * W + 115],
           g_fb[115 * W + 12], g_fb[115 * W + 115]);
    fflush(stdout);

    long long start = now_ms();
    long long next_blink = start + 500;
    long long lcd_next = start + 2000;
    int on = 1;
    while (!g_quit && now_ms() - start < 180000) {
        long long now = now_ms();
        if (now >= next_blink) {
            on = !on;
            fill_rect(56, 72, 16, 16, on ? C_TEXT : C_BG);
            next_blink = now + 500;
        }
        if (now >= lcd_next) {
            lcd_wake();
            g_dirty = 1;
            lcd_next = now + 2000;
        }
        if (g_dirty)
            flush_fb();
        struct timeval tv = {0, 200000};
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_ev_fd, &rfds);
        int r = select(g_ev_fd + 1, &rfds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(g_ev_fd, &rfds)) {
            struct input_event_local ev;
            while (read(g_ev_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                printf("EV type=%u code=%u value=%d\n", ev.type, ev.code, ev.value);
                if (ev.type == EV_KEY && ev.value == 1)
                    printf("KEYPRESS code=%u\n", ev.code);
                fflush(stdout);
            }
        }
    }
    printf("PROBE EXIT\n");
    fflush(stdout);
}

static void run_watch(void)
{
    printf("WATCH READY\n");
    fflush(stdout);
    while (!g_quit) {
        struct input_event_local ev;
        ssize_t n = read(g_ev_fd, &ev, sizeof(ev));
        if (n == (ssize_t)sizeof(ev)) {
            printf("EV type=%u code=%u value=%d\n", ev.type, ev.code, ev.value);
            fflush(stdout);
        } else if (n < 0 && errno != EINTR) {
            perror("read /dev/event0");
            break;
        }
    }
    printf("WATCH EXIT\n");
    fflush(stdout);
}

static void run_lcdtest(void)
{
    probe_pattern();
    lcd_wake();
    g_flush_verbose = 1;
    flush_fb();
    printf("LCDTEST HOLD\n");
    fflush(stdout);
    while (!g_quit) {
        struct timeval tv = {2, 0};
        select(0, NULL, NULL, NULL, &tv);
        if (g_quit)
            break;
        lcd_wake();
        g_dirty = 1;
        flush_fb();
    }
    printf("LCDTEST EXIT\n");
    fflush(stdout);
}

static void install_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
}

static int run_restore(const char *path)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/fb0");
        return 1;
    }
    uint16_t *fb = mmap(NULL, FB_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap /dev/fb0");
        close(fd);
        return 1;
    }
    int sf = open(path, O_RDONLY);
    if (sf < 0) {
        perror("open snapshot");
        munmap(fb, FB_BYTES);
        close(fd);
        return 1;
    }
    ssize_t n = read(sf, fb, FB_BYTES);
    if (n == (ssize_t)FB_BYTES) {
        lcd_wake();
        ssize_t nw = write(fd, fb, FB_BYTES);
        if (nw != (ssize_t)FB_BYTES) {
            lcd_wake();
            nw = write(fd, fb, FB_BYTES);
        }
        if (nw != (ssize_t)FB_BYTES) {
            fprintf(stderr, "restore write failed: %ld\n", (long)nw);
            close(sf);
            munmap(fb, FB_BYTES);
            close(fd);
            return 1;
        }
    }
    close(sf);
    munmap(fb, FB_BYTES);
    close(fd);
    if (n != (ssize_t)FB_BYTES) {
        fprintf(stderr, "short snapshot read: %ld\n", (long)n);
        return 1;
    }
    return 0;
}

static int run_peek(int pid, unsigned long addr, int count)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/mem", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open mem");
        return 1;
    }
    for (int i = 0; i < count; i++) {
        unsigned int v = 0;
        unsigned long a = addr + (unsigned long)i * 4;
        ssize_t n = pread(fd, &v, sizeof(v), (off_t)a);
        if (n == (ssize_t)sizeof(v))
            printf("0x%08lx = 0x%08x (%u)\n", a, v, v);
        else
            printf("0x%08lx = ? (n=%ld)\n", a, (long)n);
    }
    close(fd);
    return 0;
}

static int run_fbinfo(void)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/fb0");
        return 1;
    }
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    memset(&fix, 0, sizeof(fix));
    memset(&var, 0, sizeof(var));
    int rf = ioctl(fd, FBIOGET_FSCREENINFO, &fix);
    int rv = ioctl(fd, FBIOGET_VSCREENINFO, &var);
    printf("fix rc=%d id=%s smem_len=%u line_length=%u ypanstep=%u ywrapstep=%u type=%u visual=%u\n",
           rf, fix.id, fix.smem_len, fix.line_length, fix.ypanstep, fix.ywrapstep,
           fix.type, fix.visual);
    printf("var rc=%d %ux%u virt=%ux%u off=%u,%u bpp=%u activate=%u\n",
           rv, var.xres, var.yres, var.xres_virtual, var.yres_virtual,
           var.xoffset, var.yoffset, var.bits_per_pixel, var.activate);
    fflush(stdout);
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    install_signals();
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    if (argc > 2 && strcmp(argv[1], "--restore") == 0)
        return run_restore(argv[2]);
    if (argc > 3 && strcmp(argv[1], "--peek") == 0)
        return run_peek(atoi(argv[2]), strtoul(argv[3], NULL, 0),
                        argc > 4 ? atoi(argv[4]) : 1);
    if (argc > 1 && strcmp(argv[1], "--fbinfo") == 0)
        return run_fbinfo();

    int probe = argc > 1 && strcmp(argv[1], "--probe") == 0;
    int watch = argc > 1 && strcmp(argv[1], "--watch") == 0;
    int demo = argc > 1 && strcmp(argv[1], "--demo") == 0;
    int lcdtest = argc > 1 && strcmp(argv[1], "--lcdtest") == 0;

    int flags = watch ? O_RDONLY : (O_RDONLY | O_NONBLOCK);
    g_ev_fd = open("/dev/event0", flags);
    if (g_ev_fd < 0) {
        perror("open /dev/event0");
        return 1;
    }

    if (watch) {
        run_watch();
        close(g_ev_fd);
        return 0;
    }

    if (fb_init(1) < 0) {
        close(g_ev_fd);
        return 1;
    }
    atexit(cleanup_all);
    lcd_wake();

    if (probe) {
        run_probe();
        return 0;
    }

    if (lcdtest) {
        alarm(120);
        run_lcdtest();
        return 0;
    }

    if (demo) {
        alarm(60);
        reset_board();
        g_demo = 1;
        g_state = ST_PLAY;
        full_redraw();
        g_next_tick = now_ms() + 500;
        g_demo_next = now_ms() + 1500;
        run_game();
        return 0;
    }

    alarm(900);
    reset_board();
    g_state = ST_READY;
    full_redraw();
    run_game();
    return 0;
}
