/* Host-side renderer/self-test for PetCore. */
#define PET_SIM 1
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static long long g_sim_now = 1720000000000LL;
static long long now_ms(void) { return g_sim_now; }

#include "petcard.c"

static int g_fail;

static void check(int cond, const char *what)
{
    printf("%s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) g_fail++;
}

static void dump_raw(const char *dir, const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.raw", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { printf("FAIL open %s\n", path); g_fail++; return; }
    fwrite(g_canvas, 2, W * H, f);
    fclose(f);
    printf("FRAME %s\n", path);
}

static void scenario(const char *dir, const char *name, int stage,
                     int hunger, int mood, int energy, int intimacy,
                     int sleeping, int trans, const char *msg)
{
    g_state.stage = (uint8_t)stage;
    g_state.hunger = (uint16_t)hunger;
    g_state.mood = (uint16_t)mood;
    g_state.energy = (uint16_t)energy;
    g_state.intimacy = (uint16_t)intimacy;
    g_state.sleeping = (uint8_t)sleeping;
    g_transient_emo = trans;
    g_transient_until = trans >= 0 ? g_sim_now + 100000 : 0;
    g_msg[0] = '\0';
    if (msg) set_msg(msg, 100000);
    g_blink_until = 0;
    render_frame(g_sim_now);
    dump_raw(dir, name);
}

static void test_sprite_assets(void)
{
    static const int emotions[] = {EMO_HAPPY, EMO_HUNGRY, EMO_TIRED, EMO_ANGRY, EMO_SLEEP};
    static const char *const names[] = {"happy", "hungry", "sleepy", "grumpy", "sleep"};
    for (size_t i = 0; i < sizeof(emotions) / sizeof(emotions[0]); i++) {
        g_sprite_type = -1;
        g_sprite_loaded = 0;
        fill_rect(0, 0, W, H, C_BG);
        draw_pet(emotions[i], g_sim_now);
        char check_name[64];
        snprintf(check_name, sizeof(check_name), "%s sprite set loads and draws", names[i]);
        int pixels = 0;
        for (int y = 24; y < 104; y++)
            for (int x = 24; x < 104; x++)
                if (g_canvas[y * W + x] != C_BG) pixels++;
        check(g_sprite_loaded && pixels > 500, check_name);
    }
    check(memcmp(g_sprite_frames[0], g_sprite_frames[1], SPRITE_FRAME_BYTES) != 0,
          "sprite animation frames differ");
    check(sprite_scale_for_stage(3) > sprite_scale_for_stage(0),
          "sprite grows with pet stage");

    uint16_t frozen_frame[W * H];
    g_freeze_sprite_frame = 1;
    fill_rect(0, 0, W, H, C_BG);
    draw_pet(EMO_HUNGRY, g_sim_now);
    memcpy(frozen_frame, g_canvas, sizeof(frozen_frame));
    fill_rect(0, 0, W, H, C_BG);
    draw_pet(EMO_HUNGRY, g_sim_now + 220);
    check(memcmp(frozen_frame, g_canvas, sizeof(frozen_frame)) == 0,
          "freeze option keeps the same sprite frame over time");
    g_freeze_sprite_frame = 0;
}

static void reset_for_test(void)
{
    state_defaults();
    g_state.stage = 0;
    g_state.hunger = 3000;
    g_state.mood = 5000;
    g_state.energy = 8000;
    g_state.intimacy = 1000;
    g_transient_until = 0;
    g_selected = ACT_FEED;
    g_state.sleeping = 0;
    g_msg[0] = '\0';
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "work/sim";
    mkdir(dir, 0755);

    state_defaults();
    scenario(dir, "idle", 0, 3000, 7000, 8000, 1500, 0, -1, "HELLO!");
    scenario(dir, "happy", 1, 2000, 9000, 8000, 4000, 0, EMO_HAPPY, NULL);
    scenario(dir, "hungry", 2, 8500, 5000, 7000, 2000, 0, EMO_HUNGRY, "SO HUNGRY...");
    scenario(dir, "tired", 1, 3000, 4000, 1500, 2000, 0, EMO_TIRED, "SO SLEEPY...");
    scenario(dir, "angry", 1, 3000, 1500, 7000, 1000, 0, EMO_ANGRY, "GRR...");
    scenario(dir, "sleep", 2, 3000, 7000, 3000, 2000, 1, -1, "ZZZ...");
    scenario(dir, "eat", 1, 5000, 7000, 8000, 3000, 0, EMO_EAT, "YUM!");
    scenario(dir, "play", 1, 3000, 8000, 6000, 3000, 0, EMO_PLAY, "FUN!");
    scenario(dir, "pet", 1, 3000, 8000, 7000, 5000, 0, EMO_PET, "HEHE~");
    scenario(dir, "adult", 3, 3000, 8000, 8000, 9000, 0, EMO_HAPPY, NULL);
    test_sprite_assets();

    check(g_panel_rgb565_be == 1, "RGB565 big-endian panel format is default");
    check(encode_panel_pixel(RGB(255, 0, 0)) == 0x00f8 &&
          encode_panel_pixel(RGB(0, 255, 0)) == 0xe007 &&
          encode_panel_pixel(RGB(0, 0, 255)) == 0x1f00,
          "RGB565 panel emits MSB-first primary colors");
    check(encode_panel_pixel(C_BG) == 0x6308 &&
          encode_panel_pixel(RGB(128, 128, 128)) == 0x1084 &&
          encode_panel_pixel(RGB(0, 0, 0)) == 0 &&
          encode_panel_pixel(RGB(255, 255, 255)) == 0xffff,
          "RGB565 panel preserves dark background and neutral tones");
    g_panel_rgb565_be = 0;
    check(encode_panel_pixel(C_BG) == 0x1841,
          "legacy panel encoding remains available for comparison");

    reset_for_test();
    int h0 = g_state.hunger;
    do_feed();
    check(g_state.hunger < h0, "feed lowers hunger");
    check(g_state.exp == 2, "feed grants exp");
    check(strcmp(g_msg, "YUM!") == 0, "feed message");

    reset_for_test();
    int m0 = g_state.mood, e0 = g_state.energy;
    do_play();
    check(g_state.mood > m0, "play raises mood");
    check(g_state.energy < e0, "play costs energy");

    reset_for_test();
    int i0 = g_state.intimacy;
    do_pet();
    check(g_state.intimacy > i0, "pet raises intimacy");

    reset_for_test();
    g_state.sleeping = 1;
    g_state.energy = 3000;
    apply_minutes(60);
    check(g_state.energy == 10000 && g_state.sleeping == 0, "sleep restores energy and wakes");

    reset_for_test();
    g_state.exp = 29;
    g_state.stage = 0;
    g_state.exp += 1;
    update_stage();
    check(g_state.stage == 1, "stage 1 at 30 exp");
    g_state.exp = 100;
    update_stage();
    check(g_state.stage == 2, "stage 2 at 100 exp");
    g_state.exp = 250;
    update_stage();
    check(g_state.stage == 3, "stage 3 at 250 exp");

    reset_for_test();
    g_state.exp = 55;
    g_state.hunger = 4444;
    g_state.mood = 6666;
    g_state.energy = 7777;
    g_state.intimacy = 3333;
    init_save_paths();
    remove("/tmp/petcard_sim_state/state.bin");
    remove("/tmp/petcard_sim_state/state.bak");
    remove("/tmp/petcard_sim_state/state.tmp");
    check(save_state() == 0, "save state");
    state_defaults();
    g_state.exp = 0;
    load_state();
    check(g_state.exp == 55 && g_state.hunger == 4444 && g_state.mood == 6666,
          "load state roundtrip");

    reset_for_test();
    init_save_paths();
    remove("/tmp/petcard_sim_state_fb/state.bin");
    remove("/tmp/petcard_sim_state_fb/state.bak");
    remove("/tmp/petcard_sim_state_fb/state.tmp");
    snprintf(g_save_path, sizeof(g_save_path), "/proc/nope/state.bin");
    snprintf(g_save_bak, sizeof(g_save_bak), "/proc/nope/state.bak");
    snprintf(g_save_tmp, sizeof(g_save_tmp), "/proc/nope/state.tmp");
    g_save_fallback = 0;
    check(save_state() == 0, "save fallback succeeds");
    check(access("/tmp/petcard_sim_state_fb/state.bin", R_OK) == 0,
          "fallback file exists");
    init_save_paths();

    reset_for_test();
    on_key_event(1);
    g_sim_now += 100;
    on_key_event(0);
    g_sim_now += DOUBLE_MS + 20;
    tick_button(g_sim_now);
    check(g_selected == ACT_PLAY, "short press cycles action");

    reset_for_test();
    h0 = g_state.hunger;
    on_key_event(1);
    g_sim_now += LONG_MS + 100;
    on_key_event(0);
    check(g_state.hunger < h0, "long press feeds");

    reset_for_test();
    i0 = g_state.intimacy;
    on_key_event(1);
    g_sim_now += 80;
    on_key_event(0);
    g_sim_now += 120;
    on_key_event(1);
    g_sim_now += 80;
    on_key_event(0);
    check(g_state.intimacy > i0, "double press pets");

    char j[512];
    build_state_json(j, sizeof(j));
    check(strstr(j, "\"hunger\":") != NULL && strstr(j, "\"ok\":1") != NULL, "json output");

    printf(g_fail ? "RESULT FAIL %d\n" : "RESULT PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
