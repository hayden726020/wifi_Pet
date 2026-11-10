/*
 * PetCard V1 device core for adol D623+ (ZX297520V3, 128x128 B-R-G panel).
 * Display: full-frame canvas -> B[15:11] R[10:5] G[4:0] -> write(fb).
 * Input: only key, KEY_POWER 116. Short=cycle action, long=do action, double=pet.
 * Link: local HTTP on port 8420, served over the stock local AP (no uplink).
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
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/input.h>

#define W 128
#define H 128
#define FB_BYTES (W * H * 2)
#define HTTP_PORT 8420

#define KEY_POWER 116
#define LONG_MS 650
#define DOUBLE_MS 330

#define PET_MAGIC 0x43544550u
#define PET_VERSION 1

#ifdef PET_SIM
#define SAVE_DIR "/tmp/petcard_sim_state"
#define SAVE_FALLBACK_DIR "/tmp/petcard_sim_state_fb"
#else
#define SAVE_DIR "/mnt/userdata/petcard"
#define SAVE_FALLBACK_DIR "/tmp/petcard-state"
#endif
#define SAVE_FILE SAVE_DIR "/state.bin"
#define SAVE_BAK  SAVE_DIR "/state.bak"
#define SAVE_TMP  SAVE_DIR "/state.tmp"

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

#define C_BG         RGB(8, 12, 24)
#define C_PANEL      RGB(16, 22, 40)
#define C_LINE       RGB(45, 55, 80)
#define C_TEXT       0xFFFF
#define C_WHITE      0xFFFF
#define C_YELLOW     RGB(255, 220, 90)
#define C_DIM        RGB(150, 160, 185)
#define C_BODY       RGB(120, 225, 170)
#define C_BODY_DK    RGB(55, 140, 100)
#define C_BELLY      RGB(225, 250, 235)
#define C_EAR_IN     RGB(255, 150, 170)
#define C_EYE        RGB(25, 30, 45)
#define C_MOUTH      RGB(70, 30, 45)
#define C_TONGUE     RGB(255, 120, 140)
#define C_BLUSH      RGB(255, 130, 155)
#define C_HEART      RGB(255, 90, 120)
#define C_SHADOW     RGB(4, 6, 14)
#define C_APPLE      RGB(255, 90, 60)
#define C_LEAF       RGB(90, 200, 90)
#define C_BROWN      RGB(140, 90, 50)
#define C_BALL       RGB(80, 170, 255)
#define C_ZZZ        RGB(160, 200, 255)
#define C_BAR_BG     RGB(38, 46, 68)
#define C_HUNGER     RGB(255, 170, 70)
#define C_MOOD       RGB(255, 110, 160)
#define C_ENERGY     RGB(90, 210, 255)
#define C_LOW        RGB(255, 80, 80)
#define C_OK         RGB(120, 230, 140)
#define C_EXP        RGB(160, 140, 255)
#define C_BUBBLE_BG  RGB(250, 248, 225)
#define C_BUBBLE_ED  RGB(40, 45, 60)
#define C_BUBBLE_TX  RGB(35, 35, 45)

enum { EMO_IDLE = 0, EMO_HAPPY, EMO_HUNGRY, EMO_TIRED, EMO_ANGRY,
       EMO_SLEEP, EMO_EAT, EMO_PLAY, EMO_PET };
enum { EYE_OPEN = 0, EYE_CLOSED, EYE_HAPPY, EYE_HALF, EYE_ANGRY };
enum { ACT_FEED = 0, ACT_PLAY, ACT_SLEEP, ACT_COUNT };

static const char *const ACTION_NAMES[ACT_COUNT] = { "FEED", "PLAY", "SLEEP" };
static const char *const STAGE_NAMES[4] = { "BABY", "KID", "TEEN", "ADULT" };

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t hunger;     /* 0..10000, high = hungry */
    uint16_t mood;       /* 0..10000 */
    uint16_t energy;     /* 0..10000 */
    uint16_t intimacy;   /* 0..10000 */
    uint32_t exp;
    uint32_t age_min;
    uint32_t last_seen;  /* epoch seconds */
    uint32_t interactions;
    uint8_t sleeping;
    uint8_t stage;
    uint16_t reserved;
    uint32_t checksum;
} PetState;

typedef struct {
    char ch;
    uint8_t row[5];
} Glyph;

