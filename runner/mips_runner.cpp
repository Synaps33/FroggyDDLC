#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "libretro.h"

static int s_w = 320, s_h = 240;
static uint16_t s_frame[640 * 480];
static int s_nonzero = 0;

static void video_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!data) return;
    s_w = width; s_h = height;
    const uint8_t *src = (const uint8_t*)data;
    for (int y = 0; y < s_h; y++) memcpy(&s_frame[y * 640], src + y * pitch, s_w * 2);
}
static void audio_cb(int16_t l, int16_t r) { if (l || r) s_nonzero++; }
static size_t audio_batch_cb(const int16_t *data, size_t frames) {
    for (size_t i = 0; i < frames * 2; i++) if (data[i]) s_nonzero++;
    return frames;
}
static void input_poll_cb(void) {}
static uint32_t s_pad = 0;
static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_A && (s_pad & 1)) return 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_B && (s_pad & 2)) return 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_UP && (s_pad & 4)) return 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_DOWN && (s_pad & 8)) return 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_LEFT && (s_pad & 16)) return 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_RIGHT && (s_pad & 32)) return 1;
    return 0;
}
static bool environ_cb(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_RGB565;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)data = "."; return true;
        default: return false;
    }
}
static void dump_ppm(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", s_w, s_h);
    for (int y = 0; y < s_h; y++) for (int x = 0; x < s_w; x++) {
        uint16_t p = s_frame[y * 640 + x];
        uint8_t rgb[3] = { (uint8_t)(((p >> 11) & 31) * 255 / 31), (uint8_t)(((p >> 5) & 63) * 255 / 63), (uint8_t)((p & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static int dbg_int(const char *s, const char *key, int def) {
    const char *p = strstr(s, key);
    if (!p) return def;
    return atoi(p + strlen(key));
}
static const char *dbg_state(const char *s) {
    static char buf[64];
    const char *p = strstr(s, "state=");
    if (!p) return "";
    p += 6;
    int i = 0;
    while (p[i] && p[i] != ' ' && i < 63) { buf[i] = p[i]; i++; }
    buf[i] = 0;
    return buf;
}

extern "C" void retro_init(void);
extern "C" void retro_deinit(void);
extern "C" void retro_set_environment(retro_environment_t);
extern "C" void retro_set_video_refresh(retro_video_refresh_t);
extern "C" void retro_set_audio_sample(retro_audio_sample_t);
extern "C" void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
extern "C" void retro_set_input_poll(retro_input_poll_t);
extern "C" void retro_set_input_state(retro_input_state_t);
extern "C" bool retro_load_game(const struct retro_game_info*);
extern "C" void retro_run(void);
extern "C" void ddlc_debug_query(char*, int);

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s game [frames] [dump_every] [prefix]\n", argv[0]); return 1; }
    int total = argc > 2 ? atoi(argv[2]) : 3600;
    int every = argc > 3 ? atoi(argv[3]) : 0;
    const char *prefix = argc > 4 ? argv[4] : "shot";
    retro_set_environment(environ_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);
    retro_init();
    struct retro_game_info game; memset(&game, 0, sizeof(game)); game.path = argv[1];
    if (!retro_load_game(&game)) { fprintf(stderr, "load failed\n"); return 2; }
    mkdir("shots", 0755);
    int last = -1000;
    char dbg[256];
    for (int frame = 0; frame < total; frame++) {
        ddlc_debug_query(dbg, sizeof(dbg));
        const char *state = dbg_state(dbg);
        int cl = dbg_int(dbg, "cl=", 0);
        int x = dbg_int(dbg, "x=", 0);
        int menu = dbg_int(dbg, "menu=", 0);
        int kb = dbg_int(dbg, "kb=", 0);
        int cooldown = 18;
        if (strcmp(state, "load") == 0 || strcmp(state, "splash") == 0 || strcmp(state, "splash2") == 0) cooldown = 99999;
        else if (strcmp(state, "title") == 0) cooldown = 60;
        else if (cl == 10021 && x < 140) cooldown = 99999;
        else if (kb || menu) cooldown = 30;
        if (frame - last >= cooldown) { s_pad = 1; last = frame; } else s_pad = 0;
        retro_run();
        if (every > 0 && (frame % every == 0 || frame == total - 1)) {
            char path[256]; snprintf(path, sizeof(path), "%s_%05d.ppm", prefix, frame); dump_ppm(path);
        }
    }
    printf("done frames=%d audio_nonzero=%d\n", total, s_nonzero);
    retro_deinit();
    return 0;
}