static const Glyph FONT[] = {
    {'A', {0b111, 0b101, 0b111, 0b101, 0b101}},
    {'B', {0b110, 0b101, 0b110, 0b101, 0b110}},
    {'C', {0b111, 0b100, 0b100, 0b100, 0b111}},
    {'D', {0b110, 0b101, 0b101, 0b101, 0b110}},
    {'E', {0b111, 0b100, 0b111, 0b100, 0b111}},
    {'F', {0b111, 0b100, 0b111, 0b100, 0b100}},
    {'G', {0b111, 0b100, 0b101, 0b101, 0b111}},
    {'H', {0b101, 0b101, 0b111, 0b101, 0b101}},
    {'I', {0b111, 0b010, 0b010, 0b010, 0b111}},
    {'J', {0b001, 0b001, 0b001, 0b101, 0b111}},
    {'K', {0b101, 0b101, 0b110, 0b101, 0b101}},
    {'L', {0b100, 0b100, 0b100, 0b100, 0b111}},
    {'M', {0b101, 0b111, 0b111, 0b101, 0b101}},
    {'N', {0b101, 0b111, 0b111, 0b111, 0b101}},
    {'O', {0b111, 0b101, 0b101, 0b101, 0b111}},
    {'P', {0b111, 0b101, 0b111, 0b100, 0b100}},
    {'Q', {0b111, 0b101, 0b101, 0b111, 0b001}},
    {'R', {0b111, 0b101, 0b110, 0b101, 0b101}},
    {'S', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'T', {0b111, 0b010, 0b010, 0b010, 0b010}},
    {'U', {0b101, 0b101, 0b101, 0b101, 0b111}},
    {'V', {0b101, 0b101, 0b101, 0b101, 0b010}},
    {'W', {0b101, 0b101, 0b111, 0b111, 0b101}},
    {'X', {0b101, 0b101, 0b010, 0b101, 0b101}},
    {'Y', {0b101, 0b101, 0b010, 0b010, 0b010}},
    {'Z', {0b111, 0b001, 0b010, 0b100, 0b111}},
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
    {'!', {0b010, 0b010, 0b010, 0b000, 0b010}},
    {'?', {0b111, 0b001, 0b011, 0b000, 0b010}},
    {'.', {0b000, 0b000, 0b000, 0b000, 0b010}},
    {':', {0b000, 0b010, 0b000, 0b010, 0b000}},
    {'-', {0b000, 0b000, 0b111, 0b000, 0b000}},
    {'+', {0b000, 0b010, 0b111, 0b010, 0b000}},
    {'/', {0b001, 0b001, 0b010, 0b100, 0b100}},
    {'\'', {0b010, 0b010, 0b000, 0b000, 0b000}},
    {'%', {0b101, 0b001, 0b010, 0b100, 0b101}},
    {',', {0b000, 0b000, 0b000, 0b010, 0b100}},
    {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
};

static const uint8_t ICON_HEART[8] = {0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00};
static const uint8_t ICON_APPLE[8] = {0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
static const uint8_t ICON_BOLT[8]  = {0x18, 0x30, 0x60, 0xFC, 0x18, 0x30, 0x60, 0xC0};
static const uint8_t HEART5[5]     = {0x0A, 0x1F, 0x1F, 0x0E, 0x04};

static uint16_t g_canvas[W * H];
static uint16_t g_panel[W * H];
static uint16_t g_snapshot[W * H];

static PetState g_state;
static char g_save_path[64], g_save_bak[64], g_save_tmp[64];
static int g_save_fallback = 0;

static char g_msg[40];
static long long g_msg_until = 0;
static long long g_next_auto_msg = 0;
static int g_transient_emo = EMO_IDLE;
static long long g_transient_until = 0;

static int g_selected = ACT_FEED;
static int g_key_down = 0;
static long long g_press_at = 0;
static int g_single_pending = 0;
static long long g_single_deadline = 0;
static int g_double_candidate = 0;

static long long g_next_blink = 0;
static long long g_blink_until = 0;

static volatile sig_atomic_t g_quit = 0;
static int g_dirty = 1;
static int g_state_dirty = 1;
static int g_force_render = 1;
static long long g_last_save = 0;
static int g_debug = 0;
static int g_nohttp = 0;

static int g_fb_fd = -1;
static uint16_t *g_fb_map = NULL;
static int g_ev_fd = -1;
static int g_srv_fd = -1;
static int g_have_snapshot = 0;
static int g_lcd_warned = 0;

static int g_flush_count = 0;
static long long g_flush_us_sum = 0;
static long long g_flush_us_max = 0;

#ifndef PET_SIM
static long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static int pct(uint16_t v)
{
    int p = (v + 50) / 100;
    return clamp_int(p, 0, 100);
}

static int stage_for_exp(uint32_t exp)
{
    if (exp >= 250) return 3;
    if (exp >= 100) return 2;
    if (exp >= 30) return 1;
    return 0;
}

static uint32_t stage_prev_exp(int s)
{
    static const uint32_t p[4] = {0, 30, 100, 250};
    return p[clamp_int(s, 0, 3)];
}

static uint32_t stage_next_exp(int s)
{
    static const uint32_t n[4] = {30, 100, 250, 1000};
    return n[clamp_int(s, 0, 3)];
}

/* ---------------- drawing ---------------- */

static inline void put_px(int x, int y, uint16_t c)
{
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H)
        g_canvas[y * W + x] = c;
}

static void fill_rect(int x, int y, int w, int h, uint16_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        uint16_t *row = g_canvas + (size_t)(y + j) * W + x;
        for (int i = 0; i < w; i++) row[i] = c;
    }
}

static void fill_ellipse(int cx, int cy, int rx, int ry, uint16_t c)
{
    if (rx <= 0 || ry <= 0) return;
    int64_t rx2 = (int64_t)rx * rx;
    int64_t ry2 = (int64_t)ry * ry;
    int64_t lim = rx2 * ry2;
    for (int y = -ry; y <= ry; y++) {
        int64_t y2 = (int64_t)y * y * rx2;
        for (int x = -rx; x <= rx; x++) {
            if ((int64_t)x * x * ry2 + y2 <= lim)
                put_px(cx + x, cy + y, c);
        }
    }
}

static void fill_circle(int cx, int cy, int r, uint16_t c)
{
    fill_ellipse(cx, cy, r, r, c);
}

static int tri_edge(int ax, int ay, int bx, int by, int px, int py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int e0 = tri_edge(x0, y0, x1, y1, x, y);
            int e1 = tri_edge(x1, y1, x2, y2, x, y);
            int e2 = tri_edge(x2, y2, x0, y0, x, y);
            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0))
                put_px(x, y, c);
        }
    }
}

static void line(int x0, int y0, int x1, int y1, int thick, uint16_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fill_rect(x0 - thick / 2, y0 - thick / 2, thick, thick, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void round_rect(int x, int y, int w, int h, int r, uint16_t c)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    fill_rect(x + r, y, w - 2 * r, h, c);
    fill_rect(x, y + r, w, h - 2 * r, c);
    fill_circle(x + r, y + r, r, c);
    fill_circle(x + w - r - 1, y + r, r, c);
    fill_circle(x + r, y + h - r - 1, r, c);
    fill_circle(x + w - r - 1, y + h - r - 1, r, c);
}

static const uint8_t *find_glyph(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++)
        if (FONT[i].ch == ch) return FONT[i].row;
    return NULL;
}

static int text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    if (n <= 0) return 0;
    return n * 4 * scale - scale;
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const uint8_t *rows = find_glyph(c);
    if (!rows) return;
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
    int w = text_width(s, scale);
    draw_text((W - w) / 2, y, s, scale, color);
}

static void draw_bitmap(int x, int y, int w, int h, const uint8_t *rows, uint16_t c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (rows[j] & (1 << (w - 1 - i)))
                put_px(x + i, y + j, c);
}

/* ---------------- state ---------------- */

static uint32_t checksum_state(const PetState *s)
{
    const uint8_t *p = (const uint8_t *)s;
    size_t n = offsetof(PetState, checksum);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static void init_save_paths(void)
{
    snprintf(g_save_path, sizeof(g_save_path), "%s", SAVE_FILE);
    snprintf(g_save_bak, sizeof(g_save_bak), "%s", SAVE_BAK);
    snprintf(g_save_tmp, sizeof(g_save_tmp), "%s", SAVE_TMP);
    g_save_fallback = 0;
}

static void state_defaults(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.magic = PET_MAGIC;
    g_state.version = PET_VERSION;
    g_state.hunger = 2000;
    g_state.mood = 7000;
    g_state.energy = 8000;
    g_state.intimacy = 500;
    g_state.exp = 0;
    g_state.age_min = 0;
    g_state.last_seen = (uint32_t)time(NULL);
    g_state.interactions = 0;
    g_state.sleeping = 0;
    g_state.stage = 0;
}

static void clamp_state(void)
{
    if (g_state.hunger > 10000) g_state.hunger = 10000;
    if (g_state.mood > 10000) g_state.mood = 10000;
    if (g_state.energy > 10000) g_state.energy = 10000;
    if (g_state.intimacy > 10000) g_state.intimacy = 10000;
    if (g_state.exp > 999999) g_state.exp = 999999;
    if (g_state.age_min > 9999999) g_state.age_min = 9999999;
    if (g_state.stage > 3) g_state.stage = 3;
}

static int read_state_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    PetState s;
    ssize_t n = read(fd, &s, sizeof(s));
    close(fd);
    if (n != (ssize_t)sizeof(s)) return -1;
    if (s.magic != PET_MAGIC || s.version != PET_VERSION) return -1;
    if (s.checksum != checksum_state(&s)) return -1;
    g_state = s;
    return 0;
}

static void apply_minutes(int mins)
{
    if (mins <= 0) return;
    if (mins > 1440) mins = 1440;
    long h = (long)g_state.hunger + (long)mins * 25;
    if (h > 10000) h = 10000;
    g_state.hunger = (uint16_t)h;

    int mood_rate = (g_state.hunger >= 6000 || g_state.energy <= 2000) ? 25 : 8;
    long m = (long)g_state.mood - (long)mins * mood_rate;
    if (m < 0) m = 0;
    g_state.mood = (uint16_t)m;

    long e = (long)g_state.energy;
    if (g_state.sleeping) {
        e += (long)mins * 150;
        if (e >= 10000) { e = 10000; g_state.sleeping = 0; }
    } else {
        e -= (long)mins * 15;
        if (e < 0) e = 0;
    }
    g_state.energy = (uint16_t)e;

    if (g_state.age_min < 9999999) g_state.age_min += (uint32_t)mins;
    clamp_state();
    g_state_dirty = 1;
}

static void apply_offline_time(void)
{
    time_t now = time(NULL);
    uint32_t last = g_state.last_seen;
    if (last > 0 && (uint32_t)now > last) {
        long long sec = (long long)now - (long long)last;
        int mins = (int)(sec / 60);
        if (mins > 1440) mins = 1440;
        if (mins > 0) apply_minutes(mins);
    }
    g_state.last_seen = (uint32_t)now;
}

static int write_state_file(const char *path, const PetState *s)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    const uint8_t *p = (const uint8_t *)s;
    size_t left = sizeof(*s);
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    if (fsync(fd) < 0) { /* jffs2 may still accept the file */ }
    close(fd);
    return 0;
}

static void switch_save_fallback(void)
{
    if (g_save_fallback) return;
    g_save_fallback = 1;
    mkdir(SAVE_FALLBACK_DIR, 0755);
    snprintf(g_save_path, sizeof(g_save_path), "%s/state.bin", SAVE_FALLBACK_DIR);
    snprintf(g_save_bak, sizeof(g_save_bak), "%s/state.bak", SAVE_FALLBACK_DIR);
    snprintf(g_save_tmp, sizeof(g_save_tmp), "%s/state.tmp", SAVE_FALLBACK_DIR);
    if (g_debug) printf("SAVE fallback to %s\n", SAVE_FALLBACK_DIR);
}

static int save_state(void)
{
    if (g_save_fallback)
        init_save_paths();
    mkdir(SAVE_DIR, 0755);

    g_state.magic = PET_MAGIC;
    g_state.version = PET_VERSION;
    g_state.stage = (uint8_t)stage_for_exp(g_state.exp);
    g_state.last_seen = (uint32_t)time(NULL);
    g_state.reserved = 0;
    g_state.checksum = checksum_state(&g_state);

    int ok = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt == 0)
            mkdir(SAVE_DIR, 0755);
        else
            mkdir(SAVE_FALLBACK_DIR, 0755);
        if (write_state_file(g_save_tmp, &g_state) == 0) {
            rename(g_save_path, g_save_bak);
            if (rename(g_save_tmp, g_save_path) == 0) {
                ok = 1;
                break;
            }
        }
        if (attempt == 0)
            switch_save_fallback();
    }
    g_last_save = now_ms();
    if (!ok) {
        if (g_debug) printf("SAVE failed (primary and fallback)\n");
        return -1;
    }
    g_state_dirty = 0;
    return 0;
}

static void load_state(void)
{
    state_defaults();
    if (read_state_file(g_save_path) != 0) {
        if (read_state_file(g_save_bak) != 0) {
            if (g_debug) printf("SAVE none, using defaults\n");
        }
    }
    apply_offline_time();
    clamp_state();
    g_state.stage = (uint8_t)stage_for_exp(g_state.exp);
}

static int msg_valid(long long now)
{
    return g_msg[0] != '\0' && now < g_msg_until;
}

static void set_msg(const char *s, int ms)
{
    snprintf(g_msg, sizeof(g_msg), "%s", s);
    g_msg_until = now_ms() + ms;
    g_next_auto_msg = g_msg_until + 12000;
    g_force_render = 1;
}

static void update_stage(void)
{
    int ns = stage_for_exp(g_state.exp);
    if (ns > g_state.stage) {
        g_state.stage = (uint8_t)ns;
        g_transient_emo = EMO_HAPPY;
        g_transient_until = now_ms() + 2600;
        set_msg("LEVEL UP!", 2400);
    } else {
        g_state.stage = (uint8_t)ns;
    }
}

static const char *status_msg(void)
{
    if (g_state.hunger >= 8000) return "SO HUNGRY...";
    if (g_state.energy <= 2000) return "SO SLEEPY...";
    if (g_state.mood <= 2000) return "GRR...";
    if (g_state.mood >= 7500) return "I'M HAPPY!";
    if (g_state.intimacy < 1500) return "HELLO!";
    return "PET ME?";
}

static void do_wake(void)
{
    if (!g_state.sleeping) return;
    g_state.sleeping = 0;
    set_msg("HI!", 1500);
    g_state_dirty = 1;
    g_dirty = 1;
}

static void do_feed(void)
{
    if (g_state.sleeping) { set_msg("ZZZ...", 1500); return; }
    g_selected = ACT_FEED;
    if (g_state.hunger <= 500) {
        set_msg("I'M FULL!", 1800);
        g_transient_emo = EMO_HAPPY;
        g_transient_until = now_ms() + 1500;
        return;
    }
    g_state.hunger = (uint16_t)(g_state.hunger > 3500 ? g_state.hunger - 3500 : 0);
    g_state.mood = (uint16_t)clamp_int(g_state.mood + 500, 0, 10000);
    g_state.exp += 2;
    g_state.intimacy = (uint16_t)clamp_int(g_state.intimacy + 100, 0, 10000);
    g_state.interactions++;
    g_transient_emo = EMO_EAT;
    g_transient_until = now_ms() + 2500;
    set_msg("YUM!", 1800);
    update_stage();
    g_state_dirty = 1;
    g_dirty = 1;
}

static void do_play(void)
{
    if (g_state.sleeping) { set_msg("ZZZ...", 1500); return; }
    g_selected = ACT_PLAY;
    if (g_state.energy <= 1500) {
        set_msg("TOO TIRED", 1800);
        return;
    }
    g_state.mood = (uint16_t)clamp_int(g_state.mood + 2000, 0, 10000);
    g_state.energy = (uint16_t)(g_state.energy > 1500 ? g_state.energy - 1500 : 0);
    g_state.hunger = (uint16_t)clamp_int(g_state.hunger + 500, 0, 10000);
    g_state.exp += 3;
    g_state.intimacy = (uint16_t)clamp_int(g_state.intimacy + 200, 0, 10000);
    g_state.interactions++;
    g_transient_emo = EMO_PLAY;
    g_transient_until = now_ms() + 3000;
    set_msg("FUN!", 1800);
    update_stage();
    g_state_dirty = 1;
    g_dirty = 1;
}

static void do_pet(void)
{
    if (g_state.sleeping) { set_msg("ZZZ...", 1500); return; }
    g_state.mood = (uint16_t)clamp_int(g_state.mood + 800, 0, 10000);
    g_state.intimacy = (uint16_t)clamp_int(g_state.intimacy + 200, 0, 10000);
    g_state.exp += 1;
    g_state.interactions++;
    g_transient_emo = EMO_PET;
    g_transient_until = now_ms() + 1800;
    set_msg("HEHE!", 1600);
    update_stage();
    g_state_dirty = 1;
    g_dirty = 1;
}

static void do_toggle_sleep(void)
{
    if (g_state.sleeping) {
        do_wake();
        return;
    }
    g_state.sleeping = 1;
    g_selected = ACT_SLEEP;
    set_msg("ZZZ...", 2200);
    g_state_dirty = 1;
    g_dirty = 1;
}

static void do_short_press(void)
{
    if (g_state.sleeping) { do_wake(); return; }
    g_selected = (g_selected + 1) % ACT_COUNT;
    g_dirty = 1;
    g_force_render = 1;
}

static void do_long_press(void)
{
    if (g_state.sleeping) { do_wake(); return; }
    switch (g_selected) {
    case ACT_FEED: do_feed(); break;
    case ACT_PLAY: do_play(); break;
    default: do_toggle_sleep(); break;
    }
}

static void do_double_press(void)
{
    if (g_state.sleeping) { do_wake(); return; }
    do_pet();
}

static int current_emotion(long long now)
{
    if (g_state.sleeping) return EMO_SLEEP;
    if (now < g_transient_until) return g_transient_emo;
    if (g_state.hunger >= 8000) return EMO_HUNGRY;
    if (g_state.energy <= 2000) return EMO_TIRED;
    if (g_state.mood <= 2000) return EMO_ANGRY;
    if (g_state.mood >= 7500) return EMO_HAPPY;
    return EMO_IDLE;
}

/* ---------------- pet rendering ---------------- */

static void draw_eye(int x, int y, int style)
{
    switch (style) {
    case EYE_CLOSED:
        line(x - 4, y, x + 4, y, 2, C_EYE);
        break;
    case EYE_HAPPY:
        line(x - 4, y + 1, x, y - 2, 2, C_EYE);
        line(x, y - 2, x + 4, y + 1, 2, C_EYE);
        break;
    case EYE_HALF:
        fill_rect(x - 4, y, 8, 4, C_EYE);
        line(x - 4, y - 1, x + 4, y - 1, 1, C_BODY_DK);
        break;
    default:
        fill_ellipse(x, y, 3, 5, C_EYE);
        fill_rect(x + 1, y - 2, 2, 2, C_WHITE);
        break;
    }
}

static void draw_pet(int emo, long long t)
{
    int sc = clamp_int(g_state.stage, 0, 3);
    int rx = 26 + sc * 3;
    int ry = 21 + sc * 3;
    int cx = 64, cy = 75;
    int bob = (int)((t / 800) % 2);

    if (emo == EMO_HAPPY || emo == EMO_PLAY) {
        static const int b[4] = {0, -3, -5, -2};
        bob = b[(int)((t / 140) % 4)];
    } else if (emo == EMO_SLEEP) {
        ry += 1 + (int)((t / 900) % 2);
        cy += 2;
        bob = 0;
    }
    cy += bob;

    fill_ellipse(64, 107, rx + 2, 4, C_SHADOW);

    int ty = cy - ry;
    fill_triangle(cx - rx + 6, ty + 10, cx - rx + 12, ty - 14, cx - rx + 24, ty + 2, C_BODY_DK);
    fill_triangle(cx + rx - 6, ty + 10, cx + rx - 12, ty - 14, cx + rx - 24, ty + 2, C_BODY_DK);
    fill_triangle(cx - rx + 9, ty + 9, cx - rx + 13, ty - 9, cx - rx + 21, ty + 3, C_EAR_IN);
    fill_triangle(cx + rx - 9, ty + 9, cx + rx - 13, ty - 9, cx + rx - 21, ty + 3, C_EAR_IN);

    fill_ellipse(cx - 14, cy + ry - 2, 10, 5, C_BODY_DK);
    fill_ellipse(cx + 14, cy + ry - 2, 10, 5, C_BODY_DK);
    fill_ellipse(cx - 14, cy + ry - 3, 8, 4, C_BODY);
    fill_ellipse(cx + 14, cy + ry - 3, 8, 4, C_BODY);

    fill_ellipse(cx - rx + 2, cy + 5, 7, 10, C_BODY_DK);
    fill_ellipse(cx + rx - 2, cy + 5, 7, 10, C_BODY_DK);
    fill_ellipse(cx - rx + 3, cy + 5, 5, 8, C_BODY);
    fill_ellipse(cx + rx - 3, cy + 5, 5, 8, C_BODY);

    fill_ellipse(cx, cy, rx, ry, C_BODY_DK);
    fill_ellipse(cx, cy, rx - 2, ry - 2, C_BODY);
    fill_ellipse(cx, cy + ry / 4, rx / 2, ry / 2, C_BELLY);

    int ex = 10 + sc * 2;
    int ey = cy - ry / 4;
    int style = EYE_OPEN;
    if (emo == EMO_SLEEP) style = EYE_CLOSED;
    else if (emo == EMO_HAPPY || emo == EMO_PET || emo == EMO_PLAY) style = EYE_HAPPY;
    else if (emo == EMO_TIRED) style = EYE_HALF;
    else if (emo == EMO_ANGRY) style = EYE_ANGRY;
    else if (t < g_blink_until) style = EYE_CLOSED;

    draw_eye(cx - ex, ey, style);
    draw_eye(cx + ex, ey, style);

    if (emo == EMO_ANGRY) {
        line(cx - ex - 6, ey - 9, cx - ex + 3, ey - 6, 2, C_EYE);
        line(cx + ex + 6, ey - 9, cx + ex - 3, ey - 6, 2, C_EYE);
    }
    if (emo == EMO_HAPPY || emo == EMO_PET || emo == EMO_PLAY) {
        fill_ellipse(cx - ex - 5, ey + 7, 5, 3, C_BLUSH);
        fill_ellipse(cx + ex + 5, ey + 7, 5, 3, C_BLUSH);
    }

    int my = cy + ry / 4 + 2;
    if (emo == EMO_HAPPY || emo == EMO_PET || emo == EMO_PLAY) {
        fill_ellipse(cx, my + 1, 7, 5, C_MOUTH);
        fill_ellipse(cx, my + 3, 4, 2, C_TONGUE);
    } else if (emo == EMO_EAT) {
        if ((t / 180) % 2) {
            fill_ellipse(cx, my, 6, 5, C_MOUTH);
            fill_ellipse(cx, my + 2, 4, 2, C_TONGUE);
        } else {
            line(cx - 5, my, cx + 5, my, 2, C_MOUTH);
        }
    } else if (emo == EMO_HUNGRY) {
        fill_ellipse(cx, my + 1, 5, 4, C_MOUTH);
        fill_ellipse(cx, my + 3, 3, 2, C_TONGUE);
    } else if (emo == EMO_ANGRY) {
        line(cx - 5, my + 2, cx - 1, my, 2, C_MOUTH);
        line(cx - 1, my, cx + 5, my + 2, 2, C_MOUTH);
    } else if (emo == EMO_TIRED) {
        fill_ellipse(cx, my, 3, 3, C_MOUTH);
    } else if (emo == EMO_SLEEP) {
        line(cx - 3, my, cx + 3, my, 1, C_MOUTH);
    } else {
        line(cx - 5, my, cx - 1, my + 2, 2, C_MOUTH);
        line(cx - 1, my + 2, cx + 5, my, 2, C_MOUTH);
    }
}

static void draw_food(long long t)
{
    int phase = (int)((t / 120) % 6);
    int x = 64 + 30 - phase * 3;
    int y = 96;
    fill_circle(x, y, 6, C_APPLE);
    fill_rect(x - 1, y - 9, 2, 4, C_BROWN);
    fill_ellipse(x + 3, y - 9, 3, 2, C_LEAF);
    fill_rect(x - 3, y - 3, 2, 2, C_WHITE);
}

static void draw_ball(long long t)
{
    int ph = (int)((t / 120) % 8);
    int h = ph < 4 ? ph : 7 - ph;
    int x = 104;
    int y = 102 - h * 6;
    fill_circle(x, y, 6, C_BALL);
    fill_circle(x - 2, y - 2, 2, C_WHITE);
}

static void draw_heart(int x, int y, uint16_t c)
{
    draw_bitmap(x, y, 5, 5, HEART5, c);
}

static void draw_hearts(long long t)
{
    for (int i = 0; i < 3; i++) {
        int ph = (int)((t / 150 + i * 3) % 9);
        int x = 64 - 18 + i * 18;
        int y = 68 - ph * 4;
        if (y > 28)
            draw_heart(x, y, (i == 1) ? C_HEART : C_BLUSH);
    }
}

static void draw_zzz(long long t)
{
    int ph = (int)((t / 300) % 4);
    int y1 = 46 - ph * 3;
    int y2 = 36 - ph * 2;
    if (y1 > 26) draw_text(88, y1, "Z", 1, C_ZZZ);
    if (y2 > 26) draw_text(98, y2, "Z", 2, C_ZZZ);
}

/* ---------------- HUD / bubble / hint ---------------- */

static void draw_bar(int x, int y, int w, int h, int value, uint16_t color)
{
    fill_rect(x, y, w, h, C_BAR_BG);
    int fw = (w - 2) * clamp_int(value, 0, 100) / 100;
    if (fw > 0) fill_rect(x + 1, y + 1, fw, h - 2, color);
    fill_rect(x, y, w, 1, C_LINE);
    fill_rect(x, y + h - 1, w, 1, C_LINE);
}

static void draw_hud(void)
{
    fill_rect(0, 0, W, 22, C_PANEL);
    fill_rect(0, 22, W, 1, C_LINE);

    int sat = 100 - pct(g_state.hunger);
    draw_bitmap(2, 4, 8, 8, ICON_APPLE, C_HUNGER);
    draw_bar(12, 4, 26, 6, sat, sat <= 25 ? C_LOW : C_OK);

    int mood = pct(g_state.mood);
    draw_bitmap(45, 4, 8, 8, ICON_HEART, C_MOOD);
    draw_bar(55, 4, 26, 6, mood, mood <= 25 ? C_LOW : C_MOOD);

    int en = pct(g_state.energy);
    draw_bitmap(88, 4, 8, 8, ICON_BOLT, C_ENERGY);
    draw_bar(98, 4, 26, 6, en, en <= 25 ? C_LOW : C_ENERGY);

    int st = clamp_int(g_state.stage, 0, 3);
    uint32_t lo = stage_prev_exp(st), hi = stage_next_exp(st);
    int ep = 100;
    if (hi > lo) {
        uint32_t e = g_state.exp;
        if (e < lo) e = lo;
        if (e > hi) e = hi;
        ep = (int)((e - lo) * 100u / (hi - lo));
    }
    fill_rect(2, 14, 72, 3, C_BAR_BG);
    fill_rect(2, 14, 72 * ep / 100, 3, C_EXP);

    char buf[8];
    snprintf(buf, sizeof(buf), "L%d", st + 1);
    draw_text(78, 12, buf, 1, C_TEXT);
    draw_bitmap(94, 11, 8, 8, ICON_HEART, C_HEART);
    snprintf(buf, sizeof(buf), "%d", pct(g_state.intimacy));
    draw_text(104, 12, buf, 1, C_TEXT);
}

static void draw_bubble(long long now)
{
    if (!msg_valid(now)) return;
    char text[40];
    snprintf(text, sizeof(text), "%s", g_msg);
    int scale = strlen(text) <= 12 ? 2 : 1;
    if (text_width(text, scale) > 112) scale = 1;
    if (text_width(text, scale) > 112) {
        text[24] = '\0';
        scale = 1;
    }
    int w = text_width(text, scale) + 12;
    int h = 5 * scale + 10;
    int x = (W - w) / 2;
    int y = 24;
    round_rect(x, y, w, h, 5, C_BUBBLE_ED);
    round_rect(x + 1, y + 1, w - 2, h - 2, 4, C_BUBBLE_BG);
    fill_triangle(W / 2 - 5, y + h - 1, W / 2 + 5, y + h - 1, W / 2, y + h + 5, C_BUBBLE_ED);
    fill_triangle(W / 2 - 3, y + h - 1, W / 2 + 3, y + h - 1, W / 2, y + h + 3, C_BUBBLE_BG);
    draw_text(x + 6, y + 5, text, scale, C_BUBBLE_TX);
}

static void draw_hint(long long now)
{
    fill_rect(0, 108, W, H - 108, C_BG);

    if (g_state.sleeping) {
        draw_text_center(109, "SLEEPING", 1, C_TEXT);
        draw_text_center(118, "PRESS TO WAKE", 1, C_DIM);
    } else {
        draw_text_center(109, ACTION_NAMES[clamp_int(g_selected, 0, ACT_COUNT - 1)], 2, C_YELLOW);
        draw_text_center(120, "HOLD OK  2X PET", 1, C_DIM);
    }

    if (g_key_down) {
        long long held = now - g_press_at;
        if (held < 0) held = 0;
        if (held > LONG_MS) held = LONG_MS;
        int bw = (W - 28) * (int)held / LONG_MS;
        fill_rect(14, 125, W - 28, 3, C_BAR_BG);
        fill_rect(14, 125, bw, 3, C_YELLOW);
    }
}

static void render_frame(long long now)
{
    fill_rect(0, 0, W, H, C_BG);
    draw_hud();

    int emo = current_emotion(now);
    draw_pet(emo, now);

    if (emo == EMO_EAT) draw_food(now);
    else if (emo == EMO_PLAY) draw_ball(now);
    else if (emo == EMO_PET) draw_hearts(now);
    else if (emo == EMO_SLEEP) draw_zzz(now);

    draw_bubble(now);
    draw_hint(now);
    g_dirty = 1;
}

/* ---------------- hardware ---------------- */

#ifndef PET_SIM

#define LCD_DEBUGFS "/sys/kernel/debug/lcd_zx29/lcd_lead"

static void lcd_cmd(const char *cmd)
{
    int fd = open(LCD_DEBUGFS, O_WRONLY);
    if (fd < 0) {
        if (!g_lcd_warned) {
            fprintf(stderr, "lcd debugfs open failed: %s\n", strerror(errno));
            g_lcd_warned = 1;
        }
        return;
    }
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%s\n", cmd);
    if (n > 0) {
        ssize_t w = write(fd, buf, (size_t)n);
        if (w != (ssize_t)n && !g_lcd_warned) {
            fprintf(stderr, "lcd debugfs write failed: %s\n", strerror(errno));
            g_lcd_warned = 1;
        }
    }
    close(fd);
}

static void lcd_wake(void)
{
    lcd_cmd("sleepout");
    lcd_cmd("blgon");
}

static int fb_open(int save)
{
    g_fb_fd = open("/dev/fb0", O_RDWR);
    if (g_fb_fd < 0) {
        perror("open /dev/fb0");
        return -1;
    }
    g_fb_map = mmap(NULL, FB_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    if (g_fb_map == MAP_FAILED) {
        perror("mmap /dev/fb0");
        g_fb_map = NULL;
    }
    if (save) {
        if (g_fb_map) {
            memcpy(g_snapshot, g_fb_map, FB_BYTES);
            g_have_snapshot = 1;
        } else {
            size_t off = 0;
            while (off < FB_BYTES) {
                ssize_t n = read(g_fb_fd, (uint8_t *)g_snapshot + off, FB_BYTES - off);
                if (n <= 0) break;
                off += (size_t)n;
            }
            g_have_snapshot = (off == FB_BYTES);
        }
        if (!g_have_snapshot) fprintf(stderr, "fb snapshot short\n");
    }
    return 0;
}

static int flush_fb(void)
{
    if (g_fb_fd < 0) return 0;
    for (int i = 0; i < W * H; i++) {
        uint16_t c = g_canvas[i];
        uint16_t r5 = (c >> 11) & 0x1F;
        uint16_t g6 = (c >> 5) & 0x3F;
        uint16_t b5 = c & 0x1F;
        uint16_t r6 = (uint16_t)((r5 << 1) | (r5 >> 4));
        uint16_t g5 = (uint16_t)(g6 >> 1);
        g_panel[i] = (uint16_t)((b5 << 11) | (r6 << 5) | g5);
    }

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    errno = 0;
    if (g_fb_map)
        memcpy(g_fb_map, g_panel, FB_BYTES);
    const void *src = g_fb_map ? (const void *)g_fb_map : (const void *)g_panel;
    ssize_t n = write(g_fb_fd, src, FB_BYTES);
    gettimeofday(&t1, NULL);
    long long us = (long long)(t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec);
    g_flush_count++;
    g_flush_us_sum += us;
    if (us > g_flush_us_max) g_flush_us_max = us;

    if (n == (ssize_t)FB_BYTES) {
        g_dirty = 0;
    } else {
        lcd_wake();
        if (!g_lcd_warned) {
            fprintf(stderr, "fb write n=%ld errno=%d (panel woke, retrying)\n", (long)n, errno);
            g_lcd_warned = 1;
        }
    }
    if (g_debug && g_flush_count % 30 == 0) {
        printf("FLUSH n=%d avg_us=%lld max_us=%lld last_us=%lld\n",
               g_flush_count, g_flush_us_sum / g_flush_count, g_flush_us_max, us);
        fflush(stdout);
    }
    return n == (ssize_t)FB_BYTES ? 0 : -1;
}

static void fb_restore_and_close(void)
{
    if (g_fb_fd >= 0 && g_have_snapshot) {
        lcd_wake();
        if (g_fb_map)
            memcpy(g_fb_map, g_snapshot, FB_BYTES);
        const void *src = g_fb_map ? (const void *)g_fb_map : (const void *)g_snapshot;
        ssize_t n = write(g_fb_fd, src, FB_BYTES);
        if (n != (ssize_t)FB_BYTES) {
            lcd_wake();
            n = write(g_fb_fd, src, FB_BYTES);
        }
        if (n != (ssize_t)FB_BYTES)
            fprintf(stderr, "fb restore failed n=%ld\n", (long)n);
    }
    if (g_fb_map) {
        munmap(g_fb_map, FB_BYTES);
        g_fb_map = NULL;
    }
    if (g_fb_fd >= 0) {
        close(g_fb_fd);
        g_fb_fd = -1;
    }
}

static int run_restore(const char *path)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("open /dev/fb0"); return 1; }
    uint16_t *map = mmap(NULL, FB_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap /dev/fb0");
        map = NULL;
    }
    int sf = open(path, O_RDONLY);
    if (sf < 0) {
        perror("open snapshot");
        if (map) munmap(map, FB_BYTES);
        close(fd);
        return 1;
    }
    static uint16_t buf[W * H];
    uint16_t *dst = map ? map : buf;
    size_t off = 0;
    while (off < FB_BYTES) {
        ssize_t n = read(sf, (uint8_t *)dst + off, FB_BYTES - off);
        if (n <= 0) break;
        off += (size_t)n;
    }
    close(sf);
    if (off != FB_BYTES) {
        fprintf(stderr, "short snapshot %lu\n", (unsigned long)off);
        if (map) munmap(map, FB_BYTES);
        close(fd);
        return 1;
    }
    lcd_wake();
    ssize_t n = write(fd, dst, FB_BYTES);
    if (n != (ssize_t)FB_BYTES) {
        lcd_wake();
        n = write(fd, dst, FB_BYTES);
    }
    if (map) munmap(map, FB_BYTES);
    close(fd);
    if (n != (ssize_t)FB_BYTES) {
        fprintf(stderr, "restore write failed n=%ld\n", (long)n);
        return 1;
    }
    return 0;
}

#endif /* PET_SIM */

/* ---------------- HTTP control page ---------------- */

static const char PAGE[] =
"<!doctype html><html lang=zh><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>PetCard</title>"
"<style>body{margin:0;background:#0b1020;color:#eef;font:16px/1.5 system-ui,sans-serif}"
".card{max-width:420px;margin:24px auto;padding:20px;border-radius:18px;background:#151d33;box-shadow:0 8px 30px #0008}"
"h1{margin:0;font-size:26px;color:#8ff0c0}p.sub{margin:4px 0 16px;color:#9aa;font-size:13px}"
".row{display:flex;align-items:center;gap:8px;margin:8px 0}.row span{width:44px;color:#bcd}"
".bar{flex:1;height:10px;background:#26304d;border-radius:6px;overflow:hidden}"
".bar i{display:block;height:100%;width:0;background:#8ff0c0;border-radius:6px;transition:width .3s}"
".btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:16px}"
"button{padding:12px;border:0;border-radius:12px;background:#26345a;color:#eef;font-size:16px}"
"button:active{background:#3a4d80}#msg{min-height:24px;margin:14px 0 0;color:#ffd479;text-align:center}"
".foot{margin:14px 0 0;color:#667;font-size:12px;text-align:center}</style>"
"<div class=card><h1>MOMO</h1><p class=sub id=st>连接中...</p>"
"<div class=row><span>饱食</span><div class=bar><i id=h></i></div><b id=hv></b></div>"
"<div class=row><span>心情</span><div class=bar><i id=m></i></div><b id=mv></b></div>"
"<div class=row><span>精力</span><div class=bar><i id=e></i></div><b id=ev></b></div>"
"<div class=btns><button onclick=\"act('feed')\">喂食</button><button onclick=\"act('play')\">玩耍</button>"
"<button onclick=\"act('pet')\">摸摸</button><button onclick=\"act('sleep')\">睡觉/唤醒</button></div>"
"<p id=msg></p><p class=foot>设备本地连接，不经过互联网</p></div>"
"<script>const $=id=>document.getElementById(id);"
"async function refresh(){try{const j=await(await fetch('/state',{cache:'no-store'})).json();"
"$('st').textContent='阶段 '+(j.stage+1)+' · 年龄 '+j.age_min+' 分钟 · 亲密 '+j.intimacy;"
"$('h').style.width=(100-j.hunger)+'%';$('hv').textContent=(100-j.hunger);"
"$('m').style.width=j.mood+'%';$('mv').textContent=j.mood;"
"$('e').style.width=j.energy+'%';$('ev').textContent=j.energy;"
"$('msg').textContent=j.msg||'';}catch(e){$('st').textContent='离线';}}"
"async function act(a){try{await fetch('/action?do='+a,{cache:'no-store'});}catch(e){}refresh();}"
"refresh();setInterval(refresh,2000);</script>";

static int build_state_json(char *b, size_t n)
{
    int h = pct(g_state.hunger), m = pct(g_state.mood);
    int e = pct(g_state.energy), it = pct(g_state.intimacy);
    int st = clamp_int(g_state.stage, 0, 3);
    return snprintf(b, n,
        "{\"hunger\":%d,\"satiety\":%d,\"mood\":%d,\"energy\":%d,\"intimacy\":%d,"
        "\"exp\":%lu,\"stage\":%d,\"stage_name\":\"%s\",\"sleeping\":%d,"
        "\"age_min\":%lu,\"interactions\":%lu,\"selected\":\"%s\","
        "\"msg\":\"%s\",\"ok\":1}",
        h, 100 - h, m, e, it,
        (unsigned long)g_state.exp, st, STAGE_NAMES[st], g_state.sleeping ? 1 : 0,
        (unsigned long)g_state.age_min, (unsigned long)g_state.interactions,
        ACTION_NAMES[clamp_int(g_selected, 0, ACT_COUNT - 1)],
        msg_valid(now_ms()) ? g_msg : "");
}

static void write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        buf += n;
        len -= (size_t)n;
    }
}

static void http_send(int fd, int code, const char *ctype, const char *body)
{
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n",
        code, code == 200 ? "OK" : (code == 404 ? "Not Found" : "Bad Request"),
        ctype, (int)strlen(body));
    if (n > 0) write_all(fd, hdr, (size_t)n);
    write_all(fd, body, strlen(body));
}

static int http_do_action(const char *act)
{
    if (strcmp(act, "feed") == 0) { do_feed(); return 0; }
    if (strcmp(act, "play") == 0) { do_play(); return 0; }
    if (strcmp(act, "pet") == 0) { do_pet(); return 0; }
    if (strcmp(act, "sleep") == 0) { do_toggle_sleep(); return 0; }
    if (strcmp(act, "wake") == 0) { do_wake(); return 0; }
    return -1;
}

static void http_handle_client(int c)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    tv.tv_usec = 150000;
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    char req[640];
    size_t used = 0;
    req[0] = '\0';
    for (int tries = 0; tries < 3 && used < sizeof(req) - 1; tries++) {
        ssize_t n = recv(c, req + used, sizeof(req) - 1 - used, 0);
        if (n <= 0) break;
        used += (size_t)n;
        req[used] = '\0';
        if (strchr(req, '\n')) break;
    }
    if (used == 0) { close(c); return; }

    char method[8] = {0}, path[192] = {0};
    if (sscanf(req, "%7s %191s", method, path) != 2) {
        http_send(c, 400, "text/plain", "bad request");
        close(c);
        return;
    }
    if (strcmp(method, "GET") != 0) {
        http_send(c, 400, "text/plain", "GET only");
        close(c);
        return;
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        http_send(c, 200, "text/html; charset=utf-8", PAGE);
    } else if (strcmp(path, "/state") == 0) {
        char body[512];
        build_state_json(body, sizeof(body));
        http_send(c, 200, "application/json", body);
    } else if (strncmp(path, "/action", 7) == 0) {
        char act[16] = {0};
        const char *q = strchr(path, '?');
        if (q) {
            const char *d = strstr(q, "do=");
            if (d) {
                d += 3;
                int i = 0;
                while (d[i] && d[i] != '&' && i < (int)sizeof(act) - 1) {
                    act[i] = d[i];
                    i++;
                }
                act[i] = '\0';
            }
        }
        int rc = http_do_action(act);
        char body[512];
        build_state_json(body, sizeof(body));
        http_send(c, rc == 0 ? 200 : 400, "application/json", body);
        if (g_debug) {
            printf("HTTP action=%s rc=%d msg=%s\n", act, rc, g_msg);
            fflush(stdout);
        }
    } else if (strcmp(path, "/favicon.ico") == 0) {
        http_send(c, 200, "text/plain", "");
    } else {
        http_send(c, 404, "text/plain", "not found");
    }
    close(c);
}

static int http_init(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(HTTP_PORT);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        fprintf(stderr, "http bind %d failed: %s\n", HTTP_PORT, strerror(errno));
        close(s);
        return -1;
    }
    if (listen(s, 4) < 0) {
        perror("listen");
        close(s);
        return -1;
    }
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    g_srv_fd = s;
    if (g_debug) {
        printf("HTTP listening on 0.0.0.0:%d\n", HTTP_PORT);
        fflush(stdout);
    }
    return 0;
}

static void http_poll(void)
{
    if (g_srv_fd < 0) return;
    int c = accept(g_srv_fd, NULL, NULL);
    if (c < 0) return;
    http_handle_client(c);
}

/* ---------------- key handling ---------------- */

static void on_key_event(int value)
{
    long long now = now_ms();
    if (value == 1) {
        if (g_key_down) return;
        g_key_down = 1;
        g_press_at = now;
        if (g_single_pending) {
            g_single_pending = 0;
            g_double_candidate = 1;
        }
        g_dirty = 1;
        g_force_render = 1;
    } else if (value == 0) {
        if (!g_key_down) return;
        g_key_down = 0;
        long long dur = now - g_press_at;
        if (dur >= LONG_MS) {
            g_single_pending = 0;
            g_double_candidate = 0;
            do_long_press();
        } else if (g_double_candidate) {
            g_double_candidate = 0;
            do_double_press();
        } else {
            g_single_pending = 1;
            g_single_deadline = now + DOUBLE_MS;
        }
        g_dirty = 1;
    }
}

static void tick_button(long long now)
{
    if (g_single_pending && now >= g_single_deadline) {
        g_single_pending = 0;
        do_short_press();
        g_dirty = 1;
    }
}

struct input_event_local {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

static void drain_events(void)
{
    struct input_event_local ev;
    while (read(g_ev_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_SYN) {
            if (ev.code == SYN_DROPPED) {
                g_key_down = 0;
                g_single_pending = 0;
                g_double_candidate = 0;
            }
            continue;
        }
        if (ev.type == EV_KEY && ev.code == KEY_POWER)
            on_key_event(ev.value);
    }
}

/* ---------------- main loop ---------------- */

#ifndef PET_SIM
static int frame_interval(long long now)
{
    if (g_state.sleeping) return 500;
    if (msg_valid(now) || now < g_transient_until) return 120;
    return 220;
}

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
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

static void run_loop(void)
{
    long long next_frame = now_ms();
    long long next_min = now_ms() + 60000;
    long long next_wake = now_ms() + 2000;

    while (!g_quit) {
        long long now = now_ms();
        tick_button(now);

        if (now >= next_min) {
            int mins = (int)((now - next_min) / 60000) + 1;
            apply_minutes(mins);
            next_min = now + 60000;
            g_dirty = 1;
        }

        if (now >= next_wake) {
            lcd_wake();
            g_dirty = 1;
            next_wake = now + 2000;
        }

        if (!g_state.sleeping && now >= g_next_auto_msg && !msg_valid(now)) {
            set_msg(status_msg(), 2300);
            g_next_auto_msg = now + 30000;
        }

        if (now >= g_next_blink && g_next_blink > 0) {
            g_blink_until = now + 130;
            g_next_blink = now + 2600 + rand() % 3000;
            g_dirty = 1;
        }

        if (now >= next_frame || g_force_render) {
            render_frame(now);
            g_force_render = 0;
            next_frame = now + frame_interval(now);
        }

        if (g_dirty)
            flush_fb();

        if (g_state_dirty && now - g_last_save >= 120000)
            save_state();

        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        if (g_ev_fd >= 0) {
            FD_SET(g_ev_fd, &rfds);
            maxfd = g_ev_fd;
        }
        if (g_srv_fd >= 0) {
            FD_SET(g_srv_fd, &rfds);
            if (g_srv_fd > maxfd) maxfd = g_srv_fd;
        }
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int r = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (r > 0) {
            if (g_ev_fd >= 0 && FD_ISSET(g_ev_fd, &rfds))
                drain_events();
            if (g_srv_fd >= 0 && FD_ISSET(g_srv_fd, &rfds))
                http_poll();
        }
    }
}

static int run_status(void)
{
    init_save_paths();
    load_state();
    char body[512];
    build_state_json(body, sizeof(body));
    printf("%s\n", body);
    return 0;
}

static int run_reset(void)
{
    init_save_paths();
    unlink(g_save_path);
    unlink(g_save_bak);
    unlink(g_save_tmp);
    printf("state reset\n");
    return 0;
}

static void usage(const char *argv0)
{
    printf("usage: %s [--debug] [--minutes N] [--nohttp] [--status] [--reset]\n"
           "       %s --restore FILE\n", argv0, argv0);
}

#endif /* PET_SIM */

#ifndef PET_SIM
int main(int argc, char **argv)
{
    int minutes = 0;
    const char *restore = NULL;

    setvbuf(stdout, NULL, _IOLBF, 0);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) g_debug = 1;
        else if (strcmp(argv[i], "--nohttp") == 0) g_nohttp = 1;
        else if (strcmp(argv[i], "--status") == 0) return run_status();
        else if (strcmp(argv[i], "--reset") == 0) return run_reset();
        else if (strcmp(argv[i], "--restore") == 0 && i + 1 < argc) restore = argv[++i];
        else if (strcmp(argv[i], "--minutes") == 0 && i + 1 < argc) minutes = atoi(argv[++i]);
        else { usage(argv[0]); return 2; }
    }

    if (restore) return run_restore(restore);

    install_signals();
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    init_save_paths();
    load_state();
    g_selected = ACT_FEED;
    g_next_blink = now_ms() + 2200;
    g_next_auto_msg = now_ms() + 12000;
    g_last_save = now_ms();

    g_ev_fd = open("/dev/event0", O_RDONLY | O_NONBLOCK);
    if (g_ev_fd < 0) {
        perror("open /dev/event0");
        return 1;
    }
    if (ioctl(g_ev_fd, EVIOCGRAB, 1) < 0) {
        if (g_debug) perror("EVIOCGRAB");
    } else if (g_debug) {
        printf("EVGRAB ok\n");
    }

    if (fb_open(1) < 0) {
        close(g_ev_fd);
        return 1;
    }

    lcd_wake();

    if (!g_nohttp)
        http_init();

    if (g_debug) {
        printf("PETCARD start stage=%d hunger=%d mood=%d energy=%d exp=%lu\n",
               g_state.stage, pct(g_state.hunger), pct(g_state.mood),
               pct(g_state.energy), (unsigned long)g_state.exp);
        fflush(stdout);
    }

    set_msg("HELLO!", 3000);
    if (minutes > 0) alarm((unsigned)minutes * 60);

    run_loop();

    save_state();
    if (g_fb_fd >= 0) fb_restore_and_close();
    if (g_ev_fd >= 0) {
        ioctl(g_ev_fd, EVIOCGRAB, 0);
        close(g_ev_fd);
    }
    if (g_srv_fd >= 0) close(g_srv_fd);
    if (g_debug) {
        printf("PETCARD exit flushes=%d avg_us=%lld max_us=%lld\n",
               g_flush_count, g_flush_count ? g_flush_us_sum / g_flush_count : 0,
               g_flush_us_max);
        fflush(stdout);
    }
    return 0;
}
#endif /* PET_SIM */
